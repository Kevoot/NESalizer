// This implementation makes use of the fact that the 6502 does a memory access
// (either a read or a write) for every cycle, by running the other components
// (the PPU and the APU) from the read_mem() and write_mem() functions. This
// approach simplifies the code provided all accesses (including dummy
// accesses) are emulated.

#include "common.h"

#include "apu.h"
#include "audio.h"
#include "controller.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "opcodes.h"
#include "ppu.h"
#include "rom.h"
#include "save_states.h"
#include "sdl_backend.h"
#include "timing.h"

//
// Event signaling
//

// Set true when an event needs to be handled at the next instruction boundary.
// Avoids having to check them all for each instruction. This includes
// interrupts, end-of-frame operations, state transfers, (soft) reset, and
// shutdown.
static bool pending_event;

static bool pending_end_emulation;
static bool pending_frame_completion;
static bool pending_reset;

void end_emulation()   { pending_event = pending_end_emulation = true; }
void frame_completed() { pending_event = pending_frame_completion = true; }
void soft_reset()      { pending_event = pending_reset = true; }

// Set true if interrupt polling detects a pending IRQ or NMI. The next
// "instruction" executed is the interrupt sequence.
static bool pending_irq;
static bool pending_nmi;

//
// RAM, registers, status flags, and misc. state
//

static uint8_t ram[0x800];

// Possible optimization: Making some of the variables a natural size for the
// implementation architecture might be faster. CPU emulation is already
// relatively speedy though, and we wouldn't get automatic wrapping.

// Registers
static uint16_t pc;
static uint8_t a, s, x, y;

// Status flags

// The value the zero and negative flags are based on. Storing these together
// turns the setting of the flags into a simple assignment in most cases.
//
//  - !(zn & 0xFF) means the zero flag is set.
//  - zn & 0x180 means the negative flag is set.
//
// Having zn & 0x100 also indicate that the negative flag is set allows the two
// flags to be set separately, which is required by the BIT instruction and
// when pulling flags from the stack.
static unsigned zn;

static bool carry;
static bool irq_disable;
static bool decimal;
static bool overflow;

// The byte after the opcode byte. Always fetched, so factoring out the fetch
// saves logic.
static uint8_t op_1;

bool cpu_is_reading;
uint8_t cpu_data_bus;

//
// PPU and APU interface
//

unsigned frame_offset;

// Down counter for adding an extra PPU tick for PAL
static unsigned pal_extra_tick;

void tick() {
    // For NTSC, there are exactly three PPU ticks per CPU cycle. For PAL the
    // number is 3.2, which is emulated by adding an extra PPU tick every fifth
    // call. (This isn't perfect, but about as good as we can do without getting
    // into super-obscure hardware behavior, including PPU half-ticks and analog
    // effects.)
    if (is_pal) {
        if (--pal_extra_tick == 0) {
            pal_extra_tick = 5;
            tick_pal_ppu();
        }
        tick_pal_ppu();
        tick_pal_ppu();
        tick_pal_ppu();
    }
    else {
        tick_ntsc_ppu();
        tick_ntsc_ppu();
        tick_ntsc_ppu();
    }

    tick_apu();
    ++frame_offset;
}

//
// CPU reading and writing
//

// Optimization for read/write ticks without visible side effects

static void read_tick() {
    cpu_is_reading = true;
    tick();
}

static void write_tick() {
    cpu_is_reading = false;
    tick();
}


uint8_t read_mem(uint16_t addr) {
    read_tick();

    uint8_t res;

    switch (addr) {
    case 0x0000 ... 0x1FFF: res = ram[addr & 0x7FF];      break;
    case 0x2000 ... 0x3FFF: res = read_ppu_reg(addr & 7); break;
    case 0x4015           : res = read_apu_status();      break;
    case 0x4016           : res = read_controller(0);     break;
    case 0x4017           : res = read_controller(1);     break;
    case 0x4018 ... 0x5FFF: res = mapper_fns.read(addr);  break; // General enough?
    case 0x6000 ... 0x7FFF:
        // WRAM/SRAM. Returns open bus if none present.
        res = wram_6000_page ? wram_6000_page[addr & 0x1FFF] : cpu_data_bus;
        break;
    case 0x8000 ... 0xFFFF: res = read_prg(addr);         break;
    default:                res = cpu_data_bus;           break; // Open bus
    }

    cpu_data_bus = res;
    return res;
}

static void write_mem(uint8_t val, uint16_t addr) {
    // TODO: The write probably takes effect earlier within the CPU cycle than
    // after the three PPU ticks and the one APU tick

    write_tick();

    cpu_data_bus = val;

    switch (addr) {
    case 0x0000 ... 0x1FFF: ram[addr & 0x7FF] = val;      break;
    case 0x2000 ... 0x3FFF: write_ppu_reg(val, addr & 7); break;

    case 0x4000: write_pulse_reg_0(0, val); break;
    case 0x4001: write_pulse_reg_1(0, val); break;
    case 0x4002: write_pulse_reg_2(0, val); break;
    case 0x4003: write_pulse_reg_3(0, val); break;

    case 0x4004: write_pulse_reg_0(1, val); break;
    case 0x4005: write_pulse_reg_1(1, val); break;
    case 0x4006: write_pulse_reg_2(1, val); break;
    case 0x4007: write_pulse_reg_3(1, val); break;

    case 0x4008: write_triangle_reg_0(val); break;
    case 0x400A: write_triangle_reg_1(val); break;
    case 0x400B: write_triangle_reg_2(val); break;

    case 0x400C: write_noise_reg_0(val); break;
    case 0x400E: write_noise_reg_1(val); break;
    case 0x400F: write_noise_reg_2(val); break;

    case 0x4010: write_dmc_reg_0(val); break;
    case 0x4011: write_dmc_reg_1(val); break;
    case 0x4012: write_dmc_reg_2(val); break;
    case 0x4013: write_dmc_reg_3(val); break;

    case 0x4014: do_oam_dma(val); break;

    case 0x4015: write_apu_status(val);            break;
    case 0x4016: write_controller_strobe(val & 1); break;
    case 0x4017: write_frame_counter(val);         break;

    case 0x6000 ... 0x7FFF:
        // SRAM/WRAM/PRG RAM
        if (wram_6000_page) wram_6000_page[addr & 0x1FFF] = val;
        break;

    case 0x8000 ... 0xFFFF: write_prg(addr, val); break;
    }

    // An alternative to letting the mapper see all writes would be to have
    // separate functions for common address ranges that trigger mapper
    // operations
    mapper_fns.write(val, addr);
}

//
// Core instruction logic (reused for different addressing modes)
//

static void and_(uint8_t);
static uint8_t lsr(uint8_t);
static void sbc(uint8_t);

static void adc(uint8_t arg) {
    unsigned const sum = a + arg + carry;
    carry = sum > 0xFF;
    // The overflow flag is set when the sign of the addends is the same and
    // differs from the sign of the sum
    overflow = ~(a ^ arg) & (a ^ sum) & 0x80;
    zn = a /* (uint8_t) */ = sum;
}

// Unofficial
static void alr(uint8_t arg) {
    a = lsr(a & arg);
}

// Unofficial
static void anc(uint8_t arg) {
    and_(arg);
    carry = zn & 0x180; // Copy negative flag to carry flag
}

// 'and' is an operator in C++, so we need the underscore
static void and_(uint8_t arg) {
    zn = (a &= arg);
}

// Unofficial
static void arr(uint8_t arg) {
    zn = a = (carry << 7) | ((a & arg) >> 1);
    carry = a & 0x40;
    overflow = (a ^ (a << 1)) & 0x40;
}

static uint8_t asl(uint8_t arg) {
    carry = arg & 0x80;
    return zn = (arg << 1) & 0xFF;
}

// Unofficial
static void atx(uint8_t arg) {
    // Assume '(A | 0xFF) & arg' is calculated, which is the same as just 'arg':
    // http://forums.nesdev.com/viewtopic.php?t=3831
    zn = x = a = arg;
}

// Unofficial
static void axs(uint8_t arg) {
    carry = (a & x) >= arg;
    zn = x /* (uint8_t) */ = (a & x) - arg;
}

static void bit(uint8_t arg) {
    overflow = arg & 0x40;
    // Set the zero and negative flags separately by using bit 8 of zn for the
    // negative flag
    zn = ((arg << 1) & 0x100) | (a & arg);
}

// CMP, CPX, CPY
static void comp(uint8_t reg, uint8_t arg) {
    carry = reg >= arg;
    zn = uint8_t(reg - arg);
}

// Unofficial
static uint8_t dcp(uint8_t arg) {
    comp(a, --arg);
    return arg;
}

static uint8_t dec(uint8_t arg) {
    // Works without & 0xFF here since bit 8 (the additional negative flag bit)
    // will only get set if arg is 0, in which case bit 7 gets set as well
    return zn = arg - 1;
}

static void eor(uint8_t arg) {
    zn = (a ^= arg);
}

static uint8_t inc(uint8_t arg) {
    return zn = (arg + 1) & 0xFF;
}

// Unofficial
static void las(uint8_t arg) {
    zn = a = x = s = arg & s;
}

// Unofficial
static void lax(uint8_t arg) {
    zn = a = x = arg;
}

static void lda(uint8_t arg) { zn = a = arg; }
static void ldx(uint8_t arg) { zn = x = arg; }
static void ldy(uint8_t arg) { zn = y = arg; }

static uint8_t lsr(uint8_t arg) {
    carry = arg & 1;
    return zn = arg >> 1;
}

static void ora(uint8_t arg) {
    zn = (a |= arg);
}

// Unofficial
static uint8_t isc(uint8_t arg) {
    sbc(++arg);
    return arg;
}

// Unofficial
static uint8_t rla(uint8_t arg) {
    uint8_t const res = (arg << 1) | carry;
    carry = arg & 0x80;
    and_(res);
    return res;
}

static uint8_t rol(uint8_t arg) {
    zn = uint8_t((arg << 1) | carry);
    carry = arg & 0x80;
    return zn;
}

static uint8_t ror(uint8_t arg) {
    zn = (carry << 7) | (arg >> 1);
    carry = arg & 1;
    return zn;
}

// Unofficial
static uint8_t rra(uint8_t arg) {
    uint8_t const res = (carry << 7) | (arg >> 1);
    carry = arg & 1;
    adc(res);
    return res;
}

static void sbc(uint8_t arg) { adc(~arg); /* -arg - 1 */ }

// Unofficial
static uint8_t slo(uint8_t arg) {
    carry = arg & 0x80;
    ora(arg <<= 1);
    return arg;
}

// Unofficial
static uint8_t sre(uint8_t arg) {
    carry = arg & 1;
    eor(arg >>= 1);
    return arg;
}

// Unofficial
static void xaa(uint8_t arg) {
    // http://visual6502.org/wiki/index.php?title=6502_Opcode_8B_%28XAA,_ANE%29
    // Nestopia uses 0xEE as the magic constant.
    zn = a = (a | 0xEE) & x & arg;
}

// Conditional branches

static void poll_for_interrupt();

static void branch_if(bool cond) {
    ++pc;
    if (cond) {
        read_mem(pc); // Dummy read
        // TODO: Unsafe unsigned->signed conversion - likely to work in
        // practice
        uint16_t const new_pc = pc + (int8_t)op_1;
        if ((pc ^ new_pc) & 0x100) { // Page crossing?
            // Branch instructions perform additional interrupt polling during
            // the fixup tick
            poll_for_interrupt();
            read_mem((pc & 0xFF00) | (new_pc & 0x00FF)); // Dummy read
        }
        pc = new_pc;
    }
}


// Stack manipulation

static void push(uint8_t val) {
    write_tick();
    ram[0x100 + s--] = val;
}

static uint8_t pull() {
    read_tick();
    return ram[0x100 + ++s];
}

static void push_flags(bool with_break_bit_set) {
    push(
      (!!(zn & 0x180)     << 7) | // Negative
      (overflow           << 6) |
      (1                  << 5) |
      (with_break_bit_set << 4) |
      (decimal            << 3) |
      (irq_disable        << 2) |
      (!(zn & 0xFF)       << 1) | // Zero
      carry);
}

static void pull_flags() {
    uint8_t const flags = pull();
    // flags & 0x82  pulls out the zero and negative flags.
    //
    // ^2            flips the zero flag, since we want 0 if it's set.
    //
    // << 1          moves the negative flag into the extra negative flag bit in
    //               zn, so that the negative and zero flags can be set
    //               separately. The zero flag moves to bit 3, which won't
    //               affect the result.
    zn          = ((flags & 0x82) ^ 2) << 1;
    overflow    = flags & 0x40;
    decimal     = flags & 0x08;
    irq_disable = flags & 0x04;
    carry       = flags & 0x01;
}


// Helpers for read-modify-write instructions, which perform a dummy write-back
// of the unmodified value

#define RMW(fn, addr)                                            \
    do {                                                         \
        uint16_t const addr_ = addr;                             \
        uint8_t const val = read_mem(addr_);                     \
        write_mem(val, addr_); /* Write back unmodified value */ \
        poll_for_interrupt();                                    \
        write_mem(fn(val), addr_);                               \
    } while(0)

// Optimized versions for zero page access, which never has side effects

#define ZERO_RMW(fn)                                   \
    do {                                               \
        ++pc;                                          \
        read_tick(); /* Read effective address */      \
        read_tick(); /* Write back unmodified value */ \
        poll_for_interrupt();                          \
        write_tick();                                  \
        ram[op_1] = fn(ram[op_1]);                     \
    } while(0)

#define ZERO_X_RMW(fn)                                  \
    do {                                                \
        ++pc;                                           \
        uint8_t const addr = op_1 + x;                  \
        read_tick(); /* Read address and add x to it */ \
        read_tick(); /* Read effective address */       \
        read_tick(); /* Write back unmodified value */  \
        poll_for_interrupt();                           \
        write_tick();                                   \
        ram[addr] = fn(ram[addr]);                      \
    } while(0)

//
// Addressing modes
//

// The *_addr() functions return addresses, the *_op() functions resolved
// operands

// Zero page addressing

static uint8_t get_zero_op() {
    ++pc;
    poll_for_interrupt();
    read_tick();
    return ram[op_1];
}

static uint8_t get_zero_xy_op(uint8_t index) {
    ++pc;
    read_tick(); // Read from address, add index
    poll_for_interrupt();
    read_tick();
    return ram[(op_1 + index) & 0xFF];
}

// Writing zero page never has side effects, so we can optimize a bit

static void zero_write(uint8_t val) {
    ++pc;
    poll_for_interrupt();
    write_tick();
    ram[op_1] = val;
}

static void zero_xy_write(uint8_t val, uint8_t index) {
    ++pc;
    read_tick(); // Read from address and add x to it
    poll_for_interrupt();
    write_tick();
    ram[(op_1 + index) & 0xFF] = val;
}


// Absolute addressing

static uint16_t get_abs_addr() {
    ++pc;
    return (read_mem(pc++) << 8) | op_1;
}

static uint8_t get_abs_op() {
    uint16_t const addr = get_abs_addr();
    poll_for_interrupt();
    return read_mem(addr);
}

static void abs_write(uint8_t val) {
    uint16_t const addr = get_abs_addr();
    poll_for_interrupt();
    write_mem(val, addr);
}


// Absolute,X/Y addressing

// Absolute,X/Y address fetching for write and read-modify-write instructions
static uint16_t get_abs_xy_addr_write(uint8_t index) {
    uint16_t const addr = get_abs_addr();
    read_mem((addr & 0xFF00) | ((addr + index) & 0x00FF)); // Dummy read
    return addr + index;
}

// Absolute,X/Y operand fetching for read instructions
static uint8_t get_abs_xy_op_read(uint8_t index) {
    uint16_t const addr     = get_abs_addr();
    uint16_t const new_addr = addr + index;
    if ((addr ^ new_addr) & 0x100) // Page crossing?
        read_mem(new_addr - 0x100); // Dummy read
    poll_for_interrupt();
    return read_mem(new_addr);
}

// Instructions that use this always write the accumulator, so we can omit the
// 'val' argument
static void abs_xy_write_a(uint8_t index) {
    uint16_t const addr = get_abs_xy_addr_write(index);
    poll_for_interrupt();
    write_mem(a, addr);
}


// (Indirect,X) addressing

static uint16_t get_ind_x_addr() {
    ++pc;
    read_tick(); // Read from address, add index
    read_tick(); // Fetch effective address low
    read_tick(); // Fetch effective address high
    uint8_t const zero_addr = op_1 + x;
    return (ram[(zero_addr + 1) & 0xFF] << 8) | ram[zero_addr];
}

static uint8_t get_ind_x_op() {
    uint16_t const addr = get_ind_x_addr();
    poll_for_interrupt();
    return read_mem(addr);
}

static void ind_x_write(uint8_t val) {
    uint16_t const addr = get_ind_x_addr();
    poll_for_interrupt();
    write_mem(val, addr);
}


// (Indirect),Y addressing

// (Indirect),Y helper for fetching the address from zero page
static uint16_t get_addr_from_zero_page() {
    ++pc;
    read_tick(); // Fetch effective address low
    read_tick(); // Fetch effective address high
    return (ram[(op_1 + 1) & 0xFF] << 8) | ram[op_1];
}

// (Indirect),Y address fetching for write and read-modify-write instructions
static uint16_t get_ind_y_addr_write() {
    uint16_t const addr = get_addr_from_zero_page();
    read_mem((addr & 0xFF00) | ((addr + y) & 0x00FF)); // Dummy read
    return addr + y;
}

// (Indirect),Y operand fetching for read instructions
static uint8_t get_ind_y_op_read() {
    uint16_t const addr = get_addr_from_zero_page();
    uint16_t const new_addr = addr + y;
    if ((addr ^ new_addr) & 0x100) // Page crossing?
        read_mem(new_addr - 0x100); // Dummy read
    poll_for_interrupt();
    return read_mem(new_addr);
}

// Single caller, always writes accumulator
static void ind_y_write_a() {
    uint16_t const addr = get_ind_y_addr_write();
    poll_for_interrupt();
    write_mem(a, addr);
}

// Helper function for implementing the weird unofficial write instructions
// (AXA, XAS, TAS, SAY) that are influenced by the high byte of the address
// plus one (due to an internal bus conflict). In page crossings, the high byte
// of the target address is corrupted similarly to the value.
static void unoff_addr_write(uint16_t addr, uint8_t val, uint8_t index) {
    uint16_t const new_addr = addr + index;
    read_mem((addr & 0xFF00) | (new_addr & 0x00FF)); // Dummy read
    poll_for_interrupt();
    write_mem(val & ((addr >> 8) + 1),
              ((addr ^ new_addr) & 0x100) ?
                (new_addr & (val << 8)) | (new_addr & 0x00FF) : // Page crossing
                new_addr);                                      // No page crossing
}

//
// Interrupts
//

// IRQ from mapper hardware on the cart
static bool cart_irq;

// The OR of all IRQ sources. Updated in update_irq_status().
static bool irq_line;

// Set true when a falling edge occurs on the NMI input
static bool nmi_asserted;

static void update_irq_status() {
    irq_line = cart_irq || dmc_irq || frame_irq;
}

void set_nmi(bool s) {
    nmi_asserted = s;
}

void set_cart_irq(bool s) {
    cart_irq = s;
    update_irq_status();
}

void set_dmc_irq(bool s) {
    dmc_irq = s;
    update_irq_status();
}

void set_frame_irq(bool s) {
    frame_irq = s;
    update_irq_status();
}

enum Interrupt_type { Int_BRK = 0, Int_IRQ, Int_NMI, Int_reset };

static void do_interrupt(Interrupt_type type) {
    static uint16_t const vector_addr[] =
      { 0xFFFE,   // Int_BRK
        0xFFFE,   // Int_IRQ
        0xFFFA }; // Int_NMI

    uint16_t vec_addr;

    // Two dummy reads
    if (type != Int_BRK) {
        // For BRK, these have already been done
        read_mem(pc);
        read_mem(pc);
    }

    if (type == Int_reset) {
        // Push operations, writes inhibited
        read_tick();
        read_tick();
        read_tick();
        s -= 3;
        vec_addr = 0xFFFC;
    }
    else {
        push(pc >> 8);
        push(pc & 0xFF);

        // Interrupt glitch. An NMI asserted here can override BRK and IRQ.
        if (nmi_asserted) {
            nmi_asserted = false;
            vec_addr     = 0xFFFA;
        }
        else
            vec_addr = vector_addr[(unsigned)type];

        push_flags(type == Int_BRK);
    }
    irq_disable = true;
    // No interrupt polling happens here; the first instruction of the
    // interrupt handler always executes before another interrupt is serviced
    pc  = read_mem(vec_addr);
    pc |= read_mem(vec_addr + 1) << 8;
}

// The interrupt lines are polled at the end of the second-to-last tick for
// most instructions, making interrupts a bit less straightforward to implement
// compared to if the polling was done at the very end. For addressing modes
// where all instructions poll interrupts in the same location we do the
// polling in the addressing mode routine to save code. Same goes for
// read-modify-write instructions, which all poll in the same location.
//
// See http://wiki.nesdev.com/w/index.php/CPU_interrupts as well.
static void poll_for_interrupt() {
    // If both NMI and IRQ have been asserted, the IRQ assertion is lost at
    // this polling point (as if IRQ had never been asserted). IRQ might still
    // be detected at the next polling point.
    //
    // This behavior has been confirmed in Visual 6502.
    if (nmi_asserted) {
        nmi_asserted = false;
        pending_event = pending_nmi = true;
    }
    else if (irq_line && !irq_disable)
        pending_event = pending_irq = true;
}

// Defined in tables.c. Indexed by opcode.
extern uint8_t const polls_irq_after_first_cycle[256];

//
// Main CPU loop
//

static void set_cpu_cold_boot_state();
static void reset_cpu();

// See pending_event
static void process_pending_events() {
    if (pending_nmi) {
        pending_nmi = false;
        do_interrupt(Int_NMI);
    }

    if (pending_irq) {
        pending_irq = false;
        do_interrupt(Int_IRQ);
    }

    if (pending_frame_completion) {
        pending_frame_completion = false;
        printf("Drawing Frame\n");
        draw_frame();
        end_audio_frame();
        begin_audio_frame();
        frame_offset = 0;
    }

    if (pending_reset) {
        pending_reset = false;
        // Reset the APU and PPU first since they should tick during the
        // CPU's reset sequence
        reset_apu();
        reset_ppu();
        reset_cpu();
    }
}

void run() {
    set_apu_cold_boot_state();
    set_cpu_cold_boot_state();
    set_ppu_cold_boot_state();

    init_timing();

    do_interrupt(Int_reset);

    for (;;) {

        if (pending_event) {
            pending_event = false;
            process_pending_events();

            if (pending_end_emulation)
                break;
        }


        uint8_t const opcode = read_mem(pc++);
        if (polls_irq_after_first_cycle[opcode])
            poll_for_interrupt();
        op_1 = read_mem(pc);

        // http://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables/
        // could possibly speed this up a bit (also,
        // https://www.cs.tcd.ie/David.Gregg/papers/toplas05.pdf). CPU
        // emulation seems to account for less than 5% of the runtime though,
        // so it might not be worth uglifying the code for.

        switch (opcode) {

        //
        // Accumulator or implied addressing
        //

        case BRK:
            ++pc;
            do_interrupt(Int_BRK);
            break;

        case RTI:
            read_tick(); // Corresponds to incrementing s
            pull_flags();
            pc = pull();
            poll_for_interrupt();
            pc |= pull() << 8;
            break;

        case RTS:
            {
            read_tick(); // Corresponds to incrementing s
            uint8_t const pc_low = pull();
            pc = ((pull() << 8) | pc_low) + 1;
            poll_for_interrupt();
            read_tick(); // Increment PC
            }
            break;

        case PHA:
            poll_for_interrupt();
            push(a);
            break;

        case PHP:
            poll_for_interrupt();
            push_flags(true);
            break;

        case PLA:
            read_tick(); // Corresponds to incrementing s
            poll_for_interrupt();
            zn = a = pull();
            break;

        case PLP:
            read_tick(); // Corresponds to incrementing s
            poll_for_interrupt();
            pull_flags();
            break;

        case ASL_ACC: a = asl(a); break;
        case LSR_ACC: a = lsr(a); break;
        case ROL_ACC: a = rol(a); break;
        case ROR_ACC: a = ror(a); break;

        case CLC: carry       = false; break;
        case CLD: decimal     = false; break;
        case CLI: irq_disable = false; break;
        case CLV: overflow    = false; break;
        case SEC: carry       = true;  break;
        case SED: decimal     = true;  break;
        case SEI: irq_disable = true;  break;

        case DEX: zn = --x; break;
        case DEY: zn = --y; break;
        case INX: zn = ++x; break;
        case INY: zn = ++y; break;

        case TAX: zn = x = a; break;
        case TAY: zn = y = a; break;
        case TSX: zn = x = s; break;
        case TXA: zn = a = x; break;
        case TXS:      s = x; break;
        case TYA: zn = a = y; break;

        // The "official" NOP and various unofficial NOPs with
        // accumulator/implied addressing
        case NOP: case NO0: case NO1: case NO2: case NO3: case NO4: case NO5:
            break;

        //
        // Immediate addressing
        //

        case ADC_IMM: adc(op_1);     ++pc; break;
        case ALR_IMM: alr(op_1);     ++pc; break; // Unofficial
        case AN0_IMM: anc(op_1);     ++pc; break; // Unofficial
        case AN1_IMM: anc(op_1);     ++pc; break; // Unofficial
        case AND_IMM: and_(op_1);    ++pc; break;
        case ARR_IMM: arr(op_1);     ++pc; break; // Unofficial
        case ATX_IMM: atx(op_1);     ++pc; break; // Unofficial
        case AXS_IMM: axs(op_1);     ++pc; break; // Unofficial
        case CMP_IMM: comp(a, op_1); ++pc; break;
        case CPX_IMM: comp(x, op_1); ++pc; break;
        case CPY_IMM: comp(y, op_1); ++pc; break;
        case EOR_IMM: eor(op_1);     ++pc; break;
        case LDA_IMM: lda(op_1);     ++pc; break;
        case LDX_IMM: ldx(op_1);     ++pc; break;
        case LDY_IMM: ldy(op_1);     ++pc; break;
        case ORA_IMM: ora(op_1);     ++pc; break;
        case SB2_IMM: // Unofficial, same as SBC
        case SBC_IMM: sbc(op_1);     ++pc; break;
        case XAA_IMM: xaa(op_1);     ++pc; break; // Unofficial

        // Unofficial NOPs with immediate addressing
        case NO0_IMM: case NO1_IMM: case NO2_IMM: case NO3_IMM: case NO4_IMM:
            ++pc;
            break;

        //
        // Absolute addressing
        //

        case JMP_ABS:
            poll_for_interrupt();
            pc = (read_mem(pc + 1) << 8) | op_1;
            break;

        case JSR_ABS:
            ++pc;

            read_tick(); // Internal operation

            push(pc >> 8);
            push(pc & 0xFF);

            poll_for_interrupt();
            pc = (read_mem(pc) << 8) | op_1;
            break;

        // Read instructions

        case ADC_ABS: adc(get_abs_op());     break;
        case AND_ABS: and_(get_abs_op());    break;
        case BIT_ABS: bit(get_abs_op());     break;
        case CMP_ABS: comp(a, get_abs_op()); break;
        case CPX_ABS: comp(x, get_abs_op()); break;
        case CPY_ABS: comp(y, get_abs_op()); break;
        case EOR_ABS: eor(get_abs_op());     break;
        case LAX_ABS: lax(get_abs_op());     break; // Unofficial
        case LDA_ABS: lda(get_abs_op());     break;
        case LDX_ABS: ldx(get_abs_op());     break;
        case LDY_ABS: ldy(get_abs_op());     break;
        case ORA_ABS: ora(get_abs_op());     break;
        case SBC_ABS: sbc(get_abs_op());     break;

        // Unofficial NOP with absolute addressing (acts like a read)
        case NOP_ABS: get_abs_op(); break;

        // Read-modify-write instructions

        case ASL_ABS: RMW(asl, get_abs_addr()); break;
        case DCP_ABS: RMW(dcp, get_abs_addr()); break; // Unofficial
        case DEC_ABS: RMW(dec, get_abs_addr()); break;
        case INC_ABS: RMW(inc, get_abs_addr()); break;
        case ISC_ABS: RMW(isc, get_abs_addr()); break; // Unofficial
        case LSR_ABS: RMW(lsr, get_abs_addr()); break;
        case RLA_ABS: RMW(rla, get_abs_addr()); break; // Unofficial
        case RRA_ABS: RMW(rra, get_abs_addr()); break; // Unofficial
        case ROL_ABS: RMW(rol, get_abs_addr()); break;
        case ROR_ABS: RMW(ror, get_abs_addr()); break;
        case SLO_ABS: RMW(slo, get_abs_addr()); break; // Unofficial
        case SRE_ABS: RMW(sre, get_abs_addr()); break; // Unofficial

        // Write instructions

        case SAX_ABS: abs_write(a & x); break; // Unofficial
        case STA_ABS: abs_write(a);     break;
        case STX_ABS: abs_write(x);     break;
        case STY_ABS: abs_write(y);     break;

        //
        // Zero page addressing
        //

        // Read instructions

        case ADC_ZERO: adc(get_zero_op());     break;
        case AND_ZERO: and_(get_zero_op());    break;
        case BIT_ZERO: bit(get_zero_op());     break;
        case CMP_ZERO: comp(a, get_zero_op()); break;
        case CPX_ZERO: comp(x, get_zero_op()); break;
        case CPY_ZERO: comp(y, get_zero_op()); break;
        case EOR_ZERO: eor(get_zero_op());     break;
        case LAX_ZERO: lax(get_zero_op());     break; // Unofficial
        case LDA_ZERO: lda(get_zero_op());     break;
        case LDX_ZERO: ldx(get_zero_op());     break;
        case LDY_ZERO: ldy(get_zero_op());     break;
        case ORA_ZERO: ora(get_zero_op());     break;
        case SBC_ZERO: sbc(get_zero_op());     break;

        // Read-modify-write instructions

        case ASL_ZERO: ZERO_RMW(asl); break;
        case DCP_ZERO: ZERO_RMW(dcp); break; // Unofficial
        case DEC_ZERO: ZERO_RMW(dec); break;
        case INC_ZERO: ZERO_RMW(inc); break;
        case ISC_ZERO: ZERO_RMW(isc); break; // Unofficial
        case LSR_ZERO: ZERO_RMW(lsr); break;
        case RLA_ZERO: ZERO_RMW(rla); break; // Unofficial
        case RRA_ZERO: ZERO_RMW(rra); break; // Unofficial
        case ROL_ZERO: ZERO_RMW(rol); break;
        case ROR_ZERO: ZERO_RMW(ror); break;
        case SLO_ZERO: ZERO_RMW(slo); break; // Unofficial
        case SRE_ZERO: ZERO_RMW(sre); break; // Unofficial

        // Write instructions

        case SAX_ZERO: zero_write(a & x); break; // Unofficial
        case STA_ZERO: zero_write(a);     break;
        case STX_ZERO: zero_write(x);     break;
        case STY_ZERO: zero_write(y);     break;

        // Unofficial NOPs with zero page addressing (acts like reads)
        case NO0_ZERO: case NO1_ZERO: case NO2_ZERO:
            get_zero_op();
            break;

        //
        // Zero page indexed addressing
        //

        // Read instructions

        case ADC_ZERO_X: adc(get_zero_xy_op(x));     break;
        case AND_ZERO_X: and_(get_zero_xy_op(x));    break;
        case CMP_ZERO_X: comp(a, get_zero_xy_op(x)); break;
        case EOR_ZERO_X: eor(get_zero_xy_op(x));     break;
        case LAX_ZERO_Y: lax(get_zero_xy_op(y));     break; // Unofficial
        case LDA_ZERO_X: lda(get_zero_xy_op(x));     break;
        case LDX_ZERO_Y: ldx(get_zero_xy_op(y));     break;
        case LDY_ZERO_X: ldy(get_zero_xy_op(x));     break;
        case ORA_ZERO_X: ora(get_zero_xy_op(x));     break;
        case SBC_ZERO_X: sbc(get_zero_xy_op(x));     break;

        // Read-modify-write instructions

        case ASL_ZERO_X: ZERO_X_RMW(asl); break;
        case DCP_ZERO_X: ZERO_X_RMW(dcp); break; // Unofficial
        case DEC_ZERO_X: ZERO_X_RMW(dec); break;
        case INC_ZERO_X: ZERO_X_RMW(inc); break;
        case ISC_ZERO_X: ZERO_X_RMW(isc); break; // Unofficial
        case LSR_ZERO_X: ZERO_X_RMW(lsr); break;
        case RLA_ZERO_X: ZERO_X_RMW(rla); break; // Unofficial
        case RRA_ZERO_X: ZERO_X_RMW(rra); break; // Unofficial
        case ROL_ZERO_X: ZERO_X_RMW(rol); break;
        case ROR_ZERO_X: ZERO_X_RMW(ror); break;
        case SLO_ZERO_X: ZERO_X_RMW(slo); break; // Unofficial
        case SRE_ZERO_X: ZERO_X_RMW(sre); break; // Unofficial

        // Write instructions

        case SAX_ZERO_Y: zero_xy_write(a & x, y); break; // Unofficial
        case STA_ZERO_X: zero_xy_write(a, x);     break;
        case STX_ZERO_Y: zero_xy_write(x, y);     break;
        case STY_ZERO_X: zero_xy_write(y, x);     break;

        // Unofficial NOPs with indexed zero page addressing (acts like reads)
        case NO0_ZERO_X: case NO1_ZERO_X: case NO2_ZERO_X: case NO3_ZERO_X:
        case NO4_ZERO_X: case NO5_ZERO_X:
            get_zero_xy_op(x);
            break;

        //
        // Absolute indexed addressing
        //

        // Read instructions

        case ADC_ABS_X: adc(get_abs_xy_op_read(x));     break;
        case ADC_ABS_Y: adc(get_abs_xy_op_read(y));     break;
        case AND_ABS_X: and_(get_abs_xy_op_read(x));    break;
        case AND_ABS_Y: and_(get_abs_xy_op_read(y));    break;
        case CMP_ABS_X: comp(a, get_abs_xy_op_read(x)); break;
        case CMP_ABS_Y: comp(a, get_abs_xy_op_read(y)); break;
        case EOR_ABS_X: eor(get_abs_xy_op_read(x));     break;
        case EOR_ABS_Y: eor(get_abs_xy_op_read(y));     break;
        case LAS_ABS_Y: las(get_abs_xy_op_read(y));     break; // Unofficial
        case LAX_ABS_Y: lax(get_abs_xy_op_read(y));     break; // Unofficial
        case LDA_ABS_X: lda(get_abs_xy_op_read(x));     break;
        case LDA_ABS_Y: lda(get_abs_xy_op_read(y));     break;
        case LDX_ABS_Y: ldx(get_abs_xy_op_read(y));     break;
        case LDY_ABS_X: ldy(get_abs_xy_op_read(x));     break;
        case ORA_ABS_X: ora(get_abs_xy_op_read(x));     break;
        case ORA_ABS_Y: ora(get_abs_xy_op_read(y));     break;
        case SBC_ABS_X: sbc(get_abs_xy_op_read(x));     break;
        case SBC_ABS_Y: sbc(get_abs_xy_op_read(y));     break;

        // Read-modify-write instructions

        case ASL_ABS_X: RMW(asl, get_abs_xy_addr_write(x)); break;
        case DCP_ABS_X: RMW(dcp, get_abs_xy_addr_write(x)); break; // Unofficial
        case DCP_ABS_Y: RMW(dcp, get_abs_xy_addr_write(y)); break; // Unofficial
        case DEC_ABS_X: RMW(dec, get_abs_xy_addr_write(x)); break;
        case INC_ABS_X: RMW(inc, get_abs_xy_addr_write(x)); break;
        case ISC_ABS_X: RMW(isc, get_abs_xy_addr_write(x)); break; // Unofficial
        case ISC_ABS_Y: RMW(isc, get_abs_xy_addr_write(y)); break; // Unofficial
        case LSR_ABS_X: RMW(lsr, get_abs_xy_addr_write(x)); break;
        case RLA_ABS_X: RMW(rla, get_abs_xy_addr_write(x)); break; // Unofficial
        case RLA_ABS_Y: RMW(rla, get_abs_xy_addr_write(y)); break; // Unofficial
        case RRA_ABS_X: RMW(rra, get_abs_xy_addr_write(x)); break; // Unofficial
        case RRA_ABS_Y: RMW(rra, get_abs_xy_addr_write(y)); break; // Unofficial
        case ROL_ABS_X: RMW(rol, get_abs_xy_addr_write(x)); break;
        case ROR_ABS_X: RMW(ror, get_abs_xy_addr_write(x)); break;
        case SLO_ABS_X: RMW(slo, get_abs_xy_addr_write(x)); break; // Unofficial
        case SLO_ABS_Y: RMW(slo, get_abs_xy_addr_write(y)); break; // Unofficial
        case SRE_ABS_X: RMW(sre, get_abs_xy_addr_write(x)); break; // Unofficial
        case SRE_ABS_Y: RMW(sre, get_abs_xy_addr_write(y)); break; // Unofficial

        // Write instructions

        case AXA_ABS_Y: unoff_addr_write(get_abs_addr(), a & x, y); break; // Unofficial
        case SAY_ABS_X: unoff_addr_write(get_abs_addr(), y    , x); break; // Unofficial
        case XAS_ABS_Y: unoff_addr_write(get_abs_addr(), x    , y); break; // Unofficial
        // Unofficial
        case TAS_ABS_Y:
            s = a & x;
            unoff_addr_write(get_abs_addr(), a & x, y);
            break;

        case STA_ABS_X: abs_xy_write_a(x); break;
        case STA_ABS_Y: abs_xy_write_a(y); break;

        // Unofficial NOPs with absolute,x addressing (acts like reads)
        case NO0_ABS_X: case NO1_ABS_X: case NO2_ABS_X: case NO3_ABS_X: case NO4_ABS_X:
        case NO5_ABS_X:
            get_abs_xy_op_read(x);
            break;

        //
        // Indexed indirect addressing
        //

        // Read instructions

        case ADC_IND_X: adc(get_ind_x_op());     break;
        case AND_IND_X: and_(get_ind_x_op());    break;
        case CMP_IND_X: comp(a, get_ind_x_op()); break;
        case EOR_IND_X: eor(get_ind_x_op());     break;
        case LAX_IND_X: lax(get_ind_x_op());     break; // Unofficial
        case LDA_IND_X: lda(get_ind_x_op());     break;
        case ORA_IND_X: ora(get_ind_x_op());     break;
        case SBC_IND_X: sbc(get_ind_x_op());     break;

        // Write instructions

        case SAX_IND_X: ind_x_write(a & x); break; // Unofficial
        case STA_IND_X: ind_x_write(a);     break;

        // Read-modify-write instructions

        case DCP_IND_X: RMW(dcp, get_ind_x_addr()); break; // Unofficial
        case ISC_IND_X: RMW(isc, get_ind_x_addr()); break; // Unofficial
        case RLA_IND_X: RMW(rla, get_ind_x_addr()); break; // Unofficial
        case RRA_IND_X: RMW(rra, get_ind_x_addr()); break; // Unofficial
        case SLO_IND_X: RMW(slo, get_ind_x_addr()); break; // Unofficial
        case SRE_IND_X: RMW(sre, get_ind_x_addr()); break; // Unofficial

        //
        // Indirect indexed addressing
        //

        // Read instructions

        case ADC_IND_Y: adc(get_ind_y_op_read());     break;
        case AND_IND_Y: and_(get_ind_y_op_read());    break;
        case CMP_IND_Y: comp(a, get_ind_y_op_read()); break;
        case EOR_IND_Y: eor(get_ind_y_op_read());     break;
        case LAX_IND_Y: lax(get_ind_y_op_read());     break; // Unofficial
        case LDA_IND_Y: lda(get_ind_y_op_read());     break;
        case ORA_IND_Y: ora(get_ind_y_op_read());     break;
        case SBC_IND_Y: sbc(get_ind_y_op_read());     break;

        // Write instructions

        // Unofficial
        case AXA_IND_Y:
            ++pc;
            read_tick(); // Fetch effective address low
            read_tick(); // Fetch effective address high
            unoff_addr_write(
              (ram[(op_1 + 1) & 0xFF] << 8) | ram[op_1], // Address
              a & x, y);
            break;

        case STA_IND_Y: ind_y_write_a(); break;

        // Read-modify-write instructions

        case DCP_IND_Y: RMW(dcp, get_ind_y_addr_write()); break; // Unofficial
        case ISC_IND_Y: RMW(isc, get_ind_y_addr_write()); break; // Unofficial
        case RLA_IND_Y: RMW(rla, get_ind_y_addr_write()); break; // Unofficial
        case RRA_IND_Y: RMW(rra, get_ind_y_addr_write()); break; // Unofficial
        case SLO_IND_Y: RMW(slo, get_ind_y_addr_write()); break; // Unofficial
        case SRE_IND_Y: RMW(sre, get_ind_y_addr_write()); break; // Unofficial

        //
        // Indirect addressing
        //

        case JMP_IND:
            {
            uint16_t const addr = (read_mem(pc + 1) << 8) | op_1;
            pc = read_mem(addr);
            poll_for_interrupt();
            pc |= read_mem((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8;
            break;
            }

        //
        // Branch instructions
        //

        case BCC: branch_if(!carry);        break;
        case BCS: branch_if(carry);         break;
        case BVC: branch_if(!overflow);     break;
        case BVS: branch_if(overflow);      break;
        case BEQ: branch_if(!(zn & 0xFF));  break;
        case BMI: branch_if(zn & 0x180);    break;
        case BNE: branch_if(zn & 0xFF);     break;
        case BPL: branch_if(!(zn & 0x180)); break;

        //
        // KIL instructions (hang the CPU)
        //

        case KI0: case KI1: case KI2: case KI3: case KI4: case KI5:
        case KI6: case KI7: case KI8: case KI9: case K10: case K11:
            puts("KIL instruction executed, system hung");
            end_emulation();
            exit_sdl_thread();
        }
    }
}

//
// Initialization and resetting
//
static void set_cpu_cold_boot_state() {
    init_array(ram, (uint8_t)0xFF);
    cpu_data_bus = 0;

    // s is later decremented to 0xFD during the reset operation
    a = s = x = y = 0;

    zn          = 1    ; // Neither negative nor zero
    overflow    = false;
    decimal     = false;
    irq_disable = false; // Later set by reset
    carry       = false;

    pending_event         = false;
    pending_end_emulation = false;
    irq_line              = pending_irq = cart_irq = false;
    nmi_asserted          = pending_nmi = false;

    cpu_is_reading = true;
    pal_extra_tick = 5;
}

static void reset_cpu() {
    irq_line = pending_irq = cart_irq = false;

    // This sets the interrupt flag as a side effect
    do_interrupt(Int_reset);
}

//
// State transfers
//

template<bool calculating_size, bool is_save>
void transfer_cpu_state(uint8_t *&buf) {
    TRANSFER(ram)
    if (wram_base) TRANSFER_P(wram_base, 0x2000*wram_8k_banks)
    TRANSFER(pc)
    TRANSFER(a) TRANSFER(s) TRANSFER(x) TRANSFER(y)
    TRANSFER(zn) TRANSFER(carry) TRANSFER(irq_disable) TRANSFER(decimal)
    TRANSFER(overflow)
    TRANSFER(op_1)
    TRANSFER(cpu_is_reading)
    TRANSFER(cpu_data_bus)
    TRANSFER(cart_irq) TRANSFER(dmc_irq) TRANSFER(frame_irq) TRANSFER(irq_line)
    TRANSFER(nmi_asserted)
    TRANSFER(pending_irq) TRANSFER(pending_nmi)
    if (is_pal) TRANSFER(pal_extra_tick)
}

// Explicit instantiations

// Calculating state size
template void transfer_cpu_state<true, false>(uint8_t*&);
// Saving state to buffer
template void transfer_cpu_state<false, true>(uint8_t*&);
// Loading state from buffer
template void transfer_cpu_state<false, false>(uint8_t*&);

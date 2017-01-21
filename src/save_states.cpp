#include "common.h"

#include "apu.h"
#include "audio.h"
#include "controller.h"
#include "cpu.h"
#include "input.h"
#include "ppu.h"
#include "mapper.h"
#include "rom.h"
#include "save_states.h"
#include "timing.h"

// Buffer for an in-memory save state.
static uint8_t *state;
static size_t state_size;
static bool has_save;

template<bool calculating_size, bool is_save>
static size_t transfer_system_state(uint8_t *buf) {
    uint8_t *tmp = buf;

    transfer_apu_state<calculating_size, is_save>(buf);
    transfer_cpu_state<calculating_size, is_save>(buf);
    transfer_ppu_state<calculating_size, is_save>(buf);
    transfer_controller_state<calculating_size, is_save>(buf);
    transfer_input_state<calculating_size, is_save>(buf);

    if (calculating_size)
        mapper_fns.state_size(buf);
    else {
        if (is_save)
            mapper_fns.save_state(buf);
        else
            mapper_fns.load_state(buf);
    }

    // Return size of state in bytes
    return buf - tmp;
}

//
// Save states
//
void save_state() {
    transfer_system_state<false, true>(state);
    has_save = true;
}

void load_state() {
    if (has_save) {
        transfer_system_state<false, false>(state);
    }
}

void init_save_states_for_rom() {
    state_size = transfer_system_state<true, false>(0);
    fail_if(!(state = new (std::nothrow) uint8_t[state_size]),
      "failed to allocate %zu-byte buffer for save state", state_size);
}

void deinit_save_states_for_rom() {
    free_array_set_null(state);
    has_save = false;
}

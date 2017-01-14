#include "common.h"

#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#ifdef RUN_TESTS
#  include "test.h"
#endif

#include <SDL.h>

char const *program_name;

static int emulation_thread(void*) {
#ifdef RUN_TESTS
    run_tests();
#else
    run();
#endif

    return 0;
}

int main(int argc, char *argv[]) {
 
    install_fatal_signal_handlers();

    // One-time initialization of various components
    init_apu();
    init_input();
    init_mappers();

#ifndef RUN_TESTS
    load_rom("mario.nes", true);
#endif

    // Create a separate emulation thread and use this thread as the rendering
    // thread

    init_sdl();
    SDL_Thread *emu_thread;
    fail_if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0)),
            "failed to create emulation thread: %s", SDL_GetError());
    sdl_thread();
    SDL_WaitThread(emu_thread, 0);
    deinit_sdl();

#ifndef RUN_TESTS
    unload_rom();
#endif

    puts("Shut down cleanly");
}

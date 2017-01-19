#include "common.h"

#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include <SDL.h>

char const *program_name;

static int emulation_thread(void*) {
    run();
    return 0;
}

int main(int argc, char *argv[]) {
 
    install_fatal_signal_handlers();

    // One-time initialization of various components
    init_apu();
    init_input();
    init_mappers();

    load_rom("mario(E).nes", true);
    init_sdl();
    SDL_ShowCursor(SDL_DISABLE);

    // Create a separate emulation thread and use this thread as the rendering
    // thread
    SDL_Thread *emu_thread;
    fail_if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0)),
            "failed to create emulation thread: %s", SDL_GetError());
    
    sdl_thread();
    SDL_WaitThread(emu_thread, 0);
    deinit_sdl();
    unload_rom();
    puts("Shut down cleanly");
}

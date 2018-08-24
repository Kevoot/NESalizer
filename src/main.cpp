#include "common.h"

#include "apu.h"
#include "cpu.h"
#include "input.h"
#include "mapper.h"
#include "rom.h"
#include "sdl_backend.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>
#include <switch.h>

char const *program_name;

static int emulation_thread(void*) {
    run();
    return 0;
}

int main(int argc, char *argv[]) {
    gfxInitDefault();
    consoleInit(NULL);
    socketInitializeDefault();
    nxlinkStdio();
    printf("nxlink started\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
    gfxWaitForVsync();
    gfxExit();
 
    install_fatal_signal_handlers();
    init_apu();
    init_mappers();
    
    // Not figured out how to pass arguments on the steamlink yet.
    program_name = argv[0] ? argv[0] : "nesalizer";
    if (argc != 2) {
        load_rom("loz.nes", true);   
    }
    else 
    {
         load_rom(argv[1], true);      
    }

    init_sdl();
    SDL_ShowCursor(SDL_DISABLE);
    SDL_Thread *emu_thread;
    
    // Create a separate emulation thread and use this thread as the rendering thread
    fail_if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0)), "failed to create emulation thread: %s", SDL_GetError());
    
    sdl_thread();
    SDL_WaitThread(emu_thread, 0);
    deinit_sdl();
    unload_rom();
    puts("Shut down cleanly");
}

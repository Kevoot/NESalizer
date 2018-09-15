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
#include "gui.h"
#include "rom.h"

char const *program_name;

int main(int argc, char *argv[])
{
    
    /*gfxInitDefault();
    consoleInit(NULL);
    socketInitializeDefault();
    nxlinkStdio();
    printf("nxlink started\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
    gfxWaitForVsync();
    gfxExit();*/

    install_fatal_signal_handlers();
    init_apu();
    init_mappers();

    init_sdl();
    while (true)
    {
        if (!is_rom_loaded())
        {
            showGUI();
        }
        SDL_ShowCursor(SDL_DISABLE);
        GUI::main_run();
    }
    deinit_sdl();
    puts("Shut down cleanly");
}

[GPLv2]	(http://www.gnu.org/licenses/gpl-2.0.html)


A NES emulator using SDL2 originally written by 'ulfalizer' ported to the Steam Link.
The input system has been re-done, hard-coded keyboard removed and Gamepad support has been added and should work alongside keyboard in future
Still working on disk-save-states.

Rewind, Movie recording and some debugging code removed while I work out the best way to optimize things for ARM7. Audio still lags a bit on NTSC.
I've included a NetBeans Project but it is possible to build the code with just the script.

## Building ##
Requires the Steamlink-SDK available on github

git clone https://github.com/ValveSoftware/steamlink-sdk
cd steamlink-sdk/examples
git clone https://github.com/thecosmicslug/NESalizer
cd nesalizer
./build_steamlink.sh

## Running ##
 Right now it simply loads "mario(E).nes" from the current folder unless you provide a filename. A nice GUI is yet to come.
The save state is still in-memory and not saved to disk yet.

## Controls ##

Movement:
SDL_CONTROLLER_BUTTON_DPAD_UP
SDL_CONTROLLER_BUTTON_DPAD_DOWN
SDL_CONTROLLER_BUTTON_DPAD_LEFT
SDL_CONTROLLER_BUTTON_DPAD_RIGHT

Buttons:
SDL_CONTROLLER_BUTTON_BACK 				= Select
SDL_CONTROLLER_BUTTON_START 				= Start
SDL_CONTROLLER_BUTTON_A					= A
SDL_CONTROLLER_BUTTON_B					= B

Emulation:
SDL_CONTROLLER_BUTTON_LEFTSHOULDER		= Load State
SDL_CONTROLLER_BUTTON_RIGHTSHOULDER	= Save State
SDL_CONTROLLER_BUTTON_GUIDE				= Exit Program
SDL_CONTROLLER_BUTTON_LEFTSTICK			= Reset Rom

## Compatibility ##
iNES mappers (support circuitry inside cartridges) supported so far: 
0, 1, 2, 3, 4, 5 (including ExGrafix, split screen, and PRG RAM swapping), 7, 9, 10, 11, 13, 28, 71, 232. This covers the majority of ROMs.

Supports tricky-to-emulate games like Mig-29 Soviet Fighter, Bee 52, Uchuu Keibitai SDF, Just Breed, and Battletoads.

## Technical ##
Uses a low-level renderer that simulates the rendering pipeline in the real PPU (NES graphics processor), following the model in [this timing diagram](http://wiki.nesdev.com/w/images/d/d1/Ntsc_timing.png) that ulfalizer put together with help from the NesDev community. 

(It won't make much sense without some prior knowledge of how graphics work on the NES.

Most prediction and catch-up (two popular emulator optimization techniques) is omitted in favor of straightforward and robust code. This makes many effects that require special handling in some other emulators work automatically




## Thanks ##
 ulfalizer for the original sdl version
The Mame4All Steamlink port for idea's

 * Shay Green (blargg) for the [blip\_buf](https://code.google.com/p/blip-buf/) waveform synthesis library.
 * Quietust for the Visual 2A03 and Visual 2C02 simulators.
 * beannaich, cpow, Disch, kevtris, Movax21, Lidnariq, loopy, thefox, Tepples and lots of other people on the [NesDev](http://nesdev.com) forums and wiki and in #nesdev for help, docs, tests, and discussion.
=======
NESalizer Switch ported from TheCosmicSlug for the Steam Link
======================================
[GPLv2]	(http://www.gnu.org/licenses/gpl-2.0.html)


A NES emulator using SDL2 originally written by 'ulfalizer' and ported to the Steam Link by 'TheCosmicSlug'.
Still working on disk-save-states.

## Building ##
Make sure the latest version of libnx is installed, as well as all the SDL2, png, and ttf libraries through pacman

## Running ##
There's an initial UI for loading ROMs which can only load successfully once. This will be fixed in the near(ish) future.

## Controls ##
Exactly what you would expect, with the addition of ZR to toggle the menu

## Compatibility ##
iNES mappers (support circuitry inside cartridges) supported so far: 
0, 1, 2, 3, 4, 5 (including ExGrafix, split screen, and PRG RAM swapping), 7, 9, 10, 11, 13, 28, 71, 232. This covers the majority of ROMs.

Supports tricky-to-emulate games like Mig-29 Soviet Fighter, Bee 52, Uchuu Keibitai SDF, Just Breed, and Battletoads.

## Technical ##
Uses a low-level renderer that simulates the rendering pipeline in the real PPU (NES graphics processor), following the model in [this timing diagram](http://wiki.nesdev.com/w/images/d/d1/Ntsc_timing.png) that ulfalizer put together with help from the NesDev community. 

(It won't make much sense without some prior knowledge of how graphics work on the NES.

Most prediction and catch-up (two popular emulator optimization techniques) is omitted in favor of straightforward and robust code. This makes many effects that require special handling in some other emulators work automatically

## Thanks ##
 ulfalizer for the original sdl version
The Mame4All Steamlink port for idea's

 * TheCosmicSlug for the inital SDL2 port to the Steam Link
 * Shay Green (blargg) for the [blip\_buf](https://code.google.com/p/blip-buf/) waveform synthesis library.
 * Quietust for the Visual 2A03 and Visual 2C02 simulators.
 * beannaich, cpow, Disch, kevtris, Movax21, Lidnariq, loopy, thefox, Tepples and lots of other people on the [NesDev](http://nesdev.com) forums and wiki and in #nesdev for help, docs, tests, and discussion.

NESalizer for the Steam Link
=======================

A NES emulator using SDL2 originally written by ulfalizer ported to the Steam Link.
Still working on save-states and gamepad support.

## Building ##
SDL2 is used for the final output and is the only dependency except the SteamLink-SDK
    
## Running ##
 Right now it simply loads "mario(E).nes" from the current folder unless you provide a filename. A nice GUI is yet to come.
Controls are still hardcoded while I get gamepad support working.

<table>
  <tr><td>D-pad       </td><td>Arrow keys   </td></tr>
  <tr><td>A           </td><td>X            </td></tr>
  <tr><td>B           </td><td>Z            </td></tr>
  <tr><td>Start       </td><td>Return       </td></tr>
  <tr><td>Select      </td><td>Right shift  </td></tr>
  <tr><td>Save state  </td><td>S            </td></tr>
  <tr><td>Load state  </td><td>L            </td></tr>
</table>

The save state is still in-memory and not saved to disk yet.

## Technical ##
Uses a low-level renderer that simulates the rendering pipeline in the real PPU (NES graphics processor), following the model in [this timing diagram](http://wiki.nesdev.com/w/images/d/d1/Ntsc_timing.png) that ulfalizer put together with help from the NesDev community. 
(It won't make much sense without some prior knowledge of how graphics work on the NES. :)
Most prediction and catch-up (two popular emulator optimization techniques) is omitted in favor of straightforward and robust code. This makes many effects that require special handling in some other emulators work automagically. The emulator currently manages about 6x emulation speed on a single core on my old 2600K Core i7 CPU.

## Compatibility ##
iNES mappers (support circuitry inside cartridges) supported so far: 0, 1, 2, 3, 4, 5 (including ExGrafix, split screen, and PRG RAM swapping), 7, 9, 10, 11, 13, 28, 71, 232. This covers the majority of ROMs.
Supports tricky-to-emulate games like Mig-29 Soviet Fighter, Bee 52, Uchuu Keibitai SDF, Just Breed, and Battletoads.

## Coding style ##
For functions and variables with external linkage, the documentation appears at the declaration in the header. For stuff with internal linkage, the documentation is in the source file. The headers start with a short blurb.
The source is mostly C-like C++, but still strives for modularization, implementation hiding, and clean interfaces. Internal linkage is used for "private" data. Classes might be used for general-purpose objects with multiple instances, but there aren't any of those yet. I try to reduce clutter and boilerplate code.

All .cpp files include headers according to this scheme:

    #include "common.h"
    <#includes for local headers>
    <#includes for system headers>

The above setup allows most headers to assume that common.h has been included, which simplifies headers and often makes include guards redundant.

## Thanks ##
 ulfalizer for the original sdl version
 * Shay Green (blargg) for the [blip\_buf](https://code.google.com/p/blip-buf/) waveform synthesis library and lots of test ROMs.
 * Quietust for the Visual 2A03 and Visual 2C02 simulators.
 * beannaich, cpow, Disch, kevtris, Movax21, Lidnariq, loopy, thefox, Tepples

and lots of other people on the [NesDev](http://nesdev.com) forums and wiki and in #nesdev for help, docs, tests, and discussion.

[GPLv2](http://www.gnu.org/licenses/gpl-2.0.html)


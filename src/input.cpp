#include "common.h"
#include "sdl_backend.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>

static struct Controller_data {
    // Button states
    bool left_pushed, right_pushed, up_pushed, down_pushed,
         a_pushed, b_pushed, start_pushed, select_pushed;
} controller_data[2];

bool reset_pushed;

uint8_t read_button_states(unsigned n) {
    Controller_data &c = controller_data[n];
    return (c.right_pushed << 7) | (c.left_pushed  << 6) | (c.down_pushed   << 5) |
           (c.up_pushed    << 4) | (c.start_pushed << 3) | (c.select_pushed << 2) |
           (c.b_pushed     << 1) |  c.a_pushed;
}

uint8_t set_button_state(unsigned n, unsigned i) {
    switch(i)
    {
        case  0:
            // A
            controller_data[n].a_pushed=true;
            break;
        case  1:
            // B
            controller_data[n].b_pushed=true;
            break;
        case  11:
            // Select
            controller_data[n].select_pushed=true;
            break;
        case  10:
            // Start
            controller_data[n].start_pushed=true;
            break;
        case  13:
            // Up
            controller_data[n].up_pushed=true;
            break;
        case  15:
            // Down
            controller_data[n].down_pushed=true;
            break;
        case  12:
            // Left
            controller_data[n].left_pushed=true;
            break;
        case 14:
            // Right
            controller_data[n].right_pushed=true;
            break;
    }

    return (controller_data[n].right_pushed << 7) | (controller_data[n].left_pushed << 6) | 
            (controller_data[n].down_pushed << 5) | (controller_data[n].up_pushed << 4) | 
            (controller_data[n].start_pushed << 3) | (controller_data[n].select_pushed << 2) |
           (controller_data[n].b_pushed << 1) |  (controller_data[n].a_pushed);
}

uint8_t clear_button_state(unsigned n, unsigned i) {
    switch(i)
    {
        case  0:
            // A
            controller_data[n].a_pushed=false;
            break;
        case  1:
            // B
            controller_data[n].b_pushed=false;
            break;
        case  11:
            // Select
            controller_data[n].select_pushed=false;
            break;
        case  10:
            // Start
            controller_data[n].start_pushed=false;
            break;
        case  13:
            // Up
            controller_data[n].up_pushed=false;
            break;
        case  15:
            // Down
            controller_data[n].down_pushed=false;
            break;
        case  12:
            // Left
           controller_data[n].left_pushed=false;
            break;
        case 14:
            // Right
           controller_data[n].right_pushed=false;
            break;
    }
        return (controller_data[n].right_pushed << 7) | (controller_data[n].left_pushed << 6) | 
            (controller_data[n].down_pushed << 5) | (controller_data[n].up_pushed << 4) | 
            (controller_data[n].start_pushed << 3) | (controller_data[n].select_pushed << 2) |
           (controller_data[n].b_pushed << 1) |  (controller_data[n].a_pushed);
}

template<bool calculating_size, bool is_save>
void transfer_input_state(uint8_t *&buf) {
    for (unsigned i = 0; i < 2; ++i) {
        Controller_data &cd = controller_data[i];
        TRANSFER(cd.a_pushed)
        TRANSFER(cd.b_pushed)
        TRANSFER(cd.start_pushed)
        TRANSFER(cd.select_pushed)
        TRANSFER(cd.right_pushed)
        TRANSFER(cd.left_pushed)
        TRANSFER(cd.down_pushed)
        TRANSFER(cd.up_pushed)
    }
}

template void transfer_input_state<true, false>(uint8_t*&);
template void transfer_input_state<false, true>(uint8_t*&);
template void transfer_input_state<false, false>(uint8_t*&);

#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "common.h"

namespace GUI {

extern int BTN_UP     [];
extern int BTN_DOWN   [];
extern int BTN_LEFT   [];
extern int BTN_RIGHT  [];
extern int BTN_A      [];
extern int BTN_B      [];
extern int BTN_SELECT [];
extern int BTN_START  [];

const int TEXT_CENTER  = -1;
const int TEXT_RIGHT   = -2;
const unsigned FONT_SZ = 15;

void init(SDL_Window * scr, SDL_Renderer * rend);
void toggle_pause();
SDL_Scancode query_key();
int query_button();
void run();

SDL_Texture* gen_text(std::string text, SDL_Color color);
void render_texture(SDL_Texture* texture, int x, int y);
bool is_paused();
void render();
void update_menu(u8 select);

u8 get_joypad_state(int n);
void new_frame(u32* pixels);
void set_size(int mul);

}
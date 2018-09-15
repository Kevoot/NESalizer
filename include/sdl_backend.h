// Video, audio, and input backend. Uses SDL2.

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>

void init_sdl();
void deinit_sdl();

// SDL rendering thread. Runs separately from the emulation thread.
void sdl_thread();

// Called from the emulation thread to cause the SDL thread to exit
void exit_sdl_thread();

// Video

void put_pixel(unsigned x, unsigned y, uint32_t color);
void draw_frame();

// Audio
int const sample_rate = 96000;
//int const sample_rate = 22050;

extern SDL_mutex *frame_lock;

// Protect the audio buffer from concurrent access by the emulation thread and
// SDL
void lock_audio();
void unlock_audio();
void showGUI();

// Stop and start audio playback in SDL
void start_audio_playback();
void stop_audio_playback();

extern SDL_mutex *event_lock;

#include "common.h"

#include "audio.h"
#include "cpu.h"
#include "input.h"

#include "save_states.h"
#include "sdl_backend.h"
#include <SDL.h>

//
// Video
//

// Each pixel is scaled to scale_factor*scale_factor pixels
unsigned const scale_factor = 3;

static SDL_Window   *screen;
static SDL_Renderer *renderer;
static SDL_Texture  *screen_tex;

//
// TODO: This could probably be optimized to eliminate some copying and format
// conversions.

static Uint32 *front_buffer;
static Uint32 *back_buffer;

static SDL_mutex *frame_lock;
static SDL_cond  *frame_available_cond;
static bool ready_to_draw_new_frame;
static bool frame_available;

static bool pending_sdl_thread_exit;
SDL_mutex   *event_lock;

Uint8 const *keys;
Uint16 const sdl_audio_buffer_size = 5000;
static SDL_AudioDeviceID audio_device_id;

struct Controller_t
{
	enum Type {
		k_Available,
		k_Joystick,
		k_Gamepad,
	} type;

	SDL_JoystickID instance_id;
	SDL_Joystick *joystick;
	SDL_GameController *gamepad;
};
static Controller_t controllers[2];

void lock_audio() { SDL_LockAudioDevice(audio_device_id); }
void unlock_audio() { SDL_UnlockAudioDevice(audio_device_id); }

void start_audio_playback() { SDL_PauseAudioDevice(audio_device_id, 0); }
void stop_audio_playback() { SDL_PauseAudioDevice(audio_device_id, 1); }

void put_pixel(unsigned x, unsigned y, uint32_t color) {
    assert(x < 256);
    assert(y < 240);

    back_buffer[256*y + x] = color;
}

void draw_frame() {
    SDL_LockMutex(frame_lock);
    if (ready_to_draw_new_frame) {
        frame_available = true;
        swap(back_buffer, front_buffer);
        SDL_CondSignal(frame_available_cond);
    }
    SDL_UnlockMutex(frame_lock);
}

static void audio_callback(void*, Uint8 *stream, int len) {
    assert(len >= 0);
    read_samples((int16_t*)stream, len/sizeof(int16_t));
}

static void add_controller(Controller_t::Type type, int device_index)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type == Controller_t::k_Available) {
			if (type == Controller_t::k_Gamepad) {
				controller.gamepad = SDL_GameControllerOpen(device_index);
				if (!controller.gamepad) {
					fprintf(stderr, "Couldn't open gamepad: %s\n", SDL_GetError());
					return;
				}
				controller.joystick = SDL_GameControllerGetJoystick(controller.gamepad);
				printf("Opened game controller %s at index %d\n", SDL_GameControllerName(controller.gamepad), i);
			} else {
				controller.joystick = SDL_JoystickOpen(device_index);
				if (!controller.joystick) {
					fprintf(stderr, "Couldn't open joystick: %s\n", SDL_GetError());
					return;
				}
				printf("Opened joystick %s at index %d\n", SDL_JoystickName(controller.joystick), i);
			}
			controller.type = type;
			controller.instance_id = SDL_JoystickInstanceID(controller.joystick);
			return;
		}
	}
}

static bool get_controller_index(Controller_t::Type type, SDL_JoystickID instance_id, int *controller_index)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type != type) {
			continue;
		}
		if (controller.instance_id != instance_id) {
			continue;
		}
		*controller_index = i;
		return true;
	}
	return false;
}

static void remove_controller(Controller_t::Type type, SDL_JoystickID instance_id)
{
	for (int i = 0; i < SDL_arraysize(controllers); ++i) {
		Controller_t &controller = controllers[i];
		if (controller.type != type) {
			continue;
		}
		if (controller.instance_id != instance_id) {
			continue;
		}
		if (controller.type == Controller_t::k_Gamepad) {
			SDL_GameControllerClose(controller.gamepad);
		} else {
			SDL_JoystickClose(controller.joystick);
		}
		controller.type = Controller_t::k_Available;
		return;
	}
}
void joyprocess(Uint8 button, SDL_bool pressed, Uint8 njoy)
{
    switch(button)
    {
        case  SDL_CONTROLLER_BUTTON_A:  
            break;
        case  SDL_CONTROLLER_BUTTON_B:
            break;
        case  SDL_CONTROLLER_BUTTON_X:
            break;
        case  SDL_CONTROLLER_BUTTON_Y:
            break;
        case  SDL_CONTROLLER_BUTTON_BACK:
            break;
        case  SDL_CONTROLLER_BUTTON_START:
            break;
        case  SDL_CONTROLLER_BUTTON_GUIDE:
            if (pressed){
                // EXIT GAME
                exit_sdl_thread();
                deinit_sdl();
            }
            break;
        case  SDL_CONTROLLER_BUTTON_DPAD_UP:
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            break;
        case  SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            break;
        case  SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            break;
        case  SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            if (pressed){
                // Load Save-state
                load_state();
            }
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            if (pressed){
                // Save State
                save_state();
            }
            break;
        case  SDL_CONTROLLER_BUTTON_LEFTSTICK:
            break;
        case  SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            break;
         
    }
}
//
// SDL thread and events
// Runs from emulation thread
//void handle_ui_keys() {
    //SDL_LockMutex(event_lock);
    //switch(keys[SDL_SCANCODE_S])
    //{
    //    case SDL_SCANCODE_S:
    //        save_state();
    //        break;
    //    case SDL_SCANCODE_L:
    //        load_state();
    //        break;
    //   case SDL_SCANCODE_F5:
    //        reset_pushed = keys[SDL_SCANCODE_F5];
    //        soft_reset();
    //        break;
    //}    
    //SDL_UnlockMutex(event_lock);
//}
static void process_events() {
    SDL_Event event;
    SDL_LockMutex(event_lock);
    while (SDL_PollEvent(&event))
        switch(event.type)
        {
            case SDL_QUIT:
                end_emulation();
                pending_sdl_thread_exit = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
		add_controller(Controller_t::k_Gamepad, event.cdevice.which);
		break;
            case SDL_CONTROLLERDEVICEREMOVED:
		remove_controller(Controller_t::k_Gamepad, event.cdevice.which);
		break;
            case SDL_CONTROLLERBUTTONDOWN:
		{
		int controller_index;
		if (!get_controller_index(Controller_t::k_Gamepad, event.cbutton.which, &controller_index)) {
                    break;
                }
		joyprocess(event.cbutton.button, SDL_TRUE, controller_index);
		}
		break;
            case SDL_CONTROLLERBUTTONUP:
		{
		int controller_index;
		if (!get_controller_index(Controller_t::k_Gamepad, event.cbutton.which, &controller_index)) {
                    break;
		}
		joyprocess(event.cbutton.button, SDL_FALSE, controller_index);
		}
		break;
        }
    SDL_UnlockMutex(event_lock);
}

void sdl_thread() {
    for (;;) {
        // Wait for the emulation thread to signal that a frame has completed
        SDL_LockMutex(frame_lock);
        ready_to_draw_new_frame = true;
        while (!frame_available && !pending_sdl_thread_exit)
            SDL_CondWait(frame_available_cond, frame_lock);
        if (pending_sdl_thread_exit) {
            SDL_UnlockMutex(frame_lock);
            return;
        }
        frame_available = ready_to_draw_new_frame = false;
        SDL_UnlockMutex(frame_lock);
        process_events();
        // Draw the new frame
        fail_if(SDL_UpdateTexture(screen_tex, 0, front_buffer, 256*sizeof(Uint32)),
          "failed to update screen texture: %s", SDL_GetError());
        fail_if(SDL_RenderCopy(renderer, screen_tex, 0, 0),
          "failed to copy rendered frame to render target: %s", SDL_GetError());
        SDL_RenderPresent(renderer);
    }
}

void exit_sdl_thread() {
    SDL_LockMutex(frame_lock);
    pending_sdl_thread_exit = true;
    SDL_CondSignal(frame_available_cond);
    SDL_UnlockMutex(frame_lock);
}

//
// Initialization and de-initialization
//
void init_sdl() {

    fail_if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0,
      "failed to initialize SDL: %s", SDL_GetError());

    fail_if(!(screen =
      SDL_CreateWindow(
        "Nesalizer",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        scale_factor*256, scale_factor*240,
        SDL_WINDOW_FULLSCREEN or SDL_WINDOW_OPENGL)),
      "failed to create window: %s", SDL_GetError());

    fail_if(!(renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED)),
      "failed to create rendering context: %s", SDL_GetError());

    // Display some information about the renderer
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info))
        puts("Failed to get renderer information from SDL");
    else {
        if (renderer_info.name)
            printf("renderer: uses renderer \"%s\"\n", renderer_info.name);
        if (renderer_info.flags & SDL_RENDERER_SOFTWARE)
            puts("renderer: uses software rendering");
        if (renderer_info.flags & SDL_RENDERER_ACCELERATED)
            puts("renderer: uses hardware-accelerated rendering");
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC)
            puts("renderer: uses vsync");
        if (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE)
            puts("renderer: supports rendering to texture");
        printf("renderer: available texture formats:");
        unsigned const n_texture_formats = min(16u, (unsigned)renderer_info.num_texture_formats);
        for (unsigned i = 0; i < n_texture_formats; ++i)
            printf(" %s", SDL_GetPixelFormatName(renderer_info.texture_formats[i]));
        putchar('\n');
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    fail_if(!(screen_tex =
      SDL_CreateTexture(
        renderer,
        // SDL takes endianess into account, so this becomes GL_RGBA8
        // internally on little-endian systems
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 240)),
      "failed to create texture for screen: %s", SDL_GetError());

    static Uint32 render_buffers[2][240*256];
    back_buffer  = render_buffers[0];
    front_buffer = render_buffers[1];

    // Audio

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = sdl_audio_buffer_size;
    want.callback = audio_callback;

    audio_device_id = SDL_OpenAudioDevice(0, 0, &want, 0, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    // Input
    // We use SDL_GetKey/MouseState() instead
    SDL_EventState(SDL_KEYDOWN        , SDL_IGNORE);
    SDL_EventState(SDL_KEYUP          , SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONUP  , SDL_IGNORE);
    SDL_EventState(SDL_KEYUP          , SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION    , SDL_IGNORE);

    // Ignore window events for now
    SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);
    keys = SDL_GetKeyboardState(0);

    // SDL thread synchronization

    fail_if(!(event_lock = SDL_CreateMutex()),
      "failed to create event mutex: %s", SDL_GetError());

    fail_if(!(frame_lock = SDL_CreateMutex()),
      "failed to create frame mutex: %s", SDL_GetError());
    fail_if(!(frame_available_cond = SDL_CreateCond()),
      "failed to create frame condition variable: %s", SDL_GetError());
}

void deinit_sdl() {
    SDL_DestroyRenderer(renderer); // Also destroys the texture
    SDL_DestroyWindow(screen);
    SDL_DestroyMutex(event_lock);
    SDL_DestroyMutex(frame_lock);
    SDL_DestroyCond(frame_available_cond);
    SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER );
    SDL_CloseAudioDevice(audio_device_id); // Prolly not needed, but play it safe
    SDL_Quit();
}

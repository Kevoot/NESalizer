#include <csignal>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <switch.h>
#include "sdl_backend.h"
#include "menu.h"
#include "save_states.h"
#include "cpu.h"

namespace GUI
{

// Screen size:
const unsigned WIDTH = 256;
const unsigned HEIGHT = 240;

int BTN_UP[] = {-1, -1};
int BTN_DOWN[] = {-1, -1};
int BTN_LEFT[] = {-1, -1};
int BTN_RIGHT[] = {-1, -1};
int BTN_A[] = {-1, -1};
int BTN_B[] = {-1, -1};
int BTN_SELECT[] = {-1, -1};
int BTN_START[] = {-1, -1};

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_tex;

// Menus:
Menu *menu;
Menu *mainMenu;
Menu *settingsMenu;
Menu *videoMenu;
Menu *keyboardMenu[2];
Menu *joystickMenu[2];
FileMenu *fileMenu;

SDL_Texture *gameTexture;
SDL_Texture *background;
TTF_Font *font;
u8 const *keys;

bool pause = true;
int last_window_size = 0;
bool exitFlag = false;

/* Set the window size multiplier */
void set_size(int mul)
{
    last_window_size = mul;
    SDL_SetWindowSize(window, WIDTH * mul, HEIGHT * mul);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void updateVideoMenu()
{
    /*std::string quality("Render Quality: ");
    switch(currentRenderQuality) {
        case 0: 
            quality += "Fastest";
            break;
        case 1:
            quality += "Medium";
            break;
        case 2:
            quality += "Nicest";
            break;
        default:
            quality += "Fastest";
            break;
    }*/

    videoMenu = new Menu;
    videoMenu->add(new Entry("<", [] { menu = settingsMenu; }));
    videoMenu->add(new Entry("Size 1x", [] { set_size(1); }));
    videoMenu->add(new Entry("Size 2x", [] { set_size(2); }));
    videoMenu->add(new Entry("Size 3x", [] { set_size(3); }));
    /* =videoMenu->add(new Entry((quality), []{ set_render_quality((currentRenderQuality + 1) % 3);
                                             updateVideoMenu();
                                             menu = videoMenu;
                                             }));*/
}

bool is_paused()
{
    if (pause)
        return true;
    else
        return false;
}

void reload_rom()
{
    reload_rom();
}

void unload_rom()
{
    unload_rom();
}

void init(SDL_Window *scr, SDL_Renderer *rend)
{
    renderer = rend;
    window = scr;

    for (int p = 0; p < 2; p++)
    {
        BTN_UP[p] = 17;
        BTN_DOWN[p] = 19;
        BTN_LEFT[p] = 16;
        BTN_RIGHT[p] = 18;
        BTN_A[p] = 0;
        BTN_B[p] = 1;
        BTN_SELECT[p] = 11;
        BTN_START[p] = 10;
    }

    if (TTF_Init() < 0)
    {
        return;
    }

    gameTexture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    WIDTH, HEIGHT);

    font = TTF_OpenFont("res/font.ttf", FONT_SZ);
    if (!font)
    {
        exit(1);
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        exit(1);
    }

    SDL_Surface *backSurface = IMG_Load("res/init.png");

    background = SDL_CreateTextureFromSurface(renderer, backSurface);

    SDL_SetTextureColorMod(background, 60, 60, 60);
    SDL_FreeSurface(backSurface);

    if (!background)
    {
        exit(1);
    }

    // Menus:
    mainMenu = new Menu;
    mainMenu->add(new Entry("Load ROM", [] {
        SDL_Delay(100);
        menu = fileMenu;
    }));
    mainMenu->add(new Entry("Save State", [] {
        save_state();
    }));
    mainMenu->add(new Entry("Load State", [] {
        load_state();
    }));
    mainMenu->add(new Entry("Settings", [] { menu = settingsMenu; }));
    mainMenu->add(new Entry("Exit", [] { exit(1); }));

    settingsMenu = new Menu;
    settingsMenu->add(new Entry("<", [] { menu = mainMenu; }));
    // TODO: Add this back and enable substituting the render quality during runtime
    settingsMenu->add(new Entry("Video", [] { menu = videoMenu; }));
    // settingsMenu->add(new Entry("Controller 1", []{ menu = joystickMenu[0]; }));

    // updateVideoMenu();

    // settingsMenu->add(new Entry("Controller 2", []{ menu = joystickMenu[1]; }));
    // settingsMenu->add(new Entry("Save Settings", [] { /*save_settings();*/ menu = mainMenu; }));

    for (int i = 0; i < 2; i++)
    {
        joystickMenu[i] = new Menu;
        joystickMenu[i]->add(new Entry("<", [] { menu = settingsMenu; }));
        joystickMenu[i]->add(new ControlEntry("Up", &BTN_UP[i]));
        joystickMenu[i]->add(new ControlEntry("Down", &BTN_DOWN[i]));
        joystickMenu[i]->add(new ControlEntry("Left", &BTN_LEFT[i]));
        joystickMenu[i]->add(new ControlEntry("Right", &BTN_RIGHT[i]));
        joystickMenu[i]->add(new ControlEntry("A", &BTN_A[i]));
        joystickMenu[i]->add(new ControlEntry("B", &BTN_B[i]));
        joystickMenu[i]->add(new ControlEntry("Start", &BTN_START[i]));
        joystickMenu[i]->add(new ControlEntry("Select", &BTN_SELECT[i]));
    }

    fileMenu = new FileMenu;

    menu = mainMenu;
}
//* Render a texture on screen */
void render_texture(SDL_Texture *texture, int x, int y)
{
    int w, h;
    SDL_Rect dest;

    SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
    if (x == TEXT_CENTER)
        dest.x = WIDTH / 2 - dest.w / 2;
    else if (x == TEXT_RIGHT)
        dest.x = WIDTH - dest.w - 10;
    else
        dest.x = x + 10;
    dest.y = y + 5;

    SDL_RenderCopy(renderer, texture, NULL, &dest);
}

/* Generate a texture from text */
SDL_Texture *gen_text(std::string text, SDL_Color color)
{
    SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    return texture;
}

/* Render the screen */
void render()
{
    SDL_RenderClear(renderer);

    menu->render();

    SDL_RenderPresent(renderer);
}

void update_menu(u8 select)
{
    menu->update(select);
}

/* Play/stop the game */
void toggle_pause()
{
    pause = !pause;
    menu = mainMenu;

    // Set CPU emulation to paused
    if (pause)
    {
        SDL_LockMutex(frame_lock);
        running_state = false;
    }
    else
    {
        running_state = true;
        SDL_UnlockMutex(frame_lock);
    }

    if (pause)
    {
        SDL_SetTextureColorMod(gameTexture, 60, 60, 60);
    }
}

/* Prompt for a key, return the scancode */
SDL_Scancode query_key()
{
    SDL_Texture *prompt = gen_text("Press a key...", {255, 255, 255});
    render_texture(prompt, TEXT_CENTER, HEIGHT - FONT_SZ * 4);
    SDL_RenderPresent(renderer);

    SDL_Event e;
    while (true)
    {
        SDL_PollEvent(&e);
        if (e.type == SDL_KEYDOWN)
            return e.key.keysym.scancode;
    }
}

int query_button()
{
    SDL_Texture *prompt = gen_text("Press a button...", {255, 255, 255});
    render_texture(prompt, TEXT_CENTER, HEIGHT - FONT_SZ * 4);
    SDL_RenderPresent(renderer);

    SDL_Event e;
    while (true)
    {
        SDL_PollEvent(&e);
        if (e.type == SDL_JOYBUTTONDOWN)
        {
            return e.jbutton.button;
        }
    }
}

static int emulation_thread(void *)
{
    run();
    return 0;
}

static int menu_thread(void *)
{
    SDL_Event e;
    for (;;)
    {
        if (pause)
        {
            // Handle events:
            while (SDL_PollEvent(&e))
            {
                switch (e.type)
                {
                case SDL_QUIT:
                    return;
                case SDL_JOYBUTTONDOWN:
                    if ((e.jbutton.button == JOY_R) and get_rom_status())
                        toggle_pause();
                    else if (pause)
                    {
                        menu->update(e.jbutton.button);
                        render();
                    }
                }
            }
        }
    }
}

void stop_main_run()
{
    exitFlag = true;
}

void main_run()
{
    SDL_Thread *emu_thread;
    SDL_Thread *m_thread;

    // Get initial frame lock until ROM is chosen.
    // SDL_LockMutex(frame_lock);
    exitFlag = false;
    running_state = true;
    if(!(emu_thread = SDL_CreateThread(emulation_thread, "emulation", 0))) {
        exit(1);
    }

    while (true)
    {
        printf("In main_run loop\n");
        sdl_thread();
        // Wait and free up for next cycle
        SDL_WaitThread(emu_thread, 0);

        if (exitFlag)
        {
            exitFlag = false;
            return;
        }
    }
}
} // namespace GUI
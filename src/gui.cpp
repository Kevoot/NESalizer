#include <csignal>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "menu.h"

namespace GUI {

// Screen size:
const unsigned WIDTH  = 256;
const unsigned HEIGHT = 240;

int BTN_UP[] = {-1, -1};
int BTN_DOWN[] = {-1, -1};
int BTN_LEFT[] = {-1, -1};
int BTN_RIGHT[] = {-1, -1};
int BTN_A[] = {-1, -1};
int BTN_B[] = {-1, -1};
int BTN_SELECT[] = {-1, -1};
int BTN_START[] = {-1, -1};

static SDL_Window   *window;
static SDL_Renderer *renderer;
static SDL_Texture  *screen_tex;

// Menus:
Menu* menu;
Menu* mainMenu;
Menu* settingsMenu;
Menu* videoMenu;
Menu* keyboardMenu[2];
Menu* joystickMenu[2];
FileMenu* fileMenu;

SDL_Texture* gameTexture;
SDL_Texture* background;
TTF_Font* font;
u8 const* keys;

bool pause = true;
int last_window_size = 0;

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
    /*videoMenu->add(new Entry((quality), []{ set_render_quality((currentRenderQuality + 1) % 3);
                                             updateVideoMenu();
                                             menu = videoMenu;
                                             }));
    */
}

bool is_paused() {
    if(pause) return true;
    else return false;
}

void init(SDL_Window * scr, SDL_Renderer * rend) {
    renderer = rend;
    window = scr;

    for(int p = 0; p < 2; p++) {
        BTN_UP[p] = 17;
        BTN_DOWN[p] = 19;
        BTN_LEFT[p] = 16;
        BTN_RIGHT[p] = 18;
        BTN_A[p] = 0;
        BTN_B[p] = 1;
        BTN_SELECT[p] = 11;
        BTN_START[p] = 10;
    }

    printf("Initializing TTF\n");
    if (TTF_Init() < 0)
    {
        printf("Failed to init TTF\n");
        return;
    }

    printf("Creating gameTexture\n");
    gameTexture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    WIDTH, HEIGHT);

    printf("Opening font\n");
    font = TTF_OpenFont("res/font.ttf", FONT_SZ);
    if(!font) {
        printf("Failed to open res/font.ttf!");
        exit(1);
    }

    printf("Initializing PNGs\n");
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("Failed to initialize PNG!");
        exit(1);
    }

    printf("Loading background image...");
    SDL_Surface *backSurface = IMG_Load("res/init.png");

    background = SDL_CreateTextureFromSurface(renderer, backSurface);

    SDL_SetTextureColorMod(background, 60, 60, 60);
    SDL_FreeSurface(backSurface);

    if(!background) {
        printf("Failed to create background!");
        exit(1);
    }
    printf("Done\n");


    // Menus:
    mainMenu = new Menu;
    mainMenu->add(new Entry("Load ROM", [] { menu = fileMenu; }));
    mainMenu->add(new Entry("Settings", [] { menu = settingsMenu; }));
    mainMenu->add(new Entry("Exit", [] { exit(1); }));

    settingsMenu = new Menu;
    settingsMenu->add(new Entry("<", [] { menu = mainMenu; }));
    // TODO: Add this back and enable substituting the render quality during runtime
    // settingsMenu->add(new Entry("Video",        []{ menu = videoMenu; }));
    // settingsMenu->add(new Entry("Controller 1", []{ menu = joystickMenu[0]; }));

    // updateVideoMenu();

    // settingsMenu->add(new Entry("Controller 2", []{ menu = joystickMenu[1]; }));
    // settingsMenu->add(new Entry("Save Settings", [] { /*save_settings();*/ menu = mainMenu; }));

    for (int i = 0; i < 2; i++) {
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
    printf("Finished menu creation\n");
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

void update_menu(u8 select) {
    menu->update(select);
}

/* Play/stop the game */
void toggle_pause()
{
    printf("Toggling pause\n");
    pause = !pause;
    menu = mainMenu;

    if (pause)
    {
        printf("Paused, setting gameTexture\n");
        SDL_SetTextureColorMod(gameTexture, 60, 60, 60);
    }
    else
    {
        printf("Not paused, setting gameTexture\n");
        SDL_SetTextureColorMod(gameTexture, 255, 255, 255);
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
            printf("Button pressed: %d", e.jbutton.button);
            return e.jbutton.button;
        }
    }
}
}
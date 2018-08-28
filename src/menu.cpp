#include <dirent.h>
#include <unistd.h>

#include "menu.h"
#include "mapper.h"
#include "rom.h"

namespace GUI {

using namespace std;


Entry::Entry(string label, function<void()> callback, int x, int y) : callback(callback), x(x), y(y)
{
    setLabel(label);
}

Entry::~Entry()
{
    SDL_DestroyTexture(whiteTexture);
    SDL_DestroyTexture(redTexture);
}

void Entry::setLabel(string label)
{
    this->label = label;
    printf("Setting label for %s", label);

    if (whiteTexture != nullptr) SDL_DestroyTexture(whiteTexture);
    if (redTexture   != nullptr) SDL_DestroyTexture(redTexture);

    whiteTexture = gen_text(label, { 255, 255, 255 });
    redTexture   = gen_text(label, { 255,   0,   0 });
}

void Entry::render()
{
    printf("Entry render()\n");
    render_texture(selected ? redTexture : whiteTexture, getX(), getY());
    if(selected) {
        printf("Rendering texture as red\n");
    }
    else {
        printf("Rendering texture as white\n");
    }
}


ControlEntry::ControlEntry(string action, SDL_Scancode* key, int x, int y) : key(key),
    Entry::Entry(
        action,
        [&]{ keyEntry->setLabel(SDL_GetScancodeName(*(this->key) = query_key())); },
        x,
        y)
{
    this->keyEntry = new Entry(SDL_GetScancodeName(*key), []{}, TEXT_RIGHT, y);
}

ControlEntry::ControlEntry(string action, int* button, int x, int y) : button(button),
    Entry::Entry(
        action,
        [&]{ keyEntry->setLabel(to_string(*(this->button) = query_button())); },
        x,
        y)
{
    this->keyEntry = new Entry(to_string(*button), []{}, TEXT_RIGHT, y);
}


void Menu::add(Entry* entry)
{
    if (entries.empty())
        entry->select();
    entry->setY(entries.size() * FONT_SZ);
    entries.push_back(entry);
}

void Menu::clear()
{
    for (auto entry : entries)
        delete entry;
    entries.clear();
    cursor = 0;
}

void Menu::update(u8 select)
{
    printf("In menu update, select: %d\n", select);
    int oldCursor = cursor;

    if ((select == 32) and cursor < entries.size() - 1) {
        cursor++;
    }
    else if ((select == 16) and cursor > 0) {
        cursor--;
    }

    entries[oldCursor]->unselect();
    entries[cursor]->select();

    if (select == 1)
        entries[cursor]->trigger();

    printf("Cursor pos: %d", cursor);
}

void Menu::render()
{
    printf("Rendering Menu item\n");
    for (auto entry : entries)
        entry->render();
}


void FileMenu::change_dir(string dir)
{
    printf("In change_dir\n");
    clear();

    struct dirent* dirp;
    DIR* dp = opendir(dir.c_str());
    printf("Opened sdmc://\n");

    while ((dirp = readdir(dp)) != NULL)
    {
        string name = dirp->d_name;
        string path = dir + "/" + name;

        if (name[0] == '.' and name != "..") continue;

        if (dirp->d_type == DT_DIR)
        {
            add(new Entry(name + "/",
                          [=]{ change_dir(path); },
                          0));
        }
        else if (name.size() > 4 and name.substr(name.size() - 4) == ".nes")
        {
            add(new Entry(name,
                          [=]{ 
                              printf("\n\nLOADING ROM\n\n");
                              load_rom(path.c_str(), false); 
                              toggle_pause(); },
                          0));
        }
    }
    closedir(dp);
}

FileMenu::FileMenu()
{
    printf("In fileMenu constructor\n");
    char cwd[512];

    // change_dir(getcwd(cwd, 512));
    change_dir("sdmc://");
}


}
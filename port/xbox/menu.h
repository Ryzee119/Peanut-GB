/* MIT License
Copyright (c) [2020] [Ryan Wendland]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <windows.h>

#define ALIGN_CENTER -1
#define ITEM_TEXT_SIZE 18
#define HEADING1_TEXT_SIZE 40
#define HEADING2_TEXT_SIZE 32
#define HEADING3_TEXT_SIZE 25

static SDL_Color White = {255, 255, 255};
static SDL_Color Dark_Gray = {30, 30, 30};
static SDL_Color Gray = {70, 70, 70};
static SDL_Color Green = {152, 251, 152};

static SDL_Window *menu_window;
static SDL_Renderer *menu_renderer;
static TTF_Font* font_heading1;
static TTF_Font* font_heading2;
static TTF_Font* font_heading3;
static TTF_Font* font_body;

typedef struct menu_settings {
    char title[32];
    char sub_title[32];
    char item_path[256];
    char font_path[256];
    unsigned int screen_width;
    unsigned int screen_height;
    unsigned int max_items;
} menu_settings;

static unsigned int SCREEN_WIDTH = 640;
static unsigned int SCREEN_HEIGHT = 480;
static unsigned int XMARGIN = 0;
static unsigned int YMARGIN = 0;
static unsigned int MAX_ITEM_TEXT_WIDTH = 32;
static unsigned int MAX_ITEMS = 256;
static char* FONT_PATH = "";
static char* ITEM_PATH = "";

static menu_settings settings;
void init_menu(menu_settings * s){
    if (s == NULL){
        printf("Invalid settings, setting to some defaults\n");
        strncpy(settings.title, "Invalid Settings", sizeof(settings.title));
        strncpy(settings.sub_title, "", sizeof(settings.sub_title));
        strncpy(settings.item_path, "", sizeof(settings.item_path));
        settings.screen_width = 640;
        settings.screen_height = 480;
        settings.max_items = 256;
        return;
    }

    snprintf(settings.item_path, sizeof(settings.item_path), "%s\\*.*", s->item_path);
    strncpy(settings.title, s->title, sizeof(settings.title));
    strncpy(settings.sub_title, s->sub_title, sizeof(settings.sub_title));
    strncpy(settings.font_path, s->font_path, sizeof(settings.font_path));
    settings.screen_width = s->screen_width;
    settings.screen_height = s->screen_height;
    settings.max_items = s->max_items;
}

static int create_text(TTF_Font* font, char* text, SDL_Color font_color, int x, int y) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, font_color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(menu_renderer, surface);
    SDL_Rect rect;

    //Position text rect
    SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
    (x == ALIGN_CENTER) ? (rect.x =  SCREEN_WIDTH / 2 - rect.w / 2) : (rect.x = x);
    (y == ALIGN_CENTER) ? (rect.y =  SCREEN_HEIGHT / 2 - rect.h / 2) : (rect.y = y);

    //Cap the text length
    if (rect.w > MAX_ITEM_TEXT_WIDTH)
        rect.w = MAX_ITEM_TEXT_WIDTH;

    //Copy into renderer
    SDL_RenderCopy(menu_renderer, texture, NULL, &rect);

    //Clean up
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    return rect.h;
}

static int create_box(int x, int y, int w, int h, SDL_Color color, int alpha) {
    SDL_Surface *surface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, alpha);
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, color.r, color.g, color.b, alpha));
    SDL_Texture* texture = SDL_CreateTextureFromSurface(menu_renderer, surface);
    SDL_Rect rect;
    rect.x = x; rect.y = y;
    rect.w = w; rect.h = h;
    SDL_RenderCopy(menu_renderer, texture, NULL, &rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    return h;
}

//Sort an array alphabetically
static int compare(const void* a, const void* b) { 
    return strcmp(*(const char**)a, *(const char**)b); 
}
static void sort(char** string_array, int len) { 
    qsort(string_array, len, sizeof(char*), compare); 
}

void show_item_selection_menu(char* item_name, int max_len) {

    SCREEN_WIDTH = settings.screen_width;
    SCREEN_HEIGHT = settings.screen_height;
    XMARGIN = (SCREEN_WIDTH * 0.10);
    YMARGIN = (SCREEN_HEIGHT * 0.05);
    MAX_ITEM_TEXT_WIDTH = (SCREEN_WIDTH - 2 * XMARGIN);
    FONT_PATH = settings.font_path;
    ITEM_PATH = settings.item_path;
    MAX_ITEMS = settings.max_items;

    /* Init and Setup our SDL Libraries */
    TTF_Init();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
    SDL_GameController* controller = SDL_GameControllerOpen(0);
    menu_window = SDL_CreateWindow("Peanut-GB for Xbox",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_SHOWN);

    menu_renderer = SDL_CreateRenderer(menu_window, -1, 0);
    SDL_SetRenderDrawColor(menu_renderer, 15, 15, 15, 255);
    SDL_RenderClear(menu_renderer);

    debugPrint("%s\n", FONT_PATH);
    font_body = TTF_OpenFont(FONT_PATH, ITEM_TEXT_SIZE);
    font_heading1 = TTF_OpenFont(FONT_PATH, HEADING1_TEXT_SIZE);
    font_heading2 = TTF_OpenFont(FONT_PATH, HEADING2_TEXT_SIZE);
    font_heading3 = TTF_OpenFont(FONT_PATH, HEADING3_TEXT_SIZE);

    if (!font_body || !font_heading1 || !font_heading2 || !font_heading3){
        printf("Error opening fonts\n");
        while (1);
    }

    /* Create the Menu Text Headings */
    int ypos = YMARGIN;
    ypos += create_text(font_heading2, settings.title, Green, ALIGN_CENTER, ypos);
    ypos += create_text(font_body, settings.sub_title, Green, ALIGN_CENTER, ypos);
    ypos += create_box(0, ypos, SCREEN_WIDTH, 3, White, 255); 
    create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN * 2, SCREEN_HEIGHT - YMARGIN - ypos, Gray, 255); 
    int first_item_pos = ypos; //Store this position so we can revert to it when we redraw the item list

    /* Calculate how many items we can fit on a screen */
    int f_height = TTF_FontHeight(font_body);
    int max_items_per_screen = (SCREEN_HEIGHT - YMARGIN - ypos) / f_height - 1;

    /* Start searching for files and populate them in a list */
    //TODO: Handle sub directories
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;
    hFind = FindFirstFile(ITEM_PATH, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        create_text(font_body, "Place some items in the Item directory", White, XMARGIN, ypos);
        SDL_RenderPresent(menu_renderer);
        while (1);
    }
    int item_cnt = 0;
    char **item_array = malloc(MAX_ITEMS * sizeof(char *));
    if (!item_array) {
        fprintf(stderr, "Error allocating item_array. Too many items?\n");
        while (1);
    }
    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            item_array[item_cnt] = (char *)malloc(strlen(findFileData.cFileName) + 1/*TERMINATOR*/);
            if (item_array[item_cnt] == NULL) {
                fprintf(stderr, "Error allocating item_array. Too many items?\n");
                while (1);
            }
            strncpy(item_array[item_cnt], findFileData.cFileName, strlen(findFileData.cFileName) + 1);
            item_cnt++;
    } while (FindNextFile(hFind, &findFileData) != 0 && item_cnt < MAX_ITEMS);

    //FindNextFile isnt neccessarily alphabetical. So we sort it
    sort(item_array, item_cnt); 

    //Calculate how many pages it should have
    int num_pages = 0;
    while ((num_pages + 1) * max_items_per_screen  < item_cnt)
        num_pages++;

    /* Main Render and Input Loop */
    bool item_selected = false;
    int page_num = 0;
    int cursor_pos = 0;
    bool cursor_changed = true;
    bool page_changed = true;
    SDL_ControllerButtonEvent *controller_button;
    while (!item_selected) {
        static int fps_cnt = 0;
        static int fps_timer = 0;
        static int fps = 0;
        if (fps_cnt++ == 10)
        {
            fps = 1000 * 10 / (SDL_GetTicks() - fps_timer);
            fps_timer = SDL_GetTicks();
            fps_cnt = 0;
        }
        extern int nextRow;
        nextRow = 10;
        debugPrint("FPS: %u\n", fps);

        static SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERBUTTONDOWN)
            {
                controller_button = (SDL_ControllerButtonEvent *)&e;
                if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    if (--page_num < 0)
                        page_num = num_pages;
                    cursor_pos = 0;
                    page_changed = true;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    if (++page_num > num_pages)
                        page_num = 0;
                    cursor_pos = 0;
                    page_changed = true;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    if (++cursor_pos > max_items_per_screen - 1)
                        cursor_pos = 0;

                    if (page_num * max_items_per_screen + cursor_pos > item_cnt - 1)
                        cursor_pos = 0;

                    cursor_changed = true;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    if (--cursor_pos < 0)
                        cursor_pos = max_items_per_screen - 1;

                    if (page_num * max_items_per_screen + cursor_pos > item_cnt - 1)
                        cursor_pos = item_cnt % max_items_per_screen - 1;

                    cursor_changed = true;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_A ||
                        controller_button->button == SDL_CONTROLLER_BUTTON_START) {
                    item_selected = true;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                    page_num -= num_pages / 10;
                    if (page_num < 0)
                        page_num += num_pages;
                    cursor_pos = 0;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                    page_num += num_pages / 10;
                    if (page_num > num_pages)
                        page_num -= num_pages;
                    cursor_pos = 0;
                }
            }
        }

        if (page_changed || cursor_changed || item_selected) {

            ypos = first_item_pos;
            if (page_changed){
                create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN * 2, SCREEN_HEIGHT - YMARGIN - ypos, Gray, 255);
                static char pageofpage[32];
                snprintf(pageofpage,sizeof(pageofpage),"Page %u of %u", page_num + 1, num_pages + 1);
                create_text(font_body, pageofpage, Green, XMARGIN, SCREEN_HEIGHT - YMARGIN - f_height);
            }

            //Start creating the list of files for the current page
            for (int i = 0 ; i < max_items_per_screen; i++) {
                int item_offset = page_num * max_items_per_screen + i;
                if (item_offset < item_cnt) {
                    static int cursor_pos_old = 0;
                    char* item_string = item_array[item_offset];

                    //If you seleted this item, we're done
                    if (i == cursor_pos && item_selected) {
                        strncpy(item_name, item_string, max_len);
                        create_box(XMARGIN, first_item_pos, SCREEN_WIDTH - XMARGIN * 2,
                                   SCREEN_HEIGHT - YMARGIN - first_item_pos, Gray, 255);
                        ypos += create_text(font_heading1, "LOADING", White, ALIGN_CENTER, ALIGN_CENTER);
                        break;
                    }
                    //Clear old highlight box
                    if (i == cursor_pos_old && cursor_changed) {
                        int h = create_text(font_body, item_string, White, XMARGIN, ypos);
                        create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN * 2, h, Gray, 255);
                        cursor_pos_old = cursor_pos;
                    }
                    //Draw new highlight box
                    if (i == cursor_pos && cursor_changed) {
                        int h = create_text(font_body, item_string, Dark_Gray, XMARGIN, ypos);
                        create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN * 2, h, Green, 255);
                        ypos += create_text(font_body, item_string, Dark_Gray, XMARGIN, ypos);
                    } else {
                        //Else just drawing the item text for the list
                        ypos += create_text(font_body, item_string, White, XMARGIN, ypos); 
                    }
                }
            }
        }
        #ifdef NXDK
        //XVideoWaitForVBlank();
        #endif
        //SDL_UpdateWindowSurface(menu_window);
        SDL_RenderPresent(menu_renderer);
    }

    //Exiting the menu, clean everything up.
    //Gameboy emulator will reinit SDL libs as required.
    for (int i = 0; i < item_cnt; i++)
    {
        free(item_array[i]);
    }
    free(item_array);

    if (menu_renderer)
        SDL_DestroyRenderer(menu_renderer);

    if (menu_window)
        SDL_DestroyWindow(menu_window);

    FindClose(hFind);
    SDL_GameControllerClose(controller);

    TTF_CloseFont(font_body);
    TTF_CloseFont(font_heading1);
    TTF_CloseFont(font_heading2);
    TTF_CloseFont(font_heading3);
    TTF_Quit();
    SDL_Quit();
}
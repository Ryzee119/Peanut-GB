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
   #include <hal/debug.h>

#define ALIGN_CENTER -1

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define XMARGIN (SCREEN_WIDTH * 0.10)
#define YMARGIN (SCREEN_HEIGHT * 0.075)
#define MAX_ROMS_PER_SCREEN 13 //FIXME: SHOULD BE A FUNCTION OF TEXT SIZE/BOX HEIGHT
#define MAX_ROMS_TEXT_WIDTH (SCREEN_WIDTH - 2 * XMARGIN)

#define ROM_PATH "D:\\Roms\\*.gb"
#define MAX_ROMS 2048

#define ROM_TEXT_SIZE 18
#define HEADING2_TEXT_SIZE 25
#define HEADING1_TEXT_SIZE 32
#define LOADING_TEXT_SIZE 40

static SDL_Color White = {255, 255, 255};
static SDL_Color Dark_Gray = {20, 20, 20};
static SDL_Color Gray = {70, 70, 70};
static SDL_Color Yellow = {202, 179, 136};

static SDL_Window *menu_window;
static SDL_Renderer *menu_renderer;

static int create_text(int size, char* text, SDL_Color font_color, int x, int y) {
    TTF_Font* font = TTF_OpenFont("D:\\font.ttf", size);
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, font_color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(menu_renderer, surface);
    SDL_Rect rect;

    //Position text rect
    SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
    (x == ALIGN_CENTER) ? (rect.x =  SCREEN_WIDTH / 2 - rect.w / 2) : (rect.x = x);
    (y == ALIGN_CENTER) ? (rect.y =  SCREEN_HEIGHT / 2 - rect.h / 2) : (rect.y = y);

    //Cap the text length
    if (rect.w > MAX_ROMS_TEXT_WIDTH)
        rect.w = MAX_ROMS_TEXT_WIDTH;

    //Copy into renderer
    SDL_RenderCopy(menu_renderer, texture, NULL, &rect);

    //Clean up
    TTF_CloseFont(font);
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

void show_rom_selection_menu(char* rom_name, int max_len) {
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

    menu_renderer = SDL_CreateRenderer(menu_window, ALIGN_CENTER, 0);
    SDL_SetRenderDrawColor(menu_renderer, 15, 15, 15, 255);
    SDL_RenderClear(menu_renderer);

    /* Create the Menu Text Headings */
    int ypos = YMARGIN;
    ypos += create_text(HEADING1_TEXT_SIZE, "Peanut-GB for Xbox", White, -1, ypos);
    ypos += create_box(0, ypos, SCREEN_WIDTH, 5, Gray, 255); 
    ypos += create_text(HEADING2_TEXT_SIZE, "Select your Gameboy ROM:", White, XMARGIN, ypos);
    create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN*2, SCREEN_HEIGHT - YMARGIN - ypos, Gray, 255); 
    int first_rom_pos = ypos; //Store this position so we can revert to it when we redraw the rom list

    /* Start searching for files and populate them in a list */
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;
    hFind = FindFirstFile(ROM_PATH, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        create_text(ROM_TEXT_SIZE, "Place some games in the Roms directory", White, XMARGIN, ypos);
        SDL_RenderPresent(menu_renderer);
        while (1) {Sleep(1000);}
    }
    int rom_cnt = 0;
    char **rom_array = malloc(MAX_ROMS * sizeof(char *));
    if (!rom_array) {
        debugPrint("Error allocating rom_array. Too many roms?\n");
        while (1);
    }
    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            rom_array[rom_cnt] = (char *)malloc(strlen(findFileData.cFileName) + 1);
            if (!rom_array[rom_cnt]) {
                debugPrint("Error allocating rom_array. Too many roms?\n");
                while (1);
            }
            strncpy(rom_array[rom_cnt], findFileData.cFileName, 256);
            rom_cnt++;
    } while (FindNextFile(hFind, &findFileData) != 0 && rom_cnt < MAX_ROMS);
    sort(rom_array, rom_cnt); //FindNextFile isnt neccessarily alphabetical. sort it
    int num_pages = 0;
    while ((num_pages + 1) * MAX_ROMS_PER_SCREEN  < rom_cnt)
        num_pages++;

    /* Main Render and Input Loop */
    bool rom_selected = false;
    int page_num = 0;
    int cursor_pos = 0;
    bool render_screen = true;
    SDL_ControllerButtonEvent *controller_button;
    while (!rom_selected) {
        static SDL_Event e;
        while (SDL_PollEvent(&e)) {
             if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                render_screen = true;
                controller_button = (SDL_ControllerButtonEvent *)&e;
                if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    if (--page_num < 0)
                        page_num = num_pages;
                    cursor_pos = 0;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    if (++page_num > num_pages)
                        page_num = 0;
                    cursor_pos = 0;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    if (cursor_pos < MAX_ROMS_PER_SCREEN - 1 &&
                        page_num * MAX_ROMS_PER_SCREEN + cursor_pos < rom_cnt - 1) {
                        cursor_pos++;
                    }
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    if (cursor_pos > 0)
                        cursor_pos--;
                }
                else if (controller_button->button == SDL_CONTROLLER_BUTTON_A ||
                        controller_button->button == SDL_CONTROLLER_BUTTON_START) {
                    rom_selected = true;
                }
             }
        }

        if (render_screen || rom_selected) {
            render_screen = false;

            ypos = first_rom_pos;
            create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN * 2, SCREEN_HEIGHT - YMARGIN - ypos, Gray, 255);

            //
            static char pageofpage[32];
            snprintf(pageofpage,sizeof(pageofpage),"Page %u of %u", page_num + 1, num_pages + 1);
            create_text(ROM_TEXT_SIZE * 0.75, pageofpage, Yellow, XMARGIN, SCREEN_HEIGHT - YMARGIN * 1.5);
            //Start creating the list of files for the current page
            for (int i = 0 ; i < MAX_ROMS_PER_SCREEN; i++) {
                int rom_offset = page_num * MAX_ROMS_PER_SCREEN + i;
                if (rom_offset < rom_cnt) {
                    char* rom_string = rom_array[rom_offset];
                    //If the current rom is where the cursor is, draw a box around it
                    if (i == cursor_pos) {
                        //If you seleted this rom, we're done
                        if (rom_selected) {
                            strncpy(rom_name, rom_string, max_len);
                            create_box(XMARGIN, first_rom_pos, SCREEN_WIDTH - XMARGIN*2,
                                        SCREEN_HEIGHT - YMARGIN - first_rom_pos, Gray, 255); 
                            create_text(LOADING_TEXT_SIZE, "LOADING", White, ALIGN_CENTER, ALIGN_CENTER);
                            break;
                        } else {
                            //Draw text so we get its height, then use that to draw the highlight box
                            //Then redraw the text on top of the highlight box. This seems messy
                            int h = create_text(ROM_TEXT_SIZE, rom_string, Dark_Gray, XMARGIN, ypos);
                            create_box(XMARGIN, ypos, SCREEN_WIDTH - XMARGIN*2, h, White, 255);
                            ypos += create_text(ROM_TEXT_SIZE, rom_string, Dark_Gray, XMARGIN, ypos);
                        }
                    } else {
                        ypos += create_text(ROM_TEXT_SIZE, rom_string, White, XMARGIN, ypos); 
                    }
                }
            }
        }
        
        #ifdef NXDK
        XVideoWaitForVBlank();
        #endif
        SDL_RenderPresent(menu_renderer);
    }

    //Exiting the menu, clean everything up.
    //Gameboy emulator will reinit SDL libs as required.
    for (int i = 0; i < rom_cnt; i++) {
        free(rom_array[i]);
    } free(rom_array);

    if (menu_renderer)
        SDL_DestroyRenderer(menu_renderer);

    if (menu_window)
        SDL_DestroyWindow(menu_window);

    FindClose(hFind);
    SDL_GameControllerClose(controller);
    TTF_Quit();
    SDL_Quit();
}
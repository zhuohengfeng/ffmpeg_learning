//
// Created by hengfeng zhuo on 2019-05-28.
//

#include "main.h"
#include <SDL.h>

using namespace std;

int main(int argc, char* argv[]) {
    SDL_Renderer* render = NULL;
    SDL_Window* window = NULL;

    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    window = SDL_CreateWindow("SDL2 window", 200, 200, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

    if (!window) {
        printf("Failed to create SDL window");
        goto _EXIT;
    }

    render = SDL_CreateRenderer(window, -1, 0);
    if (!render) {
        printf("Failed to create SDL render");
        goto _EXIT;
    }

    SDL_SetRenderDrawColor(render, 255, 0, 0, 255);

    SDL_RenderClear(render);
    SDL_RenderPresent(render);

    SDL_Delay(10000);

_EXIT:
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
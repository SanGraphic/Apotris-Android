#include <SDL.h>
#include <string>
#include <vector>

extern void initShaders(SDL_Window* window, int index);

extern void drawWithShaders(SDL_Window* window, SDL_Surface* img,
                            bool shadersEnabled);

extern std::vector<std::string> findShaders();

extern void freeShaders();

extern void refreshShaderResolution(int w, int h, float s);

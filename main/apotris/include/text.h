#pragma once

#include <string>

extern void aprint(std::string, int, int);
extern void aprintColor(std::string, int, int, int);
extern void aprintSprite(std::string str, int tile);

extern void aprintf(int, int, int);

extern void aprints(std::string, int, int, int);
extern void aprintsSprite(std::string, int, int, int);

extern void setSmallTextArea(int tid, int startX, int startY, int endX,
                             int endY);
extern void clearSmallText();
extern void resetSmallText();
extern void clearText();
extern void aprintClearLine(int);
extern void aprintClearArea(int x, int y, int w, int h);

extern int getVariableWidth(std::string str);
extern void naprint(std::string str, int x, int y);
extern void naprintColor(std::string str, int x, int y, int palette);

extern void naprintSprite(std::string str, int tile);
extern void naprintSpriteOffset(std::string str, int tile, int col);

extern int textAllocateTile();

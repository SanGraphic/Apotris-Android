#include "def.h"
#include "logging.h"
#include "platform.hpp"
#include <posprintf.h>
#include <string>

#include "font1tiles_bin.h"

static int allocatedTiles = 0;
const int allocateStart = 124;

class TextArea {
public:
    int tileId;
    int startX;
    int startY;
    int endX;
    int endY;

    TextArea() {}
    TextArea(int tid, int sx, int sy, int ex, int ey) {
        tileId = tid;

        startX = sx;
        endX = ex;
        startY = sy;
        endY = ey;
    }
};

TextArea* textArea = nullptr;

void aprint(std::string str, int x, int y) { aprintColor(str, x, y, 0); }

void aprintSprite(std::string str, int tile) {
    for (int i = 0; i < (int)str.length(); i++) {
        int c = str[i] - 32;
        u8* src = (u8*)&font1tiles_bin[(c - 1) * 32];
        setSpriteTile(tile, i, 0, 1, src);
    }
}

void aprintClearLine(int y) { clearTiles(29, 32 * y, 32); }

void aprintClearArea(int x, int y, int w, int h) {
    for (int i = y; i < h + y; i++) {
        clearTilemapEntries(29, 32 * i + x, w);
    }
}

void aprintf(int n, int x, int y) {
    char buff[30];
    posprintf(buff, "%d", n);
    aprint(buff, x, y);
}

void aprintColor(std::string str, int x, int y, int palette) {
    for (int i = 0; i < (int)str.length(); i++) {
        setTile(29, x + i, y,
                tileBuild(str[i] - 32, false, false, 15 - palette));
    }
}

void clearText() {
    clearTilemap(29);
    clearTiles(2, allocateStart, allocatedTiles);
    allocatedTiles = 0;

    if (textArea == nullptr)
        return;

    clearTiles(2, textArea->tileId,
               (textArea->endX - textArea->startX + 1) *
                   (textArea->endY - textArea->startY));

    int counter = 0;
    for (int i = textArea->startY; i <= textArea->endY; i++)
        for (int j = textArea->startX; j <= textArea->endX; j++)
            setTile(29, j, i,
                    tileBuild(textArea->tileId + counter++, false, false, 15));
}

void setSmallTextArea(int tid, int startX, int startY, int endX, int endY) {
    if (textArea != nullptr)
        delete textArea;

    textArea = new TextArea(tid, startX, startY, endX, endY);

    // set screen entry to tile mem section, needs to happen once unless
    // overriden
    int counter = 0;
    for (int i = startY; i <= endY; i++)
        for (int j = startX; j <= endX; j++)
            setTile(29, j, i,
                    tileBuild(textArea->tileId + counter++, false, false, 15));
}

void clearSmallText() {
    if (textArea == nullptr)
        return;
    clearTiles(2, textArea->tileId,
               (textArea->endX - textArea->startX + 1) *
                   (textArea->endY - textArea->startY));
}

void resetSmallText() {
    if (textArea == nullptr)
        return;

    clearSmallText();

    if (textArea != nullptr) {
        delete textArea;
        textArea = nullptr;
    }
}

IWRAM_CODE void aprints(std::string str, int x, int y,
                        int colorIndex) { // x and y are pixel
    if (textArea == nullptr)
        return;

    const bool shadow = (colorIndex == 2);

    // width of current line, acts like a cursor position so that
    // text is drawn on the next line if w is greater than text area width
    int w = 0;

    for (int i = 0; i < (int)str.length(); i++) {
        if (str[i] == '\n') {
            w = 0;
            y += 8;
            continue;
        } else if (str[i] == ' ') {
            w += 4;
            continue;
        }

        u16 character = font3x5[str[i] - 32];

        // least significant byte determines if the character should be
        // drawn 1 pixel down (for characters like q g j)
        int drop = character & 1;

        const int rowSize = (textArea->endX - textArea->startX + 1);

        int counter = 0;
        for (int j = 0; j < 5; j++) {
            for (int k = 0; k < 3; k++) {
                if ((character >> (15 - counter)) & 1)
                    setTilesetPixel(2,
                                    textArea->tileId +
                                        ((y + j + drop) / 8) * rowSize +
                                        (x + w + k) / 8,
                                    (x + w + k) % 8, (y + j + drop) % 8,
                                    colorIndex + 2 * (j > 2) * shadow);

                counter++;
            }
        }
        w += 4;
    }
}

IWRAM_CODE void aprintsSprite(std::string str, int x, int y,
                              int tileId) { // x and y are pixel
    // width of current line, acts like a cursor position so that
    // text is drawn on the next line if w is greater than text area width
    int w = 0;

    for (int i = 0; i < (int)str.length(); i++) {
        if (str[i] == '\n') {
            w = 0;
            y += 8;
            continue;
        } else if (str[i] == ' ') {
            w += 4;
            continue;
        }

        u16 character = font3x5[str[i] - 32];

        // least significant byte determines if the character should be
        // drawn 1 pixel down (for characters like q g j)
        int drop = character & 1;

        int counter = 0;
        for (int j = 0; j < 5; j++) {
            for (int k = 0; k < 3; k++) {

                // figure out if pixel at j,k should be drawn
                int draw = (character >> (15 - counter)) & 1;

                if (draw)
                    setSpritePixel(tileId,
                                   (x + w + k) / 8 + ((y + j + drop) / 8), 0, 0,
                                   (((x + w + k) % 8)), (y + j + drop) % 8,
                                   2 + 2 * (j > 2));
                counter++;
            }
        }
        w += 4;
    }
}

const u8 fontWidths[97][2] = {
    {1, 1}, {2, 4}, {1, 5}, {0, 7}, {0, 7}, {0, 8}, {0, 8}, {1, 3}, {1, 5},
    {2, 5}, {0, 7}, {0, 7}, {2, 3}, {0, 7}, {2, 3}, {0, 8}, {0, 8}, {0, 8},
    {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {2, 3},
    {1, 3}, {0, 5}, {0, 7}, {2, 5}, {0, 7}, {0, 8}, {0, 8}, {0, 8}, {0, 8},
    {0, 8}, {0, 7}, {0, 7}, {0, 8}, {0, 8}, {1, 6}, {1, 6}, {0, 8}, {0, 7},
    {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 8}, {1, 7}, {0, 8},
    {0, 8}, {0, 8}, {0, 8}, {0, 8}, {0, 7}, {1, 5}, {0, 8}, {2, 5}, {0, 8},
    {0, 8}, {3, 4}, {1, 7}, {1, 6}, {1, 6}, {1, 6}, {1, 6}, {1, 6}, {1, 6},
    {1, 6}, {2, 4}, {1, 5}, {1, 6}, {1, 5}, {0, 8}, {1, 6}, {1, 6}, {1, 6},
    {1, 6}, {1, 6}, {1, 6}, {1, 6}, {1, 6}, {1, 6}, {0, 8}, {1, 6}, {1, 6},
    {1, 6}, {1, 5}, {2, 3}, {2, 5}, {0, 7}, {0, 8}, {0, 8},
};

const u16 font3x5[96] = {
    0x0000, 0x4904, 0xb400, 0xbefa, 0x7bbc, 0xa54a, 0x555c, 0x4800, 0x2922,
    0x4494, 0x1550, 0xba0,  0x0028, 0x0380, 0x0004, 0x2548, 0x76dc, 0x592e,
    0xc5ce, 0xc51c, 0x92f2, 0xf39c, 0x73dc, 0xe548, 0x77dc, 0x779c, 0x0820,
    0x0828, 0x2a22, 0x1c70, 0x88a8, 0xe504, 0x56c6, 0x77da, 0x775c, 0x7246,
    0xd6dc, 0x73ce, 0x73c8, 0x72d6, 0xb7da, 0xe92e, 0x64d4, 0xb75a, 0x924e,
    0xbeda, 0xd6da, 0x56d4, 0xd7c8, 0x76f6, 0x775a, 0x711c, 0xe924, 0xb6d6,
    0xb6d4, 0xb6fa, 0xb55a, 0xb524, 0xe54e, 0x6926, 0x9112, 0x6496, 0x5400,
    0x000e, 0x4400, 0x0ed6, 0x9adc, 0x0e46, 0x2ed6, 0x0ee6, 0x5668, 0x559d,
    0x935a, 0x4122, 0x4129, 0x975a, 0x4922, 0x17da, 0x1ada, 0x0ad4, 0xd6e9,
    0x76b3, 0x1748, 0x0f1c, 0x9a46, 0x16d6, 0x16dc, 0x16fa, 0x155a, 0xb595,
    0x1cae, 0x6b26, 0x4924, 0xc9ac, 0x5400, 0x56f0,
};

const int fontHeight = 8;
const int fontGap = 1;
const int spaceWidth = 6;

int getVariableWidth(std::string str) {
    int widthCount = 0;
    for (int i = 0; i < (int)str.size(); i++) {
        if (str[i] - 32) {
            widthCount += ((fontWidths[str[i] - 32][1]) + fontGap);
        } else
            widthCount += spaceWidth;
    }

    return widthCount;
}

void naprint(std::string str, int x, int y) { naprintColor(str, x, y, 15); }

void naprintColor(std::string str, int x, int y, int palette) {
    // calculate tiles needed to write word
    const int len = str.size();

    // count total width for variable width font
    int widthCount = getVariableWidth(str);

    // divide pixel width by 8 to get tile count andadd
    // add 1 if pixel width / 8 has a remainder
    const int tileW = ((widthCount) >> 3) + (((x + widthCount) & 0x7) != 0) + 1;
    const int tileH = ((fontHeight) >> 3) + (((y + fontHeight) & 0x7) != 0) + 1;

    // allocate tiles
    const int allocatePosition = allocateStart + allocatedTiles;
    const int toAllocate = tileW * tileH;
    allocatedTiles += toAllocate;

    // write font to tiles
    const int offsetx = x & 0x7;
    const int offsety = y & 0x7;
    int cursor = 0;
    for (int i = 0; i < len; i++) {
        const int character = str[i] - 32;

        if (!character) {
            cursor += spaceWidth;
            continue;
        }

        TILE* src = (TILE*)&font1tiles_bin[(character - 1) * 32];
        const int start = fontWidths[character][0];
        const int end = fontWidths[character][1];

        for (int py = 0; py < fontHeight; py++) {
            const int row = src->data[py];
            if (!row)
                continue;

            const int cy = (py + offsety);
            const int toy = (allocatePosition + ((cy) >> 3) * (tileW));

            for (int px = 0; px < end; px++) {
                const int color = ((row) >> (4 * (px + start))) & 0xf;

                if (color) {
                    const int cx = (px + cursor + offsetx);
                    setTilesetPixel(2, toy + ((cx) >> 3), cx & 0x7, cy & 0x7,
                                    color);
                }
            }
        }

        cursor += fontWidths[character][1] + fontGap;
    }

    // write tiles to map

    int counter = 0;
    for (int ty = 0; ty < tileH; ty++) {
        for (int tx = 0; tx < tileW; tx++) {
            setTile(
                29, (x >> 3) + tx, (y >> 3) + ty,
                tileBuild(allocatePosition + counter, false, false, palette));
            counter++;
        }
    }
}

void naprintSprite(std::string str, int tile) {
    naprintSpriteOffset(str, tile, 0);
}

IWRAM_CODE void naprintSpriteOffset(std::string str, int tile, int col) {

    // if(str.size() > 12)
    // 	str = str.substr(0,12);

    // //calculate tiles needed to write word
    // const int len = min((int)str.size(),12);
    const int len = (int)str.size();

    const int widthCount = getVariableWidth(str);

    // divide pixel width by 8 to get tile count and add
    // 1 if pixel width / 8 has a remainder
    const int tileW = ((widthCount) >> 3);

    // write font to tiles
    int cursor = col;
    for (int i = 0; i < len; i++) {
        const int character = str[i] - 32;

        if (!character) {
            cursor += spaceWidth;
            continue;
        }

        TILE* src = (TILE*)&font1tiles_bin[(character - 1) * 32];
        const int start = fontWidths[character][0];
        const int x = cursor & 0x7;

        for (int py = 0; py < fontHeight; py++) {
            const int row = src->data[py];
            if (!row)
                continue;

            const int toy = (tile + ((py) >> 3) * (tileW)) + (cursor >> 3);

            if (x) {
                int part = (row >> (4 * start)) & ((1 << ((8 - x) * 4)) - 1);
                setSpriteTileRow(toy, py & 0x7, part << (x * 4));

                part = (row >> (4 * (start + 8 - x))) & ((1 << (x * 4)) - 1);
                setSpriteTileRow(toy + 1, py & 0x7, part);
            } else {
                int part = (row >> (4 * start));
                setSpriteTileRow(toy, py & 0x7, part << (x * 4));
            }
        }

        cursor += fontWidths[character][1] + fontGap;
    }
}

int textAllocateTile() { return allocateStart + (allocatedTiles++); }

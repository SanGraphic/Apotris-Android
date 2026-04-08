#pragma once

#include "def.h"
#include "scene.hpp"

class ColorSelectorScene : public Scene {
private:
    int das = 0;
    const int arr = 4;
    const int dasMax = 20;
    int cursorFloat = 0;

    int selection = 0;
    const int options = 2;

    const int slotHeight = 9;
    OBJ_ATTR* cursorSprites[2];

    int paletteIndex = 0;

    void leave();

public:
    void draw();
    void update();
    bool control();
    void init();
    void deinit();

    Scene* previousScene() { return new TitleScene(); };

    ColorSelectorScene() {}
    ~ColorSelectorScene() { deinit(); }
};

class ColorEditorScene : public Scene {
private:
    int selection = 0;

    int maxSelection = 2;

    int das = 0;

    bool refresh = true;
    bool onColors = true;

    int cursorX = 0;
    int cursorY = 0;

    int cursorFloat = 0;

    const int dasMax = 10;
    const int arrMax = 2;

    HSV_COLOR hsv;
    bool hsvMode = false;
    int color1[3] = {0, 0, 0};
    OBJ_ATTR* colorSprites[3];
    OBJ_ATTR* cursorSprites[2];
    int cursorTimer = 0;
    int paletteIndex = 0;

    COLOR* paletteBeforeReset = nullptr;

    const int colorSelectorHeight = 2;

    int resetTimer = 0;
    const int resetTimerMax = 60;
    bool reset = false;

    int resetIndex = 0;

    bool set(int dir) {
        bool res = false;
        switch (selection) {
        case 0:
            hsv.hue += dir;
            if (hsv.hue < 0)
                hsv.hue = 359;
            else if (hsv.hue > 359)
                hsv.hue = 0;
            else
                res = true;
            break;
        case 1:
            hsv.saturation += ((float)dir) / 100.f;
            if (hsv.saturation < 0)
                hsv.saturation = 0;
            else if (hsv.saturation > 1)
                hsv.saturation = 1;
            else
                res = true;
            break;
        case 2:
            hsv.value += ((float)dir) / 100.f;
            if (hsv.value < 0)
                hsv.value = 0;
            else if (hsv.value > 1)
                hsv.value = 1;
            else
                res = true;
            break;
        }

        return res;
    }

    void showMinos();
    void showPalettes();
    void calculate();
    void showCursor();
    void setHSV();
    void setRGB();
    void drawText();

public:
    void draw();
    void update();
    bool control();
    void init();
    void deinit();

    Scene* previousScene() { return new ColorSelectorScene(); };

    ColorEditorScene(int paletteIndex) { this->paletteIndex = paletteIndex; }
    ~ColorEditorScene() { deinit(); }
};

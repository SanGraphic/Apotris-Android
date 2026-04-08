#pragma once

#include "scene.hpp"

class ProfileElement : public Element {
public:
    std::string getLabel() override { return "Profile"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.selectedProfile > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.selectedProfile < 4)
            result += ">";

        return result;
    }

    void action(int dir) override {
        int prev = savefile->settings.selectedProfile;
        if (dir > 0) {
            if (savefile->settings.selectedProfile < 4) {
                savefile->settings.selectedProfile++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.selectedProfile > 0) {
                savefile->settings.selectedProfile--;
                sfx(SFX_MENUMOVE);
            }
        }

        if (prev != savefile->settings.selectedProfile) {
            saveToProfile(&savefile->settings.profiles[prev],
                          &savefile->settings);

            profileToSave(&savefile->settings
                               .profiles[savefile->settings.selectedProfile],
                          &savefile->settings);

            setSkin();
            setPalette();
            drawFrameBackgrounds();
            setClearEffect();
        }
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.selectedProfile + 1);
    }
};

class GradientElement : public Element {
public:
    std::string getLabel() override { return "Background"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override;

    std::string getCurrentOption() override { return "..."; }
};

class GridElement : public Element {
public:
    std::string getLabel() override { return "Grid"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.backgroundGrid > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.backgroundGrid < MAX_BACKGROUNDS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.backgroundGrid < MAX_BACKGROUNDS - 1) {
                savefile->settings.backgroundGrid++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.backgroundGrid > 0) {
                savefile->settings.backgroundGrid--;
                sfx(SFX_MENUMOVE);
            }
        }
        drawGrid();
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.backgroundGrid + 1);
    }
};

class SkinElement : public Element {
public:
    std::string getLabel() override { return "Skin"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.skin > -(MAX_CUSTOM_SKINS))
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.skin < MAX_SKINS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.skin < MAX_SKINS - 1) {
                savefile->settings.skin++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.skin > -(MAX_CUSTOM_SKINS)) {
                savefile->settings.skin--;
                sfx(SFX_MENUMOVE);
            }
        }
        setSkin();
    }

    std::string getCurrentOption() override {
        if (savefile->settings.skin >= 0) {
            return std::to_string(savefile->settings.skin + 1);
        } else {
            return "C" + std::to_string(-(savefile->settings.skin));
        }
    }
};

class GhostElement : public Element {
public:
    std::string getLabel() override { return "Ghost Piece"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.shadow > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.shadow < MAX_SHADOWS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.shadow < MAX_SHADOWS - 1) {
                savefile->settings.shadow++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.shadow > 0) {
                savefile->settings.shadow--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.shadow + 1);
    }
};

class FrameColorElement : public Element {
public:
    std::string getLabel() override { return "Frame Color"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.palette > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.palette < 7)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.palette < 7) {
                savefile->settings.palette++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.palette > 0) {
                savefile->settings.palette--;
                sfx(SFX_MENUMOVE);
            }
        }
        setPalette();
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.palette + 1);
    }
};

class FrameBackgroundsElement : public Element {
public:
    std::string getLabel() override { return "Frame Backgrounds"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.frameBackground > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.frameBackground < MAX_FRAME_BACKGROUNDS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.frameBackground <
                MAX_FRAME_BACKGROUNDS - 1) {
                savefile->settings.frameBackground++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.frameBackground > 0) {
                savefile->settings.frameBackground--;
                sfx(SFX_MENUMOVE);
            }
        }
        drawFrameBackgrounds();
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.frameBackground + 1);
    }
};

class PaletteElement : public Element {
public:
    std::string getLabel() override { return "Palette"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.colors > -(MAX_CUSTOM_PALETTES))
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.colors < MAX_COLORS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.colors < MAX_COLORS - 1) {
                savefile->settings.colors++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.colors > -(MAX_CUSTOM_PALETTES)) {
                savefile->settings.colors--;
                sfx(SFX_MENUMOVE);
            }
        }

        setPalette();
    }

    std::string getCurrentOption() override {
        if (savefile->settings.colors >= 0) {
            return std::to_string(savefile->settings.colors + 1);
        } else {
            return "C" + std::to_string(-(savefile->settings.colors));
        }
    }
};

class BlockEdgesElement : public Element {
public:
    std::string getLabel() override { return "Block Edges"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.edges = !savefile->settings.edges;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.edges)
            return "ON";
        else
            return "OFF";
    }
};

class LightModeElement : public Element {
public:
    std::string getLabel() override { return "Light Mode"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.lightMode = !savefile->settings.lightMode;
        setSkin();
        setLightMode();
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.lightMode)
            return "ON";
        else
            return "OFF";
    }
};

class BoardEffectsElement : public Element {
public:
    std::string getLabel() override { return "Board Effects"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.effects > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.effects < 3 - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.effects < 3 - 1) {
                savefile->settings.effects++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.effects > 0) {
                savefile->settings.effects--;
                sfx(SFX_MENUMOVE);
            }
        }

        if (savefile->settings.effects == 1) {
            effectList.push_back(Effect(1, 4, 5));

        } else if (savefile->settings.effects == 2) {
            for (int i = 9; i < 11; i++)
                for (int j = 4; j < 6; j++)
                    current[i + 1][j + 1] = 1024;
        }
        setPalette();
    }

    std::string getCurrentOption() override {
        if (savefile->settings.effects)
            return std::to_string(savefile->settings.effects);
        else
            return "OFF";
    }
};

class ClearStyleElement : public Element {
public:
    std::string getLabel() override { return "Clear Style"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.clearEffect > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.clearEffect < MAX_CLEAR_EFFECTS - 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.clearEffect < MAX_CLEAR_EFFECTS - 1) {
                savefile->settings.clearEffect++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.clearEffect > 0) {
                savefile->settings.clearEffect--;
                sfx(SFX_MENUMOVE);
            }
        }
        setClearEffect();
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.clearEffect + 1);
    }
};

class ClearDirectionElement : public Element {
public:
    std::string getLabel() override { return "Clear Direction"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.clearDirection = !savefile->settings.clearDirection;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.clearDirection)
            return "OUT";
        else
            return "IN";
    }
};

class PlaceEffectElement : public Element {
public:
    std::string getLabel() override { return "Place Effect"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.placeEffect = !savefile->settings.placeEffect;
        if (savefile->settings.placeEffect && (int)placeEffectList.size() < 3) {
            placeEffectList.push_back(
                PlaceEffect(game->pawn.x, game->pawn.y - 20, 0, 0,
                            game->pawn.current, 0, false));
        }
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.placeEffect)
            return "ON";
        else
            return "OFF";
    }
};

class ScreenShakeElement : public Element {
public:
    std::string getLabel() override { return "Screen Shake"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.shakeAmount > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.shakeAmount < 4)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.shakeAmount < 4) {
                savefile->settings.shakeAmount++;
                savefile->settings.shake = true;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.shakeAmount > 0) {
                savefile->settings.shakeAmount--;
                sfx(SFX_MENUMOVE);
            }
        }

        if (savefile->settings.screenShakeType == 0) {
            shake = (shakeMax * (savefile->settings.shakeAmount) / 4);

        } else if (savefile->settings.screenShakeType == 1) {
            shakeVelocity =
                int2fx(((shakeMax * (savefile->settings.shakeAmount) / 4)) / 2);
        }
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.shakeAmount * 25) + "%";
    }
};

class ScreenShakeTypeElement : public Element {
public:
    std::string getLabel() override { return "Shake Type"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.screenShakeType =
            !savefile->settings.screenShakeType;

        if (savefile->settings.screenShakeType == 0) {
            shake = (shakeMax * (savefile->settings.shakeAmount) / 4);

        } else if (savefile->settings.screenShakeType == 1) {
            shakeVelocity =
                int2fx(((shakeMax * (savefile->settings.shakeAmount) / 4)) / 2);
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.screenShakeType == 0) {
            return "NORMAL";

        } else if (savefile->settings.screenShakeType == 1) {
            return "SMOOTH";
        }

        return "";
    }
};

class ClearTextElement : public Element {
public:
    std::string getLabel() override { return "Clear Text"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.clearText > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.clearText < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.clearText < 2) {
                savefile->settings.clearText++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.clearText > 0) {
                savefile->settings.clearText--;
                sfx(SFX_MENUMOVE);
            }
        }

        if (savefile->settings.clearText == 2) {
            floatingList.push_back(FloatText("quad"));
        } else {
            for (auto const& floating : floatingList) {
                int height;
                if (floating.timer < 2 * 100 / 3)
                    height = 5 * (float)floating.timer / ((float)2 * 100 / 3);
                else
                    height = (30 * (float)(floating.timer) / 100) - 15;
                aprintClearArea(9, 15 - height, 12, 1);
            }

            floatingList.clear();
        }

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.clearText) {
        case 1:
            return "OFFBOARD";
        case 2:
            return "ALL";
        default:
            return "OFF";
        }
    }
};

class AspectRatioElement : public Element {
public:
    std::string getLabel() override { return "Aspect Ratio"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
#ifdef N3DS
        n3ds::savedAspectRatio = !n3ds::savedAspectRatio;
#else
        savefile->settings.aspectRatio = !savefile->settings.aspectRatio;
#endif
        clearText();

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
#ifdef N3DS
        if (n3ds::savedAspectRatio == 0)
#else
        if (savefile->settings.aspectRatio == 0)
#endif
            return "3:2";
        else
            return "4:3";
    }
};

class ExploreElement : public Element {
public:
    std::string getLabel() override { return "Explore"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.journey = !savefile->settings.journey;
    }

    std::string getCurrentOption() override {
        if (savefile->settings.journey)
            return "ON";
        else
            return "OFF";
    }
};

class RandomGraphicsElement : public Element {
public:
    std::string getLabel() override { return "Randomize Options"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        setRandomGraphics(savefile);
        setSkin();
        setPalette();
        drawFrameBackgrounds();
        setClearEffect();

        sfx(SFX_MENUCONFIRM);

        return false;
    }

    std::string getCurrentOption() override { return "_"; }
};

class ResetGraphicsElement : public Element {
public:
    std::string getLabel() override { return "Reset Graphics"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        setDefaultGraphics(savefile, 0);
        setSkin();
        setPalette();
        drawFrameBackgrounds();
        setClearEffect();

        sfx(SFX_MENUCONFIRM);

        return false;
    }

    std::string getCurrentOption() override { return "_"; }
};

class GraphicsScene : public Scene {
    int maxDas = 12;
    int dasHor = 0;
    int dasVer = 0;

    int maxArr = 3;
    int arr = 0;

    bool moving = false;
    bool movingHor = false;
    int movingTimer = 0;
    int movingDirection = 0;

    int clearAnimationTimer = 0;
    int maxClearAnimationTimer = 20;

    bool showClearAnimation = false;

    std::list<Element*> getElementList() {
        std::list<Element*> l;

        l.push_back(new ProfileElement());
        l.push_back(new GradientElement());
        l.push_back(new GridElement());
        l.push_back(new SkinElement());
        l.push_back(new GhostElement());
        l.push_back(new FrameColorElement());
        l.push_back(new FrameBackgroundsElement());
        l.push_back(new PaletteElement());
        l.push_back(new BlockEdgesElement());
        l.push_back(new LightModeElement());
        l.push_back(new BoardEffectsElement());
        l.push_back(new ClearStyleElement());
        l.push_back(new ClearDirectionElement());
        l.push_back(new PlaceEffectElement());
        l.push_back(new ScreenShakeElement());
        l.push_back(new ScreenShakeTypeElement());
        l.push_back(new ClearTextElement());
        l.push_back(new AspectRatioElement());
        l.push_back(new ExploreElement());
        l.push_back(new RandomGraphicsElement());
        l.push_back(new ResetGraphicsElement());

        return l;
    };

    std::list<Element*> elementList;

    std::string name() { return "Graphics"; };

    int options = 0;
    int selection = 0;
    WordSprite* wordSprites[MAX_WORD_SPRITES];

    WordSprite* labels[7];

    WordSprite cursorText = WordSprite(0, 20, 2);

    OBJ_ATTR* cursorSprites[2];

    OBJ_ATTR* arrowSprites[2];

    int cursorFloat = 0;

    bool refreshText = true;
    bool refreshGame = false;
    bool refreshOption = false;

    bool showText = true;

    int listStart = 0;
    const int elementMax = 5;

    int startY = 0;

    int pathCount = 0;

    std::string p = "";

    std::string currentOption = "";
    std::string scrollText = "";

    int scrollTextLength = 0;

    int scrollTimer = 0;
    const int scrollTimerMax = 1;
    int scrollOffset = 0;
    int scrollDelay = 0;
    int scrollDelayMax = 60;

    int endDelay = 0;
    int endDelayMax = 80;
    WordSprite* scrollingText[3];

    int previousAspectRatio = 0;

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    void showClear();

    virtual Scene* previousScene() { return new SettingsScene(); };

    ~GraphicsScene() { deinit(); };
};

class GradientEditorScene : public Scene {
    int color1[3];

    int color2[3];

    HSV_COLOR hsv1;
    HSV_COLOR hsv2;

    int selection = 0;
    int colorSelection = 0;

    int maxSelection = 3;

    bool onValues = false;

    bool hsvMode = false;

    int das = 0;

    const int dasMax = 10;
    const int arrMax = 4;

    int c = 0;

    bool refreshText = false;

    OBJ_ATTR* cursorSprite;
    int cursorFloat = 0;

    OBJ_ATTR* colorSprites[6];

    WordSprite* wordSprites[MAX_WORD_SPRITES];

    void setMaxSelection() {
        maxSelection = 2;

        if (savefile->settings.backgroundType == 1)
            maxSelection++;
    }

    void set(int dir) {
        HSV_COLOR* hsv = (selection - 1) ? &hsv2 : &hsv1;

        bool res = false;
        switch (colorSelection) {
        case 0:
            hsv->hue += dir;
            if (hsv->hue < 0)
                hsv->hue = 359;
            else if (hsv->hue > 359)
                hsv->hue = 0;
            else
                res = true;
            break;
        case 1:
            hsv->saturation += ((float)dir) / 100.f;
            if (hsv->saturation < 0)
                hsv->saturation = 0;
            else if (hsv->saturation > 1)
                hsv->saturation = 1;
            else
                res = true;
            break;
        case 2:
            hsv->value += ((float)dir) / 100.f;
            if (hsv->value < 0)
                hsv->value = 0;
            else if (hsv->value > 1)
                hsv->value = 1;
            else
                res = true;
            break;
        }
    }

    void drawText();

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return new GraphicsScene(); };

    ~GradientEditorScene() { deinit(); }
};

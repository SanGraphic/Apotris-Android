#include "def.h"
#include "liba_window.h"
#include "logging.h"
#include "menu.h"
#include "scene.hpp"
#include "sceneAudio.hpp"
#include "sceneColorEditor.hpp"
#include "sceneControls.hpp"
#include "sceneGameplay.hpp"
#include "sceneGraphics.hpp"
#include "sceneHandling.hpp"
#include "sceneModes.hpp"
#include "sceneSaving.hpp"
#include "sceneStats.hpp"
#include "sceneWindow.hpp"
#include "sceneSleep.hpp"
#include "skinEditor.hpp"
#include <string>

#define FADE_LENGTH 4

void fadeToBlack();
void fadeFromBlack();

static COLOR* preTransitionPalette = nullptr;

void changeScene(Scene* newScene, Transitions transition) {
    gradient(0);
    vsync();
    setLightMode();

    switch (transition) {
    case Transitions::INSTANT:
        break;
    case Transitions::FADE:
        fadeToBlack();
        break;
    case Transitions::SCANLINE:
        break;
    default:
        break;
    }

    toggleRendering(false);
    if (preTransitionPalette != nullptr)
        loadPalette(0, 0, preTransitionPalette, 512);

    if (scene != nullptr)
        delete scene;

    scene = newScene;
    scene->init();

#ifdef ANDROID
    refreshWindowSize();
#endif

    vsync();
    toggleRendering(true);

    switch (transition) {
    case Transitions::INSTANT:
        break;
    case Transitions::FADE:
        fadeFromBlack();
        break;
    case Transitions::SCANLINE:
        break;
    default:
        break;
    }
}

void changeScene(Scene* newScene) {
    changeScene(newScene, Transitions::INSTANT);
}

void fadeToBlack() {

    if (preTransitionPalette != nullptr)
        delete[] preTransitionPalette;
    preTransitionPalette = new COLOR[512];
    savePalette(preTransitionPalette);

    int color = 0;

    if (savefile->settings.lightMode)
        color = 0x5ad6;

    int timer = 0;
    while (closed() && timer++ < FADE_LENGTH) {
        vsync();
        color_fade_palette(0, 1, &preTransitionPalette[1], color, 511,
                           timer * 8);
    }
}

void fadeFromBlack() {
    int timer = FADE_LENGTH;

    if (preTransitionPalette != nullptr)
        delete[] preTransitionPalette;
    preTransitionPalette = new COLOR[512];
    savePalette(preTransitionPalette);

    int color = 0;

    if (savefile->settings.lightMode)
        color = 0x5ad6;

    color_fade_palette(0, 1, &preTransitionPalette[1], color, 511, timer * 8);

    while (closed() && timer >= 0) {
        canDraw = 1;
        vsync();
        color_fade_palette(0, 1, &preTransitionPalette[1], color, 511,
                           timer * 8);
        timer--;
    }
}

void sceneSwitcher(std::string str) {
    if (str == "Play") {
        changeScene(new ModeListScene(), Transitions::FADE);
    } else if (str == "Settings") {
        setPreviousSettings(savefile->settings);
        menuKeys = savefile->settings.menuKeys;

        for (int i = 0; i < MAX_CUSTOM_SKINS; i++)
            previousSkins[i] = savefile->customSkins[i];

        changeScene(new SettingsScene(), Transitions::FADE);
    } else if (str == "Achievements") {

    } else if (str == "Stats") {
        changeScene(new StatScene(), Transitions::FADE);
#ifndef MULTIBOOT
    } else if (str == "Links") {
        changeScene(new LinksScene(), Transitions::FADE);
    } else if (str == "Skin Editor") {
        setPreviousSettings(savefile->settings);
        for (int i = 0; i < MAX_CUSTOM_SKINS; i++)
            previousSkins[i] = savefile->customSkins[i];

        menuKeys = savefile->settings.menuKeys;

        changeScene(new EditorScene(), Transitions::FADE);
    } else if (str == "Color Editor") {
        setPreviousSettings(savefile->settings);

        if (previousPalette != nullptr)
            delete[] previousPalette;
        previousPalette = new COLOR[3 * 7 * 3];

        memcpy16(previousPalette, savefile->customPalettes, 3 * 7 * 3);

        menuKeys = savefile->settings.menuKeys;

        changeScene(new ColorSelectorScene(), Transitions::FADE);
    } else if (str == "Credits") {
        changeScene(new CreditsScene(), Transitions::FADE);
#endif
    } else if (str == "Graphics") {
        changeScene(new GraphicsScene(), Transitions::FADE);
    } else if (str == "Audio") {
        changeScene(new AudioOptionScene(), Transitions::FADE);
    } else if (str == "Controls") {
        changeScene(new ControlOptionScene(), Transitions::FADE);
    } else if (str == "Handling") {
        changeScene(new HandlingOptionScene(), Transitions::FADE);
    } else if (str == "Gameplay") {
        changeScene(new GameplayOptionScene(), Transitions::FADE);
#ifndef MULTIBOOT
    } else if (str == "Saving") {
        changeScene(new SavingOptionScene(), Transitions::FADE);
    } else if (str == "Video") {
        changeScene(new WindowOptionScene(), Transitions::FADE);
    } else if (str == "Sleep") {
#ifdef GBA
        changeScene(new SleepOptionScene(), Transitions::FADE);
#endif
    } else if (str == "Rumble") {
        changeScene(new RumbleOptionScene(), Transitions::FADE);
    } else if (str == "Marathon") {
        changeScene(new MarathonScene(), Transitions::FADE);
    } else if (str == "Sprint") {
        changeScene(new SprintScene(), Transitions::FADE);
    } else if (str == "Dig") {
        changeScene(new DigScene(), Transitions::FADE);
    } else if (str == "Ultra") {
        changeScene(new UltraScene(), Transitions::FADE);
    } else if (str == "Blitz") {
        changeScene(new BlitzScene(), Transitions::FADE);
    } else if (str == "Combo") {
        changeScene(new ComboScene(), Transitions::FADE);
    } else if (str == "Survival") {
        changeScene(new SurvivalScene(), Transitions::FADE);
    } else if (str == "Classic") {
        changeScene(new ClassicScene(), Transitions::FADE);
    } else if (str == "Master") {
        changeScene(new MasterScene(), Transitions::FADE);
    } else if (str == "Death") {
        changeScene(new DeathScene(), Transitions::FADE);
    } else if (str == "Zen") {
        changeScene(new ZenScene(), Transitions::FADE);
#endif
    } else if (str == "2P Battle") {
#ifdef GBA
        if (emulatorPrompted) {
            changeScene(new MultBattleScene(), Transitions::FADE);
        } else {
            changeScene(new ConfirmEmuScene(), Transitions::FADE);
        }
#else
        changeScene(new MultBattleScene(), Transitions::FADE);
#endif
#ifndef MULTIBOOT
    } else if (str == "CPU Battle" || str == "Battle") {
        changeScene(new CPUBattleScene(), Transitions::FADE);
    } else if (str == "Training") {
        changeScene(new TrainingScene(), Transitions::FADE);
    } else if (str == "Website") {
        changeScene(new WebsiteLinkScene(), Transitions::FADE);
    } else if (str == "Wiki") {
        changeScene(new WikiLinkScene(), Transitions::FADE);
    } else if (str == "Donate") {
        changeScene(new DonateLinkScene(), Transitions::FADE);
    } else if (str == "Discord") {
        changeScene(new DiscordLinkScene(), Transitions::FADE);
#endif
    } else if (str == "Quit") {
        quit();
    } else {
        log("invalid option: " + str);
    }
}

std::string modeToString(BlockEngine::Modes mode) {
    switch (mode) {
    case BlockEngine::NO_MODE:
        return "None";
    case BlockEngine::MARATHON:
        return "Marathon";
    case BlockEngine::SPRINT:
        return "Sprint";
    case BlockEngine::DIG:
        return "Dig";
    case BlockEngine::BATTLE:
        return "Battle";
    case BlockEngine::ULTRA:
        return "Ultra";
    case BlockEngine::BLITZ:
        return "Blitz";
    case BlockEngine::COMBO:
        return "Combo";
    case BlockEngine::SURVIVAL:
        return "Survival";
    case BlockEngine::CLASSIC:
        return "Classic";
    case BlockEngine::MASTER:
        return "Master";
    case BlockEngine::ZEN:
        return "Zen";
    case BlockEngine::DEATH:
        return "Death";
    case BlockEngine::TRAINING:
        return "Training";
    default:
        return "";
    }
}

void WordSprite::setText(const std::string& _text) {
    if (_text == text)
        return;

    text = _text;

    if (text.empty()) {
        clearSpriteTiles(startTiles, (big) ? 20 : 12, 1);
        return;
    }

    width = getVariableWidth(text);

    clearSpriteTiles(startTiles, (big) ? 20 : 12, 1);

    naprintSprite(text, startTiles);
}

void WordSprite::setTextNum(int n) {
    char buff[64];
    posprintf(buff, "%d", n);

    std::string _text = buff;
    if (_text == text)
        return;

    text = _text;

    if (text.empty()) {
        clearSpriteTiles(startTiles, (big) ? 20 : 12, 0);
        return;
    }

    width = getVariableWidth(text);

    clearSpriteTiles(startTiles, (big) ? 20 : 12, 1);

    naprintSprite(text, startTiles);
}

void WordSprite::setup(int _id, int _index, int _tiles, bool _big) {
    id = _id;
    startIndex = _index;
    startTiles = _tiles;
    big = _big;

    for (int i = 0; i < 3 * (1 + big) - big; i++) {
        sprites[i] = &obj_buffer[startIndex + i];
    }
}

void setPreviousSettings(Settings settings) {
    if (previousSettings == nullptr)
        previousSettings = std::make_unique<Settings>();

    *previousSettings = settings;
}

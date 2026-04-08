#pragma once

#include "blockEngine.hpp"
#include "def.h"
#include "multiplayerClasses.h"
#ifdef GBA
#include "LinkUniversal.hpp"
#endif
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <tuple>

extern int previousSelection;

extern void sceneSwitcher(std::string str);
extern const std::list<std::string> menuOptions;
extern const std::list<std::string> gameOptions;
extern const std::list<std::string> settingsOptions;
extern const std::list<std::string> controlOptions;
extern const std::list<std::string> menuControlOptions;

static const int READY = 0xC0DE;

static const int START = 0xBEEF;

extern std::string getDescription(std::string element);
extern std::string getDescription(std::string mode, std::string element,
                                  std::string option);

class Scene {
public:
    virtual void draw() = 0;
    virtual void update() = 0;
    virtual bool control() = 0;
    virtual void init() = 0;
    virtual void deinit() = 0;
    virtual Scene* previousScene() = 0;

    Scene() {};

    virtual ~Scene() {};
};

extern Scene* scene;

class GameScene : public Scene {
    void draw();
    void update();
    bool control();
    void init();
    void deinit();

    WordSprite wordSprites[MAX_WORD_SPRITES];
    WordSprite creditSprites[3];

    void countdown();
    void showText();
    void updateText();
    void showTimer();
    void showComboStreak();
    int pauseMenu();
    void endScreen();
    void showModeText();
    void showStats(bool, std::string, std::string, bool);
    void setupCredits();
    void showCredits();
    void refreshCredits();
    void checkSounds();
    bool reconnect();
    int handleMultiplayer(bool duringGame);
    int endScreenSetup();
    void showReadyPlayers();

    bool counted = false;
    bool preventLayeredCountdowns = false; // TODO I feel like this can be done better
    bool skipSong = false;

    int timesPaused = 0;

    Scene* previousScene() { return NULL; };

    ~GameScene() { deinit(); };

    static void ProcessMultiplayerPacket(u16 data,
                                         RemotePlayerId remotePlayerId);

    static void ProcessGameUpdate(int command, int value,
                                  RemotePlayerId remotePlayerId);

    static void UpdateEnemyBoard(int command, int value,
                                 RemotePlayerId remotePlayerId);

    static void updateRank();
};

class TitleScene : public Scene {
    WordSprite wordSprites[MAX_WORD_SPRITES];

    WordSprite versionText = WordSprite(0, 20, 2);
    WordSprite nameText = WordSprite(0, 23, 2 + 12);

    OBJ_ATTR* cursorSprites[2];

    int flashTimer = 0;
    const int flashMax = 48;

    int demoTimer = 0;
    const int demoMax = 60 * 10;

    bool testDraw = false;

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return NULL; };

    ~TitleScene() { deinit(); };
};

class SimpleListScene : public Scene {
public:
    int maxDas = 12;
    int dasHor = 0;
    int dasVer = 0;

    int maxArr = 3;
    int arr = 0;

    bool moving = false;
    bool movingHor = false;
    int movingTimer = 0;
    int movingDirection = 0;

    virtual std::list<std::string> getOptionList() { return menuOptions; };
    std::list<std::string> optionList;

    virtual std::string name() { return ""; };

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

    int options = 0;
    int selection = 0;
    WordSprite* wordSprites[MAX_WORD_SPRITES];

    OBJ_ATTR* cursorSprites[2];

    OBJ_ATTR* arrowSprites[2];

    OBJ_ATTR* scrollSideSprites[2];

    int cursorFloat = 0;

    int listStart = 0;
    const int elementMax = 4;

    int startY = 0;

    bool refreshText = true;

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return NULL; };

    virtual std::string getDescription() {
        return ::getDescription(currentOption);
    };

    ~SimpleListScene() { deinit(); };
};

class MainMenuScene : public SimpleListScene {
public:
    void init();
    void deinit();

    Scene* previousScene();
    std::list<std::string> getOptionList() {
        std::list<std::string> result = menuOptions;

#if !(defined(GBA) || defined(WEB))

        result.push_back("Quit");

#endif

        return result;
    };
    std::string name() { return ""; };
};

class ModeListScene : public SimpleListScene {
public:
    Scene* previousScene();

    std::list<std::string> getOptionList() {
        std::list<std::string> l = gameOptions;

#if defined(PC) || defined(WEB) || defined(PORTMASTER) || defined(SWITCH) ||   \
    defined(N3DS)
        l.remove("2P Battle");
#endif

        return l;
    };
    std::string name() { return "Play"; };
};

class Element {
public:
    int value = 0;

    virtual std::string getLabel() { return ""; };
    virtual std::string getCursor(std::string text) { return ""; };
    virtual void action(int dir) {};
    virtual bool action() { return false; };
    virtual int getValue() { return 0; };
    virtual std::string getCurrentOption() { return ""; };
    virtual bool isSelectable() { return true; };

    Element() {}

    virtual ~Element() {}
};

class OptionListScene : public Scene {
public:
    int maxDas = 12;
    int dasHor = 0;
    int dasVer = 0;

    int maxArr = 3;
    int arr = 0;

    bool moving = false;
    bool movingHor = false;
    int movingTimer = 0;
    int movingDirection = 0;

    virtual std::list<Element*> getElementList() = 0;
    std::list<Element*> elementList;

    virtual std::string name() = 0;

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

    int options = 0;
    int selection = 0;
    WordSprite* wordSprites[MAX_WORD_SPRITES];

    WordSprite* labels[7];

    WordSprite cursorText = WordSprite(0, 20, 2);

    OBJ_ATTR* cursorSprites[2];

    OBJ_ATTR* arrowSprites[2];

    OBJ_ATTR* scrollSideSprites[2];

    int cursorFloat = 0;

    bool refreshText = true;
    bool refreshOption = true;

    int listStart = 0;
    const int elementMax = 5;

    int startY = 0;

    void showPath();

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    virtual Scene* previousScene() { return NULL; };

    virtual std::string getDescription() {
        return ::getDescription(currentOption);
    };

    ~OptionListScene() { deinit(); };
};

class ConfirmSaveScene : public Scene {
public:
    int timer = 0;

    bool cancel = true;

    int cursorFloat = 0;

    WordSprite wordSprites[2];

    OBJ_ATTR* cursorSprites[2];

    int lengths[2];

    int pos[2];

    int type = 0; // 0 for settings, 1 for skin editor, 2 for color editor

    void draw();
    void update();
    bool control();
    void init();
    void deinit();

    ConfirmSaveScene(int type) { this->type = type; }

    Scene* previousScene() { return new MainMenuScene(); };

    ~ConfirmSaveScene() { deinit(); };
};

class ConfirmEmuScene : public Scene {
public:
    int timer = 0;

    bool cancel = true;

    int cursorFloat = 0;

    WordSprite wordSprites[2];

    OBJ_ATTR* cursorSprites[2];

    int lengths[2];

    int pos[2];

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return new MainMenuScene(); };
};

class SettingsScene : public SimpleListScene {
public:
    Scene* previousScene() {
        if (settingsChanged())
            return new ConfirmSaveScene(0);
        else
            return new MainMenuScene();
    };

    std::list<std::string> getOptionList() {
        std::list<std::string> result = settingsOptions;

#ifndef GBA
        result.push_front("Video");
#else
        result.push_back("Sleep");
#endif

        return result;
    };

    std::string name() { return "Settings"; };
};

#ifndef MULTIBOOT

class CreditsScene : public Scene {
public:
    virtual std::list<std::string> getOptionList() { return menuOptions; };
    std::list<std::string> optionList;

    std::string name() { return "Credits"; };

    WordSprite* wordSprites[MAX_WORD_SPRITES];

    int listStart = 0;
    int maxShow = 12;

    int delay = 0;
    int delayMax = 60;

    int scrollTimer = 0;
    int scrollTimerMax = 4;
    int scrollOffset = 0;
    int space = 12;

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return new MainMenuScene(); };

    ~CreditsScene() { deinit(); };
};

class LinksScene : public SimpleListScene {

public:
    // void init();
    Scene* previousScene() { return new MainMenuScene(); };

    std::list<std::string> getOptionList() {
        return {
            "Website",
            "Wiki",
            "Donate",
            "Discord",
        };
    };

    std::string name() { return "Links"; };
};

class QRScene : public Scene {
public:
    virtual u8* getData() = 0;

    virtual std::string name() = 0;

    virtual std::string getLink() = 0;

    OBJ_ATTR* qr;

    int qrX = 0;
    int qrY = 0;

    WordSprite* wordSprites[MAX_WORD_SPRITES];

    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return new LinksScene(); };

    ~QRScene() { deinit(); };
};

#include "site_qr_bin.h"

class WebsiteLinkScene : public QRScene {
public:
    std::string name() override { return "Website"; }

    u8* getData() override { return (u8*)site_qr_bin; }

    std::string getLink() override { return "https://apotris.com"; }

    WebsiteLinkScene() {
        qrX = 29;
        qrY = 29;
    }
};

#include "wiki_qr_bin.h"

class WikiLinkScene : public QRScene {
public:
    std::string name() override { return "Wiki"; }

    u8* getData() override { return (u8*)wiki_qr_bin; }

    std::string getLink() override { return "https://apotris.com/wiki"; }

    WikiLinkScene() {
        qrX = 29;
        qrY = 29;
    }
};

#include "paypal_qr_bin.h"

class DonateLinkScene : public QRScene {
public:
    std::string name() override { return "Donate"; }

    u8* getData() override { return (u8*)paypal_qr_bin; }

    std::string getLink() override { return "https://apotris.com/donate"; }

    DonateLinkScene() {
        qrX = 29;
        qrY = 29;
    }
};

#include "discord_qr_bin.h"

class DiscordLinkScene : public QRScene {
public:
    std::string name() override { return "Discord"; }

    u8* getData() override { return (u8*)discord_qr_bin; }

    std::string getLink() override { return "https://apotris.com/discord"; }

    DiscordLinkScene() {
        qrX = 33;
        qrY = 33;
    }
};

#endif

class LabelElement : public Element {
    std::string label;

public:
    std::string getLabel() override { return label; }

    std::string getCurrentOption() override { return "\n"; }

    bool isSelectable() override { return false; }

    LabelElement(std::string l) { label = l; }
};

enum class Transitions { INSTANT, FADE, SCANLINE };

extern void changeScene(Scene* newScene);
extern void changeScene(Scene* newScene, Transitions transition);

extern int rank;

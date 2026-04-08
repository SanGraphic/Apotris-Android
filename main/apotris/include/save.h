#pragma once
#include "blockEngine.hpp"
#include "platform.hpp"

typedef struct Highscore {
    char name[9];
    int score;
} ALIGN(4) Highscore;

typedef struct Scoreboard {
    Highscore highscores[5];
} ALIGN(4) Scoreboard;

typedef struct Time {
    char name[9];
    int frames;
} ALIGN(4) Time;

typedef struct Timeboard {
    Time times[5];
} ALIGN(4) Timeboard;

typedef struct Gradeboard {
    Time times[5];
    s8 grade[5];
} ALIGN(4) Gradeboard;

typedef struct GameKeys {
    int moveLeft;
    int moveRight;
    int rotateCCW;
    int rotateCW;
    int rotate180;
    int softDrop;
    int hardDrop;
    int hold;
    int zone;

    int placeHolder[19];
} ALIGN(4) GameKeys;

typedef struct MenuKeys {
    int up;
    int down;
    int left;
    int right;
    int confirm;
    int cancel;
    int pause;
    int reset;
    int special1;
    int special2;
    int special3;

    int placeHolder[9];
} ALIGN(4) MenuKeys;

typedef struct QuickStart {
    int mode;
    int submode;
    int goal;
    int level;
    int big;
} ALIGN(4) QuickStart;

typedef struct GraphicsOptions {
    s8 clearText;
    bool shake;
    bool effects;
    bool edges;
    s8 backgroundGrid;
    s8 skin;
    s8 palette;
    s8 shadow;
    bool lightMode;
    s8 colors;
    s8 clearEffect;
    s8 shakeAmount;
    u32 backgroundGradient;
    bool placeEffect;
    s8 frameBackground;
    s8 backgroundType;
    s8 aspectRatio;
    s8 screenShakeType;
    s8 clearDirection;

    int placeHolder[6];
} ALIGN(4) GraphicsOptions;

typedef struct SongDisabling {
    u8 menu;
    u8 game;

    int menuBits[4];
    int gameBits[4];
} ALIGN(4) SongDisabling;

typedef struct GBASleepOptions {
    u32 autosleepDelay;
    u32 autosleepWakeCombo;
    u32 sleepWakeCombo;
} ALIGN(4) GBASleepOptions;

typedef struct Settings {
    bool announcer;
    bool finesse;
    bool floatText;
    bool shake;
    int effects;
    int volume;
    int das;
    int arr;
    int sfr;
    bool dropProtection;
    int backgroundGrid;
    bool edges;
    int skin;
    int palette;
    int shadow;
    bool lightMode;
    bool songList[10]; // deprecated
    int sfxVolume;
    bool directionalDas;
    int shakeAmount;
    bool noDiagonals;
    int maxQueue;
    int colors;
    int cycleSongs;
    int dropProtectionFrames;
    bool abHold;

    GameKeys keys;

    int clearEffect;
    bool resetHoldType;
    bool placeEffect;
    bool resetHoldToggle;
    int rumbleStrength;
    int diagonalType;
    bool delaySoftDrop;
    int backgroundGradient;
    bool customDas;
    int ihs;
    int irs;
    int initialType;
    int frameBackground;
    int backgroundType;
    int aspectRatio;
    int screenShakeType;
    int clearDirection;
    int big;
    int pro;
    int goalLine;
    int rotationSystem;
    int zoom;
#ifdef N3DS
    union {
        struct {
            u8 n3dsMainScreenIsTop;
            n3ds::ScaleMode n3dsScaleMode;
            u8 _n3dsPlaceholder1;
            u8 _n3dsPlaceholder2;
        };
        int _n3dsDisplayOptionValue;
    };
#else
    int integerScale;
#endif
    int peek;
    int moveSfx;
    bool pauseCountdown;
    bool interruptCountdown;
    u8   rumbleUpdateRate;
    u8   rumbleStyle;
    int clearText;
    int randomizer;

    MenuKeys menuKeys;

    GraphicsOptions profiles[5];
    int selectedProfile;

    bool journey;
    int autosave;

    SongDisabling songDisabling;
    bool showSpawn;
    bool showFPS;
    int bootlegOverrideType;
#ifdef N3DS
    int _n3dsPlaceholder3;
#else
    int fullscreen;
#endif
    int shaders;

    GBASleepOptions sleep;
    bool gameBoyPlayerSupport;
    u8 rumbleCartType;
    u8 ezFlash3in1Strength;
    u8 zoneRumbleMode;

    int placeHolder[25];

} ALIGN(4) Settings;

typedef struct Test {
    bool t1[6];
    int t2[4];
} ALIGN(4) Test;

typedef struct Test2 {
    bool t1[18];
    int t2[10];
} ALIGN(4) Test2;

typedef struct Test3 {
    bool t1[14]; // 14
    int t2[30];  // 22
} ALIGN(4) Test3;

typedef struct TotalStats {
    int timePlayed;
    int gamesStarted;
    int gamesCompleted;
    int gamesLost;
    BlockEngine::Stats gameStats;
    int maxLevel;
    int totalLines;
} ALIGN(4) TotalStats;

typedef struct Skin {
    TILE board;
    TILE smallBoard;
    bool changed;
} ALIGN(4) Skin;

#define MAX_CUSTOM_SKINS 5

#define MAX_CUSTOM_PALETTES 3

typedef struct SaveEntry {
    char name[9];
    bool pro;
    s16 grade;
    int value;
} ALIGN(4) SaveEntry;

typedef struct EntryBoard {
    SaveEntry entries[5];
} ALIGN(4) EntryBoard;

typedef struct OldScoreboards {
    Scoreboard marathon[4];
    Timeboard sprint[3];
    Timeboard dig[3];
    Scoreboard ultra[3];
    Scoreboard blitz[2];
    Scoreboard combo;
    Timeboard survival[3];
    Timeboard sprintAttack[3];
    Scoreboard digEfficiency[3];
    Scoreboard classic[2];
    Gradeboard master[2];
    Scoreboard zone[4];
} ALIGN(4) OldScoreboards;

typedef struct ModeBoards {
    EntryBoard marathon[4];
    EntryBoard sprint[3];
    EntryBoard dig[3];
    EntryBoard ultra[3];
    EntryBoard blitz[2];
    EntryBoard combo;
    EntryBoard survival[3];
    EntryBoard sprintAttack[3];
    EntryBoard digEfficiency[3];
    EntryBoard classic[2];
    EntryBoard master[2];
    EntryBoard zone[4];
    u32 zen;
    EntryBoard death[2];
    EntryBoard tower[4];
    u32 zenTower;
    u32 zenLines;
} ALIGN(4) ModeBoards;

class Save {
public:
    u8 newGame;

    Settings settings;
    int seed;
    char latestName[9];

    ModeBoards boards;

    int placeHolder[881];

    TotalStats stats;

    int placeHolder2[87];

    Skin customSkins[MAX_CUSTOM_SKINS];

    u8 platform;

    COLOR customPalettes[MAX_CUSTOM_PALETTES][7][3];

    char endTag[9] = "SAVE_END";
} ALIGN(4);

typedef struct ReplayHeader {
    u8 tag;
    u32 length;
    u32 duration;
    u32 seed;

    BlockEngine::Options options;

    int placeholder[240];
} ALIGN(4) ReplayHeader;

typedef struct Replay {
    ReplayHeader header;
    uint32_t moves[4096];
} ALIGN(4) Replay;

extern Save* savefile;

extern void saveToProfile(GraphicsOptions* profile, Settings* settings);
extern void profileToSave(GraphicsOptions* profile, Settings* settings);
extern void autosave();

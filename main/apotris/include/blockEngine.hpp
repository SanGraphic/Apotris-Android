#pragma once

#include "platform.hpp"
#include "prng.h"
#include "tetromino.hpp"
#include <list>
#include <string>
#include <tuple>
#include <vector>

class List {
public:
    const static int maxSize = 10;
    int array[maxSize];
    int length = 0;

    void push_back(int n) {
        if (length == maxSize)
            return;

        array[length++] = n;
    }

    void push_front(int n) {
        if (length == maxSize)
            return;

        for (int i = length; i > 0; i--) {
            array[i] = array[i - 1];
        }

        array[0] = n;

        length++;
    }

    void pop_front() {
        if (length == 0)
            return;

        for (int i = 0; i < length - 1; i++) {
            array[i] = array[i + 1];
        }

        length--;
    }

    void pop_back() {
        if (length == 0)
            return;

        length--;
    }

    int front() {
        if (length == 0)
            return 0;

        return array[0];
    }

    int back() {
        if (length == 0)
            return 0;

        return array[length - 1];
    }

    bool empty() { return (length == 0); }

    void clear() { length = 0; }

    int size() { return length; }

    void erase(int index) {
        if (length == 0)
            return;

        for (int i = index; i < length - 1; i++) {
            array[i] = array[i + 1];
        }

        length--;
    }

    int find(int n) {
        for (int i = 0; i < length; i++) {
            if (array[i] == n)
                return i;
        }

        return -1;
    }

    List() { memset32(array, 0, maxSize); }
};

namespace BlockEngine {
#define MAX_DISAPPEAR 300
#define CREDITS_LENGTH 3240 // 54 seconds * 60
#define maxSleep 1;

#define MAX_MOVELIST_SIZE 5

#define SEARCH_DEPTH 1

extern const uint16_t connectedConversion[24];
extern const uint16_t connectedFix[3][24];
extern int* getShape(int piece, int rotation, int rotationSystem);

enum Modes {
    NO_MODE,
    MARATHON,
    SPRINT,
    DIG,
    BATTLE,
    ULTRA,
    BLITZ,
    COMBO,
    SURVIVAL,
    CLASSIC,
    MASTER,
    TRAINING,
    ZEN,
    DEATH,
};

enum RotationSystems {
    SRS,
    NRS,
    ARS,
    BARS,
    SDRS,
    A_SRS,
};

enum Pieces {
    PIECE_I,
    PIECE_J,
    PIECE_L,
    PIECE_O,
    PIECE_S,
    PIECE_T,
    PIECE_Z,
};

enum Randomizers {
    RANDOM,
    BAG_7,
    BAG_35,
};

extern bool enableFumen;
extern bool enableFumenQueue;

extern u8 fumenQueue[5];

extern std::string fumenString;

class Stats {
public:
    int clears[4] = {0, 0, 0, 0};
    int tspins = 0;
    int perfectClears = 0;
    int maxStreak = 0;
    int maxCombo = 0;
    int holds = 0;
    int maxZonedLines = 0;
    int secretGrade = 0;

    Stats() {}

    Stats(const Stats& old) {
        for (int i = 0; i < 4; i++)
            clears[i] = old.clears[i];

        tspins = old.tspins;
        perfectClears = old.perfectClears;
        maxStreak = old.maxStreak;
        maxCombo = old.maxCombo;
        holds = old.holds;
        maxZonedLines = old.maxZonedLines;
        secretGrade = old.secretGrade;
    }

    void add(const Stats& old) {
        for (int i = 0; i < 4; i++)
            clears[i] += old.clears[i];

        tspins += old.tspins;
        perfectClears += old.perfectClears;
        holds += old.holds;

        if (old.maxStreak > maxStreak)
            maxStreak = old.maxStreak;

        if (old.maxCombo > maxCombo)
            maxCombo = old.maxCombo;

        if (old.maxZonedLines > maxZonedLines)
            maxZonedLines = old.maxZonedLines;

        if (old.secretGrade > secretGrade)
            secretGrade = old.secretGrade;
    }
};

class Color {
public:
    int r;
    int g;
    int b;
};

class Drop {
public:
    int on = false;
    int startX = 0;
    int endX = 0;
    int startY = 0;
    int endY = 0;
    int rawEndY = 0;

    int x;
    int y;

    int dx;
    int dy;

    int piece;
    int rotation;
    int rotating;

    Drop() {}
    Drop(const Drop& oldDrop) {
        on = oldDrop.on;
        startX = oldDrop.startX;
        endX = oldDrop.endX;
        startY = oldDrop.startY;
        endY = oldDrop.endY;
        rawEndY = oldDrop.rawEndY;
        x = oldDrop.x;
        y = oldDrop.y;
        dx = oldDrop.dx;
        dy = oldDrop.dy;
        piece = oldDrop.piece;
        rotation = oldDrop.rotation;
        rotating = oldDrop.rotating;
    }
};

class Move {
public:
    int dx = 0;
    int rotation = 0;
    int height = 40;
    int holes = 0;
    int piece = -1;
    int clears = 0;
    // int wells = 0;
    int score = -0x7fffffff;

    Move() {}

    Move(const Move& oldMove) {
        dx = oldMove.dx;
        rotation = oldMove.rotation;
        height = oldMove.height;
        holes = oldMove.holes;
        piece = oldMove.piece;
        clears = oldMove.clears;
        score = oldMove.score;
    }
};

class Garbage {
public:
    int id;
    int amount;
    int timer = 60;

    Garbage(int _id, int _amount) {
        id = _id;
        amount = _amount;
    }
};

class Score {
public:
    int linesCleared = 0;
    int score = 0;
    int combo = 0;
    int isTSpin = 0;
    int isPerfectClear = 0;
    int isBackToBack = 0;
    int isDifficult = 0;
    Drop drop;

    Score() {}
    Score(int lC, int sc, int co, int isTS, int isPC, int isBTB, int isDif,
          Drop drp) {
        linesCleared = lC;
        score = sc;
        combo = co;
        isTSpin = isTS;
        isPerfectClear = isPC;
        isBackToBack = isBTB;
        isDifficult = isDif;
        drop = drp;
    }

    Score(const Score& oldScore) {
        linesCleared = oldScore.linesCleared;
        score = oldScore.score;
        combo = oldScore.combo;
        isTSpin = oldScore.isTSpin;
        isPerfectClear = oldScore.isPerfectClear;
        isBackToBack = oldScore.isBackToBack;
        isDifficult = oldScore.isDifficult;
        drop = Drop(oldScore.drop);
    }
};

class SoundFlags {
public:
    int shift = 0;
    int shiftDas = 0;
    int rotate = 0;
    int place = 0;
    int invalid = 0;
    int hold = 0;
    int clear = 0;
    int levelUp = 0;
    int finesse = 0;
    int section = 0;
    int disappear = 0;
    int zone = 0;
    int meter = 0;
    int initial = 0;
    int tspin = 0;
    int bone = 0;
    int towerScroll = 0;

    SoundFlags() {}
    SoundFlags(const SoundFlags& oldFlags) {
        shift = oldFlags.shift;
        shiftDas = oldFlags.shiftDas;
        rotate = oldFlags.rotate;
        place = oldFlags.place;
        invalid = oldFlags.invalid;
        hold = oldFlags.hold;
        clear = oldFlags.clear;
        levelUp = oldFlags.levelUp;
        finesse = oldFlags.finesse;
        section = oldFlags.section;
        disappear = oldFlags.disappear;
        zone = oldFlags.zone;
        meter = oldFlags.meter;
        initial = oldFlags.initial;
        tspin = oldFlags.tspin;
        bone = oldFlags.bone;
        towerScroll = oldFlags.towerScroll;
    }
};

class Pawn {
public:
    int x;
    int y;
    int type;
    int current = -1;
    int rotation = 0;
    int board[4][4][4];
    int boardLowest[4][4];
    int heighest[4];
    int lowestBlock[4];
    int lowest;
    bool big = false;
    void setBlock(int system);

    Pawn(int newX, int newY) {
        for (int i = 0; i < 4; i++) {
            heighest[i] = -1;
            for (int j = 0; j < 4; j++)
                boardLowest[i][j] = -1;
        }

        x = newX;
        y = newY;
    }

    Pawn(const Pawn& oldPawn) {
        x = oldPawn.x;
        y = oldPawn.y;

        type = oldPawn.type;
        current = oldPawn.current;
        rotation = oldPawn.rotation;
        lowest = oldPawn.lowest;
        big = oldPawn.big;

        memcpy32(board, oldPawn.board, 4 * 4 * 4);
        memcpy32(boardLowest, oldPawn.boardLowest, 4 * 4);
        memcpy32(lowestBlock, oldPawn.lowestBlock, 4);
        memcpy32(heighest, oldPawn.heighest, 4);
    }
};

class Tuning {
public:
    int das = 8;
    int arr = 2;
    int sfr = 2;
    int dropProtection = 8;
    bool directionalDas = false;
    bool delaySoftDrop = true;
    bool ihs = false;
    bool irs = false;
    bool initialType = false;
};

class Options {
public:
    Modes mode = NO_MODE;
    int goal = 0;
    int level = 0;
    Tuning tuning;

    bool trainingMode = false;
    bool bigMode = false;

    int bTypeHeight = 0;
    int subMode = 0;
    int rotationSystem = SRS;
    int randomizer = BAG_7;
};

class Timestamp {
public:
    int timer;
    uint16_t move;
    bool dir;
    bool shiftTimer = false;

    Timestamp(int _timer, int _move, int _dir) {
        timer = _timer;
        move = _move;
        dir = _dir;
    }

    Timestamp(bool shift) { shiftTimer = shift; }

    Timestamp(uint32_t m) {
        timer = (m >> 11) & 0x3fffff;
        dir = (m >> 9) & 1;
        move = m & 0x1ff;
    }
} ALIGN(4);

class Game {
private:
    void fillBag();
    void fillQueue(int);
    void fillQueueSeed(int, int);
    bool moveLeft();
    bool moveRight();
    void moveDown();
    int clear(const Drop&);
    void lockCheck();
    void place();
    void generateGarbage(int, int);
    Drop calculateDrop();
    void setMasterTuning();
    void setDeathTuning();
    void rotate(int, int);
    int checkSpecialRotation(int, int);
    int checkITouching(int, int);
    void rotatePlace(int, int, int, int);
    void updateDisappear();
    void endZone();
    void clearBoard();
    void fixConnected(const std::list<int>&);
    void connectBoard(int startY, int endY);
    void updateColumns(int height);
    void setupPawnSpawn();
    void setupRNG();
    void checkSecretGrade();
    int getNbr(int x, int y, int ex, int sy, int ey);
    void setShiftSoundFlag();
    void calculatePeek();
    void deathGarbage();
    void toggleBigMode();
    void scrollTower();
    int checkIntegrity();

    int bigBag[35];

    float speedCounter = 0;

    // 7  117
    // 8  133
    // 9  150
    // 11 183
    // 16 267

    int maxDas = 8;
    int das = 0;
    int arr = 2;
    int arrCounter = 0;

    int softDropCounter = 0;
    int maxSoftDrop = 10;
    int softDropSpeed = 2;
    int softDropRepeatTimer = 0;

    int lockMoveCounter = 15;

    int left = 0;
    int right = 0;
    int down = 0;

    int lastMoveRotation = 0;
    int lastMoveDx = 0;
    int lastMoveDy = 0;

    int finesseCounter = 0;

    Drop lastDrop;

    bool dropProtection = true;
    bool directionCancel = true;

    int dropLockTimer = 0;
    int dropLockMax = 8;

    bool specialTspin = false;

    int pieceHistory = -1;

    int gracePeriod = 0;

    int section = 0;
    int sectionStart = 0;
    int previousSectionTime = 1000000;

    int internalGrade = 0;
    int gradePoints = 0;

    bool cool = false;
    bool regret = false;
    int decayTimer = 0;

    bool stopLockReset = false;
    bool fromLockHold = false;
    bool fromLockRotate = false;

    int pieceDrought[7];
    bool delaySoftDrop = true;

    bool ihs = false;
    bool irs = false;
    bool initialType = false;

    bool rotates[3] = {false, false, false};

    bool zoneExit = false;

    bool sonicDrop = false;

    List bag;
    List historyList;

public:
    int checkRotation(int, int, int);
    void next();
    void rotateCW(int dir);
    void rotateCCW(int dir);
    void rotateTwice(int dir);
    void hardDrop();
    void update();
    int lowest();
    static Color color(int);
    void hold(int dir);
    void keyLeft(int);
    void keyRight(int);
    void keyDown(int);
    void keyDrop(int);
    void removeClearLock();
    void resetSounds();
    void resetRefresh();
    void setLevel(int);
    void setGoal(int);
    Drop getDrop();
    void setTuning(Tuning);
    void clearAttack(int);
    void setWin();
    void addToGarbageQueue(int, int);
    static std::list<int> getBestFinesse(int, int, int);
    int getIncomingGarbage();
    Move findBestDrop();
    void setTrainingMode(bool);
    void demoClear();
    void demoFill();
    void bType(int);
    void setSubMode(int);
    void setSpeed();
    void setRotationSystem(int);
    void setRandomizer(int);
    void removeEventLock();
    void activateZone(int);
    void liftKeys();
    void setOptions(Options newOptions);
    void updateDas();
    void chargeDas();
    void loadFumen();
    std::string getFumen();
    void demoGarbage();
    u32 random();
    int setRandomSeed(int newSeed);
    std::tuple<int, int> getSpawnPosition();

    const int randomMax = ((1 << 15) - 1);
    int randomSeed = randNext2();

    int lengthX = 10;
    int lengthY = 40;
    int board[40][10];

    uint16_t bitboard[40];
    int columnHeights[10];

    Pawn pawn = Pawn(0, 0);
    int held = -1;
    float speed;
    int linesCleared = 0;
    int level = 0;
    unsigned int score = 0;
    int comboCounter = -1;
    bool lineClearArray[40];
    bool clearLock = false;
    int lost = 0;
    int gameMode;
    SoundFlags sounds;
    Score previousClear = Score(0, 0, 0, 0, 0, 0, 0, Drop());
    int timer = 0;
    int refresh = 0;
    int won = 0;
    int goal = 40;
    int finesseFaults = 0;
    int garbageCleared = 0;
    int garbageHeight = 0;
    int pushDir = 0;
    int b2bCounter = 0;
    int bagCounter = 0;
    int linesSent = 0;
    int pieceCounter = 0;
    int previousKey = 0;
    bool softDrop = false;
    bool canHold = true;
    bool holding = false;
    bool trainingMode = false;
    int seed = 0;
    int initSeed = 0;
    int initialLevel = 0;
    int eventTimer = 0;

    bool dropping = false;
    int entryDelay = 0;
    int areMax = 0;
    int lineAre = 0;
    int maxClearDelay = 1;

    int bTypeHeight = 0;

    Stats statTracker;

    int subMode = 0;
    int zoneCharge = 0;
    int zoneTimer = 0;
    int zonedLines = 0;
    int zoneScore = 0;
    int zoneStart = 0;
    bool fullZone = false;
    bool inversion = false;

    int grade = 0;
    int coolCount = 0;
    int regretCount = 0;

    int rotationSystem = SRS;

    int randomizer = BAG_7;

    int maxLockTimer = 30;
    int lockTimer = maxLockTimer;

    uint16_t disappearTimers[40][10];
    int disappearing = 0;

    float creditGrade = 0;

    bool eventLock = false;

    int finesseStreak = 0;

    int inGameTimer = 0;

    int stackHeight = lengthY;

    int replayTimerSections = 0;

    int boardOffset = lengthY / 2;

    int activePiece = -1;

    bool toEndZone = false;

    int verticalKick = 0;
    int verticalKickMax = 8;

    bool replayElligible = true;

    int fullLine = 0x3ff;

    int peek = 0;

    int deathGarbageCounter = 0;
    int towerScrollCount = 0;
    bool towerClearingPhase = false;
    bool towerMediumClearing = false;
    int towerMediumTimer = 0;
    int towerMediumDelay = 0;

    List queue;
    List moveHistory;                // max 10
    List previousBest;               // max 10? but probably even less
    std::list<int> linesToClear;     // max 40
    std::list<Garbage> attackQueue;  // no max but should be small
    std::list<Garbage> garbageQueue; // no max but should be small
    std::list<std::tuple<uint8_t, uint8_t>>
        toDisappear; // no max, depends on pps but gets cleared every frame
    std::list<Timestamp> replay; // max is MAX_REPLAY_SIZE

    Game() : Game(0, 0, 0) {}

    Game(int gm, int sd, bool big);

    Game(int gm, bool big) : Game(gm, (int)randNext(), big) {}

    Game(Game* oldGame);

    ~Game();
};

typedef struct Weights {
    int clears = 0;
    int holes = 0;
    int height = 0;
    int lowest = 0;
    int jag = 0;
    int well = 0;
    int row = 0;
    int col = 0;
    int quadWell = 0;
} Weights;

typedef struct Values {
    int rowTrans;
    int colTrans;
    int holes;
    int wells;
    int quadWell;
    int wellX;
} Values;

class Bot {
public:
    bool thinking = true;
    int thinkingI = -6;
    int thinkingJ = 0;
    Move current;
    Move best;
    int sleepTimer = 0;
    int exploring = 0;

    int dx = 0;
    int rotation = 0;
    Game* game;
    Game* testGame = nullptr;
    // uint16_t testBoard[40];
    // int columnHeights[10];

    Values currentValues;

    std::list<Move> moves;
    Move firstMoves[MAX_MOVELIST_SIZE];
    Move bestMoves[MAX_MOVELIST_SIZE];
    int bestFuture = 0;

    int checking = 0;
    int previous = 0;

    Weights weights{
        1,  // clears
        -4, // holes
        0,
        -1, // lowest
        0,
        -1, // well
        -1, // row
        -1, // col
        8,  // quadwell
    };

    void run();

    void move();

    // int findBestDrop(int ii,int jj, Game * game);

    void stopThinking();

    void executeMove(Move move);

    Bot() {}
    Bot(Game* _game) {
        game = _game;
        testGame = new Game(game);

        game->replayElligible = false;
    }

    ~Bot() {
        if (testGame != nullptr)
            delete testGame;
    }
};

} // namespace BlockEngine

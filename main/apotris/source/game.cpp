#include "blockEngine.hpp"
#include "def.h"
#include "logging.h"
#include "menu.h"
#include "platform.hpp"
#include "rumblePatterns.hpp"
#include "sprite60tiles_bin.h"
#include "sprites.h"
#include "tetromino.hpp"
#include "text.h"
#include <string>
#include <tuple>

#ifdef GBA
#include <rumble.h>
#endif // GBA

#include "achievementStructure.h"
#include "scene.hpp"
#include "sceneHandling.hpp"
#include <posprintf.h>

using namespace BlockEngine;

void diagnose();
void clearGlow();
void addGlow(Drop);
void addPlaceEffect(Drop);
void drawFrame(int layer);
void drawFrameBackgrounds();
void drawGrid();
void progressBar(int layer);
void showBar(int, int, int, int, int);
void showBestMove();
bool checkDiagonal(uint32_t);
void showSpeedMeter(int);
void hideMinos();
void disappear();
INLINE int getBoard(int, int);
void zoneFlash();
void resetZonePalette();
void rainbowPalette();
void frameSnow(int layer);
void showZoneText();
void showFullMeter();
Function getActionFromKey(int key);
void liftKeys();
void setJourneyGraphics(Save* save, int level);
void replayControl();
void checkPeek();
void showSpawn();
void showTowerScrollIndicator();
static void screenShake();

INLINE int prep(int n);
IWRAM_CODE void updateGrid();

Game* game = nullptr;
OBJ_ATTR* pawnSprite;
OBJ_ATTR* pawnShadow;
OBJ_ATTR* holdSprite;
OBJ_ATTR* holdFrameSprite;
OBJ_ATTR* queueFrameSprites[3];

OBJ_ATTR* queueSprites[5];

OBJ_ATTR* moveSprites[3];

bool onStates = false;

int clearTimer = 0;

int maxClearTimer = 20;

std::string clearTypeText = "";
#define maxClearTextTimer 100
#define clearTextHeight 15

s16 glow[20][10];

int push = 0;

int restartTimer = 0;
#define maxRestartTimer 20

int attackFlashTimer = 0;
#define attackFlashMax 10

#define rumbleMax 1

std::list<FloatText> floatingList;

std::list<Effect> effectList;

std::list<PlaceEffect> placeEffectList;

#define eventPauseTimerMax 60

int eventPauseTimer = 0;

BlockEngine::Drop latestDrop;

static bool holdingSave = false;

#define flashTimerMax 16
static int flashTimer = 0;

COLOR* previousPalette = nullptr;

static int rainbowTimer = 1;
static bool rainbowIncreasing = 0;

static u16 rainbow[5];

static int wiggleTimer = 0;

static bool creditRefresh = false;
static u16 fullMeterTimer = 0;
#define fullMeterTimerMax 5 * 60;
#define fullMeterAnimationLength 15

bool enableBot = false;
static bool refreshSkin = false;

FIXED shakeBuff;
FIXED shakeVelocity;

bool replaying = false;
bool exitGame = false;

int current[22][12];
int previous[22][12];

int gridUpdateTimer = 0;
int gridUpdateTimerMax = 4;

int damp = 2;

int dampTimer = 0;
int dampTimerMax = 60;

int previousHeight = 0;

int peek = 0;
FIXED peekValue = 0;
int peekShift = 0;
bool refresh = false;

int goalLineHeight = 0;

bool handlingTest = false;

int framesSinceLastSave = 0;

void GameScene::draw() {
    // std::string prof;
    // profile_start();
    if (control())
        return;

    if (paused)
        return;

    checkSounds();

    showPawn();
    showShadow();
    showSpawn();

    showHold();
    showQueue(0);

    drawFrame(-1);

    showGoalLine();

    screenShake(); //~320

    showComboStreak();
    showClearText(); //~1200

    showPlaceEffect(); // 300 idle, 3600 per effect when on

    if (game->refresh)
        updateText();

    showSprites(128);

    if (game->refresh || refresh) {
        showBackground(peek);
        game->resetRefresh();
        refresh = false;
    } else if (game->clearLock && !eventPauseTimer) {
        showBackground(peek);
        showTimer();
    } else {
        showTimer();
    }
}

void GameScene::updateText() {
    if (!game->zoneTimer) {
        showText();
    } else {
        showZoneText();
    }

    showTimer();
    showClearText();

    if (proMode) {
        showFinesseCombo();
        showPPS();
    }
}

void GameScene::checkSounds() {
    if (game->sounds.hold)
        sfx(SFX_HOLD);

    if (savefile->settings.moveSfx >= 1 && game->sounds.shift)
        sfx(SFX_SHIFT2);

    if (savefile->settings.moveSfx == 2 && game->sounds.shiftDas)
        sfx(SFX_SHIFT2);

    if (game->sounds.place) {
        if (game->zoneTimer) {
            sfxRate(SFX_PLACE, 0.5);

            // Ripple Zone Rumble mode, Ripples on piece Drop
            if (savefile->settings.zoneRumbleMode == 2) {
                u8 zoneStrengthMod = 0;
                switch (game->zonedLines) {
                case 0 ... 3:
                    zoneStrengthMod = 0;
                    break;
                case 4 ... 7:
                    zoneStrengthMod = 1;
                    break;
                case 8 ... 11:
                    zoneStrengthMod = 2;
                    break;
                case 12 ... 15:
                    zoneStrengthMod = 2;
                    break;
                case 16 ... 19:
                    zoneStrengthMod = 3;
                    break;
                default:
                    zoneStrengthMod = 3;
                    break;
                }
                rumblePatternLowPri(RUMBLE_ZONE_RIPPLE, zoneStrengthMod);
            } else {
                // Zone Rumble Continuous/Off
                rumblePatternLowPri(RUMBLE_PLACE);
            }

        } else {
            sfx(SFX_PLACE);

            rumblePatternLowPri(RUMBLE_PLACE);
        }

        if (savefile->settings.screenShakeType == 0) {
            shake = (shakeMax * (savefile->settings.shakeAmount) / 4) / 2;

        } else if (savefile->settings.screenShakeType == 1) {
            shakeVelocity =
                int2fx(((shakeMax * (savefile->settings.shakeAmount) / 4)) /
                       2) *
                2;
        }

        if (!game->sounds.finesse && game->trainingMode)
            for (int i = 0; i < 3; i++)
                sprite_hide(moveSprites[i]);
    }

    if (game->sounds.invalid) {
        sfx(SFX_INVALID);
        rumblePatternLowPri(RUMBLE_PLACE);
    }

    if (game->sounds.rotate)
        sfx(SFX_ROTATE);

    if (game->sounds.finesse) {
        if (game->trainingMode)
            showBestMove();

        sfx(SFX_MENUCANCEL);
    }

    if (game->sounds.clear) {
        int speed = game->comboCounter;
        if (speed > 10)
            speed = 10;

        if (game->previousClear.linesCleared <= 4) {
            sfxRate(SFX_LEVELUP,
                    (1.0 + (float)speed / 10) / (1 + (game->zoneTimer != 0)));
        } else {
            sfx(SFX_MULTICLEAR);
        }

        int soundEffect = -1;

        clearTypeText = "";
        if (game->zoneTimer) {

        } else if (game->previousClear.isPerfectClear == 1) {
            if ((game->gameMode == MARATHON && game->subMode == 2) ||
                (game->gameMode == ZEN && game->subMode == 1)) {
                soundEffect = SFX_PERFECTSCROLL;
                clearTypeText = "perfect scroll";
            } else {
                soundEffect = SFX_PERFECTCLEAR;
                clearTypeText = "perfect clear";
            }
            effectList.push_back(Effect(0));
            rumblePattern(RUMBLE_PERFECT_CLEAR, speed);
        } else if (game->previousClear.isTSpin == 2) {
            if (game->previousClear.isBackToBack == 1) {
                soundEffect = SFX_BACKTOBACKTSPIN;
                rumblePattern(RUMBLE_TSPIN_B2B, speed);
            } else {
                soundEffect = SFX_TSPIN;
                rumblePattern(RUMBLE_TSPIN, speed);
            }
            if (game->previousClear.linesCleared == 1) {
                clearTypeText = "t-spin single";
            } else if (game->previousClear.linesCleared == 2) {
                clearTypeText = "t-spin double";
            } else if (game->previousClear.linesCleared == 3) {
                clearTypeText = "t-spin triple";
            }
        } else if (game->previousClear.isTSpin == 1) {
            soundEffect = SFX_TSPINMINI;

            clearTypeText = "t-spin mini";
            rumblePattern(RUMBLE_TSPIN_MINI, speed);
        } else if (game->previousClear.linesCleared > 4) {
            int n = game->previousClear.linesCleared;
            if (n < 8) {
                clearTypeText = "quad";
                soundEffect = SFX_QUAD;
                rumblePattern(RUMBLE_QUAD, speed);
            } else if (n < 12) {
                clearTypeText = "octo";
                soundEffect = SFX_OCTORIS;
                rumblePattern(RUMBLE_OCTORIS, speed);
            } else if (n < 16) {
                clearTypeText = "dodeca";
                soundEffect = SFX_DODECATRIS;
                rumblePattern(RUMBLE_DODECATRIS, speed);
            } else if (n < 18) {
                clearTypeText = "decahexa";
                soundEffect = SFX_DECAHEXATRIS;
                rumblePattern(RUMBLE_DECAHEXATRIS, speed);
            } else if (n < 20) {
                clearTypeText = "perfectus";
                soundEffect = SFX_PERFECTRIS;
                rumblePattern(RUMBLE_PERFECTRIS, speed);
            } else {
                clearTypeText = "ultimus";
                soundEffect = SFX_ULTIMATRIS;
                rumblePattern(RUMBLE_ULTIMATRIS, speed);
            }
        } else if (game->previousClear.linesCleared == 4) {
            if (game->previousClear.isBackToBack == 1) {
                soundEffect = SFX_BACKTOBACKQUAD;
                rumblePattern(RUMBLE_QUAD_B2B, speed);
            } else {
                soundEffect = SFX_QUAD;
                rumblePattern(RUMBLE_QUAD, speed);
            }
            clearTypeText = "quad";
            // rumblePattern(testQuad);
        } else if (game->previousClear.linesCleared == 3) {
            soundEffect = SFX_TRIPLE;
            clearTypeText = "triple";
            rumblePattern(RUMBLE_TRIPLE, speed);
        } else if (game->previousClear.linesCleared == 2) {
            soundEffect = SFX_DOUBLE;
            clearTypeText = "double";
            rumblePattern(RUMBLE_DOUBLE, speed);
        }

        if (savefile->settings.clearText == 2 && clearTypeText != "")
            floatingList.push_back(FloatText(clearTypeText));

        if (savefile->settings.announcer && soundEffect != -1)
            sfx(soundEffect);

        if (!game->zoneTimer && (demo || savefile->settings.journey) &&
            game->linesCleared / 10 >
                (game->linesCleared - game->previousClear.linesCleared) / 10) {
            flashTimer = flashTimerMax;
            setJourneyGraphics(savefile, game->linesCleared / 10);
            setPalette();
            zoneFlash();
            refreshSkin = true;
            sfx(SFX_LEVELUPSOUND);
            rumblePatternLowPri(RUMBLE_LEVEL_UP, speed);
        }
    } else if (game->sounds.tspin != 0) {
        int sound = 0;
        if (game->sounds.tspin == 1) {
            clearTypeText = "t-spin mini";
            sound = SFX_TSPINMINI;
            rumblePattern(RUMBLE_TSPIN_MINI);
        } else {
            clearTypeText = "t-spin";
            sound = SFX_TSPIN;
            rumblePattern(RUMBLE_TSPIN);
        }

        if (savefile->settings.announcer)
            sfx(sound);

        if (savefile->settings.clearText == 2)
            floatingList.push_back(FloatText(clearTypeText));
    }

    if (game->sounds.levelUp) {
        sfx(SFX_LEVELUPSOUND);
        rumblePatternLowPri(RUMBLE_LEVEL_UP);

        if (savefile->settings.colors == 3) {
            int n = getClassicPalette();

            for (int i = 0; i < 8; i++) {
                loadPalette(i, 1, &nesPalette[n][0], 4);
                loadPalette(i + 16, 1, &nesPalette[n][0], 4);
            }
        }

        if (game->gameMode == MASTER || game->gameMode == DEATH) {
            maxClearTimer = game->maxClearDelay;
        }

        if (((game->gameMode == MARATHON &&
              game->subMode == 2) || // Tower Explore
             (game->gameMode == ZEN && game->subMode == 1)) &&
            (demo || savefile->settings.journey)) {
            flashTimer = flashTimerMax;
            setJourneyGraphics(savefile, game->linesCleared / 10);
            setPalette();
            zoneFlash();
            refreshSkin = true;
        }
    }

    if (game->sounds.initial) {
        sfx(SFX_INITIAL);
    }

    std::string sectionText = "";

    switch (game->sounds.section) {
    case -1:
        sectionText = "regret!!";
        sfx(SFX_REGRET);
        rumblePattern(RUMBLE_REGRET);
        break;
    case 1:
        sectionText = "cool!!";
        sfx(SFX_COOL);
        rumblePattern(RUMBLE_COOL);
        break;
    case 2:
        sfx(SFX_SECRET);
        rumblePattern(RUMBLE_SECRET);
        break;
    }

    if (sectionText.size() && savefile->settings.clearText == 2)
        floatingList.push_back(FloatText(sectionText));

    if (game->sounds.disappear) {
        disappear();
    }

    if (game->sounds.zone == 1) {
        clearText();
        gradient(0);

        holdingSave = true;

        if (!savefile->settings.journey)
            setPreviousSettings(savefile->settings);

        savefile->settings.colors = 4;
        savefile->settings.palette = 7;
        savefile->settings.clearEffect = 2;
        setPalette();
        setClearEffect();
        showBackground(0);

        flashTimer = flashTimerMax;
        zoneFlash();

        for (int layer = 0; layer < 2; layer++) {
            for (int i = 0; i < 20; i++) {
                setTile(25 + layer, 9, i, 0);
                setTile(25, 20, i, 0);
            }

            for (int i = 0; i < 12; i++) {
                setTile(25 + layer, 9 + i, 20, 0);
            }
        }

        if (savefile->settings.lightMode) {
            for (int i = 0; i < 4; i++) {
                COLOR c = RGB15(10 - i, 10 - i, 10 - i);
                addColorToPalette(i, 5, c, 1);
            }
        }

        setMusicTempo(512);
        setMusicVolume(512 * ((float)savefile->settings.volume / 20));

        sfx(SFX_ZONESTART);
    } else if (game->sounds.zone == 2) {
        if (previousSettings != nullptr)
            savefile->settings.lightMode = !previousSettings->lightMode;

        setPalette();
        if (savefile->settings.lightMode) {
            for (int i = 0; i < 4; i++) {
                COLOR c = RGB15(10 - i, 10 - i, 10 - i);
                addColorToPalette(i, 5, c, 1);
            }
        }

    } else if (game->sounds.zone == -1) {
        aprintClearArea(10, 0, 10, 20);
        resetZonePalette();

        flashTimer = flashTimerMax;
        eventPauseTimer = flashTimerMax + 2;
    }

    if (game->sounds.meter) {
        fullMeterTimer = fullMeterAnimationLength;
    }

    if (game->gameMode == MARATHON && game->subMode) {
        showFullMeter();
    }

    if (game->sounds.bone > 0) {
        savefile->settings.skin = 1000 + game->sounds.bone;
        savefile->settings.edges = false;
        refreshSkin = true;
    } else if (game->sounds.bone == -1) {
        if (previousSettings != nullptr) {
            savefile->settings.skin = previousSettings->skin;
            savefile->settings.edges = previousSettings->edges;
        }

        refreshSkin = true;
    }

    if (game->sounds.towerScroll) {
        sfx(SFX_TOWERSCROLL);
        if (savefile->settings.clearText == 2)
            floatingList.push_back(FloatText("Tower Scroll"));
    }

    const RumbleSequence currentRumble = rumbleGetCurrent();

    bool doContinuousZoneRumble =
        game->zoneTimer &&
        (currentRumble == RUMBLE_NONE ||
         (currentRumble >= RUMBLE_ZONE && currentRumble <= RUMBLE_ZONE_6));

    if (doContinuousZoneRumble && savefile->settings.zoneRumbleMode == 1) {
        switch (game->zonedLines) {
        case 0 ... 3:
            rumblePatternLowPri(RUMBLE_ZONE);
            break;
        case 4 ... 7:
            rumblePatternLowPri(RUMBLE_ZONE_2);
            break;
        case 8 ... 11:
            rumblePatternLowPri(RUMBLE_ZONE_3);
            break;
        case 12 ... 15:
            rumblePatternLowPri(RUMBLE_ZONE_4);
            break;
        case 16 ... 19:
            rumblePatternLowPri(RUMBLE_ZONE_5);
            break;
        default:
            rumblePatternLowPri(RUMBLE_ZONE_6);
            break;
        }
    }

    game->resetSounds();
}

int edges[16] = {
    0x0000, // none
    0x080f, 0x0410, 0x0c16, 0x0010, 0x0816, 0x0011, 0x0817,
    0x000f, 0x0012, 0x0416, 0x0418, 0x0016, 0x0018, 0x0017,
    0x0019, // all
};

void showBackground(int offset) {
    bool showEdges = savefile->settings.edges;

    bool up, down, left, right;
    bool before = false, after = false;

    int startY = 19 - (offset * (1 + game->pawn.big));

    const int clearTile =
        tileBuild(3, false, false, savefile->settings.lightMode);

    if (!game->pawn.big) {
        for (int i = startY; i < game->lengthY && (i - 20 + offset) < 20; i++) {
            int y = i - 20 + offset;

            if (game->bitboard[i] == 0 &&
                !(i < game->lengthY - 1 && game->bitboard[i + 1] != 0)) {
                for (int j = 0; j < 10; j++)
                    setTile(25, 10 + j, y, 0);
                continue;
            }

            if (game->clearLock) {
                before = game->lineClearArray[i - 1];

                after = (i < game->lengthY - 1 && game->lineClearArray[i + 1]);
            }

            for (int j = 0; j < game->lengthX; j++) {
                int b = game->board[i][j];

                if (!b || game->lineClearArray[i] ||
                    game->disappearTimers[i][j] == 1) {
                    if (!showEdges) {
                        setTile(25, 10 + j, y, 0);
                        continue;
                    }

                    if (game->disappearing == 0) {
                        up = (game->board[i - 1][j] > 0 && !before);
                        left = (j - 1 >= 0 && game->board[i][j - 1] > 0 &&
                                !(game->lineClearArray[i]));
                        right = (j + 1 <= 9 && game->board[i][j + 1] > 0 &&
                                 !(game->lineClearArray[i]));
                        down = (i + 1 <= 39 && game->board[i + 1][j] > 0 &&
                                !after);
                    } else {
                        up = (getBoard(j, i - 1) > 0 && !before);
                        left = (j - 1 >= 0 && getBoard(j - 1, i) > 0 &&
                                !(game->lineClearArray[i]));
                        right = (j + 1 <= 9 && getBoard(j + 1, i) > 0 &&
                                 !(game->lineClearArray[i]));
                        down =
                            (i + 1 <= 39 && getBoard(j, i + 1) > 0 && !after);
                    }

                    int count = (up << 3) + (left << 2) + (right << 1) + down;

                    setTile(25, 10 + j, y,
                            tileBuild(edges[count] +
                                      (savefile->settings.lightMode * 0x1000)));
                    continue;
                }

                int offset = 1;

                int n = (b - 1) & 0xf;

                if (savefile->settings.skin == 7 ||
                    savefile->settings.skin == 8)
                    offset = 48 + (n);
                else if (savefile->settings.skin >= 11 &&
                         savefile->settings.skin < 1000)
                    offset = 128 + GameInfo::connectedConversion[b >> 4];

                if (n != 8)
                    setTile(
                        25, 10 + j, y,
                        tileBuild(offset, false, false, n * !game->zoneTimer));
                else
                    setTile(25, 10 + j, y, tileBuild(3, false, false, 0));
            }
        }

        if (savefile->settings.clearEffect == 3) {
            return;
        }

        for (auto const& i : game->linesToClear) {
            int y = i - 20 + offset;

            if (savefile->settings.clearDirection == 0) {
                for (int j = 0; j < 5; j++) {
                    if (clearTimer < maxClearTimer - 10 + j * 2)
                        setTile(25, 10 + j, y, clearTile);
                }
                for (int j = 5; j < 10; j++) {
                    if (clearTimer < maxClearTimer - 10 + (9 - j) * 2)
                        setTile(25, 10 + j, y, clearTile);
                }
            } else if (savefile->settings.clearDirection == 1) {
                for (int j = 0; j < 5; j++) {
                    if (clearTimer < maxClearTimer - 10 + (4 - j) * 2)
                        setTile(25, 10 + j, y, clearTile);
                }
                for (int j = 5; j < 10; j++) {
                    if (clearTimer < maxClearTimer - 10 + (j - 5) * 2)
                        setTile(25, 10 + j, y, clearTile);
                }
            }
        }

    } else {
        for (int i = startY - 1; i < 40 && (i - 20 + offset * 2) < 20; i++) {
            int y = i - 20 + offset * 2;

            if (game->bitboard[i / 2] == 0 &&
                !(i / 2 < game->lengthY - 1 &&
                  game->bitboard[i / 2 + 1] != 0)) {
                for (int j = 0; j < 10; j++)
                    setTile(25, 10 + j, y, 0);
                continue;
            }

            if (game->clearLock) {
                before = game->lineClearArray[(i - 1) / 2];

                after = ((i + 1) / 2 < game->lengthY &&
                         game->lineClearArray[(i + 1) / 2]);
            }

            for (int j = 0; j < 10; j++) {
                int b = game->board[i / 2][j / 2];

                if (!b || game->lineClearArray[i / 2] ||
                    game->disappearTimers[i / 2][j / 2] == 1) {
                    if (!showEdges) {
                        setTile(25, 10 + j, y, 0);
                        continue;
                    }

                    if (game->disappearing == 0) {
                        up = (game->board[(i - 1) / 2][j / 2] > 0 && !before);
                        left = ((j - 1) / 2 >= 0 &&
                                game->board[i / 2][(j - 1) / 2] > 0 &&
                                !(game->lineClearArray[i / 2]));
                        right = ((j + 1) / 2 < game->lengthX &&
                                 game->board[i / 2][(j + 1) / 2] > 0 &&
                                 !(game->lineClearArray[i / 2]));
                        down = ((i + 1) / 2 < game->lengthY &&
                                game->board[(i + 1) / 2][j / 2] > 0 && !after);
                    } else {
                        up = (getBoard(j / 2, (i - 1) / 2) > 0 && !before);
                        left = ((j - 1) / 2 >= 0 &&
                                getBoard((j - 1) / 2, i / 2) > 0 &&
                                !(game->lineClearArray[i / 2]));
                        right = ((j + 1) / 2 < game->lengthX &&
                                 getBoard((j + 1) / 2, i / 2) > 0 &&
                                 !(game->lineClearArray[i / 2]));
                        down = ((i + 1) / 2 < game->lengthY &&
                                getBoard(j / 2, (i + 1) / 2) > 0 && !after);
                    }

                    int count = (up << 3) + (left << 2) + (right << 1) + down;

                    setTile(25, 10 + j, y,
                            tileBuild(edges[count] +
                                      (savefile->settings.lightMode * 0x1000)));
                    continue;
                }

                int offset = 1;

                int n = (b - 1) & 0xf;

                if (savefile->settings.skin == 7 ||
                    savefile->settings.skin == 8)
                    offset = 48 + (n);
                else if (savefile->settings.skin >= 11 &&
                         savefile->settings.skin < 1000)
                    offset = 128 + GameInfo::connectedConversion[b >> 4];

                if (n != 8)
                    setTile(
                        25, 10 + j, y,
                        tileBuild(offset, false, false, n * !game->zoneTimer));
                else
                    setTile(25, 10 + j, y, tileBuild(3, false, false, 0));
            }
        }

        if (savefile->settings.clearEffect == 3) {
            return;
        }

        for (auto const& line : game->linesToClear) {
            int l = line * 2 - 20 + offset * 2;

            for (int y = l; y < l + 2; y++) {
                if (savefile->settings.clearDirection == 0) {
                    for (int j = 0; j < 5; j++) {
                        if (clearTimer < maxClearTimer - 10 + j * 2)
                            setTile(25, 10 + j, y, clearTile);
                    }
                    for (int j = 5; j < 10; j++) {
                        if (clearTimer < maxClearTimer - 10 + (9 - j) * 2)
                            setTile(25, 10 + j, y, clearTile);
                    }
                } else if (savefile->settings.clearDirection == 1) {
                    for (int j = 0; j < 5; j++) {
                        if (clearTimer < maxClearTimer - 10 + (4 - j) * 2)
                            setTile(25, 10 + j, y, clearTile);
                    }
                    for (int j = 5; j < 10; j++) {
                        if (clearTimer < maxClearTimer - 10 + (j - 5) * 2)
                            setTile(25, 10 + j, y, clearTile);
                    }
                }
            }
        }
    }
}

void showPawn() {
    pawnSprite = &obj_buffer[111];
    if (game->clearLock || game->activePiece == -1) {
        sprite_hide(pawnSprite);
        return;
    }

#ifdef TE
    if (flashTimer) {
        sprite_hide(pawnSprite);
        return;
    }
#endif

    sprite_unhide(pawnSprite, 0);

    int* b = BlockEngine::getShape(game->activePiece, game->pawn.rotation,
                                   game->rotationSystem);

    clearSpriteTiles(16 * 7, 4, 4);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int n = b[i * 4 + j];
            if (n > 0) {
                if (savefile->settings.skin == 11)
                    setSpriteTile(
                        16 * 7, j, i, 4,
                        &sprite38tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                else if (savefile->settings.skin == 12)
                    setSpriteTile(
                        16 * 7, j, i, 4,
                        &sprite39tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                else if (savefile->settings.skin == 13)
                    setSpriteTile(
                        16 * 7, j, i, 4,
                        &sprite40tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                else if (savefile->settings.skin == 14)
                    setSpriteTile(
                        16 * 7, j, i, 4,
                        &sprite53tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                else if (savefile->settings.skin < 7 ||
                         savefile->settings.skin > 8)
                    setSpriteTile(16 * 7, j, i, 4, blockSprite);
                else
                    setSpriteTile(16 * 7, j, i, 4,
                                  classicTiles[savefile->settings.skin - 7]
                                              [game->activePiece]);
            }
        }
    }

    delete[] b;

    int n = game->activePiece;

    int blend = 0;

    if (game->maxLockTimer > 1)
        blend += max(16 - (game->lockTimer * 16) / game->maxLockTimer, 0);

    if (!game->zoneTimer) {
        setPawnPalette(11, n, blend, false);
    }

    if (!game->pawn.big) {
        if (game->pawn.y + peek < 16) {
            sprite_hide(pawnSprite);
            return;
        }

        sprite_set_attr(pawnSprite, ShapeSquare, 2, 16 * 7, 11, 2);
        sprite_enable_mosaic(pawnSprite);
        sprite_set_pos(pawnSprite,
                       (10 + game->pawn.x) * 8 +
                           push * savefile->settings.shake,
                       (game->pawn.y - game->boardOffset + peek) * 8 +
                           shake * savefile->settings.shake + peekShift);
    } else {
        if ((game->pawn.y + peek) * 2 < 14) {
            sprite_hide(pawnSprite);
            return;
        }
        sprite_set_attr(pawnSprite, ShapeSquare, 2, 16 * 7, 11, 2);
        sprite_enable_mosaic(pawnSprite);
        sprite_enable_affine(pawnSprite, 31, true);
        sprite_set_size(pawnSprite, 1 << 7, 31);
        sprite_set_pos(pawnSprite,
                       (10 + game->pawn.x * 2) * 8 +
                           push * savefile->settings.shake,
                       (game->pawn.y - game->boardOffset + peek) * 8 * 2 +
                           shake * savefile->settings.shake + peekShift);
    }
}

void showShadow() {
    pawnShadow = &obj_buffer[113];
    if (game->clearLock || game->activePiece == -1 ||
        game->gameMode == CLASSIC ||
        (game->gameMode == MASTER && game->level >= 100)) {
        sprite_hide(pawnShadow);
        return;
    }

#ifdef TE
    if (flashTimer) {
        sprite_hide(pawnShadow);
        return;
    }
#endif

    u8* shadowTexture;

    bool bld = false;
    bool flip = false;
    switch (savefile->settings.shadow) {
    case 0:
        shadowTexture = (u8*)sprite2tiles_bin;
        break;
    case 1:
        shadowTexture = (u8*)sprite15tiles_bin;
        break;
    case 2:
        shadowTexture = (u8*)sprite16tiles_bin;
        break;
    case 3:
        if (savefile->settings.skin < 7 || savefile->settings.skin > 8)
            shadowTexture = blockSprite;
        else
            shadowTexture = (u8*)
                classicTiles[savefile->settings.skin - 7][game->activePiece];
        bld = true;
        break;
    case 4:
        sprite_hide(pawnShadow);
        return;
    case 7:
        if (savefile->settings.skin < 7 || savefile->settings.skin > 8)
            shadowTexture = blockSprite;
        else
            shadowTexture = (u8*)
                classicTiles[savefile->settings.skin - 7][game->activePiece];
        bld = true;
        flip = true;
        break;
    default:
        shadowTexture = (u8*)sprite2tiles_bin;
        break;
    }

    sprite_unhide(pawnShadow, 0);

    int* b = BlockEngine::getShape(game->activePiece, game->pawn.rotation,
                                   game->rotationSystem);

    clearSpriteTiles(16 * 8, 4, 4);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int n = b[i * 4 + j];
            if (n > 0) {
                if (savefile->settings.shadow == 5) {
                    setSpriteTile(
                        16 * 8, j, i, 4,
                        &sprite51tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                } else if (savefile->settings.shadow == 6) {
                    setSpriteTile(
                        16 * 8, j, i, 4,
                        &sprite60tiles_bin
                            [GameInfo::connectedConversion[(n) >> 4] * 32]);
                } else {
                    setSpriteTile(16 * 8, j, i, 4, shadowTexture);
                }
            }
        }
    }

    delete[] b;

    int n = game->activePiece;

    if (!game->zoneTimer) {
        setPawnPalette(10, n, 14 * bld, flip);
    }

    if (!game->pawn.big) {
        if (game->pawn.y + peek < 16) {
            sprite_hide(pawnShadow);
            return;
        }

        sprite_set_attr(pawnShadow, ShapeSquare, 2,
                        16 * (8 - (savefile->settings.shadow == 3 ||
                                   savefile->settings.shadow == 7)),
                        10, 2);
        sprite_set_pos(pawnShadow,
                       (10 + game->pawn.x) * 8 +
                           push * savefile->settings.shake,
                       (game->lowest() - game->boardOffset + peek) * 8 +
                           shake * savefile->settings.shake + peekShift);
    } else {
        if ((game->pawn.y + peek) * 2 < 14) {
            sprite_hide(pawnShadow);
            return;
        }
        sprite_set_attr(pawnShadow, ShapeSquare, 2,
                        16 * (8 - (savefile->settings.shadow == 3 ||
                                   savefile->settings.shadow == 7)),
                        10, 2);
        sprite_enable_mosaic(pawnShadow);
        sprite_enable_affine(pawnShadow, 30, true);
        sprite_set_size(pawnShadow, 1 << 7, 30);

        sprite_set_pos(pawnShadow,
                       (10 + game->pawn.x * 2) * 8 +
                           push * savefile->settings.shake,
                       (game->lowest() - game->boardOffset + peek) * 2 * 8 +
                           shake * savefile->settings.shake + peekShift);
    }
}

void showHold() {
    holdSprite = &obj_buffer[114];
    holdFrameSprite = &obj_buffer[115];
    OBJ_ATTR* background = &obj_buffer[116];

    if (game->gameMode != CLASSIC && !game->zoneTimer) {
        sprite_unhide(holdFrameSprite, 0);
        sprite_set_attr(holdFrameSprite, ShapeSquare, 2, 512, 8, 3);
        sprite_set_pos(holdFrameSprite, 4 * 8 + 7 + (push < 0) * push,
                       9 * 8 - 2);

        sprite_unhide(background, 0);
        sprite_set_attr(background, ShapeSquare, 2, 512 + 400, 8, 3);
        sprite_enable_blend(background);
        sprite_set_pos(background, 4 * 8 + 7 + (push < 0) * push, 9 * 8 - 2);
    } else {
        sprite_hide(holdFrameSprite);
        sprite_hide(background);
    }

    if (game->held == -1) {
        sprite_hide(holdSprite);
        return;
    }

#ifdef TE
    if (flashTimer) {
        sprite_hide(holdSprite);
        return;
    }
#endif

    int add = !(game->held == 0 || game->held == 3);
    int palette = (game->canHold) ? game->held : 7;

    const int skin = savefile->settings.skin;
    const int rs = game->rotationSystem;

    int yoffset =
        -(7 * (rs != SRS && (skin >= 7 && !(skin == 9 || skin == 10))));

    if (skin < 7 || skin == 9 || skin == 10 || skin > 1000) {
        sprite_unhide(holdSprite, 0);
        sprite_set_attr(holdSprite, ShapeWide, 2, 9 * 16 + 8 * game->held,
                        palette * (game->zoneTimer == 0), 3);

        sprite_set_pos(
            holdSprite, (5) * 8 + add * 3 + 3 + (push < 0) * push,
            (10) * 8 -
                3 * (game->held == PIECE_I && !(rs == ARS || rs == BARS)) +
                yoffset +
                3 * ((rs == ARS || rs == BARS) && game->held == PIECE_I));
    } else {
        sprite_unhide(holdSprite, ATTR0_AFF);
        sprite_set_attr(holdSprite, ShapeSquare, 2, 16 * game->held,
                        palette * (game->zoneTimer == 0), 3);
        sprite_enable_affine(holdSprite, 5, false);

        FIXED size;
        if (skin < 9)
            size = 357; //~1.4
        else
            size = 349; //~1.4

        sprite_set_size(holdSprite, size, 5);

        sprite_set_pos(
            holdSprite, (5) * 8 + add * 3 + 3 - 4 + (push < 0) * push,
            (10) * 8 -
                3 * (game->held == PIECE_I && !(rs == ARS || rs == BARS)) +
                yoffset +
                3 * ((rs == ARS || rs == BARS) && game->held == PIECE_I) - 4);
    }
}

void showQueue(bool offsetSpriteIndex) {
    const int frameIndex = (offsetSpriteIndex) ? 122 : 117;
    const int spriteIndex = (offsetSpriteIndex) ? 117 : 6;

    int maxQueue = savefile->settings.maxQueue;

    if (game->gameMode == CLASSIC)
        maxQueue = 1;
    else if (game->gameMode == MASTER || game->gameMode == DEATH)
        maxQueue = (maxQueue < 3) ? maxQueue : 3;

    for (int i = 0; i < 5; i++)
        queueSprites[i] = &obj_buffer[spriteIndex + i];

    for (int i = 0; i < 3; i++)
        queueFrameSprites[i] = &obj_buffer[frameIndex + i];

    OBJ_ATTR* background[3];
    for (int i = 0; i < 3; i++)
        background[i] = &obj_buffer[frameIndex + 3 + i];

    if (game->zoneTimer) {
        for (int i = 0; i < 3; i++) {
            sprite_hide(queueFrameSprites[i]);
            sprite_hide(background[i]);
        }
    } else if (maxQueue > 1) {
        for (int i = 0; i < 3; i++) {
            sprite_unhide(queueFrameSprites[i], 0);
            sprite_set_attr(queueFrameSprites[i], ShapeSquare, 2,
                            512 + 16 + 16 * i, 8, 3);
            sprite_set_pos(queueFrameSprites[i], 173 + (push > 0) * push,
                           12 + 32 * i - (i * 9 * (5 - maxQueue)));

            sprite_unhide(background[i], 0);
            sprite_set_attr(background[i], ShapeSquare, 2, 512 + 416 + i * 16,
                            8, 3);
            sprite_enable_blend(background[i]);
            sprite_set_pos(background[i], 173 + (push > 0) * push, 12 + 32 * i);
        }
    } else {
        sprite_unhide(queueFrameSprites[0], 0);
        sprite_set_attr(queueFrameSprites[0], ShapeSquare, 2, 512, 8, 3);
        sprite_enable_mosaic(queueFrameSprites[0]);
        sprite_set_pos(queueFrameSprites[0], 173 + (push > 0) * push, 12);

        sprite_unhide(background[0], 0);
        sprite_set_attr(background[0], ShapeSquare, 2, 512 + 416, 8, 3);
        sprite_enable_blend(background[0]);
        sprite_set_pos(background[0], 173 + (push > 0) * push, 12);

        for (int i = 1; i < 3; i++) {
            sprite_hide(queueFrameSprites[i]);
            sprite_hide(background[i]);
        }
    }

#ifdef TE
    if (flashTimer) {
        for (int i = 0; i < 5; i++)
            sprite_hide(queueSprites[i]);
        return;
    }
#endif

    int startX = 22 * 8 + 1;
    const int skin = savefile->settings.skin;
    const int rs = game->rotationSystem;

    int yoffset =
        4 * (maxQueue == 1) -
        (6 * (rs != SRS && (skin >= 7 && !(skin == 9 || skin == 10))));

    for (int k = 0; k < 5; k++) {
        if (k >= maxQueue || k >= game->queue.size()) {
            sprite_hide(queueSprites[k]);
            continue;
        }

        int n = game->queue.array[k];

        int add = !(n == 0 || n == 3);
        if (skin < 7 || skin == 9 || skin == 10 || skin > 1000) {
            sprite_unhide(queueSprites[k], 0);
            sprite_set_attr(queueSprites[k], ShapeWide, 2, 16 * 9 + 8 * n,
                            n * (game->zoneTimer == 0), 3);
            sprite_enable_mosaic(queueSprites[k]);

            sprite_set_pos(queueSprites[k],
                           startX + add * 3 + (push > 0) * push,
                           (3 + (k * 3)) * 6 - 3 * (n == PIECE_I) + yoffset +
                               8 * ((rs == ARS || rs == BARS && n == PIECE_I)));
        } else {
            sprite_unhide(queueSprites[k], ATTR0_AFF);
            sprite_set_attr(queueSprites[k], ShapeSquare, 2, 16 * n,
                            n * (game->zoneTimer == 0), 3);
            sprite_enable_affine(queueSprites[k], k, false);
            FIXED size;
            if (skin < 9)
                size = 357; //~1.4
            else
                size = 349; //~1.4

            sprite_set_size(queueSprites[k], size, k);

            sprite_set_pos(queueSprites[k],
                           startX + add * 3 + (push > 0) * push - 4,
                           (3 + (k * 3)) * 6 - 3 * (rs != ARS && n == PIECE_I) -
                               4 + yoffset + 8 * (rs == BARS && n == PIECE_I));
        }
    }
}

bool GameScene::control() {
    if (paused || !counted)
        return false;

    key_poll();

    if (demo && key_hit(KEY_FULL)) {
        if (key_hit(KEY_SELECT)) {
            if (botThinkingSpeed != 11) {
                botThinkingSpeed = 11;
                botSleepDuration = 1;
                botStepMax = 10;
            } else {
                botThinkingSpeed = 6;
                botSleepDuration = 5;
                botStepMax = 1;
            }
        } else {
            exitGame = true;
            demo = false;
            return true;
        }
    }

    if (replaying && key_hit(KEY_FULL)) {
        exitGame = true;

        stopMusic();
        playSongRandom(0);

        return true;
    }

    GameKeys k = savefile->settings.keys;
    MenuKeys m = savefile->settings.menuKeys;

    if (handlingTest && key_hit(m.pause)) {
        exitGame = true;
        playSongRandom(0);

        return true;
    }

#ifdef N3DS
    const bool shouldPause =
        (!demo && !replaying && !handlingTest && n3ds::wasAppPaused) ||
        key_hit(m.pause);
#else
    const bool shouldPause = key_hit(m.pause);
#endif
    if (shouldPause && !multiplayer && !eventPauseTimer && !flashTimer) {
        sfx(SFX_MENUCONFIRM);
        paused = true;
        showTowerScrollIndicator();
        pauseSong();
        clearText();
        updateText();
    }

    if (replaying) {
        replayControl();
        return false;
    }

    if (key_hit(k.hold) && !checkDiagonal(k.hold)) {
        game->hold(1);
    }

    if (key_released(k.hold)) {
        game->hold(0);
    }

    if (key_hit(k.moveLeft) && !checkDiagonal(k.moveLeft))
        game->keyLeft(1);
    if (key_hit(k.moveRight) && !checkDiagonal(k.moveRight))
        game->keyRight(1);

    if (key_hit(k.hardDrop) && !checkDiagonal(k.hardDrop))
        game->keyDrop(1);

    if (key_hit(k.softDrop) && !checkDiagonal(k.softDrop))
        game->keyDown(1);

    if (key_is_down(KEY_A) && key_is_down(KEY_B) && savefile->settings.abHold) {
        game->hold(1);
    } else {
        if (key_hit(k.rotateCW) && !checkDiagonal(k.rotateCW))
            game->rotateCW(1);

        if (key_hit(k.rotateCCW) && !checkDiagonal(k.rotateCCW))
            game->rotateCCW(1);
    }

    if (key_hit(k.rotate180) && !checkDiagonal(k.rotate180)) {
        game->rotateTwice(1);
    }

    if (savefile->settings.abHold &&
        (key_released(KEY_A) || key_released(KEY_B))) {
        game->hold(0);
    }

    if (key_released(k.rotateCW))
        game->rotateCW(0);

    if (key_released(k.rotateCCW))
        game->rotateCCW(0);

    if (key_released(k.rotate180)) {
        game->rotateTwice(0);
    }

    if (key_released(k.hardDrop))
        game->keyDrop(0);

    if (key_released(k.softDrop))
        game->keyDown(0);

    if (key_released(k.moveLeft)) {
        game->keyLeft(0);
    }

    if (key_released(k.moveRight)) {
        game->keyRight(0);
    }

    if (((key_is_down(KEY_L) && key_is_down(KEY_R)) || key_is_down(m.reset)) &&
        !(game->gameMode == BATTLE || game->gameMode == MASTER ||
          game->gameMode == DEATH) &&
        !eventPauseTimer) {
        if (!savefile->settings.resetHoldToggle &&
            (restartTimer++ > maxRestartTimer ||
             !savefile->settings.resetHoldType)) {
            playAgain = true;
        }
    } else {
        restartTimer = 0;
    }

    if (key_hit(k.zone) && game->gameMode == MARATHON && game->subMode == 1 &&
        !game->zoneTimer) {
        game->activateZone(1);
    }

    if (savefile->settings.diagonalType == 1 || game->rotationSystem == ARS) {
        if (key_released(KEY_RIGHT) || key_released(KEY_LEFT)) {
            if (key_is_down(KEY_UP)) {
                Function f = getActionFromKey(KEY_UP);
                (game->*f.gameFunction)(1);
            } else if (key_is_down(KEY_DOWN)) {
                Function f = getActionFromKey(KEY_DOWN);
                (game->*f.gameFunction)(1);
            }
        }
    }

    return false;
}

void GameScene::showTimer() {
    if (!(game->gameMode == TRAINING || game->gameMode == ZEN || demo ||
          handlingTest)) {

        std::string timer =
            timeToString(gameSeconds, savefile->settings.aspectRatio);
        aprintClearArea(0, 1, 10, 1);
        aprint(timer, 9 - (int)timer.size(), 1);
    }

    if (game->trainingMode) {
        wordSprites[4].setText("Finesse");
        wordSprites[4].show(9 * 8 - wordSprites[4].width, 14 * 8, 15);

        wordSprites[5].setTextNum(game->finesseFaults);
        wordSprites[5].show(9 * 8 - wordSprites[5].width, 15 * 8, 15);
    }

    if (proMode) {
        showPPS();
    }

    if (demo) {
        const int x = (savefile->settings.aspectRatio == 0) ? 1 : 3;
        if (frameCounter % 60 < 30)
            aprint("DEMO", x, 1);
        else
            aprint("    ", x, 1);
    }

    if (handlingTest) {
        const int x = (savefile->settings.aspectRatio == 0) ? 1 : 3;
        if (frameCounter % 60 < 30) {
            wordSprites[0].setText("Press");
            wordSprites[0].show(x * 8, 1 * 8, 15);

#ifndef WEB
            wordSprites[1].setText(
                getStringFromKey(savefile->settings.menuKeys.pause));
            wordSprites[1].show(x * 8, 2 * 8, 15);
#endif

            wordSprites[2].setText("to exit");
            wordSprites[2].show(x * 8, 3 * 8, 15);
        }
    }
}

const char units[] = {'k', 'm', 'b'};

void GameScene::showText() {
    int gm = game->gameMode;

    if (gm == MARATHON || gm == ULTRA || gm == BLITZ || gm == CLASSIC ||
        gm == ZEN || (gm == MARATHON && game->subMode == 2)) {

        if (gm == ZEN && game->subMode == 1) {
            wordSprites[0].setText("Height");
            wordSprites[0].show(9 * 8 - wordSprites[0].width, 3 * 8, 15);

            int height =
                game->towerScrollCount + (game->lengthY - game->stackHeight);

            wordSprites[1].setText(std::to_string(height));
            wordSprites[1].show(9 * 8 - wordSprites[1].width, 5 * 8, 15);
        } else {
            wordSprites[0].setText("Score");
            wordSprites[0].show(9 * 8 - wordSprites[0].width, 3 * 8, 15);

            int score = game->score;
            std::string unit = "";

            if (savefile->settings.aspectRatio == 1) {
                int unitCount = 0;

                while (score >= 1000000 && unitCount < 3) {
                    score /= 1000;
                    unit = units[unitCount++];
                }
            }

            wordSprites[1].setText(std::to_string(score) + unit);
            wordSprites[1].show(9 * 8 - wordSprites[1].width, 5 * 8, 15);

            if (gm != ULTRA && gm != ZEN) {
                wordSprites[2].setText("Level");
                wordSprites[2].show(9 * 8 - wordSprites[2].width, 14 * 8, 15);
                wordSprites[3].setTextNum(game->level);
                wordSprites[3].show(9 * 8 - wordSprites[3].width, 15 * 8, 15);
            }
        }

    } else if (gm == TRAINING) {
        wordSprites[0].setText("Training");
        wordSprites[0].show(1 * 8, 1 * 8, 15);
    } else if (gm == DIG && game->subMode) {
        wordSprites[0].setText("Pieces");
        wordSprites[0].show(9 * 8 - wordSprites[0].width, 14 * 8, 15);

        wordSprites[1].setTextNum(game->pieceCounter);
        wordSprites[1].show(9 * 8 - wordSprites[1].width, 15 * 8, 15);
    } else if (gm == MASTER || gm == DEATH) {
        std::string str;

        wordSprites[0].setText("Grade");
        wordSprites[0].show(9 * 8 - wordSprites[0].width, 3 * 8, 15);

        if (gm == MASTER) {
            str = GameInfo::masterGrades[game->grade + game->coolCount];

            status.gradeString = str;
            status.gradeIndex = game->grade + game->coolCount;
        } else if (gm == DEATH) {
            const int index = game->level / 100 - game->regretCount;

            str = GameInfo::deathGrades[index];

            status.gradeString = str;
            status.gradeIndex = index;
        }

        wordSprites[1].setText(str);
        wordSprites[1].show(9 * 8 - wordSprites[1].width, 5 * 8, 15);

        wordSprites[2].setText("Level");
        wordSprites[2].show(9 * 8 - wordSprites[2].width, 14 * 8, 15);

        str = std::to_string(game->level);

        wordSprites[3].setTextNum(game->level);
        wordSprites[3].show(9 * 8 - wordSprites[3].width, 16 * 8, 15);

        showSpeedMeter((int)game->speed);

        int n = ((game->level / 100) + 1) * 100;

        if (n == 1000 && game->gameMode == MASTER)
            n = 999;
        else if (n == 1400 && game->gameMode == DEATH)
            n = 1300;

        wordSprites[4].setTextNum(n);
        wordSprites[4].show(9 * 8 - wordSprites[4].width, 18 * 8, 15);
    }

    if (gm != BATTLE && gm != BLITZ && gm != ZEN &&
        !(gm == SPRINT && game->subMode == 1) &&
        !(gm == MASTER || gm == DEATH)) {

        if ((gm == MARATHON && game->subMode == 2)) {
            wordSprites[6].setText("Height");
        } else {
            wordSprites[6].setText("Lines");
        }

        wordSprites[6].show(9 * 8 - wordSprites[6].width, 17 * 8, 15);

        int n = 0;
        if (gm == DIG) {
            n = game->garbageCleared;
        } else if (gm == MARATHON && game->subMode == 2) {
            // Calculate Height = (Scrolls) + (Current Stack Height)
            n = game->towerScrollCount + (game->lengthY - game->stackHeight);
        } else {
            n = game->linesCleared;
        }

        wordSprites[7].setTextNum(n);
        wordSprites[7].show(9 * 8 - wordSprites[7].width, 18 * 8, 15);

    } else if (gm == BLITZ) {
        wordSprites[6].setText("Lines");
        wordSprites[6].show(9 * 8 - wordSprites[6].width, 17 * 8, 15);

        std::string str;
        if (game->level > 1)
            str += std::to_string(game->linesCleared -
                                  GameInfo::blitzLevels[game->level - 2]);
        else
            str += std::to_string(game->linesCleared);

        str += " / ";

        if (game->level > 1)
            str += std::to_string(GameInfo::blitzLevels[game->level - 1] -
                                  GameInfo::blitzLevels[game->level - 2]);
        else
            str += "3";

        wordSprites[7].setText(str);
        wordSprites[7].show(9 * 8 - wordSprites[7].width, 18 * 8, 15);

    } else if (gm == BATTLE || (gm == SPRINT && game->subMode == 1)) {
        wordSprites[6].setText("Attack");
        wordSprites[6].show(9 * 8 - wordSprites[6].width, 17 * 8, 15);

        wordSprites[7].setTextNum(game->linesSent);
        wordSprites[7].show(9 * 8 - wordSprites[7].width, 18 * 8, 15);
    } else if (gm == ZEN && game->subMode == 0) {
        wordSprites[4].setText("Lines");
        wordSprites[4].show(9 * 8 - wordSprites[4].width, 14 * 8, 15);

        wordSprites[5].setTextNum(game->linesCleared);
        wordSprites[5].show(9 * 8 - wordSprites[5].width, 15 * 8, 15);

        wordSprites[6].setText("Total");
        wordSprites[6].show(9 * 8 - wordSprites[6].width, 17 * 8, 15);

        wordSprites[7].setTextNum(game->linesCleared +
                                  savefile->boards.zenLines);
        wordSprites[7].show(9 * 8 - wordSprites[7].width, 18 * 8, 15);
    }

    if (replaying) {
        OBJ_ATTR* replaySprite = &obj_buffer[108];

        sprite_unhide(replaySprite, 0);

        sprite_set_attr(replaySprite, ShapeSquare, 1, 255, 15, 1);

        const int width = (savefile->settings.aspectRatio == 0) ? 240 : 227;

        sprite_set_pos(replaySprite, width - 16 - 4, 4);
    }
}

void showPPS() {
    FIXED t = (gameSeconds + game->eventTimer) * float2fx(0.0167f);

    FIXED pps;

    if (t <= 0) {
        pps = 0;
    } else {
        pps = fxdiv(int2fx(game->pieceCounter), (t));
    }

    std::string str = "";

    str += std::to_string(fx2int(pps)) + ".";

    int fractional = pps & 0xff;
    for (int i = 0; i < 2; i++) {
        fractional *= 10;
        str += '0' + (fractional >> 8);
        fractional &= 0xff;
    }

    clearTiles(2, 110, 15);

    const int x = 7;

    aprints(str, 25 + x, 7, 2);

    aprints("PPS:", x, 7, 2);
}

void GameScene::showComboStreak() {
    if (savefile->settings.clearText == 0)
        return;

    if (game->gameMode != CLASSIC && !game->zoneTimer) {
        if (game->comboCounter > 0) {
            // aprint("Combo x", 21, clearTextHeight - 1);
            wordSprites[8].setText("Combo");
            wordSprites[8].show(21 * 8 + 4, (clearTextHeight - 1) * 8, 15);

            // aprintf(game->comboCounter, 28, clearTextHeight - 1);
            wordSprites[9].setText("x" + std::to_string(game->comboCounter));
            wordSprites[9].show(21 * 8 + 4, (clearTextHeight)*8, 15);
        } else {
            // aprintClearArea(20, clearTextHeight - 1, 10, 1);
            wordSprites[8].hide();
            wordSprites[9].hide();
        }

        if (game->b2bCounter > 0) {
            // aprint("Streak", 22, clearTextHeight + 1);
            wordSprites[10].setText("Streak");
            wordSprites[10].show(21 * 8 + 4, (clearTextHeight + 2) * 8, 15);

            // aprint("x", 24, clearTextHeight + 2);
            // aprintf(game->b2bCounter + 1, 25, clearTextHeight + 2);
            wordSprites[11].setText("x" + std::to_string(game->b2bCounter));
            wordSprites[11].show(21 * 8 + 4, (clearTextHeight + 3) * 8, 15);
        } else {
            // aprintClearArea(20, clearTextHeight + 1, 10, 2);
            wordSprites[10].hide();
            wordSprites[11].hide();
        }
    }
}

void showClearText() {

    if (game->zoneTimer && !game->lost)
        return;

    auto index = floatingList.begin();
    while (index != floatingList.end()) {
        std::string text = index->text;

        if (++index->timer > maxClearTextTimer) {
            index = floatingList.erase(index);
            aprintClearArea(9, 0, 12, 1);
        } else {
            int height = 0;
            if (index->timer < 2 * maxClearTextTimer / 3)
                height = 5 * (float)index->timer /
                         ((float)2 * maxClearTextTimer / 3);
            else
                height = (30 * (float)(index->timer) / maxClearTextTimer) - 15;

            if (text.size() <= 10) {
                aprint(text, 15 - text.size() / 2, 15 - height);

            } else {
                std::size_t pos = text.find(" ");

                if (pos != std::string::npos) {
                    // aprint("            ", 9, 15 - height);
                    aprintClearArea(9, 15 - height, 12, 1);
                    std::string part1 = text.substr(0, pos);
                    std::string part2 = text.substr(pos + 1);

                    if (15 - height - 1 > 0)
                        aprint(part1, 15 - part1.size() / 2, 15 - height - 1);
                    aprint(part2, 15 - part2.size() / 2, 15 - height);
                } else {
                    aprint(text, 15 - text.size() / 2, 15 - height);
                }
            }
            aprintClearArea(9, 15 - height + 1, 12, 1);
            ++index;
        }
    }
}

void gameLoop() {
    changeScene(new GameScene(), Transitions::FADE);

    exitGame = false;
}

void addGlow(BlockEngine::Drop location) {
    if (game->pawn.big) {
        location.startY *= 2;
        location.endY *= 2;
        location.startX *= 2;
        location.endX *= 2;
    }

    for (int i = 0; i < location.endY && i < 20; i++) {
        for (int j = location.startX; j < location.endX; j++) {
            glow[i][j] = glowDuration;
        }
    }

    if (game->comboCounter >= 0 || game->zoneTimer) {
        int xCenter = (location.endX - location.startX) / 2 + location.startX;
        if (game->previousClear.isTSpin) {
            for (int i = 0; i < 20; i++)
                for (int j = 0; j < 10; j++)
                    glow[i][j] = glowDuration + abs(xCenter - j) +
                                 abs(location.endY - i);
        } else {
            for (int i = 0; i < 20; i++)
                for (int j = 0; j < 10; j++)
                    glow[i][j] =
                        glowDuration +
                        Sqrt(abs(xCenter - j) * abs(xCenter - j) +
                             abs(location.endY - i) * abs(location.endY - i));
        }

        if (game->previousClear.isBackToBack == 1)
            effectList.emplace_back(
                Effect(1 + (game->previousClear.isTSpin != 0), xCenter,
                       location.endY));

        for (int i = max(location.startY, 0); i < location.endY && i < 20; i++)
            memset32_fast(&current[i + 1][1], 2048, 10);

        damp = 4;

        dampTimer = dampTimerMax;

    } else {
        for (int i = max(location.startY, 0); i < location.endY && i < 20; i++)
            for (int j = location.startX; j < location.endX; j++)
                current[i + 1][j + 1] = 1024;
    }
}

void clearGlow() {
    for (int i = 0; i < 20; i++)
        for (int j = 0; j < 10; j++)
            glow[i][j] = 0;
    drawGrid();
}

void GameScene::countdown() {
    int timer = 0;
    const int timerMax = 120;

    if (handlingTest)
        timer = timerMax;

    push = 0;
    shake = 0;
    shakeVelocity = 0;
    shakeBuff = 0;

    screenShake();

    showTimer();
    showQueue(0);
    showHold();
    showPawn();
    showShadow();
    showSpawn();
    showZoneMeter();
    showGoalLine();

    showSprites(128);

    if (!game->zoneTimer)
        gradient(1);

    showBackground(0);

    paused = true;

    GameKeys k = savefile->settings.keys;
    MenuKeys m = savefile->settings.menuKeys;

    bool interrupted = false;

    while (closed() && timer++ < timerMax - 1) {
        canDraw = true;
        vsync();
        key_poll();

        if (!multiplayer && !replaying && !interrupted && !demo &&
            savefile->settings.interruptCountdown &&
            (timer < 2 * timerMax / 3) &&
            key_hit(k.rotateCW | k.rotateCCW | k.rotate180)) {
            interrupted = true;
            timer = timerMax - 30;
            sfx(SFX_GO);
        } else if (key_hit(KEY_FULL & ~(KEY_SELECT))) {
            if (demo) {
                exitGame = true;
                demo = false;
                break;
            } else if (!multiplayer && savefile->settings.interruptCountdown &&
                       key_hit(m.pause)) { // TODO maybe add the
                                           // n3ds::wasAppPaused check here too?
                sfx(SFX_MENUCONFIRM);
                paused = true;
                pauseSong();
                clearText();
                updateText();
                preventLayeredCountdowns = true;

                if (pauseMenu()) {
                    paused = false;
                    preventLayeredCountdowns = false;
                    return;
                } else {
                    timer = 0;
                    paused = true;
                    preventLayeredCountdowns = false;
                    clearText();
                    updateText();

                    // drawFrame(0);
                    // if (game->zoneTimer)
                    //     frameSnow(0);
                    //
                    // showBackground(0);
                    // resetSmallText();
                    // clearText();
                    // setSmallTextArea(110, 3, 7, 9, 10);
                    // showText();

                    showTimer();
                    showQueue(0);
                    showHold();
                    showPawn();
                    showShadow();
                    showSpawn();
                    showZoneMeter();
                    showGoalLine();

                    showSprites(128);

                    if (!game->zoneTimer)
                        gradient(1);

                    showBackground(0);
                }
            }
        }

        if (timer < timerMax / 3) {
            aprint("READY?", 12, 10);
            if (timer == 1 && savefile->settings.announcer) {
                sfx(SFX_READY);
            }
        } else if (timer < 2 * timerMax / 3) {
            aprint("READY?", 12, 10);
            if ((key_is_down(KEY_L) && key_is_down(KEY_R)) ||
                key_is_down(savefile->settings.menuKeys.reset)) {
                playAgain = true;
                break;
            }
        } else {
            aprint("  GO  ", 12, 10);
            if (timer == 2 * timerMax / 3 && savefile->settings.announcer &&
                !interrupted)
                sfx(SFX_GO);
        }
#ifdef GBA
        if (multiplayer) {
            linkConnection->sync();
            if (linkConnection->playerCount() != initialPlayerCount)
                return;
        }
#endif
    }

    paused = false;

    clearText();
    updateText();

    game->update();

    if (replaying) {
        replayControl();
    } else {
        if (key_is_down(k.hold))
            game->hold(1);
        if (key_is_down(k.rotateCW))
            game->rotateCW(1);
        if (key_is_down(k.rotateCCW))
            game->rotateCCW(1);
        if (key_is_down(k.rotate180))
            game->rotateTwice(1);
        if (key_is_down(k.hardDrop))
            game->keyDrop(1);
        if (key_is_down(k.moveLeft)) {
            game->keyLeft(1);
        } else if (key_is_down(k.moveRight)) {
            game->keyRight(1);
        } else if (key_is_down(k.softDrop)) {
            game->keyDown(1);
        }
    }

    game->chargeDas();

    counted = true;
}

void screenShake() {
    // int scroll = 0;
    // if(clearTimer){
    //     int n = game->previousClear.linesCleared;

    //     FIXED s = fxdiv(int2fx(8 * n * clearTimer),int2fx(maxClearTimer));

    //     scroll = fx2int(s);
    // }

    setLayerScroll(1, -push, -(shake + peekShift));
    setLayerScroll(2, -push, -(shake + peekShift));

    enableLayerWindow(1, 0, shake, 240, 160 + shake, false);
    enableLayerWindow(2, 0, shake, 240, 160 + shake, false);

#if defined(TE) || defined(N3DS)
    setSpriteMaskRegion(shake);
#endif

    if (savefile->settings.screenShakeType == 0) {
        if (shake) {
            if (shake > 0)
                shake--;
            else
                shake++;
            shake *= -1;
        }

    } else if (savefile->settings.screenShakeType == 1) {
        shake = fx2int(shakeBuff);
        const FIXED deccel = float2fx(0.768);
        if (shakeBuff) {

            shakeBuff = fxmul(shakeBuff, deccel);

            if (abs(shakeBuff) < int2fx(1))
                shakeBuff = 0;
        }

        if (shakeVelocity) {
            shakeBuff += shakeVelocity;
            shakeVelocity = fxmul(shakeVelocity, deccel);
            if (shakeVelocity <= 0) {
                shakeVelocity = 0;
            }

            if (shake >= (shakeMax * (savefile->settings.shakeAmount) / 4)) {
                shake = (shakeMax * (savefile->settings.shakeAmount) / 4);
            }
        }
    }

    if (game->pushDir != 0) {
        if (abs(push) < pushMax * (savefile->settings.shakeAmount) / 4)
            push += game->pushDir * (1 + (savefile->settings.shakeAmount > 2));
    } else {
        if (push > 0)
            push--;
        else if (push < 0)
            push++;
    }
}

void drawFrame(int layer) {
    int l = 25 + layer;

    int p = 8;
    if (game->zoneTimer)
        p = 0;

    if (layer >= 0) {
        for (int i = 0; i < 20; i++) {
            for (int j = 0; j < 2; j++) {
                setTile(l, j * 11 + 9, i, tileBuild(0x4, (j > 0), false, p));
            }
        }

#if defined(TE) || defined(N3DS)

        if (!game->zoneTimer) {
            setTile(l, 20, 20,
                    tileBuild(29, true, true, p)); // right bottom frame corner
            setTile(l, 9, 20,
                    tileBuild(29, false, true, p)); // left bottom frame corner

            for (int i = 0; i < 10; i++)
                setTile(l, 10 + i, 20, tileBuild(28, false, true, p));
        }

#endif

    } else {
        int frameShake = shake * savefile->settings.shake;

#ifdef GBA
        frameShake = 0;
#endif

        // left
        for (int i = 0; i < 5; i++) {
            OBJ_ATTR* frameSprite = &obj_buffer[77 + i];
            sprite_unhide(frameSprite, 0);
            sprite_set_attr(frameSprite, ShapeTall, 1, 200 + i * 4, p, 1);
            sprite_set_pos(frameSprite, 9 * 8 + push * savefile->settings.shake,
                           i * 32 + frameShake);
        }

        // right
        for (int i = 0; i < 5; i++) {
            OBJ_ATTR* frameSprite = &obj_buffer[77 + 5 + i];
            sprite_unhide(frameSprite, 0);
            sprite_set_attr(frameSprite, ShapeTall, 1, 220 + i * 4, p, 1);
            sprite_enable_flip(frameSprite, true, false);
            sprite_set_pos(frameSprite,
                           20 * 8 + push * savefile->settings.shake,
                           i * 32 + frameShake);
        }

        // bottom
        for (int i = 0; i < 3; i++) {
            OBJ_ATTR* frameSprite = &obj_buffer[77 + 10 + i];
            sprite_unhide(frameSprite, 0);
            sprite_set_attr(frameSprite, ShapeWide, 1, 240 + i * 4, p, 1);
            sprite_enable_flip(frameSprite, false, true);
            sprite_set_pos(frameSprite,
                           9 * 8 + i * 32 + push * savefile->settings.shake,
                           20 * 8 + frameShake);
        }
    }

    progressBar(layer);

    drawGrid();

    showTowerScrollIndicator();
}

const u8 queueBGLengths[5] = {32, 42, 60, 78, 96};

void drawFrameBackgrounds() {
    int maxQueue = savefile->settings.maxQueue;

    if (game->gameMode == MASTER || game->gameMode == DEATH)
        maxQueue = (maxQueue < 3) ? maxQueue : 3;

    const int l = queueBGLengths[maxQueue - 1];

    TILE* t;

    u8* src;

    int n = savefile->settings.frameBackground;

    switch (n) {
    case 0:
        src = (u8*)sprite44tiles_bin;
        break;
    case 1:
        src = (u8*)sprite43tiles_bin;
        break;
    case 2:
        for (int i = 0; i < 4; i++)
            clearSpriteTiles(512 + 400 + i * 16, 4, 4);
        return;
    default:
        src = (u8*)sprite43tiles_bin;
    }

    // draw hold background
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == 0 && j == 0) {
                t = (TILE*)&src[32];
            } else if (i == 0 && j == 3) {
                t = (TILE*)&src[64];
            } else if (i == 3 && j == 0) {
                t = (TILE*)&src[96];
            } else if (i == 3 && j == 3) {
                t = (TILE*)&src[128];
            } else {
                t = (TILE*)src;
            }

            loadSpriteTilesPartial(512 + 400, t, j, i, 1, 1, 4);
        }
    }

    for (int i = 0; i < 3; i++)
        clearSpriteTiles(512 + 416 + i * 16, 4, 4);

    int segments = l / 8;
    int pixelRows = l % 8 - 2;

    if (pixelRows < 0) {
        pixelRows = 0;
    }

    // draw queue background
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == 0 && j == 0) {
                t = (TILE*)&src[32];
            } else if (i == 0 && j == 3) {
                t = (TILE*)&src[64];
            } else if ((i == 3 || i == 11) && j == 0) {
                t = (TILE*)&src[96];
            } else if ((i == 3 || i == 11) && j == 3) {
                t = (TILE*)&src[128];
            } else {
                t = (TILE*)src;
            }

            loadSpriteTilesPartial(512 + 416 + 16 * ((int)i / 4), t, j, i % 4,
                                   1, 1, 4);
        }
    }

    TILE* temp = new TILE();
    u32* c = (u32*)src;

    for (int i = 0; i < 8; i++) {
        temp->data[i] = (*c++) * (i < pixelRows);
    }

    for (int i = 0; i < 4; i++)
        loadSpriteTilesPartial(512 + 416 + 16 * ((int)segments / 4), temp, i,
                               segments % 4, 1, 1, 4);

    delete temp;
}

INLINE int prep(int n) { return min(clamp((n + 500) / 8, 0, 255) / 16, 14); }

IWRAM_CODE void updateGrid() {
    for (int i = 1; i < 21; i++) {
        for (int j = 1; j < 11; j++) {
            current[i][j] = (previous[i - 1][j] + previous[i + 1][j] +
                             previous[i][j - 1] + previous[i][j + 1]) /
                                2 -
                            current[i][j];

            current[i][j] -= current[i][j] >> damp;
        }
    }
}

void drawGrid() {
    std::list<Effect>::iterator index = effectList.begin();

    for (int i = 0; i < (int)effectList.size() && (savefile->settings.effects);
         i++) {
        if (index->timer < index->duration) {
            switch (index->type) {
            case 0:
                if (index->timer % glowDuration == 0) {
                    for (int i = 0; i < 20; i++)
                        for (int j = 0; j < 10; j++)
                            glow[i][j] = glowDuration;
                }
                break;
            case 1:
                if (index->timer == index->duration - 1) {
                    for (int i = 0; i < 20; i++)
                        for (int j = 0; j < 10; j++)
                            if (glow[i][j] < glowDuration)
                                glow[i][j] =
                                    glowDuration +
                                    Sqrt(abs(index->x - j) * abs(index->x - j) +
                                         abs(index->y - i) * abs(index->y - i));
                }
                break;
            case 2:
                if (index->timer == index->duration - 1) {
                    for (int i = 0; i < 20; i++)
                        for (int j = 0; j < 10; j++)
                            if (glow[i][j] < glowDuration)
                                glow[i][j] = glowDuration + abs(index->x - j) +
                                             abs(index->y - i);
                }
                break;
            case 3:
                if (index->timer == 0) {
                    for (int i = 0; i < 20; i++)
                        for (int j = 0; j < 10; j++)
                            glow[i][j] = glowDuration + abs(index->x - j) +
                                         abs(index->y - i);
                }
                break;
            }

            index->timer++;
        } else {
            index = effectList.erase(index);
        }

        index++;
    }

    u32 gridTile;
    int palOffset = 4;

    switch (savefile->settings.backgroundGrid) {
    case 0:
        gridTile = 0x0002;
        break;
    case 1:
        gridTile = 0x000c;
        break;
    case 2:
        gridTile = 0x001a;
        break;
    case 3:
        gridTile = 0x001e;
        break;
    case 4:
        gridTile = 0x001f;
        break;
    case 5:
        gridTile = 0x0020;
        break;
    case 6:
        gridTile = 0x0024;
        break;
    case 7:
        gridTile = 0x0000;
        break;
    default:
        gridTile = 0x0002;
        break;
    }

    bool wrap = false;

#ifdef GBA
    setTiles(26, 31 * 32 + 10, 10,
             tileBuild(gridTile, false, false,
                       palOffset *
                           (savefile->settings.lightMode && !game->zoneTimer)));
    setTiles(26, 20 * 32 + 10, 10,
             tileBuild(gridTile, false, false,
                       palOffset *
                           (savefile->settings.lightMode && !game->zoneTimer)));

    wrap = true;
#endif

    int peekAmount = peek * (1 + game->pawn.big);

    if (savefile->settings.effects < 2) {
        u32 t = tileBuild(gridTile, false, false,
                          palOffset * (savefile->settings.lightMode));

        for (int i = -2; i < 20; i++) {
            for (int j = 0; j < 10; j++) {
                if (i < peekAmount) {
                    if (i >= 0 && glow[i][j] > 0)
                        glow[i][j]--;

                    if (peekValue)
                        setTile(26, 10 + j, i + ((wrap && i < 0) ? 32 : 0),
                                tileBuild(gridTile, false, false, 15));
                    else
                        setTile(26, 10 + j, (32 + i) & 31, t);
                } else if (glow[i][j] == 0 || !savefile->settings.effects) {
                    setTile(26, 10 + j, i, t);
                } else if (i >= 0 && glow[i][j] > glowDuration) {
                    glow[i][j]--;
                    setTile(26, 10 + j, i, t);
                } else if (i >= 0 && glow[i][j] > 0) {
                    glow[i][j]--;
                    int color = 0;
                    if (glow[i][j] >= glowDuration * 3 / 4)
                        color = 3;
                    else if (glow[i][j] >= glowDuration * 2 / 4)
                        color = 2;
                    else if (glow[i][j] >= glowDuration * 1 / 4)
                        color = 1;
                    setTile(
                        26, 10 + j, i,
                        tileBuild(gridTile, false, false,
                                  color + (palOffset) *
                                              (savefile->settings.lightMode &&
                                               !game->zoneTimer)));
                }
            }
        }
    } else if (savefile->settings.effects == 2) {
        for (int i = -2; i < 20; i++) {
            for (int j = 0; j < 10; j++) {
                if (i < peekAmount)
                    setTile(26, j + 10, i,
                            tileBuild(gridTile, false, false, 15));
                else
                    setTile(26, j + 10, i,
                            tileBuild(gridTile, false, false,
                                      prep(current[i + 1][j + 1])));
            }
        }
    }
}

void progressBar(int layer) {
    if (game == nullptr || game->goal == 0 || game->zoneTimer ||
        (game->gameMode == ZEN && game->subMode == 1))
        return;

    int l = 25 + layer;

    int current = 0;
    int max = game->goal;

    if (game->gameMode == MARATHON && game->subMode == 2)
        current = game->towerScrollCount + (game->lengthY - game->stackHeight);
    else if (game->gameMode == DIG)
        current = game->garbageCleared;
    else if (game->gameMode == ULTRA || game->gameMode == BLITZ)
        current = game->timer;
    else if (game->gameMode == SPRINT && game->subMode)
        current = game->linesSent;
    else if (game->gameMode != SURVIVAL)
        current = game->linesCleared;

    if (game->gameMode != BATTLE) {
        showBar(current, max, 20, 8, layer);

        if (layer >= 0) {
            setTile(
                l, 20, 20,
                tileBuild(33, false, false, 8)); // right bottom frame corner
            setTile(l, 9, 20,
                    tileBuild(29, false, true, 8)); // left bottom frame corner
        } else {

            u32* src = (u32*)sprite42tiles_bin;

            TILE tile;
            for (int i = 0; i < 8; i++)
                tile.data[i] = src[7 - i];

            // board frame corners
            loadSpriteTilesPartial(248, &tile, 3, 0, 1, 1, 4);
        }
    } else {
        if (++attackFlashTimer > attackFlashMax)
            attackFlashTimer = 0;

        int p = 8;

        if (layer < 0)
            p += 16;

        if (attackFlashTimer < attackFlashMax / 2) {
            setPaletteColor(p, 4, 0x421f, 1);
        } else {
            setPaletteColor(p, 4, 0x7fff, 1);
        }

        // attack bar
        showBar(game->getIncomingGarbage(), 20, 9, 8, layer);

        if (layer >= 0) {
            setTile(l, 9, 20,
                    tileBuild(33, true, false, 8)); // left bottom frame corner
        } else {
            u32* src = (u32*)sprite54tiles_bin;

            TILE tile;
            for (int i = 0; i < 8; i++)
                tile.data[i] = src[7 - i];

            loadSpriteTilesPartial(237, &tile, 3, 0, 1, 1, 4);
        }
    }
}

void showBar(int current, int max, int x, int palette, int layer) {
    if (max > 10000) {
        current /= 10;
        max /= 10;
    }

    int l = 25 + layer;

    int pixels =
        fx2int(fxmul(fxdiv(int2fx(current), int2fx(max)), int2fx(158)));
    int segments =
        fx2int(fxdiv(int2fx(current), fxdiv(int2fx(max), int2fx(20))));
    int edge = pixels - segments * 8 + 1 * (segments != 0);
    int t = 0;

    if (layer >= 0) {
        // top
        t = 6;
        if (segments == 19 && edge != 0) {
            t = 10;
            for (int j = 0; j < 7; j++) {
                if (j == 0) {
                    if (6 - j > edge)
                        setTileRow(0, 10, j + 1, 0x13300332);
                    else
                        setTileRow(0, 10, j + 1, 0x13344332);
                } else {
                    if (6 - j > edge)
                        setTileRow(0, 10, j + 1, 0x13000032);
                    else
                        setTileRow(0, 10, j + 1, 0x13444432);
                }
            }
        } else if (segments >= 20) {
            t = 8;
        }

        setTile(l, x, 0, tileBuild(t, false, false, palette));

        // bottom
        t = 8;
        if (segments == 0) {
            t = 6;
            if (edge != 0) {
                t = 10;
                for (int j = 0; j < 7; j++) {
                    if (j == 0) {
                        if (j >= edge)
                            setTileRow(0, 10, j + 1, 0x13300332);
                        else
                            setTileRow(0, 10, j + 1, 0x13344332);
                    } else {
                        if (j >= edge)
                            setTileRow(0, 10, j + 1, 0x13000032);
                        else
                            setTileRow(0, 10, j + 1, 0x13444432);
                    }
                }
            }
        }

        setTile(l, x, 19, tileBuild(t, false, true, palette));

        // middle
        for (int i = 1; i < 19; i++) {
            t = 9;
            if (19 - i > segments) {
                t = 7;
            } else if (19 - i == segments) {
                t = 11;
                for (int j = 0; j < 7; j++) {
                    if (7 - j > edge) {
                        setTileRow(0, 11, j + 1, 0x13000032);
                    } else {
                        setTileRow(0, 11, j + 1, 0x13444432);
                    }
                }
            }

            setTile(l, x, i, tileBuild(t, false, false, palette));
        }
    } else {
        TILE temp;

        // top
        if (segments == 19 && edge != 0) {
            memcpy32_fast(&temp, &sprite8tiles_bin[32 * (4)], 8);
            for (int j = 0; j < 7; j++) {
                if (j == 0) {
                    if (6 - j > edge) {
                        temp.data[j + 1] = 0x13300332;
                    } else {
                        temp.data[j + 1] = 0x13344332;
                    }
                } else {
                    if (6 - j > edge) {
                        temp.data[j + 1] = 0x13000032;
                    } else {
                        temp.data[j + 1] = 0x13444432;
                    }
                }
            }
            setSpriteTile(200 + (x > 9) * 20, 0, 0, 1, &temp);
        } else if (segments >= 20) {
            t = 8;
            setSpriteTile(200 + (x > 9) * 20, 0, 0, 1,
                          &sprite8tiles_bin[32 * (2)]);
        } else {
            setSpriteTile(200 + (x > 9) * 20, 0, 0, 1,
                          &sprite8tiles_bin[32 * (0)]);
        }

        // bottom
        if (segments == 0) {
            memcpy32_fast(&temp, &sprite8tiles_bin[32 * (8)], 8);

            if (edge != 0) {
                for (int j = 0; j < 7; j++) {
                    if (j == 6) {
                        if (6 - j >= edge) {
                            temp.data[j] = 0x13300332;
                        } else {
                            temp.data[j] = 0x13344332;
                        }
                    } else {
                        if (6 - j >= edge) {
                            temp.data[j] = 0x13000032;
                        } else {
                            temp.data[j] = 0x13444432;
                        }
                    }
                }
                setSpriteTile(200 + (x > 9) * 20 + 16, 0, 3, 1, &temp);
            } else {
                setSpriteTile(200 + (x > 9) * 20 + 16, 0, 3, 1,
                              &sprite8tiles_bin[32 * (12 - 6)]);
            }
        } else {
            setSpriteTile(200 + (x > 9) * 20 + 16, 0, 3, 1,
                          &sprite8tiles_bin[32 * (13 - 6)]);
        }

        // middle
        for (int i = 1; i < 19; i++) {
            if (19 - i > segments) {
                setSpriteTile(200 + (x > 9) * 20 + (i / 4) * 4, 0, i % 4, 1,
                              &sprite8tiles_bin[32 * (1)]);
            } else if (19 - i == segments) {
                memcpy32_fast(&temp, &sprite8tiles_bin[32 * (5)], 8);
                for (int j = 0; j < 7; j++) {
                    if (7 - j > edge) {
                        temp.data[j + 1] = 0x13000032;
                    } else {
                        temp.data[j + 1] = 0x13444432;
                    }
                }
                setSpriteTile(200 + (x > 9) * 20 + (i / 4) * 4, 0, i % 4, 1,
                              &temp);
            } else {
                setSpriteTile(200 + (x > 9) * 20 + (i / 4) * 4, 0, i % 4, 1,
                              &sprite8tiles_bin[32 * (3)]);
            }
        }
    }
}

void showBestMove() {
    List moveList = game->previousBest;

    moveList.pop_back();

    for (int i = 0; i < 3; i++)
        moveSprites[i] = &obj_buffer[16 + i];

    for (int i = 0; i < 3; i++) {
        if (i > (int)moveList.size() - 1) {
            sprite_hide(moveSprites[i]);
            continue;
        }
        sprite_unhide(moveSprites[i], 0);

        sprite_set_attr(moveSprites[i], ShapeSquare, 0,
                        512 + 128 + moveList.array[i], 15, 2);

        sprite_set_pos(moveSprites[i], 38 + 12 * i, 50);
    }
}

bool checkDiagonal(uint32_t key) {
    if ((!savefile->settings.diagonalType || game->rotationSystem == NRS) &&
        game->rotationSystem != ARS)
        return false;

#if PC
    return (
        (splitKey(key) == splitKey(KEY_DOWN) ||
         splitKey(key) == splitKey(KEY_UP)) &&
        (key_is_down(splitKey(KEY_LEFT)) || key_is_down(splitKey(KEY_RIGHT))));
#endif // PC

    return ((key == (KEY_DOWN) || key == (KEY_UP)) &&
            (key_is_down(KEY_LEFT) || key_is_down(KEY_RIGHT)));
}

void showPlaceEffect() {
    const int tileStart = 512 - 32 * 3;

    for (int i = 0; i < 3; i++)
        sprite_hide(&obj_buffer[19 + i]);

    auto it = placeEffectList.begin();
    for (int i = 0; it != placeEffectList.end(); i++) {
        if (it->timer == 0) {
            sprite_hide(it->sprite);
            placeEffectList.erase(it++);
            i--;
            continue;
        }

        bool flip = false;
        int xoffset = 0;
        int yoffset = 0;
        int r = it->rotation;

        int n = 2 - ((it->timer - 1) / 4);

        switch (it->piece) {
        case 0:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n], 8, 3);
            if (game->rotationSystem == NRS || game->rotationSystem == SDRS) {
                if (r % 2 == 1)
                    r = 1;
                else
                    r = 2;
            } else if (game->rotationSystem == ARS) {
                if (r % 2 == 1)
                    r = 1;
                else
                    r = 0;
            }
            break;
        case 1:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 3], 8, 3);
            xoffset = yoffset = -4;

            if (game->rotationSystem == ARS && r == 2 || (game->rotationSystem == SDRS || game->rotationSystem == BARS) && r == 0)
                yoffset += 8;

            if (game->rotationSystem == NRS || game->rotationSystem == ARS)
                r = (r + 2) % 4;

            break;
        case 2:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 3], 8, 3);
            xoffset = yoffset = -4;
            flip = true;

            if (game->rotationSystem == ARS && r == 2 || (game->rotationSystem == SDRS || game->rotationSystem == BARS) && r == 0)
                yoffset += 8;

            if (game->rotationSystem == NRS || game->rotationSystem == ARS)
                r = (r + 2) % 4;
            break;
        case 3:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 6], 8, 3);
            yoffset = -4 + (game->rotationSystem == NRS ||
                            game->rotationSystem == ARS || game->rotationSystem == SDRS) *
                               8;
            r = 0;
            break;
        case 4:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 9], 8, 3);
            xoffset = -4;
            yoffset = -4;
            if (game->rotationSystem == NRS || game->rotationSystem == ARS) {
                if (r % 2 == 1)
                    r = 1 + 2 * (game->rotationSystem == ARS);
                else
                    r = 2;
            }
            if ((game->rotationSystem == SDRS || game->rotationSystem == BARS) && r == 0)
                yoffset += 8;
            break;
        case 5:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 12], 8, 3);
            xoffset = yoffset = -4;

            if (game->rotationSystem == ARS && r == 2 || 
            (game->rotationSystem == SDRS || game->rotationSystem == BARS) && r == 0)
                yoffset += 8;

            if (game->rotationSystem == NRS || game->rotationSystem == ARS)
                r = (r + 2) % 4;
            break;
        case 6:
            loadSpriteTiles(tileStart + 32 * i, placeEffectTiles[n + 9], 8, 3);
            xoffset = -4;
            yoffset = -4;
            flip = true;
            if (game->rotationSystem == NRS || game->rotationSystem == ARS) {
                if (r % 2 == 1)
                    r = 1;
                else
                    r = 2;
            }
            if ((game->rotationSystem == SDRS || game->rotationSystem == BARS) && r == 0)
                yoffset += 8;
            break;
        default:
            break;
        }

        if (!flip) {
            if ((r == 1 || r == 2))
                xoffset += -1;
            if (r > 1)
                yoffset += -1;
        } else {
            if ((r == 1 || r == 2))
                yoffset += -1;
            if (r < 2)
                xoffset += -1;
        }

        FIXED spin = 0;
        int x, y;

        if (it->timer > 5 && it->piece != 3) {
            spin = 0x4000 * (it->rotating) * ((it->timer - 6));
            spin /= 12;

            if (it->dx || it->dy) {
                int mix = 255 * (6 - (it->timer / 2));

                x = fx2int(
                    lerp(int2fx(it->x - (it->dx * 2)), int2fx(it->x), mix / 6));
                y = fx2int(
                    lerp(int2fx(it->y - (it->dy * 2)), int2fx(it->y), mix / 6));
            } else {
                x = it->x;
                y = it->y;
            }
        } else {
            x = it->x;
            y = it->y;
        }

        const bool doubleSize = (it->piece == PIECE_I && (spin != 0 || r != 0));

        x += xoffset + push * savefile->settings.shake + !doubleSize * 32;
        y += yoffset + shake * savefile->settings.shake + peek * 8 + peekShift +
             16 * !doubleSize;

        it->sprite = &obj_buffer[19 + i];
        sprite_unhide(it->sprite, ATTR0_AFF_DBL);
        sprite_set_attr(it->sprite, ShapeWide, 3, tileStart + 32 * i,
                        (game->zoneTimer != 0) ? 11 : it->piece,
                        it->rotating ? 0 : 3);
        sprite_enable_affine(it->sprite, 7 + i, doubleSize);
        sprite_set_pos(it->sprite, x, y);
        sprite_rotscale(it->sprite, ((flip) ? -1 : 1) << 8, 1 << 8,
                        -0x4000 * (r) + spin, 7 + i);

        it->timer--;
        it++;
    }
}

void addPlaceEffect(BlockEngine::Drop drop) {
    if ((int)placeEffectList.size() >= 3 || !savefile->settings.placeEffect ||
        game->pawn.big)
        return;

    placeEffectList.push_back(
        PlaceEffect(drop.x, drop.y, drop.dx, drop.dy, drop.piece, drop.rotation,
                    drop.rotating * (game->gameMode != CLASSIC)));
}

void hideMinos() {
    sprite_hide(pawnSprite);
    sprite_hide(pawnShadow);
    sprite_hide(holdSprite);
    sprite_hide(&obj_buffer[110]); // spawn sprite

    for (int i = 0; i < 5; i++)
        sprite_hide(queueSprites[i]);

    for (int i = 0; i < 3; i++)
        sprite_hide(&obj_buffer[19 + i]);

    sprite_hide(&obj_buffer[22]);
}

void showSpeedMeter(int fill) {
    const int maxLength = 20;

    fill *= 2;

    OBJ_ATTR* sprite = &obj_buffer[22];

    // set palette
    setPaletteColor(16 + 13, 1, 0x03e0, 1);
    setPaletteColor(16 + 13, 2, 0x001f, 1);
    setPaletteColor(16 + 13, 3, 0x4a52, 1);

    clearSpriteTiles(256 + 3, 4, 1);

    if (fill > maxLength)
        fill = maxLength;

    // write color at specific location depending on if x is less than max
    // length
    for (int x = 0; x < maxLength + 1; x++) {
        int shift = (x % 8);
        if (x < maxLength) {
            setSpritePixel(256 + 3, x / 8, 0, 4, shift, 0, (1 + (x < fill)));
            if (x > 0)
                setSpritePixel(256 + 3, x / 8, 0, 4, shift, 1, 3);
        } else {
            setSpritePixel(256 + 3, x / 8, 0, 4, shift, 1, 3);
        }
    }

    sprite_unhide(sprite, 0);
    sprite_set_attr(sprite, ShapeWide, 1, 256 + 3, 13, 0);
    sprite_set_pos(sprite, 9 * 8 - 22, 139);
}

void disappear() {
    std::tuple<u8, u8> coords;
    // u16* dest = (u16*)se_mem[25];

    bool found = false;

    while (!game->toDisappear.empty()) {
        coords = game->toDisappear.front();

        int x = std::get<0>(coords);
        int y = std::get<1>(coords);

        // clearTiles(25,(y-20)*32 + x + 10,1);
        setTile(25, x + 10, y - 20, 0);

        found = true;

        game->toDisappear.pop_front();
    }

    if (found) {
        showBackground(peek);
    }
}

INLINE int getBoard(int x, int y) {
    if (game->disappearTimers[y][x] == 1)
        return 0;

    return game->board[y][x];
}

void showZoneMeter() {
    if (!(game->gameMode == MARATHON && game->subMode == 1))
        return;

    OBJ_ATTR* sprite = &obj_buffer[23];

#ifdef TE
    if (flashTimer) {
        sprite_hide(sprite);
        return;
    }
#endif

    sprite_set_attr(sprite, ShapeSquare, 2, 256 + 3, 12, 0);
    sprite_enable_mosaic(sprite);
    sprite_set_pos(sprite, 13, 74);
    sprite_unhide(sprite, 0);

    const int color = 0x7fff;
    const int anticolor = 0x0c63;
    const int disabled = 0x5294;

    int n;
    if (!game->zoneTimer) {
        int part = game->zoneCharge / 8;
        n = part * 3 + (game->zoneCharge % 8) / 3;
    } else {
        n = game->zoneTimer / 100 + !(game->zoneTimer == 1);
    }

    for (int i = 0; i < 12; i++) {
        if (i < n && n <= 2 && !game->zoneTimer)
            setPaletteColor(12 + 16, 4 + i, disabled, 1);
        else if (i < n)
            setPaletteColor(12 + 16, 4 + i, color, 1);
        else
            setPaletteColor(12 + 16, 4 + i, anticolor, 1);
    }
}

void showTowerScrollIndicator() {
    OBJ_ATTR* sprite = &obj_buffer[30];

    if (!game->towerClearingPhase || paused || ended) {
        sprite_hide(sprite);
        wiggleTimer = 0;
        return;
    }

    loadSpriteTiles(1008, sprite61tiles_bin, 2, 2);

    sprite_set_attr(sprite, ShapeSquare, 1, 1008, 15, 0);

    int wiggleOffset = 0;
    if (wiggleTimer < 120) {
        int phase = (wiggleTimer / 8) % 4;
        if (phase == 1)
            wiggleOffset = 1;
        else if (phase == 3)
            wiggleOffset = -1;
        wiggleTimer++;
    }

    sprite_set_pos(sprite, 19, 77 + wiggleOffset);

    sprite_unhide(sprite, 0);
}

void zoneFlash() {
    if (flashTimer == flashTimerMax) {
        if (previousPalette != nullptr)
            delete[] previousPalette;
        previousPalette = new COLOR[512];
        savePalette(previousPalette);
        previousPalette[0] = 0x5ad6 * savefile->settings.lightMode;
    }

    flashTimer--;

    setMosaic(flashTimer, flashTimer);

    int n = ((float)flashTimer / flashTimerMax) * 31;

    color_fade_fast(16, 0, previousPalette, 0x7fff, 128, n);

    bool cond = ((flashTimer < flashTimerMax / 2) && eventPauseTimer);

    if (cond && game->gameMode == MARATHON)
        gradient(true);

    mirrorPalettes(cond, 8);

    if (flashTimer < flashTimerMax / 2 &&
        (game->gameMode == MASTER || game->gameMode == DEATH)) {
        if (!savefile->settings.lightMode)
            setPaletteColor(0, 0, 0x0000, 1);
        else
            setPaletteColor(0, 0, 0x5ad6, 1);
    }
}

void resetZonePalette() {
    if (!holdingSave)
        return;

    holdingSave = false;

    if (previousSettings != nullptr)
        savefile->settings = *previousSettings;

    setPalette();
    setClearEffect();

    setMusicTempo(1024);
    setMusicVolume(512 * ((float)savefile->settings.volume / 10));

    loadTiles(0, 4, sprite5tiles_bin, 1);

    // board side frame tiles
    for (int i = 0; i < 10; i++)
        for (int j = 0; j < 4; j++)
            loadSpriteTilesPartial(200 + i * 4, &sprite5tiles_bin, 0, j, 1, 1,
                                   1);

    if (game->lost)
        return;

    for (int layer = 0; layer < 2; layer++) {
        for (int i = 0; i < 20; i++) {
            setTile(25 + layer, 9, i, 0);
            setTile(25, 20, i, 0);
        }

        for (int i = 0; i < 12; i++) {
            setTile(25 + layer, 9 + i, 20, 0);
        }
    }

    if (!eventPauseTimer)
        flashTimer = 0;
}

void showFinesseCombo() {
    OBJ_ATTR* sprite = &obj_buffer[24];

    if (game->rotationSystem != SRS || game->finesseStreak < 3) {
        sprite_hide(sprite);
        return;
    }

    clearSpriteTiles(275, 2, 1);

    std::string text = "x" + std::to_string(game->finesseStreak);
    aprintsSprite(text, 0, 0, 275);

    sprite_set_attr(sprite, ShapeWide, 0, 275, 15, 0);
    sprite_set_pos(sprite, 72 - 4 * text.size(), 104);
    sprite_unhide(sprite, 0);
}

void frameSnow(int layer) {
    if (layer != -1)
        log("snow" + std::to_string(layer));
    const int fillAmount = 20;

    TILE* t = (TILE*)sprite5tiles_bin;

    TILE* dest = new TILE();
    TILE* topTile = new TILE();

    for (int i = 0; i < 8; i++) {
        int add1 = 0;
        int add2 = 0;
        for (int j = 0; j < 4; j++) {
            add1 += ((randNext() % 100 < fillAmount) * 0xf) << ((j + 4) * 4);
            add2 += (((randNext() % 100) < ((10 - i) * 10)) * 0xf)
                    << ((j + 4) * 4);
        }

        dest->data[i] = t->data[i] & ~add1;
        topTile->data[i] = t->data[i] & ~add2;
    }

    loadTiles(0, 4, dest, 1);
    loadTiles(0, 46, topTile, 1);

    if (layer >= 0) {
        int l = 25 + layer;

        int i;
        for (i = 0; i < 20 - ((20 * game->zoneTimer) / game->zoneStart); i++) {
            setTile(l, 9, i, 0);
            setTile(l, 20, i, 0);
        }

        if (i == 20)
            i = 19;

        setTile(l, 9, i, tileBuild(46, false, false, 0));
        setTile(l, 20, i, tileBuild(46, true, false, 0));

#if defined(TE) || defined(N3DS)

        setTile(l, 20, 20,
                tileBuild(29, true, true, 0)); // right bottom frame corner
        setTile(l, 9, 20,
                tileBuild(29, false, true, 0)); // left bottom frame corner

        for (i = 0; i < 10; i++)
            setTile(l, 10 + i, 20, tileBuild(28, false, true, 0));

#endif
    } else {
        const int timer =
            min(20 - ((20 * game->zoneTimer) / game->zoneStart), 19);

        for (int i = 0; i < 20; i++) {
            if (i < (timer)) {
                clearSpriteTile(200 + (i / 4) * 4, 0, i % 4, 1);
                clearSpriteTile(220 + (i / 4) * 4, 0, i % 4, 1);
            } else if (i == timer) {
                setSpriteTile(200 + (i / 4) * 4, 0, i % 4, 1, topTile);
                setSpriteTile(220 + (i / 4) * 4, 0, i % 4, 1, topTile);
            } else {
                setSpriteTile(200 + (i / 4) * 4, 0, i % 4, 1, dest);
                setSpriteTile(220 + (i / 4) * 4, 0, i % 4, 1, dest);
            }
        }
        // board frame corners
        loadSpriteTilesPartial(240, &sprite20tiles_bin[32 * 2], 0, 0, 1, 1, 4);
        loadSpriteTilesPartial(248, &sprite20tiles_bin[32 * 3], 3, 0, 1, 1, 4);
    }

    delete dest;
    delete topTile;
}

void rainbowPalette() {
    int n = (rainbowTimer >> 3) + 24;

    int color = 0;
    if (!savefile->settings.lightMode)
        color = RGB15(31, n, n);
    else
        color = RGB15(n, 31, 31);

    clr_rgbscale((COLOR*)rainbow, (COLOR*)&palette[0][1], 5, color);

    if (savefile->settings.lightMode) {
        loadPalette(16, 1, rainbow, 1);
        loadPalette(16, 2, rainbow, 4);
    }

    if (!savefile->settings.lightMode)
        color_fade((COLOR*)rainbow, (COLOR*)rainbow, 0, 4, 10);
    else
        color_adj_MEM((COLOR*)rainbow, (COLOR*)rainbow, 4, float2fx(0.25));

    loadPalette(0, 1, rainbow, 4);

    if (!savefile->settings.lightMode)
        loadPalette(16, 1, rainbow, 4);

    if (savefile->settings.shadow != 3) {
        loadPalette(16 + 10, 1, rainbow, 4);
    } else {
        if (savefile->settings.lightMode)
            color_fade_palette(16 + 10, 1, (COLOR*)rainbow, 0, 4, 14);
        else
            color_adj_brightness(16 + 10, 1, (COLOR*)rainbow, 4,
                                 (float2fx(0.25)));
    }
    loadPalette(16 + 11, 1, rainbow, 4);
}

void showZoneText() {
    if (game->zonedLines == 0)
        return;

    aprintClearArea(10, 0, 10, 20);

    int height = (game->lengthY - game->zonedLines) / 2;

    if (game->pawn.big)
        height *= 2;

    char buff[4];

    int lines = game->zonedLines;

    posprintf(buff, "%d", lines);

    std::string text = buff;

    text += " line";

    if (lines > 1)
        text += "s";

    const u16 pal[2][2] = {{0x5294, 0x0421}, {0x5294, 0x318c}};

    if (!savefile->settings.lightMode) {
        loadPalette(13, 2, pal[0], 2);
    } else {
        loadPalette(13, 2, pal[1], 2);
    }

    aprintColor(text, 15 - text.size() / 2, height, 2);
}

static FIXED creditCurrentHeight = int2fx(SCREEN_HEIGHT);
static u8 creditLastRead = 0;

const FIXED creditSpeed = float2fx(0.6);
const int creditSpace = 200;

void GameScene::setupCredits() {
    creditCurrentHeight = int2fx(SCREEN_HEIGHT);
    creditLastRead = 0;

    // clearSpriteTiles(256, MAX_WORD_SPRITES * 12, 1);

    auto index = GameInfo::credits.begin();
    std::advance(index, creditLastRead++);

    std::string text = *index;
    std::size_t pos = text.find(" ");

    if (pos != std::string::npos) {
        std::string part1 = text.substr(0, pos);
        std::string part2 = text.substr(pos + 1);

        creditSprites[0].setText(part1);
        creditSprites[1].setText(part2);
    } else {
        creditSprites[1].setText(text);
    }

    creditSprites[2].setText("akouzoukos");

    for (int i = 0; i < 3; i++) {
        creditSprites[i].setPriority(3);
    }

#ifdef GBA
    toggleSpriteSorting(true);
#endif
}

void GameScene::showCredits() {

    creditCurrentHeight -= creditSpeed;

    int height = fx2int(creditCurrentHeight);

    for (int j = 0; j < 3; j++) {
        int y = 8 * (j + (j == 2)) + height;

        if (y > SCREEN_HEIGHT || y < -8) {
            creditSprites[j].hide();
        } else {
            creditSprites[j].show(10 * 8, y, 15);
        }
    }

    if (fx2int(creditCurrentHeight) <= -32)
        creditRefresh = true;
}

void GameScene::refreshCredits() {
    creditRefresh = false;

    creditCurrentHeight += int2fx(creditSpace);

    auto index = GameInfo::credits.begin();
    std::advance(index, creditLastRead);

    std::string text;
    if (creditLastRead >= 10)
        text = "";
    else {
        text = *index;
        creditLastRead++;
    }

    std::size_t pos = text.find(" ");

    if (pos != std::string::npos) {
        std::string part1 = text.substr(0, pos);
        std::string part2 = text.substr(pos + 1);

        creditSprites[0].setText(part1);
        creditSprites[1].setText(part2);
    } else {
        creditSprites[0].setText("");
        creditSprites[1].setText(text);
    }

    if (creditLastRead < 11)
        creditSprites[2].setText("akouzoukos");
    else
        creditSprites[2].setText("");
}

void showFullMeter() {
    setPaletteColor(16 + 12, 2, 0x7fff, 1);

    if (game->zoneCharge != 32) {
        if (fullMeterTimer < fullMeterAnimationLength) {
            fullMeterTimer = 0;
            loadSpriteTiles(256 + 3, meterTiles[0], 4, 4);
        }
        return;
    }

    if (--fullMeterTimer <= 0) {
        fullMeterTimer = fullMeterTimerMax;
        loadSpriteTiles(256 + 3, meterTiles[0], 4, 4);
        return;
    }

    if (fullMeterTimer < fullMeterAnimationLength) {
        int n = 2 - (fullMeterTimer / (fullMeterAnimationLength / 3));
        loadSpriteTiles(256 + 3, meterTiles[n + 1], 4, 4);
    }
}

const Function gameFunctions[9] = {
    {&Game::keyLeft},   {&Game::keyRight},    {&Game::rotateCW},
    {&Game::rotateCCW}, {&Game::rotateTwice}, {&Game::keyDown},
    {&Game::keyDrop},   {&Game::hold},        {&Game::activateZone},
};

Function getActionFromKey(int key) {
    int* keys = (int*)&savefile->settings.keys;
    int k = 0;
    for (int i = 0; i < 9; i++) {
#ifndef PC
        if (keys[i] == key) {
            k = i;
            break;
        }
#else
        if (splitKey(keys[i]) == (splitKey(key))) {
            k = i;
            break;
        }
#endif // PC
    }

    std::string text = std::to_string(key) + " " + std::to_string(k);

    return gameFunctions[k];
}

void liftKeys() { game->liftKeys(); }

void setRandomGraphics(Save* save) {
    save->settings.edges = randNext() % 2;
    save->settings.palette = randNext() % 8;

    if (save->settings.shadow != 4) {
        do {
            save->settings.shadow = randNext() % (MAX_SHADOWS);
        } while (save->settings.shadow == 4);
    }

    save->settings.colors = randNext() % (MAX_COLORS);
    save->settings.backgroundGradient =
        (RGB15((int)(randNext() % 31), (int)(randNext() % 31),
               (int)(randNext() % 31))
         << 16) +
        RGB15((int)(randNext() % 31), (int)(randNext() % 31),
              (int)(randNext() % 31));
    save->settings.frameBackground = randNext() % (MAX_FRAME_BACKGROUNDS);

    save->settings.backgroundType = ((randNext() % 10) > 3);

    if (save->settings.colors == 3) {
        save->settings.shadow = 3;
    }

    if (randNext() % 10 == 0)
        save->settings.backgroundGradient = 0;

    u32 skin = 0;
    int count = 0;
    do {
        skin = randNext() % (MAX_SKINS + MAX_CUSTOM_SKINS) - MAX_CUSTOM_SKINS;
    } while (skin < 0 && !save->customSkins[(skin + 1) * -1].changed &&
             count++ < 20);

    if (count >= 20)
        skin = 0;

    save->settings.skin = skin;

    int grid = 0;
    do {
        grid = randNext() % (MAX_BACKGROUNDS);
    } while ((skin == 9 || skin == 10) && (grid == 0 || grid == 2));

    save->settings.backgroundGrid = grid;

    save->settings.effects = randNext() % 3;

    save->settings.clearEffect = randNext() % MAX_CLEAR_EFFECTS;
    save->settings.clearDirection = randNext() % 2;
}

void setJourneyGraphics(Save* save, int level) {
    switch (level) {
    // case 1:
    //     save->settings.backgroundGradient = 0x7dc8;
    //     save->settings.backgroundGrid = 5;
    //     save->settings.skin = 11;
    //     save->settings.shadow = 3;
    //     save->settings.palette = 5;
    //     save->settings.colors = 1;
    //     save->settings.edges = true;
    //     save->settings.lightMode = false;
    //     break;
    // case 2:
    //     save->settings.backgroundGradient = RGB15(0,0,0);
    //     save->settings.backgroundGrid = 0;
    //     save->settings.skin = 0;
    //     save->settings.shadow = 3;
    //     save->settings.palette = 0;
    //     save->settings.colors = 1;
    //     save->settings.edges = false;
    //     save->settings.lightMode = false;
    //     break;
    // case 3:
    //     save->settings.backgroundGradient = RGB15(0,0,0);
    //     save->settings.backgroundGrid = 0;
    //     save->settings.skin = 0;
    //     save->settings.shadow = 3;
    //     save->settings.palette = 0;
    //     save->settings.colors = 4;
    //     save->settings.edges = false;
    //     save->settings.lightMode = false;
    //     break;
    default:
        setRandomGraphics(save);
        break;
    }
}

std::list<Timestamp*>::iterator replayIterator;

void replayControl() {
    Timestamp* t = *replayIterator;

    while (replayIterator != currentReplay.end() &&
           t->timer <= game->timer + game->eventTimer) {
        bool dir = t->dir;
        switch (t->move) {
        case 0:
            game->keyLeft(dir);
            break;
        case 1:
            game->keyRight(dir);
            break;
        case 2:
            game->rotateCW(dir);
            break;
        case 3:
            game->rotateCCW(dir);
            break;
        case 4:
            game->rotateTwice(dir);
            break;
        case 5:
            game->hold(dir);
            break;
        case 6:
            game->keyDown(dir);
            break;
        case 7:
            game->keyDrop(dir);
            break;
        case 8:
            game->activateZone(dir);
            break;
        }

        ++replayIterator;
        t = *replayIterator;
    }
}

void GameScene::init() {
    // TILE d;
    // memset32_fast(&d, 0x11111111, 8);
    // for(int i = 0; i < 1024; i++)
    //     loadSpriteTiles(i, &d, 1, 1);

#if defined(TE) || defined(N3DS)
    buildBG(2, 0, 25, 0, 0, true);
#endif

    reset();

    setPreviousSettings(savefile->settings);

    // hold frame tiles
    loadSpriteTiles(512, sprite6tiles_bin, 4, 4);

    // queue frame tiles
    loadSpriteTiles(512 + 16, sprite6tiles_bin, 4, 3);
    loadSpriteTilesPartial(512 + 16, &sprite6tiles_bin[128], 0, 3, 4, 1, 4);
    loadSpriteTiles(512 + 32, &sprite6tiles_bin[128], 4, 2);
    loadSpriteTilesPartial(512 + 32, &sprite6tiles_bin[128], 0, 2, 4, 2, 4);
    loadSpriteTiles(512 + 48, &sprite6tiles_bin[128], 4, 1);
    loadSpriteTilesPartial(512 + 48, &sprite6tiles_bin[128], 0, 1, 4, 3, 4);

    // replay sprite tiles
    loadSpriteTiles(255, &sprite50tiles_bin, 2, 2);

    // board side frame tiles
    for (int i = 0; i < 10; i++)
        for (int j = 0; j < 4; j++)
            loadSpriteTilesPartial(200 + i * 4, &sprite5tiles_bin, 0, j, 1, 1,
                                   1);

    // board bottom frame tiles
    for (int i = 1; i < 11; i++)
        loadSpriteTilesPartial(240 + (i / 4) * 4, &sprite20tiles_bin[32], i % 4,
                               0, 1, 1, 4);

    // board frame corners
    loadSpriteTilesPartial(240, &sprite20tiles_bin[32 * 2], 0, 0, 1, 1, 4);
    loadSpriteTilesPartial(248, &sprite20tiles_bin[32 * 3], 3, 0, 1, 1, 4);

    setGradient(savefile->settings.backgroundGradient);

    // finesse move sprites
    for (int i = 0; i < 8; i++)
        loadSpriteTiles(512 + 128 + i, moveSpriteTiles[i], 1, 1);

    // enemy board frame
    loadSpriteTiles(408, sprite59tiles_bin, 2, 4);

    setSkin();

    clearTilemap(25);

    clearTiles(2, 110, 400);

    for (int i = 0; i < 3; i++)
        clearSpriteTiles((927 - 32 * 3) + i * 8 * 4, 8,
                         4); // clear tiles in place Effect area

    setSmallTextArea(110, 3, 7, 9, 10);
    clearText();
    gameSeconds = 0;

    loadSpriteTiles(256 + 3, meterTiles[0], 4, 4);

    for (int i = 0; i < 3; i++)
        moveSprites[i] = &obj_buffer[16 + i];

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i].setup(i, 40 + i * 3, 672 + i * 12, false);

    for (int i = 0; i < 3; i++)
        creditSprites[i].setup(i, 90 + i * 3, 672 + (MAX_WORD_SPRITES + i) * 12,
                               false);

    clearSprites(128);
    updateText();

    drawFrame(-1);
    drawFrameBackgrounds();

    stopMusic();

    showHold();

    showQueue(0);

    showSprites(128);

    if (game->gameMode == MASTER || game->gameMode == DEATH) {
        maxClearTimer = game->maxClearDelay;
    } else if (proMode && game->gameMode != CLASSIC) {
        maxClearTimer = 1;
        game->maxClearDelay = 1;
    } else {
        maxClearTimer = 20;
        game->maxClearDelay = 20;
    }

    enableBlend((0b101010 << 8) + (1 << 6) + (0b1010));

    setPalette();

    if (game->gameMode == MASTER || game->gameMode == DEATH)
        showSpeedMeter((int)game->speed);

    showZoneMeter();

    ended = false;
}

void GameScene::update() {
    framesSinceLastSave++;

    if (refreshSkin) {
        refreshSkin = false;
        setSkin();
        drawFrameBackgrounds();
    }

    if (!counted) {
        countdown();

        if (!(game->gameMode == TRAINING || handlingTest))
            playSongRandom(1);
        return;
    }

    if (skipSong) {
        skipSong = false;
        if (savefile->settings.cycleSongs == 1) { // CYCLE
            playNextSong();
        } else if (savefile->settings.cycleSongs == 2) { // SHUFFLE
            playSongRandom(currentMenu);
        }
    }

    diagnose();

    if (exitGame) {
        if (handlingTest) {
            handlingTest = false;
            changeScene(new HandlingOptionScene(), Transitions::FADE);
        } else if (replaying) {
            replaying = false;
            path.clear();
            path.push_back("Play");
            clearText();
            buildBG(3, 0, 27, 0, 1, true);
            sceneSwitcher(modeToString(previousGameOptions->mode));
        } else {
            enableBot = false;
            changeScene(new TitleScene(), Transitions::FADE);
        }

        for (auto& item : currentReplay)
            delete item;
        currentReplay.clear();

        delete currentReplayHeader;
        currentReplayHeader = nullptr;

        exitGame = false;
        return;
    }

    if (!game->lost && !paused && !game->eventLock) {
        game->update();
    }

    if (handleMultiplayer(1)) {
        return;
    }

    if (game->gameMode == BATTLE && !multiplayer) {
        if (!botGame->clearLock) {
            testBot->run();
            botGame->update();
        }
        handleBotGame();
    } else if (enableBot && !replaying) {
        testBot->run();
    }

    if (creditRefresh && !game->clearLock)
        refreshCredits();

    gameSeconds = game->timer;

    if (game->clearLock && !(game->eventLock && !(game->gameMode == MASTER ||
                                                  game->gameMode == DEATH))) {
        clearTimer++;
    }

    if (game->eventLock) {
        if (eventPauseTimer == 0) {
            eventPauseTimer = eventPauseTimerMax;
            if (game->gameMode == MASTER || game->gameMode == DEATH) {
                showBackground(peek);
                setupCredits();
                flashTimer = flashTimerMax;
                gradient(false);
            }
        } else {
            eventPauseTimer--;

            if (eventPauseTimer == 0)
                game->removeEventLock();
        }
    }

    latestDrop = game->getDrop();

    if (latestDrop.on) {
        addGlow(latestDrop);
        addPlaceEffect(latestDrop);
    }

    canDraw = true;

    if (clearTimer >= maxClearTimer || maxClearTimer <= 0) {
        game->removeClearLock();

        if (savefile->settings.screenShakeType == 0) {
            shake = (shakeMax * (savefile->settings.shakeAmount) / 4);

        } else if (savefile->settings.screenShakeType == 1) {
            // shakeVelocity = int2fx(((shakeMax *
            // (savefile->settings.shakeAmount) / 4)) / 2);
        }

        rumblePatternLowPri(savefile->settings.rumbleStyle == 1 ? RUMBLE_SINGLE
                                                        : RUMBLE_CLASSIC_CLEAR,
                    game->comboCounter);
        clearTimer = 0;
        updateText();
    }

    if ((game->won || game->lost) && !(flashTimer || eventPauseTimer)) {
        paused = true;
        showTowerScrollIndicator();

        endScreen();

        paused = false;
        if (!(playAgain && multiplayer)) {
            return;
        } else {
            playAgain = false;
            counted = false;
        }
    }

    if (paused) {
        liftKeys();
        if (pauseMenu()) {
            paused = false;
            return;
        }
    }

    if (playAgain) {
        playAgain = false;
        addGameStats();
        paused = true;
        startGame();
        changeScene(new GameScene());
        return;
    }

    if (game->zoneTimer) {
        if (gameSeconds % 4 == 0) {
            frameSnow(-1);
        }

        if (!flashTimer)
            rainbowPalette();
    }

    if (game->gameMode == MARATHON && game->subMode && gameSeconds % 2 == 0) {
        if (rainbowIncreasing)
            rainbowTimer++;
        else
            rainbowTimer--;

        if (rainbowTimer >= (32 * 2) - 1 || rainbowTimer == 0)
            rainbowIncreasing = !rainbowIncreasing;

        showZoneMeter();
    }

    if ((game->gameMode == MASTER || game->gameMode == DEATH) &&
        game->eventTimer) {
        showCredits();
    }

    if (demo && game->timer > 60 * 60 * 60 * 3) {
        changeScene(new TitleScene(), Transitions::FADE);
        return;
    }

    if (flashTimer) {
        zoneFlash();
    }

    updateFluid();

    checkPeek();
}

void GameScene::deinit() {
    gradient(0);

    if (previousSettings != nullptr)
        savefile->settings = *previousSettings;

    setPalette();

    disableLayerWindow(1);
    disableLayerWindow(2);

    clearText();

    autosave();

    setSkin();

#if defined(TE) || defined(N3DS)
    buildBG(2, 0, 25, 0, 2, true);
#endif

    resetSmallText();
    clearText();
}

void updateFluid() {
    if (savefile->settings.effects != 2)
        return;

    if (++gridUpdateTimer >= gridUpdateTimerMax) {
        gridUpdateTimer = 0;
        updateGrid();

        FIXED temp[22][12];

        memcpy32_fast(temp, current, 22 * 12);
        memcpy32_fast(current, previous, 22 * 12);
        memcpy32_fast(previous, temp, 22 * 12);
    }

    if (dampTimer) {
        dampTimer--;
        if (!dampTimer)
            damp = 2;
    }
}

void showGoalLine() {
    if (!savefile->settings.goalLine)
        return;

    const int gm = game->gameMode;

    int goal = 0;
    if (gm == DIG) {
        goal = game->garbageCleared;
    } else if (gm == SPRINT && game->subMode) {
        goal = game->linesSent;
    } else if ((gm == MARATHON && game->subMode == 2) ||
               (gm == ZEN && game->subMode == 1)) {
        goal = game->towerScrollCount;
    } else if (gm != SURVIVAL) {
        goal = game->linesCleared;
    } else {
        return;
    }

    int remaining = game->goal - goal;

    if (game->pawn.big) {
        remaining *= 2;
    }

    if (remaining < 20 && remaining > 0) {
        if (!game->clearLock)
            goalLineHeight = 20 - remaining;
    } else {
        return;
    }

    TILE d;
    memset32_fast(&d, 0, 8);

    if (savefile->settings.goalLine == 1)
        d.data[0] = 0x00333300;
    else if (savefile->settings.goalLine == 2)
        d.data[0] = 0x33333333;

    for (int i = 0; i < 4; i++)
        loadSpriteTiles(280 + i, &d, 1, 1);

    for (int i = 0; i < 3; i++) {
        OBJ_ATTR* sprite = &obj_buffer[100 + i];

        sprite_unhide(sprite, 0);
        sprite_set_attr(sprite, ShapeWide, 2, 280, 8, 1);
        sprite_set_pos(
            sprite, (10 + 3 * i) * 8 + push * savefile->settings.shake,
            goalLineHeight * 8 - 1 + shake * savefile->settings.shake +
                peekShift + peek * 8);
    }
}

void checkPeek() {
    if (!savefile->settings.peek)
        return;

    int prev = peek;

    FIXED goal = int2fx(game->peek * 8);

    if (game->lost || game->won)
        goal = 0;

    const int speed = 200;

    if (peekValue != goal) {
        if (abs(peekValue - goal) < speed)
            peekValue = goal;
        else
            peekValue += speed * ((peekValue < goal) ? 1 : -1);
    }

    peek = fx2int(peekValue / 8);
    peekShift = (fx2int(peekValue) & 7) * (1 + game->pawn.big);

    if (prev != peek)
        refresh = true;
}

void showSpawn() {
    if (!savefile->settings.showSpawn)
        return;

    OBJ_ATTR* sprite = &obj_buffer[110];

    if (game->lengthY - game->stackHeight < 17 / (1 + game->pawn.big)) {
        sprite_hide(sprite);
        return;
    }

#ifdef TE
    if (flashTimer) {
        sprite_hide(sprite);
        return;
    }
#endif

    sprite_unhide(sprite, 0);

    int piece = game->queue.front();
    int* b =
        BlockEngine::getShape(game->queue.front(), 0, game->rotationSystem);

    bool add = (game->rotationSystem == ARS || game->rotationSystem == NRS ||
                game->rotationSystem == BARS || game->rotationSystem == SDRS);

    std::tuple<int, int> pos = game->getSpawnPosition();

    int x = std::get<0>(pos);
    int y = std::get<1>(pos) + add;

    const int tile = 288;

    clearSpriteTiles(tile, 4, 2);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            int n = b[(i + add) * 4 + j];
            if (n > 0) {
                setSpriteTile(tile, j, i, 4, sprite52tiles_bin);
            }
        }
    }

    delete[] b;

    if (!game->pawn.big) {
        sprite_set_attr(sprite, ShapeWide, 2, tile, 15, 2);
        sprite_enable_mosaic(sprite);
        sprite_set_pos(
            sprite, (10 + x) * 8 + push * savefile->settings.shake,
            (y - game->boardOffset + peek) * 8 +
                shake * savefile->settings.shake + peekShift -
                8 * (game->rotationSystem == BARS && piece != PIECE_I || game->rotationSystem == SDRS));
    } else {
        sprite_set_attr(sprite, ShapeWide, 2, tile, 15, 2);
        sprite_enable_mosaic(sprite);
        sprite_enable_affine(sprite, 29, true);
        sprite_set_size(sprite, 1 << 7, 29);
        sprite_set_pos(
            sprite, (10 + x * 2) * 8 + push * savefile->settings.shake,
            (y - game->boardOffset + peek) * 8 * 2 +
                shake * savefile->settings.shake + peekShift -
                16 * (game->rotationSystem == BARS && piece != PIECE_I || game->rotationSystem == SDRS));
    }
}

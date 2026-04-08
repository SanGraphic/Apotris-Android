#include "def.h"
#include "platform.hpp"
#include "rumble.h"
#include "rumblePatterns.hpp"
#include "scene.hpp"
#include "tetromino.hpp"
// #include "soundbank.h"
#include "blockEngine.hpp"
#include "logging.h"
#include "text.h"
#include <cstring>
#include <map>
#include <posprintf.h>
#include <string>

#ifdef PC
#include <fstream>
#endif

#include "qrcodegen.h"

using namespace BlockEngine;

void endAnimation();
void showScore();
int onRecord();
std::string nameInput(int);
void qrScene();

void saveReplay();

int mode = 0;
bool showingStats = false;

bool saveExists = false;
BlockEngine::Game* quickSave = nullptr;
BlockEngine::Bot* testBot = nullptr;

int igt = 0;

bool ended = false;

const std::string modeStrings[] = {
    "Marathon", "Sprint",  "Dig",    "Battle",   "Ultra", "Blitz", "Combo",
    "Survival", "Classic", "Master", "Training", "Zen",   "Death",
};

const std::string modeOptionStrings[][5] = {
    {"150", "200", "300", "Endless"},
    {"20", "40", "100"},
    {"10", "20", "100"},
    {"EASY", "MEDIUM", "HARD", "V.HARD", "INSANE"},
    {"3", "5", "10"},
    {""},
    {""},
    {"EASY", "MEDIUM", "HARD"},
    {"", ""},
    {"Normal", "Classic"},
    {""},
    {""},
    {"Normal", "Classic"},
};

void GameScene::endScreen() {
    const int optionsHeight = 11;
    int max = 3;
    int selection = 0;

    // - calculate pps
    FIXED t = gameSeconds * float2fx(0.0167f);
    FIXED pps = fxdiv(int2fx(game->pieceCounter), (t));

    std::string ppsStr = std::to_string(fx2int(pps)) + ".";

    igt = game->inGameTimer;

    int fractional = pps & 0xff;
    for (int i = 0; i < 2; i++) {
        fractional *= 10;
        ppsStr += '0' + (fractional >> 8);
        fractional &= 0xff;
    }

    std::string totalTime = timeToString(gameSeconds, false);

    stopMusic();

    int prevBld = blendInfo;

    int record = endScreenSetup();

    if (multiplayer)
        max = 2;

    playSongRandom(0);

    if (savefile->settings.announcer) {
        if (game->gameMode == ULTRA || game->gameMode == BLITZ) {
            if (game->won == 1) {
                sfx(SFX_TIME);
                rumblePattern(RUMBLE_TSPIN);
            } else if (game->lost == 1) {
                sfx(SFX_GAMEOVER);
                rumblePattern(RUMBLE_YOULOSE);
            }
        } else if (game->gameMode != BATTLE) {
            if (game->gameMode == MASTER && game->won == 1 &&
                game->level / 100 == 5)
            {
                sfx(SFX_TIME);
                rumblePattern(RUMBLE_TSPIN);
            }
            else if (game->won == 1) {
                sfx(SFX_CLEAR);
                rumblePattern(RUMBLE_YOUWIN);
            }
            else if (game->lost == 1) {
                sfx(SFX_GAMEOVER);
                rumblePattern(RUMBLE_YOULOSE);
            }
        } else {
            if (game->won == 1)
            {
                sfx(SFX_YOUWIN);
                rumblePattern(RUMBLE_YOUWIN);
            } else if (game->lost == 1) {
                sfx(SFX_YOULOSE);
                rumblePattern(RUMBLE_YOULOSE);
            }
        }
    }

    showStats(showingStats, totalTime, ppsStr, true);

    if (!replaying) {
        saveReplay();
    } else {
        delete currentReplayHeader;
        currentReplayHeader = nullptr;

        for (auto& item : currentReplay)
            delete item;
        currentReplay.clear();

        replaying = false;
    }

    int cursorFloat = 0;
    OBJ_ATTR* cursorSprite;

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    cursorSprite = &obj_buffer[1];
    sprite_hide(cursorSprite);

    std::string placeString;
    if (record != -1 && game->gameMode != BATTLE &&
        (game->won || game->gameMode == MARATHON || game->gameMode >= ULTRA)) {
        switch (record) {
        case 0:
            placeString = "1st";
            break;
        case 1:
            placeString = "2nd";
            break;
        case 2:
            placeString = "3rd";
            break;
        case 3:
            placeString = "4th";
            break;
        case 4:
            placeString = "5th";
            break;
        }
    } else if (game->gameMode == BATTLE) {
        switch (rank - 1) {
        case 0:
            placeString = "1st";
            break;
        case 1:
            placeString = "2nd";
            break;
        case 2:
            placeString = "3rd";
            break;
        case 3:
            placeString = "4th";
            break;
        case 4:
            placeString = "5th";
            break;
        }
    }

    if (placeString != "") {
        placeString += " Place";
        naprint(placeString, 15 * 8 - getVariableWidth(placeString) / 2, 5 * 8);
    }

    MenuKeys k = savefile->settings.menuKeys;

    while (closed()) {
        if (handleMultiplayer(0)) {
            return;
        }

        vsync();
        key_poll();

        showScore();

        cursorFloat += 6;
        if (cursorFloat >= 512)
            cursorFloat = 0;
        int offset = (sinLut(cursorFloat) * 2) >> 12;
        FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

        sprite_unhide(cursorSprite, 0);
        sprite_set_attr(cursorSprite, ShapeSquare, 0, 7 * 16, 5, 0);
        sprite_enable_affine(cursorSprite, 6, true);
        sprite_set_size(cursorSprite, scale, 6);

        int x = 240 / 2 - ((20) + offset + 4) - 16;

        sprite_set_pos(cursorSprite, x,
                       ((optionsHeight + selection * 3) * 8) - 5);

        int counter = 0;

        wordSprites[1].show(12 * 8, (optionsHeight + counter) * 8,
                            14 + (selection == 0));
        wordSprites[2].show(14 * 8, (optionsHeight + counter + 1) * 8,
                            14 + (selection == 0));
        counter += 3;

        if (!multiplayer) {
            wordSprites[3].show(12 * 8, (optionsHeight + counter) * 8,
                                14 + (selection == 1));
            wordSprites[4].show(14 * 8, (optionsHeight + counter + 1) * 8,
                                14 + (selection == 1));
            counter += 3;
        }

        wordSprites[5].show(12 * 8, (optionsHeight + counter) * 8,
                            14 + (selection == 1 + !multiplayer));
        wordSprites[6].show(14 * 8, (optionsHeight + counter + 1) * 8,
                            14 + (selection == 1 + !multiplayer));

        if (playAgain && multiplayer) {
            startMultiplayerGame(nextSeed);
            changeScene(new GameScene());
            break;
        }

        if (key_hit(k.confirm) || key_hit(k.pause)) {
            if (selection == 0) {
                shake = 0;

                if (!multiplayer) {
                    sfx(SFX_MENUCONFIRM);
                    startGame();
                    changeScene(new GameScene());
                    break;
                } else {
                    if (connected == 1) {
                        sfx(SFX_MENUCONFIRM);
                        // Mark that we are ready
                        playAgainMutex->updatePlayerReady(currentPlayerId,
                                                          true);
                        if (!playAgainMutex->checkAllReady()) {
#ifdef GBA
                            nextSeed = (u16)randNext() % 0x1FFE;
                            linkConnection->send((1 << 13) + nextSeed);
#endif
                        } else {
#ifdef GBA
                            linkConnection->send((1 << 13) + 0x1FFF);
#endif
                            multiplayerStart = true;
                            playAgainMutex->clear();
                        }
                    } else {
                        sfx(SFX_MENUCANCEL);
                    }
                }
            } else if (selection == max - 1) {
                sfx(SFX_MENUCANCEL);
                enableBlend(prevBld);
                buildBG(3, 0, 27, 0, 1, true);
#ifdef GBA
                if (linkConnection->isActive())
                    linkConnection->deactivate();
#endif
                multiplayer = false;
                changeScene(new MainMenuScene, Transitions::FADE);
                return;
            } else if (selection == 1) {
                sfx(SFX_MENUCANCEL);
                enableBlend(prevBld);
                path.clear();
                path.push_back("Play");
                clearText();
                buildBG(3, 0, 27, 0, 1, true);
                sceneSwitcher(modeToString(previousGameOptions->mode));
                nextSeed = 0;
                return;
            }
        }

        if (key_hit(k.up)) {
            if (selection == 0)
                selection = max - 1;
            else
                selection--;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.down)) {
            if (selection == max - 1)
                selection = 0;
            else
                selection++;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.special1)) {
            sfx(SFX_MENUCONFIRM);
            showingStats = !showingStats;
            clearSmallText();
            showStats(showingStats, totalTime, ppsStr, true);
        }

        if (!multiplayer && key_hit(k.special2)) {
            if (game->replayElligible) {
                sfx(SFX_MENUCONFIRM);
                loadReplay();
                break;
            } else {
                sfx(SFX_MENUCANCEL);
            }
        }

        if (multiplayer) {
            showReadyPlayers();
        }

        updateGrid();
        updateFluid();
        drawGrid();

        showSprites(128);
    }

    buildBG(3, 0, 27, 0, 1, true);
    enableBlend(prevBld);
    return;
}

void endAnimation() {
    ended = true;
    shake = 0;
    push = 0;

    clearText();
    clearSprites(128);
    showHold();

    showQueue(1);
    drawFrame(-1);

    showSprites(128);

    setLayerScroll(1, 0, 0);
    setLayerScroll(2, 0, 0);

    enableLayerWindow(1, 0, 0, 240, 160, false);
    enableLayerWindow(2, 0, 0, 240, 160, false);

    rumblePatternStop();

    sfx(SFX_END);

    for (int i = 0; i < 40; i++) {
        closed();
        vsync();
        drawGrid();
        showClearText();

        updateFluid();
        checkPeek();

        if (i % 2 != 0)
            continue;

        showBackground(i / 2);
    }

    int counter = 0;
    while (!floatingList.empty() && closed()) {
        vsync();
        showClearText();

        if (++counter > 120)
            break;
        updateFluid();
        drawGrid();
        checkPeek();
    }
    clearText();

    setLayerScroll(1, 0, 0);
    setLayerScroll(2, 0, 0);
}

void showScore() {
    if (game->gameMode == BATTLE) {
        if (game->lost) {
            aprint("YOU LOSE", 11, 3);
        } else {
            aprint("YOU WIN!", 11, 3);
        }

        aprint("Lines Sent", 10, 7);
        aprintf(game->linesSent, 14, 9);

    } else if (game->gameMode == MASTER) {
        int grade = game->grade + game->coolCount + (int)game->creditGrade;
        if (grade > 32)
            grade = 32;

        std::string gradeText = GameInfo::masterGrades[grade];

        if (gradeText[0] == ' ')
            gradeText.erase(0, 1);

        gradeText = "Grade: " + gradeText;

        if (game->won && game->level / 100 == 5) {
            aprint("TIME!", 13, 3);
        } else if (game->won && game->level == 999) {
            aprint("CLEAR!", 12, 3);
        } else {
            aprint("GAME OVER", 11, 3);
        }

        aprint(gradeText, 15 - ((int)gradeText.size() / 2), 7);
    } else if (game->gameMode == DEATH) {

        int section = game->level / 100;

        if (game->won && (section == 5 || section == 10)) {
            aprint("TIME!", 13, 3);
        } else if (game->won && game->level == 1300) {
            aprint("CLEAR!", 12, 3);
        } else {
            aprint("GAME OVER", 11, 3);
        }

        int grade = max(section - game->regretCount, 0);

        if (grade > 0 && grade <= 13) {
            std::string gradeText = "Grade: S" + std::to_string(grade);

            aprint(gradeText, 15 - ((int)gradeText.size() / 2), 7);
        }
    } else if (game->gameMode == MARATHON || game->lost ||
               game->gameMode >= ULTRA ||
               (game->gameMode == DIG && game->subMode == 1)) {
        std::string score;

        if (game->gameMode == DIG) {
            if (game->subMode == 1)
                score = std::to_string(game->pieceCounter);
        } else if (game->gameMode == ZEN && game->subMode == 1) {
            score = std::to_string(game->towerScrollCount +
                                   (game->lengthY - game->stackHeight));
        } else if (game->gameMode != COMBO)
            score = std::to_string(game->score);
        else
            score = std::to_string(game->statTracker.maxCombo);

        if (game->lost)
            aprint("GAME OVER", 11, 3);
        else {
            if (game->gameMode != ULTRA && game->gameMode != BLITZ)
                aprint("CLEAR!", 12, 3);
            else
                aprint("TIME!", 13, 3);
        }

        if (game->gameMode == MARATHON || game->gameMode == ULTRA ||
            game->gameMode == BLITZ || game->gameMode == COMBO ||
            game->gameMode == DIG || game->gameMode == CLASSIC)
            aprint(score, 15 - ((int)score.size() / 2), 7);
        else if (game->gameMode == SURVIVAL)
            aprint(timeToString(gameSeconds, false), 11, 7);
    } else {
        aprint("CLEAR!", 12, 3);

        aprint(timeToString(gameSeconds, false), 11, 7);
    }
}

void GameScene::showStats(bool moreStats, std::string time, std::string pps,
                          bool end) {
    if (!moreStats) {
        aprints("Press " +
                    getStringFromKey(savefile->settings.menuKeys.special1),
                0, 0, 2);
        aprints("for Stats", 0, 7, 2);

        if (end && !(multiplayer || game->gameMode == BATTLE)) {
            if (game->replayElligible) {
                aprints("Press " + getStringFromKey(
                                       savefile->settings.menuKeys.special2),
                        0, 21, 2);
                aprints("for Replay", 0, 28, 2);
            } else {
                aprints("Replay", 0, 21, 1);
                aprints("not available", 0, 28, 1);
            }
        } else if (game->gameMode != TRAINING) {
            std::string keyString =
                getStringFromKey(savefile->settings.menuKeys.special3);

            std::transform(keyString.begin(), keyString.end(),
                           keyString.begin(), ::toupper);

            aprints("Press " + keyString, 0, 21, 2);
            aprints("to Skip Track", 0, 28, 2);
        }

        return;
    }

    int counter = 0;

    int g = game->gameMode;

    aprints("Time: " + time, 0, 7 * counter++, 2);
    if (!proMode)
        aprints("IGT: " + timeToString(igt, false), 0, 7 * counter++, 2);

    if (g == MARATHON || g == BLITZ || g == CLASSIC) {
        if (g != BLITZ)
            aprints("Start Level: " + std::to_string(game->initialLevel), 0,
                    7 * counter++, 2);

        aprints("Final Level: " + std::to_string(game->level), 0, 7 * counter++,
                2);
        aprints("Score: " + std::to_string(game->score), 0, 7 * counter++, 2);
    } else if (g == ULTRA) {
        aprints("Score: " + std::to_string(game->score), 0, 7 * counter++, 2);
    } else if (g == MASTER || g == DEATH) {
        aprints("Level: " + std::to_string(game->level), 0, 7 * counter++, 2);
    }

    if ((g == MARATHON && game->subMode == 2) ||
        (g == ZEN && game->subMode == 1)) {
        int height =
            game->towerScrollCount + (game->lengthY - game->stackHeight);
        aprints("Height: " + std::to_string(height), 0, 7 * counter++, 2);
    }

    aprints("Lines: " + std::to_string(game->linesCleared), 0, 7 * counter++,
            2);
    aprints("Pieces: " + std::to_string(game->pieceCounter), 0, 7 * counter++,
            2);

    aprints("PPS: " + pps, 0, 7 * counter++, 2);

    if (game->rotationSystem == SRS)
        aprints("Finesse: " + std::to_string(game->finesseFaults), 0,
                7 * counter++, 2);

    aprints("Singles: " + std::to_string(game->statTracker.clears[0]), 0,
            7 * counter++, 2);
    aprints("Doubles: " + std::to_string(game->statTracker.clears[1]), 0,
            7 * counter++, 2);
    aprints("Triples: " + std::to_string(game->statTracker.clears[2]), 0,
            7 * counter++, 2);
    aprints("Quads: " + std::to_string(game->statTracker.clears[3]), 0,
            7 * counter++, 2);

    if (game->gameMode != CLASSIC) {
        aprints("T-Spins: " + std::to_string(game->statTracker.tspins), 0,
                7 * counter++, 2);

        if (savefile->settings.aspectRatio == 0)
            aprints("Perfect Clears: " +
                        std::to_string(game->statTracker.perfectClears),
                    0, 7 * counter++, 2);
        else if (savefile->settings.aspectRatio == 1)
            aprints("PCs: " + std::to_string(game->statTracker.perfectClears),
                    0, 7 * counter++, 2);

        aprints("Max Streak: " + std::to_string(game->statTracker.maxStreak), 0,
                7 * counter++, 2);
        aprints("Max Combo: " + std::to_string(game->statTracker.maxCombo), 0,
                7 * counter++, 2);
        aprints("Times Held: " + std::to_string(game->statTracker.holds), 0,
                7 * counter++, 2);
    }

    if (game->gameMode == MASTER) {
        aprints("Section Cools: " + std::to_string(game->coolCount), 0,
                7 * counter++, 2);
        aprints("Section Regrets: " + std::to_string(game->regretCount), 0,
                7 * counter++, 2);
    }

    if (game->statTracker.maxZonedLines > 0) {
        aprints("Max MultiClear: " +
                    std::to_string(game->statTracker.maxZonedLines),
                0, 7 * counter++, 2);
    }

    aprints("Times Paused: " + std::to_string(timesPaused), 0, 7 * counter++,
            2);

    if (game->statTracker.secretGrade > 0) {
        std::string str =
            "Secret Grade: " +
            (std::string)
                GameInfo::secretGrades[game->statTracker.secretGrade - 1];

        aprints(str, 0, 7 * counter++, 2);
    }
}

int GameScene::pauseMenu() {
    int selection = 0;
    int maxSelection;

    int optionsHeight = 10;
    int optionsCounter = 0;

    timesPaused++;

    clearText();
    setSmallTextArea(220, 1 + (savefile->settings.aspectRatio != 0), 1, 12, 20);

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i].hide();

    int prevBld = blendInfo;
    enableBlend((1 << 6) + (0b11101 << 9) + (1 << 3));
    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));
    buildBG(3, 0, 27, 0, 0, 0);
    clearTilemap(25);

    drawFrame(1);
    if (game->zoneTimer)
        frameSnow(1);

    setLayerScroll(2, 0, 0);

    // hide Sprites
    hideMinos();
    sprite_hide(&obj_buffer[23]); // hide meter
    sprite_hide(&obj_buffer[24]); // hide finesse combo counter
    sprite_hide(&obj_buffer[25]); // hide enemyBoard
    for (int i = 0; i < 3; i++)
        sprite_hide(&obj_buffer[16 + i]);

    showSprites(128);

    rumblePatternStop();

    // calculate pps
    FIXED t = gameSeconds * float2fx(0.0167f);
    FIXED pps = 0;
    if (t > 0)
        pps = fxdiv(int2fx(game->pieceCounter), (t));

    std::string ppsStr = std::to_string(fx2int(pps)) + ".";

    int fractional = pps & 0xff;
    for (int i = 0; i < 2; i++) {
        fractional *= 10;
        ppsStr += '0' + (fractional >> 8);
        fractional &= 0xff;
    }

    std::string totalTime = timeToString(gameSeconds, false);
    igt = game->inGameTimer;

    showModeText();

    bool shown = false;

    bool skipped = false;

    bool count = true;

    int cursorFloat = 0;
    OBJ_ATTR* cursorSprites[2];

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[1 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    MenuKeys k = savefile->settings.menuKeys;

    while (closed()) {
        if (!onStates) {
            maxSelection = 4;
            if (game->gameMode != TRAINING)
                maxSelection--;

            if (maxSelection == 3) {
                optionsHeight = 11;
            }
        } else
            maxSelection = 3;

        vsync();
        key_poll();
        #ifdef GBA
            if (sleep_combo_check(key_curr_state())) {
                // Re-poll keys to avoid triggering stats/track skip.
                // I'm not a fan of this but it works with the current scene infrastructure.
                key_poll();
            }
        #endif

        wordSprites[0].setText("PAUSE!");
        wordSprites[0].show(15 * 8 - wordSprites[0].width / 2, 4 * 8, 15);

        optionsCounter = 0;

        auto showOption = [this, &optionsCounter, optionsHeight,
                           selection](std::string str) {
            wordSprites[1 + optionsCounter].setText(str);
            wordSprites[1 + optionsCounter].show(
                15 * 8 - wordSprites[1 + optionsCounter].width / 2,
                (optionsHeight + optionsCounter * 2) * 8,
                15 - !(selection == optionsCounter));
            optionsCounter++;
        };

#ifdef GBA
        if (!multiplayer) {
            std::string keyString = (sleep_combo == 0) ? "Unbound" : getStringFromKey(sleep_combo);

            std::replace(keyString.begin(), keyString.end(), ' ', '+');

            aprints("Sleep/Wake: " + keyString, 0, 140, 2);
        }
#endif

        if (!onStates) {

            showOption("Resume");
            showOption("Restart");

            if (game->gameMode == TRAINING)
                showOption("Saves");

            showOption("Quit");

            if (key_hit(k.confirm)) {
                int n = selection;
                if (!(game->gameMode == TRAINING) && selection >= 2)
                    n++;

                if (n == 0) {
                    sfx(SFX_MENUCONFIRM);
                    clearText();
                    paused = false;
                    break;
                } else if (n == 1) {
                    addGameStats();
                    startGame();
                    changeScene(new GameScene());
                    paused = false;
                    count = false;
                    sfx(SFX_MENUCONFIRM);
                    return 0;
                } else if (n == 2) {
                    onStates = true;
                    selection = 0;
                    clearText();
                } else if (n == 3) {
                    enableBlend(prevBld);
                    sfx(SFX_MENUCANCEL);
                    buildBG(3, 0, 27, 0, 1, true);
                    addGameStats();
                    playSongRandom(0);
                    changeScene(new MainMenuScene(), Transitions::FADE);
                    return 1;
                }
            }
        } else {
            optionsCounter = 0;

            showOption("Load");
            showOption("Save");
            showOption("Back");

            for (int i = optionsCounter + 1; i < 6; i++)
                wordSprites[i].hide();

            if (key_hit(k.confirm)) {
                if (selection == 0) {
                    if (saveExists) {
                        delete game;
                        game = new Game(quickSave);
                        game->setTuning(getTuning());
                        floatingList.clear();
                        placeEffectList.clear();
                        clearGlow();

                        if (savefile->settings.colors == 3)
                            setPalette();

                        if (enableBot) {
                            delete testBot;
                            testBot = new Bot(game);
                        }

                        aprint("Loaded!",
                               22 - (savefile->settings.aspectRatio == 1), 18);
                        sfx(SFX_MENUCONFIRM);
                    } else {
                        sfx(SFX_MENUCANCEL);
                    }
                } else if (selection == 1) {
                    if (saveExists)
                        delete quickSave;
                    quickSave = new Game(game);
                    saveExists = true;

                    aprint(" Saved!",
                           22 - (savefile->settings.aspectRatio == 1), 18);
                    sfx(SFX_MENUCONFIRM);
                } else if (selection == 2) {
                    sfx(SFX_MENUCONFIRM);
                    showStats(showingStats, totalTime, ppsStr, false);
                    onStates = false;
                    selection = 0;
                }
            }
        }

        int len = wordSprites[selection + 1].width;

        cursorFloat += 6;
        if (cursorFloat >= 512)
            cursorFloat = 0;
        int offset = (sinLut(cursorFloat) * 2) >> 12;
        FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

        for (int i = 0; i < 2; i++) {
            sprite_unhide(cursorSprites[i], 0);
            sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
            sprite_enable_affine(cursorSprites[i], i, true);
            sprite_set_size(cursorSprites[i], scale, i);

            int x = 240 / 2 - ((len + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

            sprite_set_pos(cursorSprites[i], x,
                           ((optionsHeight + selection * 2) * 8) - 5);
        }

        if (key_hit(k.pause)) {
            sfx(SFX_MENUCONFIRM);
            clearText();
            // updateText();
            paused = false;
            break;
        }

        if (key_hit(k.special3)) {
#ifndef NO_DIAGNOSE
            qrScene();
#endif
            if (game->gameMode != TRAINING) {
                skipSong = true;
                sfx(SFX_MENUCONFIRM);
                if (!skipped) {
                    skipped = true;
                    std::string trackSkip = "Track Skipped!";
                    naprint(
                        trackSkip,
                        ((savefile->settings.aspectRatio) ? (240 - 13) : 240) -
                            getVariableWidth(trackSkip),
                        18 * 8);
                }
            }
        }

        if (key_hit(k.cancel)) {
            sfx(SFX_MENUCONFIRM);
            if (onStates) {
                onStates = false;
                selection = 0;
                showStats(showingStats, totalTime, ppsStr, false);
            } else {
                clearText();
                paused = false;
                break;
            }
        }

        if (key_hit(k.up)) {
            if (selection == 0)
                selection = maxSelection - 1;
            else
                selection--;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.down)) {
            if (selection == maxSelection - 1)
                selection = 0;
            else
                selection++;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.special1) && !onStates) {
            sfx(SFX_MENUCONFIRM);
            showingStats = !showingStats;
            clearSmallText();
            showStats(showingStats, totalTime, ppsStr, false);
        }

        if (!shown && !onStates) {
            shown = true;
            showStats(showingStats, totalTime, ppsStr, false);
        }

        showSprites(128);
    }

    clearTilemap(27);
    clearTilemap(26);
    buildBG(3, 0, 27, 0, 1, true);

    enableBlend(prevBld);

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i].hide();

    for (int i = 0; i < 2; i++)
        sprite_hide(cursorSprites[i]);

    drawFrame(0);
    if (game->zoneTimer)
        frameSnow(0);

    showBackground(0);
    resetSmallText();
    clearText();
    setSmallTextArea(110, 3, 7, 9, 10);
    showText();

    if (count && savefile->settings.pauseCountdown && !preventLayeredCountdowns)
        countdown();

    if (!preventLayeredCountdowns)
        resumeSong();

    return 0;
}

// dir 1 is ascending, -1 is descending
int saveRecord(EntryBoard& board, int value, int dir) {

    for (int i = 0; i < 5; i++) {
        if (value * dir > board.entries[i].value * dir &&
            !(dir == 1 && board.entries[i].value == 0))
            continue;

        for (int j = 3; j >= i; j--)
            board.entries[j + 1] = board.entries[j];

        std::string name = nameInput(i);

        board.entries[i].value = value;

        board.entries[i].pro = proMode;

        strncpy(board.entries[i].name, name.c_str(), 9);

        return i;
    }

    return -1;
}

int onRecord() {
    int place = -1;

    if (replaying || game->pawn.big)
        return place;

    int subMode = game->subMode;

    if (game->gameMode == MARATHON) {
        if (subMode == 0) {
            place =
                saveRecord(savefile->boards.marathon[mode], game->score, -1);
        } else if (subMode == 1) {
            place = saveRecord(savefile->boards.zone[mode], game->score, -1);
        } else if (subMode == 2) {
            place = saveRecord(savefile->boards.tower[mode], game->score, -1);
        }
    } else if (game->gameMode == SPRINT && game->won == 1) {
        if (subMode == 0) {
            place = saveRecord(savefile->boards.sprint[mode], gameSeconds, 1);
        } else {
            place =
                saveRecord(savefile->boards.sprintAttack[mode], gameSeconds, 1);
        }
    } else if (game->gameMode == DIG && game->won == 1) {
        if (subMode == 0) {
            place = saveRecord(savefile->boards.dig[mode], gameSeconds, 1);
        } else {
            place = saveRecord(savefile->boards.digEfficiency[mode],
                               game->pieceCounter, 1);
        }
    } else if (game->gameMode == ULTRA) {
        place = saveRecord(savefile->boards.ultra[mode], game->score, -1);

    } else if (game->gameMode == BLITZ) {
        place = saveRecord(savefile->boards.blitz[0], game->score, -1);

    } else if (game->gameMode == COMBO) {
        place =
            saveRecord(savefile->boards.combo, game->statTracker.maxCombo, -1);

    } else if (game->gameMode == SURVIVAL) {
        place = saveRecord(savefile->boards.survival[mode], gameSeconds, -1);

    } else if (game->gameMode == CLASSIC) {
        place = saveRecord(savefile->boards.classic[subMode], game->score, -1);

    } else if (game->gameMode == MASTER) {
        int grade = game->grade + game->coolCount + (int)game->creditGrade;

        if (grade > 32)
            grade = 32;

        EntryBoard& board = savefile->boards.master[subMode];

        for (int i = 0; i < 5; i++) {
            if (grade < board.entries[i].grade ||
                (grade == board.entries[i].grade &&
                 (gameSeconds > board.entries[i].value &&
                  board.entries[i].value != 0)))
                continue;

            for (int j = 3; j >= i; j--)
                board.entries[j + 1] = board.entries[j];

            std::string name = nameInput(i);

            board.entries[i].value = gameSeconds;
            board.entries[i].grade = grade;

            strncpy(board.entries[i].name, name.c_str(), 9);

            place = i;
            break;
        }
    } else if (game->gameMode == DEATH) {
        int grade = (game->level / 100) - game->regretCount;

        if (grade > 13)
            grade = 13;

        EntryBoard& board = savefile->boards.death[subMode];

        for (int i = 0; i < 5; i++) {
            if (grade < board.entries[i].grade ||
                (grade == board.entries[i].grade &&
                 (gameSeconds > board.entries[i].value &&
                  board.entries[i].value != 0)))
                continue;

            for (int j = 3; j >= i; j--)
                board.entries[j + 1] = board.entries[j];

            std::string name = nameInput(i);

            board.entries[i].value = gameSeconds;
            board.entries[i].grade = grade;

            strncpy(board.entries[i].name, name.c_str(), 9);

            place = i;
            break;
        }
    }

    if (place >= 0) {
        saveSavefile();
        framesSinceLastSave = 0;
    }

    return place;
}

void GameScene::showModeText() {
    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 227;

    if (game->gameMode > 0 && game->gameMode <= 13) {
        int counter = 1;
        std::string str;
        std::string str2;

        str = modeStrings[game->gameMode - 1];

        if (game->gameMode == TRAINING)
            str = "Training";

        naprintColor(str, width - 4 - getVariableWidth(str), counter * 8, 14);
        counter++;

        str = "";
        if (game->gameMode == TRAINING) {
            if (game->trainingMode)
                str = "Finesse";
        } else {
            if (game->gameMode == ZEN) {
                if (game->subMode == 1)
                    str = "Tower";
                else
                    str = "Normal";
            } else if (game->gameMode == BATTLE && mode >= 0)
                str = modeOptionStrings[game->gameMode - 1][mode - 1];
            else if (game->gameMode != CLASSIC && game->gameMode != MASTER)
                str = modeOptionStrings[game->gameMode - 1][mode];
            else if (game->gameMode == MASTER)
                str = modeOptionStrings[game->gameMode - 1][game->subMode];
            else
                str = modeOptionStrings[game->gameMode - 1][0];
        }

        if (str != "") {
            naprintColor(str, width - 4 - getVariableWidth(str), counter * 8,
                         14);
            counter++;
        }

        str = "";
        str2 = "";
        if (game->subMode) {
            switch (game->gameMode) {
            case MARATHON:
                if (game->subMode == 2)
                    str = "Tower";
                else
                    str = "Zone";
                break;
            case SPRINT:
                str = "Attack";
                break;
            case DIG:
                str = "Efficiency";
                break;
            case CLASSIC:
                str = "B-Type";
                str2 = std::to_string(initialLevel) + "-" +
                       std::to_string(game->bTypeHeight);
                break;
            }
        } else {
            if (game->gameMode == CLASSIC)
                str = "A-Type";
        }

        if (str != "") {
            naprintColor(str, width - 4 - getVariableWidth(str), counter * 8,
                         14);
            counter++;
        }
        if (str2 != "") {
            naprintColor(str2, width - 4 - getVariableWidth(str2), counter * 8,
                         14);
            counter++;
        }

        if (bigMode) {
            naprintColor("BIG", width - 4 - getVariableWidth("BIG"),
                         counter * 8, 14);
            counter++;
        }

        int mode = game->gameMode;

        if (game->rotationSystem != SRS) {
            if (mode != CLASSIC &&
                !((mode == MASTER || mode == DEATH) && game->subMode)) {
                std::string text;

                switch (game->rotationSystem) {
                case BlockEngine::NRS:
                    text = "NRS";
                    break;
                case BlockEngine::ARS:
                    text = "ARS";
                    break;
                case BlockEngine::BARS:
                    text = "BARS";
                    break;
                case BlockEngine::SDRS:
                    text = "SDRS";
                    break;
                default:
                    text = "SRS";
                    break;
                }

                naprintColor(text, width - 4 - getVariableWidth(text),
                             counter * 8, 14);
                counter++;
            }
        }

        if (game->randomizer != BAG_7) {
            if (mode != CLASSIC &&
                !((mode == MASTER || mode == DEATH) && game->subMode)) {
                std::string text;

                switch (game->randomizer) {
                case BlockEngine::BAG_7:
                    text = "7-BAG";
                    break;
                case BlockEngine::BAG_35:
                    text = "35-BAG";
                    break;
                default:
                    text = "RANDOM";
                    break;
                }

                naprintColor(text, width - 4 - getVariableWidth(text),
                             counter * 8, 14);
                counter++;
            }
        }
    }
}

Replay* test = nullptr;
// ReplayHeader test;

void saveReplay() {
    if (replaying)
        return;

    const int len = game->replay.size();
    if (len >= 4096) {
        log("replay size too big (" + std::to_string(len) + ")");
        return;
    }

    if (!game->replayElligible) {
        log("replay not elligible");
        return;
    }

    if (test != nullptr)
        delete test;

    test = new Replay();

    test->header.tag = savefile->newGame;
    test->header.length = len;
    test->header.duration = game->timer;
    test->header.seed = game->initSeed;

    test->header.options = *previousGameOptions;

    auto it = game->replay.begin();
    for (int i = 0; i < len; i++) {
        Timestamp move = *it;

        test->moves[i] = (((move.timer & 0x3fffff) << 11) +
                          ((move.dir & 1) << 9) + (move.move & 0x1ff));

        ++it;
    }

#ifdef PC
#ifdef __APPLE__
    std::string filePath = savefileDir() + "/replay.sav";
    std::ofstream output(filePath, std::ios::binary | std::ios::out);
#else
    std::ofstream output("replay.sav", std::ios::binary | std::ios::out);
#endif

    char* src = (char*)test;
    output.write(src, sizeof(Replay));

    if (!output) {
        log("Error when trying to write replay.");
        return;
    } else {
        log("Wrote save to file.");
    }

    output.close();
#endif
}

ReplayHeader* currentReplayHeader = nullptr;
std::list<Timestamp*> currentReplay;

void loadReplay() {
    for (auto& item : currentReplay)
        delete item;

    currentReplay.clear();

    if (test != nullptr) {
        if (currentReplayHeader == nullptr)
            currentReplayHeader = new ReplayHeader();

        memcpy32_fast(currentReplayHeader, &test->header,
                      sizeof(ReplayHeader) / 4);

        for (int i = 0; i < (int)test->header.length; i++) {
            currentReplay.push_back(new Timestamp(test->moves[i]));
        }

        replaying = true;
        replayIterator = currentReplay.begin();
        startGame(currentReplayHeader->options, currentReplayHeader->seed);

        gameLoop();
        return;
    }

#ifdef PC

#ifdef __APPLE__
    std::string filePath = savefileDir() + "/replay.sav";
    std::ifstream input(filePath, std::ios::binary | std::ios::in);
#else
    std::ifstream input("replay.sav", std::ios::binary | std::ios::in);
#endif

    Replay rep;
    char* dst = (char*)&rep;

    log(std::to_string(sizeof(Replay)));

    input.read(dst, sizeof(Replay));

    input.close();

    if (!input) {
        log("Error when trying to load replay.");
        return;
    } else {
        log("Found replay file.");
    }

    if (rep.header.tag != savefile->newGame) {
        log("Invalid replay (old version, tag: " +
            std::to_string(rep.header.tag) + " ).");
        return;
    } else {
        log("Correct version number.");
    }

    if (rep.header.length > 4096) {
        log("Invalid replay (too large).");
        return;
    }

    log("Attemping to load moves...");
    for (int i = 0; i < (int)rep.header.length; i++) {
        currentReplay.push_back(new Timestamp(rep.moves[i]));
    }
    log("Loaded moves.");

    if (currentReplayHeader == nullptr)
        currentReplayHeader = new ReplayHeader();

    memcpy32_fast(currentReplayHeader, &rep.header, sizeof(ReplayHeader) / 4);

    replaying = true;
    replayIterator = currentReplay.begin();

    log("trying to start game...");
    startGame(currentReplayHeader->options, currentReplayHeader->seed);

    gameLoop();

#endif
}

void qrScene() {

    const std::string base = "https://knewjade.github.io/fumen-for-mobile/#?d=";

    const std::string fumen = game->getFumen();

    log(base + fumen);

    // const std::string link = base + fumen;

    // enum qrcodegen_Ecc errCorLvl = qrcodegen_Ecc_LOW;  // Error correction
    // level

    // const char *text = link.c_str();

    // // Make and print the QR Code symbol
    // uint8_t *qrcode = new uint8_t[qrcodegen_BUFFER_LEN_MAX];
    // uint8_t *tempBuffer = new uint8_t[qrcodegen_BUFFER_LEN_MAX];
    // bool ok = qrcodegen_encodeText(text, tempBuffer, qrcode, errCorLvl,
    //     qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO,
    //     true);

    // if(ok){
    //     int size = qrcodegen_getSize(qrcode);

    //     log(size);
    //     int border = 4;

    //     clearSprites(128);

    //     OBJ_ATTR * qr = &obj_buffer[0];
    //     const int qrTile = 300;

    //     clearSpriteTiles(qrTile, 8, 8);

    //     sprite_unhide(qr,0);
    //     sprite_set_attr(qr, ShapeSquare, 3, qrTile, 0, 0);
    //     sprite_enable_affine(qr, 8, true);
    //     sprite_set_size(qr, 1 << 7, 8);
    //     sprite_set_pos(qr,120 - (size + 4), 10);

    //     for (int y = -border; y < size + border; y++) {
    //         for (int x = -border; x < size + border; x++) {
    //             int color = (qrcodegen_getModule(qrcode, x, y) ? 6 : 7);

    //             int i = y + border;
    //             int j = x + border;
    //             setSpritePixel(qrTile, j / 8, i / 8, 8, j % 8, i % 8, color);
    //         }
    //     }

    //     showSprites(128);

    //     while(closed()){
    //         vsync();
    //         key_poll();

    //         if(key_hit(KEY_FULL)){

    //             break;
    //         }
    //     }
    // }

    // delete [] qrcode;
    // delete [] tempBuffer;
}

void addGameStats() {
    savefile->stats.timePlayed += frameCounter;
    frameCounter = 1;

    savefile->stats.gameStats.add(game->statTracker);
    savefile->stats.totalLines += game->linesCleared;

    if (game->gameMode == MARATHON && game->level > savefile->stats.maxLevel)
        savefile->stats.maxLevel = game->level;

    if (game->gameMode == ZEN) {
        if (game->subMode == 1) {
            int height =
                game->towerScrollCount + (game->lengthY - game->stackHeight);

            if (height > (int)savefile->boards.zenTower)
                savefile->boards.zenTower = height;
        } else {
            savefile->boards.zen = game->score;
            savefile->boards.zenLines = savefile->boards.zenLines + game->linesCleared;
        }

        saveSavefile();
    }
}

int GameScene::endScreenSetup() {
    clearSmallText();

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i].hide();

    if (!ended)
        endAnimation();

    enableBlend((1 << 6) + (0b11101 << 9) + (1 << 3));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));
    buildBG(3, 0, 27, 0, 0, true);
    clearTilemap(25);

    drawFrame(1);

    resetZonePalette();

    showScore();

    if (savefile->settings.skin >= 1000 && previousSettings != nullptr) {
        savefile->settings.skin = previousSettings->skin;
        savefile->settings.edges = previousSettings->edges;
        setSkin();
    }

    if (!replaying) {
        savefile->stats.gamesCompleted++;
        if (game->lost)
            savefile->stats.gamesLost++;

        addGameStats();
    }

    int record = -1;
    if (multiplayer) {
        progressBar(1);
    } else {
        record = onRecord();
    }

    setSmallTextArea(220, 1 + (savefile->settings.aspectRatio != 0), 1, 10, 20);

    const int optionsHeight = 11;

    int counter = 0;

    wordSprites[1].setText("Play");
    wordSprites[1].show(12 * 8, (optionsHeight + counter) * 8, 14);
    wordSprites[2].setText("Again");
    wordSprites[2].show(14 * 8, (optionsHeight + counter + 1) * 8, 14);

    counter += 3;

    if (!multiplayer) {
        wordSprites[3].setText("Change");
        wordSprites[3].show(12 * 8, (optionsHeight + counter) * 8, 14);
        wordSprites[4].setText("Options");
        wordSprites[4].show(14 * 8, (optionsHeight + counter + 1) * 8, 14);

        counter += 3;
    } else {
        showReadyPlayers();
    }

    wordSprites[5].setText("Main");
    wordSprites[5].show(12 * 8, (optionsHeight + counter) * 8, 14);
    wordSprites[6].setText("Menu");
    wordSprites[6].show(14 * 8, (optionsHeight + counter + 1) * 8, 14);

    showModeText();
    return record;
}

void GameScene::showReadyPlayers() {
    int readyPlayers = playAgainMutex->getReadyCount();

    std::string readyText =
        std::to_string(readyPlayers) + "/" + std::to_string(initialPlayerCount);

    wordSprites[8].setText(readyText);
    wordSprites[8].show(236 - wordSprites[8].width, 160 - 12 - 8, 14);

    if (playAgainMutex->checkCurrentReady()) {
        wordSprites[7].setText("Ready!");
        wordSprites[7].show(236 - wordSprites[7].width, 160 - 12, 15);
    } else {
        wordSprites[7].setText("Not Ready");
        wordSprites[7].show(236 - wordSprites[7].width, 160 - 12, 15);
    }
}

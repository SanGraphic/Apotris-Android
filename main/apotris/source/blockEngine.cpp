#include <algorithm>
#include <iostream>
#include <math.h>
#include <string>
#include <tuple>

#include "blockEngine.hpp"
#include "logging.h"
#include "rumblePatterns.hpp"
#include "tetromino.hpp"
#include <posprintf.h>

using namespace BlockEngine;
using namespace GameInfo;

bool BlockEngine::enableFumen = false;
bool BlockEngine::enableFumenQueue = false;

u8 BlockEngine::fumenQueue[5] = {PIECE_T, 0, 0, 0, 0};

std::string BlockEngine::fumenString =
    "v115@neC8GeA8AeC8EeA8CeE8AeI8AeI8AeI8AeI8AeI8Ae?"
    "I8AeI8AeI8AeI8AeI8AeI8AeI8AeI8AeI8AeI8AeI8KeAgH";

int Game::checkRotation(int dx, int dy, int r) {
    const int x = dx + pawn.x;
    const int y = dy + pawn.y;

    int total = 0;
    for (int j = 0; j < 4; j++) {
        for (int i = pawn.boardLowest[r][j]; i >= 0; i--) {
            if (pawn.board[r][i][j] == 0)
                continue;

            if (x + j < 0 || x + j > lengthX - 1 || y + i < 0 ||
                y + i > lengthY - 1 || board[i + y][j + x] != 0)
                return 0;

            if (++total >= 4)
                return 1;
        }
    }

    return 1;
}

int Game::checkSpecialRotation(int dx, int r) {
    int x = dx + pawn.x;

    int total = 0;
    for (int i = 0; i < 4; i++) {
        if (total >= 4)
            break;

        for (int j = 0; j < 4; j++) {
            if (pawn.board[r][i][j] == 0)
                continue;

            total++;

            if (x + j < 0 || x + j > lengthX - 1 || pawn.y + i < 0 ||
                pawn.y + i > lengthY - 1 || board[i + pawn.y][j + x] != 0)
                return i * 4 + j + 1;

            if (total >= 4)
                break;
        }
    }

    return 0;
}

void Pawn::setBlock(int system) {
    memset32_fast(boardLowest, -1, 16);
    memset32_fast(heighest, -1, 4);
    memset32_fast(lowestBlock, 0, 4);

    for (int r = 0; r < 4; r++) {
        int* shape = getShape(current, r, system);

        for (int iy = 0; iy < 4; iy++) {
            for (int ix = 0; ix < 4; ix++) {
                board[r][iy][ix] = shape[iy * 4 + ix];

                if (shape[iy * 4 + ix]) {
                    if (boardLowest[r][ix] < iy)
                        boardLowest[r][ix] = iy;

                    if (lowestBlock[r] < iy)
                        lowestBlock[r] = iy;

                    if (heighest[r] == -1)
                        heighest[r] = iy;
                }
            }
        }

        delete[] shape;
    }
}

int Game::getNbr(int x, int y, int ex, int sy, int ey) {
    int count = 0;

    if (y - 1 >= sy && board[y - 1][x])
        count++;
    if (y + 1 <= ey - 1 && board[y + 1][x])
        count++;
    if (x - 1 >= 0 && board[y][x - 1])
        count++;
    if (x + 1 <= ex - 1 && board[y][x + 1])
        count++;

    if (y - 1 >= sy && board[y - 1][x] && x - 1 >= 0 && board[y][x - 1] &&
        board[y - 1][x - 1])
        count++;
    if (y - 1 >= sy && board[y - 1][x] && x + 1 <= ex && board[y][x + 1] &&
        board[y - 1][x + 1])
        count++;
    if (y + 1 <= ey && board[y + 1][x] && x - 1 >= 0 && board[y][x - 1] &&
        board[y + 1][x - 1])
        count++;
    if (y + 1 <= ey && board[y + 1][x] && x + 1 <= ex && board[y][x + 1] &&
        board[y + 1][x + 1])
        count++;

    return count;
}

void Game::rotateCW(int dir) {
    rotates[0] = dir;

    replay.emplace_back(timer + eventTimer, 2, dir);

    rotate(1, dir);
}

void Game::rotateCCW(int dir) {
    rotates[1] = dir;

    replay.emplace_back(timer + eventTimer, 3, dir);

    rotate(-1, dir);
}

void Game::rotateTwice(int dir) {
    if (rotationSystem == NRS)
        return;

    rotates[2] = dir;

    replay.emplace_back(timer + eventTimer, 4, dir);

    rotate(2, dir);
}

void Game::rotate(int dir, int on) {

    if (!on)
        return;

    if ((clearLock || entryDelay)) {
        fromLockRotate = true;
        return;
    } else if (eventLock) {
        return;
    }

    if (rotationSystem == ARS && dir == 2)
        dir = -1;

    int dx = 0;
    int dy = 0;
    int r = pawn.rotation + dir;
    if (r > 3)
        r -= 4;
    else if (r < 0)
        r += 4;

    if (rotationSystem == SRS) {
        for (int i = 0; i < 5 + (dir > 1); i++) {
            int dx;
            int dy;
            if (dir <= 1) {
                dx = kicks[(pawn.current == PIECE_I)][(dir < 0)][pawn.rotation]
                          [i][0];
                dy = kicks[(pawn.current == PIECE_I)][(dir < 0)][pawn.rotation]
                          [i][1];
            } else {
                dx = kickTwice[pawn.rotation][i][0];
                dy = kickTwice[pawn.rotation][i][1];
            }

            if (checkRotation(dx, dy, r)) {
                specialTspin =
                    (pawn.current == PIECE_T && (abs(dx) + abs(dy) >= 2));
                lastMoveRotation = dir;

                if (dy < 0)
                    verticalKick++;

                if (verticalKick >= verticalKickMax)
                    hardDrop();

                rotatePlace(dir, dx, dy, r);
                return;

            } else {
                sounds.invalid = 1;
            }
        }

    } else if (rotationSystem == BARS) {
        for (int i = 0; i < 7 + (dir > 1); i++) {
            int dx;
            int dy;
            if (dir <= 1) {
                dx = barsKicks[(pawn.current == 0)][(dir < 0)][pawn.rotation][i]
                              [0];
                dy = barsKicks[(pawn.current == 0)][(dir < 0)][pawn.rotation][i]
                              [1];
            } else {
                dx = barsKickTwice[pawn.rotation][i][0];
                dy = barsKickTwice[pawn.rotation][i][1];
            }

            if (checkRotation(dx, dy, r)) {
                specialTspin = (pawn.current == 5 && (abs(dx) + abs(dy) >= 2));
                lastMoveRotation = dir;

                if (dy < 0)
                    verticalKick++;

                if (verticalKick >= verticalKickMax)
                    hardDrop();

                rotatePlace(dir, dx, dy, r);
                return;

            } else {
                sounds.invalid = 1;
            }
        }

    } else if (rotationSystem == SDRS) {
        for (int i = 0; i < 10 + (dir > 1); i++) {
            int dx;
            int dy;
            if (dir <= 1) {
                dx = sdrsKicks[(pawn.current == 0)][(dir < 0)][pawn.rotation][i]
                              [0];
                dy = sdrsKicks[(pawn.current == 0)][(dir < 0)][pawn.rotation][i]
                              [1];
            } else {
                // SDRS uses BARS' 180 kicks because they're identical
                dx = barsKickTwice[pawn.rotation][i][0];
                dy = barsKickTwice[pawn.rotation][i][1];
            }

            if (checkRotation(dx, dy, r)) {
                specialTspin = (pawn.current == 5 && (abs(dx) + abs(dy) >= 2));
                lastMoveRotation = dir;

                if (dy < 0)
                    verticalKick++;

                if (verticalKick >= verticalKickMax)
                    hardDrop();

                rotatePlace(dir, dx, dy, r);
                return;

            } else {
                sounds.invalid = 1;
            }
        }

    } else if (rotationSystem == NRS) {
        if (checkRotation(0, 0, r)) {
            rotatePlace(dir, dx, dy, r);
            return;
        } else
            sounds.invalid = 1;

    } else if (rotationSystem == ARS) {

        if ((pawn.current == PIECE_J || pawn.current == PIECE_L ||
             pawn.current == PIECE_T) &&
            (pawn.rotation % 2 == 0)) {
            int n = checkSpecialRotation(0, r);
            if ((n - 1) % 4 == 1)
                return;
        } else if (pawn.current == PIECE_I) {
            if (checkRotation(0, 0, r)) {
                rotatePlace(dir, dx, dy, r);
                return;
            } else {
                int touching = checkITouching(r, (pawn.rotation % 2 == 0));
                if (pawn.rotation % 2 == 1) {
                    if (touching == 1)
                        dx = 2;
                    else
                        dx = -1;
                } else {
                    if (touching)
                        dy = -2;
                }

                if (checkRotation(dx, dy, r)) {
                    rotatePlace(dir, dx, dy, r);
                    return;
                } else
                    sounds.invalid = 1;
            }
            return;
        }

        bool TFloorKick = (pawn.current == PIECE_T && r == 2);

        for (int i = 0; i < 3 + TFloorKick; i++) {
            dx = 0;
            dy = 0;

            if (i == 1)
                dx = 1;
            else if (i == 2)
                dx = -1;
            else if (i == 3)
                dy = -1;

            if (checkRotation(dx, dy, r)) {
                rotatePlace(dir, dx, dy, r);

                if ((pawn.current == PIECE_I || pawn.current == PIECE_T) && dy)
                    stopLockReset = true;

                return;
            } else
                sounds.invalid = 1;

            if (dy)
                break;
        }
    }
}

void Game::rotatePlace(int dir, int dx, int dy, int r) {
    pawn.rotation = r;

    lockCheck();
    pawn.x += dx;
    pawn.y += dy;

    sounds.rotate = 1;

    switch (dir) {
    case -1:
        moveHistory.push_front(5);
        break;
    case 1:
        moveHistory.push_front(4);
        break;
    case 2:
        moveHistory.push_front(6);
        break;
    }

    lastMoveDx = dx;
    lastMoveDy = dy;
}

void Game::setShiftSoundFlag() {
    if (das != maxDas)
        sounds.shift = 1;
    else
        sounds.shiftDas = 1;
}

bool Game::moveLeft() {
    if (das != maxDas)
        moveHistory.push_back(0);
    else if (!(moveHistory.size() > 0 && moveHistory.back() == 2)) {
        if (moveHistory.size() > 0 && moveHistory.back() == 0)
            moveHistory.pop_back();

        moveHistory.push_back(2);
    }

    if (checkRotation(-1, 0, pawn.rotation)) {
        pawn.x--;
        setShiftSoundFlag();
        lastMoveRotation = 0;
        lockCheck();
        return true;
    } else {
        pushDir = -1;
        if (gameMode == CLASSIC) {
            das = maxDas;
            arrCounter = 0;
        }
        return false;
    }
}

bool Game::moveRight() {
    if (das != maxDas)
        moveHistory.push_back(1);
    else if (!(moveHistory.size() > 0 && moveHistory.back() == 3)) {
        if (moveHistory.size() > 0 && moveHistory.back() == 1)
            moveHistory.pop_back();

        moveHistory.push_back(3);
    }

    if (checkRotation(1, 0, pawn.rotation)) {
        pawn.x++;
        setShiftSoundFlag();
        lastMoveRotation = 0;
        lockCheck();
        return true;
    } else {
        pushDir = 1;
        if (gameMode == CLASSIC) {
            das = maxDas;
            arrCounter = 0;
        }
        return false;
    }
}

void Game::moveDown() {
    if (checkRotation(0, 1, pawn.rotation)) {
        pawn.y++;

        if (rotationSystem == ARS && !stopLockReset)
            lockTimer = maxLockTimer;

        if (gameMode != CLASSIC)
            setShiftSoundFlag();
        lastMoveRotation = 0;
    } else if (rotationSystem == NRS || rotationSystem == ARS) {
        place();
        down = 0;
    }
}

void Game::hardDrop() {
    if (clearLock || eventLock || dropLockTimer || entryDelay)
        return;

    int diff = pawn.lowest - pawn.y;

    if (diff)
        lastMoveRotation = 0;

    int offset = 0;
    for (int i = 0; i < 4; i++) {
        if (pawn.boardLowest[pawn.rotation][i] != -1) {
            offset = i;
            break;
        }
    }

    std::list<int> bestFinesse =
        getBestFinesse(pawn.current, pawn.x + offset, pawn.rotation);

    previousBest.clear();
    for (auto item : bestFinesse) {
        previousBest.push_back(item);
    }

    int difference = 0;
    bool correct = false;
    if (moveHistory.size() > (int)bestFinesse.size() - 1) {
        for (const auto& move : bestFinesse) {
            if (move == 6 && moveHistory.size() == (int)bestFinesse.size()) {
                correct = true;
                break;
            }
        }
        if (!correct) {
            difference = moveHistory.size() - (bestFinesse.size() - 1);
        }
    } else {
        correct = true;
    }

    if (!correct && !softDrop) {
        finesseFaults += difference;
        finesseStreak = 0;

        if (trainingMode) {
            lockTimer = maxLockTimer;
            lockMoveCounter = 15;
            verticalKick = 0;
            sounds.finesse = 1;
            refresh = 1;
            dropLockTimer = dropLockMax;
            softDrop = false;

            pushDir = 0;

            moveHistory.clear();
            previousKey = 0;

            pawn.y = (int)lengthY / 2;
            pawn.x = (int)lengthX / 2 - 2;
            pawn.rotation = 0;

            return;
        }
    } else {
        finesseStreak++;
    }

    pawn.y = pawn.lowest;

    if (!sonicDrop)
        place();

    score += diff * 2;

    return;
}

void Game::update() {

    if (!disappearing)
        timer++;
    else
        eventTimer++;

    if (dropLockTimer)
        dropLockTimer--;

    if (zoneTimer > 1) {
        zoneTimer--;
    }

    if (clearLock || lost)
        return;

    if (!disappearing)
        inGameTimer++;

    if (entryDelay) {
        entryDelay--;
    }

    if (pawn.current == -1 && !entryDelay) {
        next();
    }

    updateDas();

    if (entryDelay) {
        if (gameMode == DEATH) {
            int l = level / 100;

            if (l >= 5 && l < 10 &&
                deathGarbageCounter >= deathQuota[min(4, l - 5)]) {
                deathGarbage();
                deathGarbageCounter = 0;
            }
        }

        return;
    }

    if (das == maxDas && !(left && right)) {
        if (arr >= 0 && --arrCounter <= 0 && !(gameMode == CLASSIC && down)) {
            bool moved = false;

            for (int i = 0; i < 1 + (arr == 0);
                 i++) { // move piece twice if arr is 0
                if (left)
                    moved = moveLeft();
                else if (right)
                    moved = moveRight();

                if (speed == 20 && gameMode != BLITZ) {
                    int counter = 0;
                    while (counter++ < 20 && checkRotation(0, 1, pawn.rotation))
                        pawn.y++;
                }
            }

            if (!(gameMode == CLASSIC && !moved))
                arrCounter = arr;
        } else if (arr < 0) {
            bool moved = true;
            while (moved) {
                if (left)
                    moved = moveLeft();
                else if (right)
                    moved = moveRight();

                if (speed == 20 && gameMode != BLITZ) {
                    int counter = 0;
                    while (counter++ < 20 && checkRotation(0, 1, pawn.rotation))
                        pawn.y++;
                }
            }
        }
    }

    if (gameMode == BATTLE) {
        auto index = garbageQueue.begin();

        int count = 0;
        while (index != garbageQueue.end()) {
            if (index->timer > count++) {
                index->timer--;
            }
            ++index;
        }
    } else if (gameMode == SURVIVAL) {
        int rate = 60 * (4 - goal);
        if (timer % rate == 0) {
            generateGarbage(1, 0);
            refresh = 1;

            while (!checkRotation(0, 0, pawn.rotation)) {
                if (!checkRotation(0, -1, pawn.rotation))
                    lost = 1;
                else
                    pawn.y--;
            }
        }
    } else if (gameMode == MASTER && !disappearing) {
        if (decayTimer && comboCounter < 2) {
            decayTimer--;
        } else {
            decayTimer = gradeTable[internalGrade][1];
            if (gradePoints)
                gradePoints--;
        }
    }

    if ((linesCleared >= goal && gameMode == SPRINT && goal && subMode == 0) ||
        (linesSent >= goal && gameMode == SPRINT && goal && subMode == 1) ||
        (goal && linesCleared >= goal && gameMode == MARATHON) ||
        (garbageCleared >= goal && gameMode == DIG) ||
        (timer > goal && (gameMode == ULTRA || gameMode == BLITZ)) ||
        (linesCleared >= goal && gameMode == CLASSIC && goal)) {
        won = 1;
        if (gameMode == CLASSIC && goal) {
            score += (level + bTypeHeight) * 1000;
        }
    } else if ((gameMode == MASTER || gameMode == DEATH) && disappearing &&
               (eventTimer) >= CREDITS_LENGTH) {
        won = 1;

        creditGrade += creditPoints[4][disappearing - 1];
    }

    if (gracePeriod) {
        gracePeriod--;
    } else if (!zoneTimer)
        speedCounter += speed;

    int n = (int)speedCounter;
    for (int i = 0; i < n; i++) {
        if (checkRotation(0, 1, pawn.rotation)) {
            pawn.y++;
            if (rotationSystem == ARS)
                lockTimer = maxLockTimer;

            lastMoveRotation = false;
        } else if (rotationSystem == NRS) {
            place();
        }
    }

    speedCounter -= n;

    pawn.lowest = lowest();

    bool placed = false;

    if (dropping) {
        hardDrop();
        dropping = false;
        return;
    } else {
        if (pawn.lowest <= pawn.y && !zoneTimer && speed)
            lockTimer--;

        if (lockTimer == 0 && rotationSystem != NRS) {
            place();
            placed = true;
        }
    }

    if (down) {
        if (rotationSystem == NRS || gameMode == MASTER || gameMode == DEATH)
            softDropCounter = maxDas;
        else if (softDropCounter < maxDas)
            softDropCounter++;
    } else
        softDropCounter = 0;

    if (((!delaySoftDrop && down) || softDropCounter == maxDas) && !placed) {
        if (softDropSpeed >= 0 && ++softDropRepeatTimer >= softDropSpeed) {
            int n = 1 + (softDropSpeed ==
                         0); // move piece twice if softDropSpeed is 0

            for (int i = 0; i < n; i++) {
                moveDown();
                if (pawn.y != pawn.lowest)
                    score++;
            }
            softDropRepeatTimer = 0;
        } else if (softDropSpeed < 0) {
            int n = pawn.lowest - pawn.y;

            pawn.y = pawn.lowest;
            score += n;
        }
    }

    if (disappearing)
        updateDisappear();

    while ((int)moveHistory.size() > 10)
        moveHistory.pop_front();

    if (!replayElligible || replay.size() > MAX_REPLAY_SIZE) {
        replay.clear();

        replayElligible = false;
    }

    calculatePeek();
}

int Game::lowest() {
    int max = 0;

    if (stackHeight - 4 >= pawn.y) {
        for (int x = 0; x < 4; x++) {
            if (pawn.boardLowest[pawn.rotation][x] == -1)
                continue;

            int n =
                pawn.boardLowest[pawn.rotation][x] + columnHeights[pawn.x + x];
            if (n > max)
                max = n;
        }

        return lengthY - (max)-1;
    } else {
        int start = pawn.y;

        for (int i = start; i < lengthY; i++) {
            for (int j = 0; j < 4; j++) {
                if (pawn.boardLowest[pawn.rotation][j] == -1)
                    continue;
                int x = pawn.x + j;
                int y = i + pawn.boardLowest[pawn.rotation][j];
                if (y >= lengthY || board[y][x])
                    return i - 1;
            }
        }
    }

    return pawn.y;
}

void Game::place() {
    if (pawn.current == -1)
        return;

    lastDrop = calculateDrop();

    for (int j = 0; j < 4; j++) {
        for (int i = pawn.boardLowest[pawn.rotation][j]; i >= 0; i--) {
            if (pawn.board[pawn.rotation][i][j] == 0)
                continue;

            int x = pawn.x + j;
            int y = pawn.y + i;

            if (y > lengthY - 1 || x > lengthX)
                continue;

            board[y][x] = pawn.current + pawn.board[pawn.rotation][i][j];

            bitboard[y] |= 1 << x;
            if ((lengthY - y) > columnHeights[x])
                columnHeights[x] = lengthY - y;

            if (disappearing == 1)
                disappearTimers[y][x] = MAX_DISAPPEAR;
            else if (disappearing == 2)
                disappearTimers[y][x] = 1;
        }
    }

    // Zen Tower Hole Check
    if (gameMode == ZEN && subMode == 1) {
        const int holeY = checkIntegrity();

        if (holeY != -1) {
            const int linesToRemove = lengthY - holeY;
            for (int y = lengthY - 1; y >= stackHeight; y--) {
                if (y >= linesToRemove) {
                    const int srcY = y - linesToRemove;

                    memcpy32_fast(board[y], board[srcY], 10);

                    if (disappearing) {
                        memcpy32_fast(disappearTimers[y], disappearTimers[srcY],
                                      10);
                    }

                    bitboard[y] = bitboard[srcY];
                } else {
                    memset32_fast(board[y], 0, 10);

                    if (disappearing)
                        memset32_fast(disappearTimers[y], 0, 10);

                    bitboard[y] = 0;
                }
            }

            for (int x = 0; x < lengthX; x++) {
                columnHeights[x] = max(columnHeights[x] - linesToRemove, 0);
            }

            stackHeight += linesToRemove;
        }
    }

    toEndZone = false;
    if ((pawn.y + pawn.lowestBlock[pawn.rotation]) <= (lengthY / 2 - 1)) {
        if (zoneTimer)
            toEndZone = true;
        else if (gameMode == TRAINING)
            clearBoard();
        // Tower: wait for the scroll before game over
        else if (!((gameMode == MARATHON && subMode == 2) ||
                   (gameMode == ZEN && subMode == 1))) {
            lost = 1;
            return;
        }
    }

    int prevLevel = level;

    if (clear(lastDrop)) {
        if (rotationSystem != NRS)
            comboCounter++;
        else
            comboCounter = 0;

        if (gameMode == MASTER || gameMode == DEATH)
            entryDelay = lineAre;

    } else {
        if (gameMode == COMBO && comboCounter > -1)
            lost = 1;

        comboCounter = -1;

        int targetHeight = 9;
        if (pawn.big)
            targetHeight = 4;

        if (gameMode == DIG && garbageHeight < targetHeight) {
            int toAdd = targetHeight - garbageHeight;
            if (garbageCleared + toAdd + garbageHeight <= goal)
                generateGarbage(toAdd, 0);
            else if (garbageCleared + toAdd + garbageHeight > goal &&
                     garbageCleared + garbageHeight < goal)
                generateGarbage(goal - (garbageCleared + garbageHeight), 0);
        } else if (gameMode == BATTLE) {
            int sum = 0;
            std::vector<Garbage> collectedGarbage;
            auto index = garbageQueue.begin();
            while (index != garbageQueue.end()) {
                if (index->timer == 0) {
                    sum += index->amount;
                    collectedGarbage.emplace_back(index->id, index->amount);
                    garbageQueue.erase(index++);
                } else {
                    ++index;
                }
            }
            generateGarbage(sum, 1);

            for (auto& garbage : collectedGarbage)
                acknowledgeGarbage(this, garbage);
        }

        if (gameMode == MASTER || gameMode == DEATH)
            entryDelay = areMax;
    }

    if (comboCounter > statTracker.maxCombo)
        statTracker.maxCombo = comboCounter;

    if (gameMode == CLASSIC) {
        int add = 0;
        while (pawn.y + add < lengthY - 3)
            add += 4;

        entryDelay = 10 + add;
    }

    if (rotationSystem == NRS) {
        down = 0;
    } else if (gameMode == MASTER) {
        if (level % 100 < 99 && level < 998)
            level++;

        if (level % 100 >= 70 && prevLevel % 100 < 70) {
            if (sectionTimeGoal[(level / 100)][0] > timer - sectionStart &&
                (timer - sectionStart < previousSectionTime + 120)) {
                cool = true;
                previousSectionTime = timer - sectionStart;
            } else {
                previousSectionTime = 1000000;
            }
        } else if (cool && level % 100 >= 82 && prevLevel % 100 < 82) {
            sounds.section = 1;
        } else if ((level % 100 == 99 && prevLevel % 100 < 99 &&
                    level != 999) ||
                   (level == 998 && prevLevel != 998)) {
            sounds.section = 2;
        }
    } else if (gameMode == DEATH) {
        if (level % 100 < 99 && level < 1300)
            level++;

        if ((level % 100 == 99 && prevLevel % 100 < 99 && level != 999) ||
            (level == 998 && prevLevel != 998)) {
            sounds.section = 2;
        }
    }

    if (entryDelay) {
        pawn.current = -1;
        activePiece = -1;
    }

    canHold = true;

    // if (!clearLock && !entryDelay && !(zoneTimer == 1 && comboCounter > 0))
    if (!clearLock && !entryDelay)
        next();

    lockTimer = maxLockTimer;
    lockMoveCounter = 15;
    verticalKick = 0;
    dropLockTimer = dropLockMax;
    stopLockReset = false;

    sounds.place = 1;
    refresh = 1;

    pushDir = 0;
    previousKey = 0;
    pieceCounter++;

    moveHistory.clear();

    if (gameMode == MARATHON && subMode == 2 && goal > 0 &&
        (towerScrollCount + (lengthY - stackHeight)) >= goal) {
        setWin();
    }

    if (zoneTimer == 1 || toEndZone)
        endZone();

    if ((maxClearDelay == 1 || gameMode == SURVIVAL) && clearLock) {
        removeClearLock();
    }

    checkSecretGrade();
}

int Game::clear(const Drop& drop) {
    int clearCount = 0;
    int attack = 0;
    int isTSpin = 0;
    int isPerfectClear = 1;
    int isBackToBack = 0;
    int isDifficult = 0;

    // check for t-spin
    if (pawn.current == PIECE_T && lastMoveRotation && gameMode != CLASSIC) {
        int frontCount = 0;
        int backCount = 0;
        int x = pawn.x;
        int y = pawn.y;
        int offset = 2;

        switch (pawn.rotation) {
        case 0:
            frontCount += (board[y][x] != 0) + (board[y][x + offset] != 0);
            if (y + offset > lengthY - 1)
                backCount = 2;
            else
                backCount += (board[y + offset][x] != 0) +
                             (board[y + offset][x + offset] != 0);
            break;
        case 1:
            frontCount += (board[y][x + offset] != 0) +
                          (board[y + offset][x + offset] != 0);
            if (x < 0)
                backCount = 2;
            else
                backCount += (board[y][x] != 0) + (board[y + offset][x] != 0);
            break;
        case 2:
            frontCount += (board[y + offset][x] != 0) +
                          (board[y + offset][x + offset] != 0);
            backCount += (board[y][x] != 0) + (board[y][x + offset]);
            break;
        case 3:
            frontCount += (board[y][x] != 0) + (board[y + offset][x] != 0);
            if (x > lengthX - 1)
                backCount = 2;
            else
                backCount += (board[y][x + offset] != 0) +
                             (board[y + offset][x + offset] != 0);
            break;
        }
        if ((frontCount == 2 && backCount > 0) ||
            (frontCount > 0 && backCount == 2 && specialTspin))
            isTSpin = 2;
        else if (frontCount > 0 && backCount == 2)
            isTSpin = 1;
    }

    // check for lines to clear
    int garbageToRemove = 0;
    int zonedBefore = zonedLines;

    std::list<int> linesToMove;

    int start = lastDrop.startY + boardOffset;
    int end = lastDrop.rawEndY + 1;

    if (zonedLines && !zoneTimer) {
        start = 0;
        end = lengthY;
    }

    for (int i = start; i < end; i++) {

        bool toClear = (bitboard[i] == fullLine);

        if (toClear) {
            if ((gameMode == MARATHON && subMode == 2) ||
                (gameMode == ZEN && subMode == 1)) {
                // In tower mode, the line doesn't
                // disappear but is still counted for level progression
                linesCleared++;
                score += 100 * level; // bonus

                if (gameMode != ZEN && linesCleared % 10 == 0) {
                    level++;
                    sounds.levelUp = 1;
                    setSpeed();
                }
            } else if (!zoneTimer) {
                linesToClear.push_back(i);
                lineClearArray[i] = true;
                clearCount++;
                if (gameMode == DIG && i >= lengthY - (garbageHeight))
                    garbageToRemove++;
            } else {
                linesToMove.push_back(i);
            }
        }
    }

    // Tower
    if ((gameMode == MARATHON && subMode == 2) ||
        (gameMode == ZEN && subMode == 1)) {
        if (gameMode == MARATHON && checkIntegrity() != -1) {
            lost = 1;
            sounds.invalid = 1;
            return 0;
        } else {
            const int limit = (lengthY / 2) + 4;

            if (stackHeight <= limit && !towerClearingPhase &&
                bitboard[lengthY - 1] == fullLine) {
                towerClearingPhase = true;
                sounds.towerScroll = 1;
            }

            if (towerClearingPhase) {
                if (bitboard[lengthY - 1] == fullLine) {
                    scrollTower();
                } else {
                    towerClearingPhase = false;
                }
            }
        }
    }

    if (!linesToMove.empty()) {

        fixConnected(linesToMove);

        int offset = 0;
        for (auto const& line : linesToMove) {
            int i = line - offset;
            if (i >= lengthY - zonedLines)
                break;

            for (int ii = i; ii < lengthY - 1 - zonedLines; ii++) {
                for (int j = 0; j < lengthX; j++) {
                    board[ii][j] = board[ii + 1][j];
                }
                bitboard[ii] = bitboard[ii + 1];
            }

            zonedLines++;

            for (int j = 0; j < lengthX; j++)
                board[lengthY - zonedLines][j] = 9;

            bitboard[lengthY - zonedLines] = fullLine;

            offset++;
        }

        if (zonedLines > zonedBefore) {
            sounds.clear = 1;
            clearCount = zonedLines - zonedBefore;
        }

        if (!inversion && zonedLines >= 8 && sounds.zone != -1) {
            inversion = true;
            sounds.zone = 2;
        }

        for (int x = 0; x < lengthX; x++) {
            for (int y = stackHeight; y < lengthY; y++) {
                if (board[y][x]) {
                    columnHeights[x] = lengthY - y;
                    break;
                }
            }
        }

        linesToMove.clear();
    }

    garbageHeight -= garbageToRemove;
    garbageCleared += garbageToRemove;

    // FIXME: perfect clears should be counted during zone

    if (clearCount == 0) {
        if (isTSpin > 0) {
            // previousClear = Score(clearCount, scoring[1 + isTSpin * 3][0],
            // comboCounter, isTSpin, isPerfectClear, isBackToBack, isDifficult,
            // drop);
            score += scoring[1 + isTSpin * 3][0];
            sounds.tspin = isTSpin;
            statTracker.tspins++;
        }

        return 0;
    }

    // if(pawn.big)
    //     clearCount/=2;

    if (!zoneTimer)
        linesCleared += clearCount;

    for (int y = lengthY - 1; y >= stackHeight; y--) {
        if (bitboard[y] != 0 && bitboard[y] != fullLine) {
            isPerfectClear = false;
            break;
        }
    }

    fixConnected(linesToClear);

    // check for level up
    int prevLevel = level;
    int currentSectionLevel = 0;
    switch (gameMode) {
    case MARATHON:
        level = ((int)linesCleared / 10) + 1;
        if (subMode && !zonedLines) {
            if (zoneCharge < 32) {
                if ((zoneCharge + clearCount) >= 32) {
                    sounds.meter = 1;
                    zoneCharge = 32;
                } else
                    zoneCharge += clearCount;
            }
        }
        break;
    case BLITZ:
        for (int i = 0; i < 15; i++) {
            if (linesCleared < blitzLevels[i]) {
                level = i + 1;
                break;
            }
        }
        break;
    case CLASSIC:
        if (goal)
            break;

        if (initialLevel) {
            int init =
                min(initialLevel * 10 + 10, max(100, initialLevel * 10 - 50));
            int requirement = linesCleared - init;
            if (requirement >= 0)
                level = (requirement / 10) + initialLevel + 1;
        } else {
            level = ((int)linesCleared / 10);
        }
        break;
    case MASTER:
        if (level == 999) {
            if (clearCount && disappearing)
                creditGrade += creditPoints[clearCount - 1][disappearing - 1];

            break;
        }

        level += clearCount + (clearCount > 2) + (clearCount > 3);

        if (level > 999)
            level = 999;

        if (level == 999 && !disappearing) {
            disappearing = 1 + (coolCount == 9);
            sectionStart = timer;
            eventLock = true;

            clearBoard();
        }

        currentSectionLevel = level / 100;

        if (currentSectionLevel > prevLevel / 100) {
            if (currentSectionLevel == 5 && timer >= 25200) {
                won = 1;
                break;
            }

            int currentSession = timer - sectionStart;

            if (cool) {
                coolCount++;
                cool = false;
            }

            if (currentSession >
                sectionTimeGoal[currentSectionLevel - 1][1]) { // regret
                if (grade) {
                    int newInternal = 31;
                    while (newInternal != 0 &&
                           gradeTable[newInternal][0] >= grade)
                        newInternal--;

                    internalGrade = newInternal;
                    grade--;
                    gradePoints = 0;
                }

                regretCount++;
                sounds.section = -1;
            }

            sounds.levelUp = 1;
            sectionStart = timer;
            setMasterTuning();
        }

        break;
    case DEATH:
        level += clearCount + (clearCount > 2) + (clearCount > 3);

        if (level > 1300)
            level = 1300;

        if (level == 1300 && !disappearing) {
            clearBoard();

            toggleBigMode();

            sectionStart = timer;
            eventLock = true;
            disappearing = -1;
        }

        currentSectionLevel = level / 100;

        if (currentSectionLevel > prevLevel / 100) {
            if (!subMode) {
                if ((currentSectionLevel == 5 && timer >= 10980) ||
                    (currentSectionLevel == 10 && timer >= 10980 * 2)) {
                    won = 1;
                    break;
                }
            } else {
                if ((currentSectionLevel == 5 && timer >= 8880) ||
                    (currentSectionLevel == 10 && timer >= 8880 * 2)) {
                    won = 1;
                    break;
                }
            }

            if (currentSectionLevel == 10) {
                sounds.bone = 1 + !subMode;
            }

            if (timer - sectionStart > 3600) { // regret
                regretCount++;

                sounds.section = -1;
            }

            sounds.levelUp = 1;
            sectionStart = timer;
            setDeathTuning();
        }

        deathGarbageCounter -= clearCount;

        if (deathGarbageCounter < 0)
            deathGarbageCounter = 0;

        break;
    }

    if (level < prevLevel)
        level = prevLevel;

    if (prevLevel < level &&
        (gameMode == MARATHON || gameMode == BLITZ || gameMode == CLASSIC)) {
        sounds.levelUp = 1;
    }

    if (gameMode == ZEN || gameMode == ULTRA)
        prevLevel = 1;

    // calculate score
    int add = 0;
    if (clearCount <= 4 && !isPerfectClear) {
        if (isTSpin == 0) {
            // if no t-spin
            if (gameMode != CLASSIC) {
                add += scoring[clearCount - 1][0] * prevLevel;
                isDifficult = scoring[clearCount - 1][1];
                attack = scoring[clearCount - 1][2];
            } else {
                add += classicScoring[clearCount - 1] * (level + 1);
            }

            statTracker.clears[clearCount - 1]++;
        } else if (isTSpin == 1) {
            // if t-spin mini
            add += scoring[clearCount + 4][0] * prevLevel;
            isDifficult = scoring[clearCount + 4][1];
            attack = scoring[clearCount + 4][2];
            statTracker.tspins++;
        } else if (isTSpin == 2) {
            // if t-spin
            add += scoring[clearCount + 7][0] * prevLevel;
            attack = scoring[clearCount + 7][2];
            isDifficult = true;
            statTracker.tspins++;
        }
    }

    int combo = comboCounter + 1;

    if (gameMode == MASTER && !disappearing) {

        gradePoints +=
            (int)((float)gradeTable[internalGrade][clearCount + 1] *
                      masterComboMultiplayer[(combo < 10) ? combo : 9]
                                            [clearCount - 1] +
                  1) *
            ((int)(level / 250) + 1);

        if (gradePoints >= 100) {
            gradePoints = 0;
            internalGrade++;

            if (internalGrade > 31)
                internalGrade = 31;
        }

        grade = gradeTable[internalGrade][0];
    }

    isBackToBack = previousClear.isDifficult &&
                   (clearCount == 4 || isTSpin == 2) && gameMode != CLASSIC;

    if (isBackToBack) {
        add = (int)add * 3 / 2;
        b2bCounter++;
        attack += 1;
    } else if (!isDifficult) {
        b2bCounter = 0;
    }

    if (b2bCounter > statTracker.maxStreak)
        statTracker.maxStreak = b2bCounter;

    // add combo bonus
    if (comboCounter >= 0) {
        add += scoring[16][0] * level * comboCounter;
        attack += comboTable[comboCounter];
    }

    if (isPerfectClear && gameMode != CLASSIC && clearCount <= 4) {
        if (gameMode != BLITZ)
            add += scoring[clearCount - 1 + 11][0] * level;
        else
            add += 3500 * level;
        attack += scoring[clearCount - 1 + 11][2];

        statTracker.perfectClears++;
    }

    if (zoneTimer) {
        zoneScore += add;
    } else if (zonedLines) {
        int n = (1 + fullZone + (zonedLines >= 8));
        zoneScore = zoneScore * n;
        score += zoneScore;
        zoneScore = 0;
    } else
        score += add;

    previousClear = Score(clearCount, add, comboCounter, isTSpin,
                          isPerfectClear, isBackToBack, isDifficult, drop);

    sounds.clear = 1;

    if (!zoneTimer)
        clearLock = true;
    specialTspin = false;

    if (!garbageQueue.empty()) {
        auto index = garbageQueue.begin();
        while (attack > 0 && index != garbageQueue.end()) {
            bool cleared = false;
            if (attack > index->amount) {
                attack -= index->amount;
                cleared = true;

                garbageQueue.erase(index++);
            } else {
                index->amount -= attack;
                attack = 0;
            }
            if (!cleared)
                ++index;
        }
    }

    if (attack) {
        attackQueue.emplace_back(timer & 0xff, attack);
        linesSent += attack;
    }

    setSpeed();

    return 1;
}

Color Game::color(int n) {
    Color result{};
    result.r = colors[n][0];
    result.g = colors[n][1];
    result.b = colors[n][2];

    return result;
}

void Game::fillBag() {
    bag.clear();
    if (randomizer == BAG_7) {
        for (int i = 0; i < 7; i++)
            bag.push_back(i);
        bagCounter++;
    } else if (randomizer == BAG_35) {
        // fill bag
        for (int i = 0; i < 35; i++)
            bigBag[i] = i / 5;

        // fill drought history
        for (int& i : pieceDrought)
            i = 4;

        // fill piece history
        historyList.push_back(PIECE_Z);
        historyList.push_back(PIECE_Z);
        historyList.push_back(PIECE_S);
        historyList.push_back(PIECE_S);
    }
}

void Game::next() { //~21-29k cycles
    pawn.current = queue.front();
    activePiece = pawn.current;
    queue.pop_front();
    fillQueue(1);

    pawn.setBlock(rotationSystem); //~15k cycles

    setupPawnSpawn();

    lastMoveDx = 0;
    lastMoveDy = 0;

    arrCounter = 0;

    speedCounter = 0;

    softDrop = false;

    if (holding && canHold &&
        ((ihs && ((fromLockHold && maxClearDelay > 1) || !initialType)) ||
         rotationSystem == ARS)) {
        hold(1);

        sounds.initial = 1;
    }
    fromLockRotate = false;

    if (gameMode == DEATH && level >= 500) {
        deathGarbageCounter++;
    }
}

void Game::fillQueue(int count) {
    for (int i = 0; i < count; i++) {
        if (randomizer == BAG_7) {
            int n = random() % bag.size();

            queue.push_back(bag.array[n]);
            bag.erase(n);

            if (bag.size() == 0)
                fillBag();
        } else if (randomizer == RANDOM) {
            int n = random() % 8;

            if (n == 7 || n == pieceHistory)
                n = random() % 7;

            queue.push_back(n);
            pieceHistory = n;
        } else if (randomizer == BAG_35) {
            int n = -1;
            int record = 0;
            int droughted = 0;
            if (queue.empty()) {
                do {
                    n = random() % 7;
                } while (n == PIECE_O || n == PIECE_Z || n == PIECE_S);

                historyList.push_front(n);
                historyList.pop_back();

                queue.push_back(n);
                continue;
            }

            int r = 0;
            for (int i = 0; i < 6; i++) {
                r = random() % 35;

                n = bigBag[r];

                if (historyList.find(n) < 0)
                    break;

                // find most droughted
                for (int i = 0; i < 7; i++) {
                    if (record < pieceDrought[i]) {
                        record = pieceDrought[i];
                        droughted = i;
                    }
                }

                // insert droughted into bag
                bigBag[r] = droughted;

                // in case all previous rolls failed
                if (i == 5) {
                    r = random() % 35;
                    n = bigBag[r];
                }
            }

            // update droughted list
            for (int& ii : pieceDrought)
                ii++;
            pieceDrought[n] = 0;

            // find most droughted
            // record is intentionally not reset; this bug was in the original
            // game
            for (int i = 0; i < 7; i++) {
                if (record < pieceDrought[i]) {
                    record = pieceDrought[i];
                    droughted = i;
                }
            }

            // insert droughted into bag
            bigBag[r] = droughted;

            historyList.push_front(n);
            historyList.pop_back();

            queue.push_back(n);
        }
    }
}

void Game::hold(int dir) {
    holding = dir;

    replay.emplace_back(timer + eventTimer, 5, dir);

    if ((clearLock || entryDelay) && gameMode != CLASSIC) {
        // holding = dir;
        fromLockHold = true;
        return;
    } else if (dir == 0) {
        fromLockHold = false;
        return;
    }

    if (!canHold) {
        rumblePattern(RUMBLE_ERR_2);
        return;
    }

    // if (!canHold || clearLock || entryDelay || gameMode == CLASSIC ||
    // pawn.current == -1)
    if (!canHold || gameMode == CLASSIC || pawn.current == -1)
        return;

    canHold = false;

    if (held == -1) {
        int temp = fromLockRotate;
        held = pawn.current;
        next();
        fromLockRotate = temp;
    } else {
        int temp = held;
        held = pawn.current;
        pawn.current = temp;
        activePiece = temp;
        pawn.setBlock(rotationSystem);

        setupPawnSpawn();
    }

    sounds.hold = 1;

    lockTimer = maxLockTimer;
    lockMoveCounter = 15;
    verticalKick = 0;
    arrCounter = 0;

    dropLockTimer = 0;
    fromLockHold = false;
    moveHistory.clear();

    statTracker.holds++;
}

int* BlockEngine::getShape(int n, int r, int rotationSystem) {
    int* result;
    result = new int[4 * 4];

    int* source;

    switch (rotationSystem) {
    case SRS:
        source = (int*)tetrominos[n][r];
        break;
    case NRS:
        source = (int*)classic[n][r];
        break;
    case ARS:
        source = (int*)ars[n][r];
        break;
    case BARS:
        source = (int*)bars[n][r];
        break;
    case SDRS:
        source = (int*)sdrs[n][r];
        break;
    default:
        source = (int*)tetrominos[n][r];
        break;
    }

    memcpy32_fast(result, source, 16);
    return result;
}

void Game::lockCheck() {
    if (pawn.lowest == pawn.y && lockMoveCounter > 0 &&
        (rotationSystem == SRS || rotationSystem == BARS || rotationSystem == SDRS)) {
        lockTimer = maxLockTimer;
        lockMoveCounter--;
    }
}

void Game::keyLeft(int dir) {
    replay.emplace_back(timer + eventTimer, 0, dir);

    if (clearLock || eventLock || entryDelay || (gameMode == CLASSIC && down)) {
        left = dir;
        return;
    }

    bool moved = false;
    if (left == 0 && dir)
        moved = moveLeft();
    left = dir;

    previousKey = -1;
    dropLockTimer = 0;

    if (gameMode == CLASSIC && dir && moved) {
        das = 0;
        arrCounter = 0;
    }

    if (!dir) {
        pushDir = 0;

        if (directionCancel && gameMode != CLASSIC) {
            das = 0;
            arrCounter = 0;
        }
    }
}

void Game::keyRight(int dir) {
    replay.emplace_back(timer + eventTimer, 1, dir);

    if (clearLock || eventLock || entryDelay || (gameMode == CLASSIC && down)) {
        right = dir;
        return;
    }

    bool moved = false;
    if (right == 0 && dir)
        moved = moveRight();
    right = dir;

    previousKey = 1;
    dropLockTimer = 0;

    if (gameMode == CLASSIC && dir && moved) {
        das = 0;
        arrCounter = 0;
    }

    if (!dir) {
        pushDir = 0;
        if (directionCancel && gameMode != CLASSIC) {
            das = 0;
            arrCounter = 0;
        }
    }
}

void Game::keyDown(int dir) {
    replay.emplace_back(timer + eventTimer, 6, dir);
    if (clearLock || eventLock || entryDelay) {
        if (!(entryDelay && rotationSystem == NRS))
            down = dir;
        return;
    }

    if (down == 0 && dir) {
        moveDown();
        if (pawn.y != pawn.lowest)
            score++;
    }

    down = dir;
    softDrop = true;
    dropLockTimer = 0;
    gracePeriod = 0;
}

void Game::removeClearLock() {
    if (!clearLock)
        return;
    clearLock = false;

    auto index = linesToClear.rbegin();

    const int len = (int)linesToClear.size();

    int pos = linesToClear.back();
    for (int i = pos; i >= stackHeight; i--) {
        if (index != linesToClear.rend() && *index == i) {
            index++;
        } else {
            memcpy32_fast(board[pos], board[i], 10);
            bitboard[pos] = bitboard[i];
            if (disappearing)
                memcpy16_fast(disappearTimers[pos], disappearTimers[i], 10);
            pos--;
        }
    }

    for (int i = stackHeight; i < stackHeight + len; i++) {
        memset32_fast(board[i], 0, 10);
        bitboard[i] = 0;
        if (disappearing)
            memset16(disappearTimers[i], 0, 10);
    }

    stackHeight += len;

    for (int x = 0; x < lengthX; x++) {
        int found = -1;
        for (int y = lengthY - (columnHeights[x] - len) - 1; y < lengthY; y++) {
            if (board[y][x]) {
                found = y;
                break;
            }
        }

        if (found >= 0) {
            columnHeights[x] = lengthY - found;
        } else {
            columnHeights[x] = 0;
        }
    }

    if (gameMode == COMBO) {
        if (!pawn.big) {
            for (int i = lengthY / 2 - 4; i < lengthY; i++) {
                if (bitboard[i] != 0) {
                    break;
                }
                for (int j = 0; j < 10; j++) {
                    if (j > 2 && j < 7)
                        continue;
                    board[i][j] = (i + comboCounter + 1) % 7 + 1;
                    bitboard[i] |= 1 << j;
                }
                connectBoard(i, i);
            }
        } else {
            for (int i = lengthY / 2 - 4; i < lengthY; i++) {
                if (bitboard[i] != 0) {
                    break;
                }
                board[i][0] = (i + comboCounter + 1) % 7 + 1;
                bitboard[i] |= 1;
                connectBoard(i, i);
            }
        }
        stackHeight = lengthY / 2 - 4;
    }

    linesToClear.clear();
    memset32_fast(lineClearArray, 0, 40 / 4);

    if (!entryDelay && !zoneExit) {
        next();
    } else if (zoneExit) {
        zoneExit = false;
    }

    refresh = 2;
    zonedLines = 0;
}

void Game::resetSounds() { sounds = SoundFlags(); }

void Game::resetRefresh() { refresh = 0; }

void Game::setLevel(int newLevel) {
    initialLevel = level = newLevel;
    setSpeed();
}

void Game::setGoal(int newGoal) { goal = newGoal; }

void Game::generateGarbage(int height, int localMode) {
    int hole = random() % lengthX;

    // shift up
    for (int i = 0; i < lengthY; i++) {
        if (i < lengthY - height) {
            for (int j = 0; j < lengthX; j++)
                board[i][j] = board[i + height][j];
            bitboard[i] = bitboard[i + height];
        } else {
            memset32_fast(board[i], 0, 10);
            bitboard[i] = 0;
        }
    }

    for (int i = lengthY - height; i < lengthY; i++) {
        int prevHole = hole;
        int count = 0;
        if (!localMode || random() % 10 < 3) {
            do {
                hole = random() % lengthX;
            } while (++count < 11 &&
                     ((!board[i - 1][hole] && height < garbageHeight) ||
                      hole == prevHole));
        }
        for (int j = 0; j < lengthX; j++) {
            if (j == hole)
                continue;
            board[i][j] = 8;
        }

        bitboard[i] = fullLine & ~(1 << hole);
    }

    garbageHeight += height;

    stackHeight -= height;

    updateColumns(height);

    if (pawn.big)
        return;

    for (int i = lengthY - height; i < lengthY; i++) {
        connectBoard(i, i);
    }
}

void Game::keyDrop(int dir) {
    replay.emplace_back(timer + eventTimer, 7, dir);

    if (dir == 0 && (gameMode == MASTER || gameMode == DEATH))
        dropping = false;

    if (!dir)
        return;

    if (gameMode == CLASSIC)
        return;

    dropping = true;
}

Drop Game::getDrop() {
    Drop result = lastDrop;
    lastDrop = Drop();
    return result;
}

Drop Game::calculateDrop() {
    Drop result;

    result.on = true;

    result.x = pawn.x;
    result.y = pawn.y;

    result.dx = lastMoveDx;
    result.dy = lastMoveDy;

    result.piece = pawn.current;
    result.rotation = pawn.rotation;
    result.rotating = lastMoveRotation;

    result.startX = pawn.x;

    int start = 0;
    int end = 3;
    while (pawn.boardLowest[pawn.rotation][start] == -1)
        start++;
    while (pawn.boardLowest[pawn.rotation][end] == -1)
        end--;

    result.startX += start;
    result.endX = pawn.x + end + 1;

    result.startY = pawn.y + pawn.heighest[pawn.rotation];

    int add = 0;
    for (int i = 0; i < 4; i++)
        if (pawn.boardLowest[pawn.rotation][i] > add)
            add = pawn.boardLowest[pawn.rotation][i];

    result.rawEndY = result.endY = pawn.y + add;

    if (rotationSystem != NRS) {
        if (((pawn.current == PIECE_L && pawn.rotation == 3) ||
             (pawn.current == PIECE_J && pawn.rotation == 1)))
            result.endY--;
    } else {
        if (((pawn.current == PIECE_L && pawn.rotation == 1) ||
             (pawn.current == PIECE_J && pawn.rotation == 3)))
            result.endY--;
    }

    if (result.startY < stackHeight)
        stackHeight = result.startY;

    // if(pawn.big){
    //     result.startX *=2;
    //     result.endX *=2;
    //     result.startY *=2;
    //     result.endY *=2;
    //     result.rawEndY *=2;
    // }

    result.startY -= boardOffset;
    result.endY -= boardOffset;
    result.y -= boardOffset;

    return result;
}

void Game::setTuning(Tuning newTune) {
    if (gameMode == CLASSIC)
        return;

    if (rotationSystem != ARS) {
        ihs = newTune.ihs;
        irs = newTune.irs;
        initialType = newTune.initialType;
    }

    if (gameMode == MASTER || gameMode == DEATH)
        return;

    maxDas = newTune.das;
    arr = newTune.arr;
    softDropSpeed = newTune.sfr;
    dropLockMax = newTune.dropProtection;
    directionCancel = newTune.directionalDas;
    delaySoftDrop = newTune.delaySoftDrop;
    maxClearDelay = 20;
}

void Game::setMasterTuning() {

    int n = 0;
    int speedLevel = level + coolCount * 100;

    if (speedLevel >= 500) {
        n = (speedLevel - 400) / 100;
    }

    if (n > 8)
        n = 8;

    arr = 1;
    areMax = masterDelays[n][0];
    lineAre = masterDelays[n][1];
    maxDas = masterDelays[n][2];
    lockTimer = maxLockTimer = masterDelays[n][3];
    maxClearDelay = masterDelays[n][4];
    softDropSpeed = 1;
}

void Game::setDeathTuning() {

    int n = level / 100;

    if (n > 13)
        n = 13;

    arr = 1;
    areMax = deathDelays[n][0];
    lineAre = deathDelays[n][1];
    maxDas = deathDelays[n][2];
    maxLockTimer = deathDelays[n][3];
    maxClearDelay = deathDelays[n][4];
    softDropSpeed = 1;
}

void Game::clearAttack(int id) {
    auto index = attackQueue.begin();

    while (index != attackQueue.end()) {
        if (index->id == id) {
            attackQueue.erase(index++);
        } else {
            ++index;
        }
    }
}

void Game::setWin() { won = true; }

void Game::addToGarbageQueue(int id, int amount) {
    if (amount <= 0)
        return;

    // for(const auto& attack : garbageQueue)
    //     if(attack.id == id)
    //         return;

    garbageQueue.emplace_back(id, amount);
}

int Game::getIncomingGarbage() {
    int result = 0;

    for (const auto& atck : garbageQueue) {
        result += atck.amount;
    }

    return result;
}

std::list<int> Game::getBestFinesse(int piece, int dx, int rotation) {
    std::list<int> result;

    int r = 0;
    switch (piece) {
    case 0:
        r = rotation % 2;
        break;
    case 1:
        r = rotation;
        break;
    case 2:
        r = rotation;
        break;
    case 3:
        r = 0;
        break;
    case 4:
        r = rotation % 2;
        break;
    case 5:
        r = rotation;
        break;
    case 6:
        r = rotation % 2;
        break;
    }

    for (int i = 0; i < 4; i++) {
        result.push_back(finesse[piece][r][dx][i]);
        if (finesse[piece][r][dx][i] == 7)
            break;
    }

    return result;
}

void Game::setTrainingMode(bool localMode) {
    if (pawn.big)
        return;
    trainingMode = localMode;
}

void Game::demoClear() {
    clearLock = true;

    for (int i = lengthY - 2; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            board[i][j] = 8;
        }
    }

    clear(Drop());
}

void Game::demoFill() {
    clearLock = false;
    for (int i = lengthY - 2; i < lengthY; i++) {
        int n = random() % lengthX;
        board[i][n] = 0;
    }
}

void Game::bType(int height) {
    int n = 0;
    if (height)
        n = height * 2 + (height + 1) / 2 - (height == 5);

    for (int i = lengthY - n; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            if (random() % 2 && !(i == lengthY - n && j == 0)) {
                board[i][j] = (random() % 7);

                if (columnHeights[j] < lengthY - i)
                    columnHeights[j] = lengthY - i;

                bitboard[i] |= 1 << j;
            }
        }
    }

    stackHeight -= n;
}

void Game::setSubMode(int sm) {

    subMode = sm;

    if (!sm)
        return;

    if (gameMode == MASTER || gameMode == DEATH) {
        setRotationSystem(ARS);

        sonicDrop = true;
    } else if (gameMode == CLASSIC) {
        bType(bTypeHeight);
        goal = 25;
    }
}

void Game::setSpeed() {
    if ((gameMode == MARATHON || gameMode == TRAINING ||
         (gameMode == ZEN && subMode == 1)) &&
        !(subMode == 1 && zoneTimer)) {
        if (level)
            speed = gravity[(level < 19) ? level - 1 : 18];
        else
            speed = 0;
    } else if (gameMode == BLITZ)
        speed = blitzGravity[(level < 15 && level) ? level - 1 : 14];
    else if (gameMode == CLASSIC)
        speed = classicGravity[(level < 30) ? level : 29];
    else if (gameMode == MASTER) {
        int speedLevel = level + coolCount * 100;
        if (speedLevel >= 500)
            speed = masterGravity[29][1];
        else {
            for (int i = 28; i >= 0; i--) {
                if (speedLevel >= masterGravity[i][0]) {
                    speed = masterGravity[i][1];
                    break;
                }
            }
        }
        setMasterTuning();
    } else if (gameMode == DEATH) {
        speed = 20;
    } else
        speed = gravity[0];
}

void Game::setRotationSystem(int rs) { rotationSystem = rs; }

int Game::checkITouching(int r, int oriantation) {
    int x = pawn.x;
    int y = pawn.y;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (oriantation == 0) {
                if ((j == 1 || j == 3) &&
                    ((x + j < 0) || (x + j > lengthX - 1) ||
                     ((y + i >= 0 && y + i <= lengthY - 1) &&
                      board[i + y][j + x])))
                    return j;
            } else {
                if (i == 2 && ((y + i > lengthY - 1) ||
                               ((x + j >= 0 && x + j <= lengthX - 1) &&
                                (board[y + i][x + j]))))
                    return i;
            }
        }
    }

    return 0;
}

void Game::updateDisappear() {
    if (!disappearing)
        return;

    bool found = false;

    for (int i = 0; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            if (disappearTimers[i][j] <= 1)
                continue;
            disappearTimers[i][j]--;

            if (disappearTimers[i][j] == 1) {
                found = true;
                toDisappear.emplace_back(j, i);
            }
        }
    }

    if (found)
        sounds.disappear = 1;
}

void Game::removeEventLock() {
    eventLock = false;
    inversion = false;
}

void Game::activateZone(int dir) {
    if (!dir)
        return;

    replay.push_back(Timestamp(timer, 8, dir));

#ifndef NO_DIAGNOSE
    zoneCharge = 32;
#endif

    if (zoneCharge < 8)
        return;

    fullZone = (zoneCharge == 32);

    previousClear.isTSpin = 0;
    previousClear.isBackToBack = 0;
    previousClear.isPerfectClear = 0;

    zoneTimer = (float)37.5 * zoneCharge;

    zoneStart = zoneTimer;

    zoneCharge = 0;

    sounds.zone = 1;
}

void Game::endZone() {
    if (zonedLines > statTracker.maxZonedLines)
        statTracker.maxZonedLines = zonedLines;

    zoneTimer = 0;
    sounds.zone = -1;

    clear(Drop());

    eventLock = true;

    if (zonedLines)
        zoneExit = true;

    previousClear.isPerfectClear = false;
}

void Game::clearBoard() {
    for (int i = 0; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            board[i][j] = 0;
        }
        bitboard[i] = 0;
    }

    memset32_fast(columnHeights, 0, 10);

    garbageHeight = 0;

    stackHeight = lengthY;

    linesToClear.clear();
    memset32_fast(lineClearArray, 0, 40 / 4);

    clearLock = false;
}

void Game::fixConnected(const std::list<int>& sourceList) {
    std::list<std::tuple<int, int>> fixList;

    for (auto const& line : sourceList) {
        bool found = false;
        if (!fixList.empty()) {
            int n = std::get<0>(fixList.back());
            if (n == line) {
                fixList.pop_back();
                found = true;
            } else if (n == line - 1) {
                std::get<1>(fixList.back())++;
                found = true;
            }
        }

        if (!found && line > 0) {
            fixList.emplace_back(line - 1, 0);
        }

        if (line < lengthY - 1) {
            fixList.emplace_back(line + 1, 1);
        }
    }

    for (auto const& line : fixList) {
        int iy = std::get<0>(line);
        int type = std::get<1>(line);
        for (int ix = 0; ix < lengthX; ix++) {
            int n = board[iy][ix];
            board[iy][ix] =
                (n & 0xf) + (GameInfo::connectedFix[type][n >> 4] << 4);
        }
    }
}

void Game::connectBoard(int startY, int endY) {
    int ex = lengthX;
    int sy = startY;
    int ey = endY;

    for (int iy = startY; iy < endY + 1; iy++) {

        for (int ix = 0; ix < lengthX; ix++) {
            if (!board[iy][ix] || (board[iy][ix] >> 4) != 0)
                continue;

            int n = getNbr(ix, iy, ex, sy, ey);

            if (n == 1) {
                if (iy > sy && board[iy - 1][ix])
                    board[iy][ix] += 1 << 4;
                else if (iy < ey && board[iy + 1][ix])
                    board[iy][ix] += 2 << 4;
                else if (ix > 0 && board[iy][ix - 1])
                    board[iy][ix] += 3 << 4;
                else if (ix < ex && board[iy][ix + 1])
                    board[iy][ix] += 4 << 4;
            } else if (n == 2) {
                if (iy > sy && board[iy - 1][ix] && iy < ey &&
                    board[iy + 1][ix])
                    board[iy][ix] += 5 << 4;
                else if (ix > 0 && board[iy][ix - 1] && ix < ex &&
                         board[iy][ix + 1])
                    board[iy][ix] += 6 << 4;
                else if (iy < ey && board[iy + 1][ix] && ix < ex &&
                         board[iy][ix + 1])
                    board[iy][ix] += 7 << 4;
                else if (iy < ey && board[iy + 1][ix] && ix > 0 &&
                         board[iy][ix - 1])
                    board[iy][ix] += 8 << 4;
                else if (iy > sy && board[iy - 1][ix] && ix < ex &&
                         board[iy][ix + 1])
                    board[iy][ix] += 9 << 4;
                else if (iy > sy && board[iy - 1][ix] && ix > 0 &&
                         board[iy][ix - 1])
                    board[iy][ix] += 10 << 4;
            } else if (n == 3) {
                if (iy < ey && board[iy + 1][ix] && ix < ex &&
                    board[iy][ix + 1] && board[iy + 1][ix + 1])
                    board[iy][ix] += 11 << 4;
                else if (iy < ey && board[iy + 1][ix] && ix > 0 &&
                         board[iy][ix - 1] && board[iy + 1][ix - 1])
                    board[iy][ix] += 12 << 4;
                else if (iy > sy && board[iy - 1][ix] && ix < ex &&
                         board[iy][ix + 1] && board[iy - 1][ix + 1])
                    board[iy][ix] += 13 << 4;
                else if (iy > sy && board[iy - 1][ix] && ix > 0 &&
                         board[iy][ix - 1] && board[iy - 1][ix - 1])
                    board[iy][ix] += 14 << 4;
                else if (iy > sy && board[iy - 1][ix] && iy < ey &&
                         board[iy + 1][ix] && ix < ex && board[iy][ix + 1])
                    board[iy][ix] += 15 << 4;
                else if (ix > 0 && board[iy][ix - 1] && ix < ex &&
                         board[iy][ix + 1] && iy < ey && board[iy + 1][ix])
                    board[iy][ix] += 16 << 4;
                else if (iy > sy && board[iy - 1][ix] && iy < ey &&
                         board[iy + 1][ix] && ix > 0 && board[iy][ix - 1])
                    board[iy][ix] += 17 << 4;
                else if (ix > 0 && board[iy][ix - 1] && ix < ex &&
                         board[iy][ix + 1] && iy > sy && board[iy - 1][ix])
                    board[iy][ix] += 18 << 4;
            } else if (n == 5) {
                if (iy < ey && ix < ex && ix > 0 && board[iy + 1][ix - 1] &&
                    board[iy + 1][ix + 1])
                    board[iy][ix] += 19 << 4;
                else if (iy > sy && ix < ex && ix > 0 &&
                         board[iy - 1][ix - 1] && board[iy - 1][ix + 1])
                    board[iy][ix] += 20 << 4;
                else if (ix < ex && iy < ey && iy > sy &&
                         board[iy - 1][ix + 1] && board[iy + 1][ix + 1])
                    board[iy][ix] += 21 << 4;
                else if (ix > 0 && iy < ey && iy > sy &&
                         board[iy - 1][ix - 1] && board[iy + 1][ix - 1])
                    board[iy][ix] += 22 << 4;
            } else if (n == 8) {
                board[iy][ix] += 23 << 4;
            }
        }
    }
}

void Game::liftKeys() {
    for (int i = 0; i < 3; i++)
        rotates[0] = false;

    holding = 0;
    left = 0;
    right = 0;
    down = 0;

    pushDir = 0;
}

void Game::setOptions(Options newOptions) {
    goal = newOptions.goal;
    setLevel(newOptions.level);
    setTuning(newOptions.tuning);

    trainingMode = newOptions.trainingMode;

    bTypeHeight = newOptions.bTypeHeight;
    setRotationSystem(newOptions.rotationSystem);
    setSubMode(newOptions.subMode);

    if (newOptions.subMode &&
        (newOptions.mode == MASTER || newOptions.mode == DEATH)) {
        newOptions.randomizer = BAG_35;
    }

    setRandomizer(newOptions.randomizer);
}

void Game::updateColumns(int height) {

    for (int x = 0; x < lengthX; x++) {
        int found = -1;
        for (int y = lengthY - (columnHeights[x] + height) - 1; y < lengthY;
             y++) {
            if (board[y][x]) {
                found = y;
                break;
            }
        }

        if (found >= 0) {
            columnHeights[x] = lengthY - found;
        } else {
            columnHeights[x] = 0;
        }
    }
}

void Game::updateDas() {
    if (!(left || right)) {
        if (gameMode != CLASSIC) {
            das = 0;
            arrCounter = 0;
        }
    } else if (das < maxDas) {
        das++;
    }
}

void Game::chargeDas() {
    if (!(left || right))
        return;

    das = maxDas;
}

int poll(int max, const std::string& buffer) {
    int result = 0;

    for (int i = 0; i < max; i++) {
        std::string c = buffer.substr(i, 1);

        int n = base64Decode.at(c);

        result += n * pow(64, i);
    }

    return result;
}

void Game::loadFumen() {
    if (pawn.big)
        return;

    if ((int)fumenString.size() < 5 || fumenString.substr(0, 5) != "v115@") {
        log("invalid fumen");
        return;
    }

    log("valid fumen!");

    std::string buffer = fumenString.substr(5);

    int blockCount = 0;
    while (blockCount < 240 && buffer.size() >= 2) {

        if (buffer[0] == '?')
            buffer = buffer.substr(1);

        int n = poll(2, buffer);
        buffer = buffer.substr(2);

        int block = n / 240 - 8;
        int count = n % 240 + 1;

        if (block == 0) {
            blockCount += count;
            continue;
        }

        for (int i = 0; i < count; i++) {
            int x = (blockCount + i) % 10;
            int y = (blockCount + i) / 10 + 17;

            if (y > 39)
                continue;

            board[y][x] = fumenBlocksDecode[block];

            bitboard[y] |= 1 << x;
            if ((lengthY - y) > columnHeights[x])
                columnHeights[x] = lengthY - y;

            if (stackHeight > y)
                stackHeight = y;
        }

        blockCount += count;
    }

    checkSecretGrade();
}

INLINE std::string fumenData(int block, int count) {
    int n = (fumenBlocksEncode[block] + 8) * 240 + count - 1;

    return base64Encode.at(n % 64) + base64Encode.at(n / 64);
}

std::string Game::getFumen() {
    std::string result = "v115@";

    int previous = 0;
    int count = 0;
    for (int i = 17; i < 40; i++) {
        for (int j = 0; j < 10; j++) {
            if (board[i][j] == previous) {
                count++;
            } else {
                result += fumenData(previous, count);

                count = 1;
                previous = board[i][j] & 0xf;
            }
        }
    }

    result += fumenData(previous, count);
    result += fumenData(0, 10);
    result += "AgH";

    log("Fumen: ");
    log(result);
    return result;
}

Game::~Game() = default;

Game::Game(int gm, int sd, bool big) {
    gameMode = gm;
    initSeed = sd;
    setRandomSeed(sd);
    towerScrollCount = 0;
    towerClearingPhase = false;
    towerMediumClearing = false;
    towerMediumTimer = 0;
    towerMediumDelay = 0;

    for (int i = 0; i < 40; i++) {
        bitboard[i] = 0;
        lineClearArray[i] = false;

        for (int j = 0; j < 10; j++)
            board[i][j] = 0;
    }

    for (int j = 0; j < lengthX; j++)
        columnHeights[j] = 0;

    if (gameMode == CLASSIC) {
        maxLockTimer = 1;

        maxDas = 16;
        arr = 6;
        softDropSpeed = 2;
        gracePeriod = 90;
    }

    if (enableFumen && enableFumenQueue) {
        for (int i = 0; i < 5; i++)
            queue.push_back(fumenQueue[i]);
    }

    pawn = Pawn(0, 0);
    pawn.big = big;

    if (enableFumen)
        loadFumen();

    if (big) {
        lengthY /= 2;
        stackHeight = lengthY;
        lengthX /= 2;
        boardOffset = lengthY / 2;
        fullLine >>= 5;
    }

    if (gameMode == SPRINT)
        goal = 40;
    else if (gameMode == MARATHON)
        goal = 150;
    else if (gameMode == DIG) {
        goal = 100;
        if (!big)
            generateGarbage(9, 0);
        else
            generateGarbage(4, 0);
    } else if (gameMode == COMBO) {
        if (!big) {
            for (int i = lengthY / 2 - 4; i < lengthY; i++) {
                for (int j = 0; j < 10; j++) {
                    if (j > 2 && j < 7 && !(i == lengthY - 2 && j < 5) &&
                        !(i == lengthY - 1 && j < 4))
                        continue;
                    board[i][j] = i % 7 + 1;
                    bitboard[i] |= 1 << j;

                    if ((lengthY - i) > columnHeights[j])
                        columnHeights[j] = lengthY - i;
                }
                connectBoard(i, i);
            }
            stackHeight = lengthY / 2 - 4;
        } else {
            for (int i = (lengthY / 2) - 4; i < lengthY; i++) {
                for (int j = 0; j < 5; j++) {
                    if (j != 0 && !(i == (lengthY)-2 && j < 3) &&
                        !(i == (lengthY)-1 && j < 2))
                        // if(j != 0)
                        continue;
                    board[i][j] = i % 7 + 1;
                    bitboard[i] |= 1 << j;

                    if ((lengthY - i) > columnHeights[j])
                        columnHeights[j] = lengthY - i;
                }
                connectBoard(i, i);
            }
            stackHeight = lengthY / 2 - 4;
        }
    } else if (gameMode == MASTER) {
        level = 0;
        speed = GameInfo::masterGravity[0][1];
        setMasterTuning();
    } else if (gameMode == DEATH) {
        level = 0;
        speed = 20;
        setDeathTuning();
    } else if (gameMode == BATTLE) {
        replayElligible = false;
    }

    for (int i = 0; i < 40; i++) {
        for (int j = 0; j < 10; j++)
            disappearTimers[i][j] = 0;
    }
}

Game::Game(Game* oldGame) {
    oldGame->removeClearLock();

    for (int i = 0; i < 40; i++) {
        bitboard[i] = oldGame->bitboard[i];
        for (int j = 0; j < 10; j++) {
            board[i][j] = oldGame->board[i][j];
            disappearTimers[i][j] = oldGame->disappearTimers[i][j];
        }

        lineClearArray[i] = oldGame->lineClearArray[i];
    }

    for (int j = 0; j < 10; j++)
        columnHeights[j] = oldGame->columnHeights[j];

    queue = oldGame->queue;
    canHold = oldGame->canHold;
    bag = oldGame->bag;

    pawn = Pawn(oldGame->pawn);
    statTracker = Stats(oldGame->statTracker);

    held = oldGame->held;
    level = oldGame->level;
    score = oldGame->score;
    comboCounter = oldGame->comboCounter;
    linesCleared = oldGame->linesCleared;
    towerScrollCount = oldGame->towerScrollCount;
    towerClearingPhase = oldGame->towerClearingPhase;
    towerMediumClearing = oldGame->towerMediumClearing;
    towerMediumTimer = oldGame->towerMediumTimer;
    towerMediumDelay = oldGame->towerMediumDelay;
    refresh = oldGame->refresh;
    won = oldGame->won;
    lost = oldGame->lost;
    goal = oldGame->goal;
    finesseFaults = oldGame->finesseFaults;
    garbageCleared = oldGame->garbageCleared;
    garbageHeight = oldGame->garbageHeight;
    pushDir = oldGame->pushDir;
    b2bCounter = oldGame->b2bCounter;
    bagCounter = oldGame->bagCounter;
    setRandomSeed(oldGame->randomSeed);
    initSeed = oldGame->initSeed;
    subMode = oldGame->subMode;
    initialLevel = oldGame->initialLevel;
    trainingMode = oldGame->trainingMode;
    clearLock = oldGame->clearLock;
    linesCleared = oldGame->linesCleared;
    dropping = oldGame->dropping;
    stackHeight = oldGame->stackHeight;
    pieceCounter = oldGame->pieceCounter;
    speed = oldGame->speed;
    activePiece = oldGame->activePiece;
    linesToClear = oldGame->linesToClear;
    randomizer = oldGame->randomizer;
    rotationSystem = oldGame->rotationSystem;
}

void Game::demoGarbage() {
    generateGarbage(3, 0);

    for (int i = 20; i < 40; i++)
        connectBoard(i, i);
}

void Game::setupPawnSpawn() {
    pawn.y = (int)lengthY / 2;
    pawn.x = (int)lengthX / 2 - 2;

    if (game->pawn.big) {
        pawn.x++;
    }

    if (gameMode == CLASSIC) {
        down = 0;
        if (pawn.current != 0 && pawn.current != 3)
            pawn.x++;
        if (pawn.current == PIECE_I)
            pawn.y--;
    }

    if (rotationSystem == BARS) {
        if (pawn.current != PIECE_I)
            pawn.y--;
    }

    if (rotationSystem == ARS || (rotationSystem == NRS && gameMode != CLASSIC)|| rotationSystem == SDRS) {
        pawn.y--;
        if (rotationSystem == NRS && pawn.current == PIECE_I && gameMode != CLASSIC)
            pawn.y--;
    }

    pawn.rotation = 0;
    if ((irs && (((fromLockRotate) && maxClearDelay > 1) || !initialType)) ||
        rotationSystem == ARS) {
        if (rotationSystem == ARS) {
            if (rotates[1])
                rotate(-1, 1);
            else if (rotates[0])
                rotate(1, 1);
            else if (rotates[2])
                rotate(-1, 1);
        } else {
            int sum = rotates[0] + (rotates[1] * -1) +
                      (rotates[2] * ((rotationSystem == ARS) ? -1 : 2));

            pawn.rotation = sum + 4 * (sum < 0);
        }

        if (pawn.rotation)
            sounds.initial = 1;
    }

    // check if stack has reached top 3 lines
    const int line = 24 / (1 + pawn.big);
    bool check = bitboard[line] != 0;

    if (pawn.rotation && rotationSystem == SRS) {
        int lowestPart = pawn.lowestBlock[pawn.rotation];

        int n = (40 - 19) - (lowestPart + pawn.y);

        if (pawn.big) {
            n = (20 - 9) - (lowestPart + pawn.y);
        }

        pawn.y += std::min(n, 0);
    }

    if (check || !checkRotation(0, 0, pawn.rotation) || gameMode == CLASSIC) {
        pawn.y -= 1;
        if (!checkRotation(0, 0, pawn.rotation)) {
            if (zoneTimer && zonedLines) {
                if (!toEndZone)
                    endZone();
            } else if (gameMode == TRAINING || gameMode == NO_MODE ||
                       gameMode == ZEN) {
                clearBoard();

                if (gameMode == ZEN && subMode) {
                    towerClearingPhase = false;
                }
            } else {
                lost = 1;
            }
        }
    }
}

std::tuple<int, int> Game::getSpawnPosition() {
    int y = (int)lengthY / 2;
    int x = (int)lengthX / 2 - 2;

    if (game->pawn.big) {
        x++;
    }

    int curr = queue.front();

    if (gameMode == CLASSIC) {
        if (curr != 0 && curr != 3)
            x++;
        if (curr == 0)
            y--;
    }

    const int line = 24 / (1 + pawn.big);
    bool check = bitboard[line] != 0;

    if (check)
        y--;

    return std::make_tuple(x, y);
}

void Game::checkSecretGrade() {
    int result = 0;
    for (int i = lengthY - 1; i > lengthY / 2; i--) {
        int n = lengthY - 1 - i;

        int x = n;

        if (n > lengthX - 1)
            x = 2 * (lengthX - 1) - x;

        // log("checking " + std::to_string(n) + " " + std::to_string(x));
        if (bitboard[i] != (fullLine & ~(1 << x)) || board[i - 1][x] == 0) {
            break;
        }

        result++;
    }

    statTracker.secretGrade = max(result - (lengthX - 1), 0);
}

void Game::calculatePeek() {
    // height of the pawn's heighest block
    const int heighestBlock = pawn.lowest + pawn.heighest[pawn.rotation];

    // increase this to make peek kick in lower
    const int height = 3;

    peek = max((lengthY / 2 + height) - (heighestBlock), 0);
}

void Game::setRandomizer(int rand) {

    randomizer = rand;

    bagCounter = 0;

    queue.clear();

    fillBag();

    if (enableFumen && enableFumenQueue) {
        for (unsigned char i : fumenQueue)
            queue.push_back(i);
    } else {
        fillQueue(5);
    }
}

void Game::deathGarbage() {

    // shift up
    for (int i = 0; i < lengthY; i++) {
        if (i < lengthY - 1) {
            for (int j = 0; j < lengthX; j++)
                board[i][j] = board[i + 1][j];
            bitboard[i] = bitboard[i + 1];
        } else {
            memset32_fast(board[i], 0, 10);
            bitboard[i] = 0;
        }
    }

    // duplicate bottom
    for (int i = 0; i < lengthX; i++) {
        if (board[lengthY - 2][i])
            board[lengthY - 1][i] = 8;
    }

    bitboard[lengthY - 1] = bitboard[lengthY - 2];

    stackHeight -= 1;

    updateColumns(1);

    refresh = 1;
}

void Game::toggleBigMode() {
    if (pawn.big) {
        pawn.big = false;
        lengthY = 40;
        lengthX = 10;
        boardOffset = 20;
        fullLine = 0x3ff;
    } else {
        pawn.big = true;
        lengthY = 20;
        lengthX = 5;
        boardOffset = 10;
        fullLine = 0x1f;
    }

    refresh = 1;
}

u32 Game::random() {
    randomSeed = randNext2();
    return randomSeed & randomMax;
}

int Game::setRandomSeed(int newSeed) {
    int old = randomSeed;
    randomSeed = newSeed;
    randSetFullState2((u32)randomSeed);
    // Mix up seed into state array with 10 iterations to init xorshiro
    for (int i = 0; i < 10; i++) {
        randomSeed = randNext2();
    }
    return old;
}

void Game::scrollTower() {
    towerScrollCount++;

    if (gameMode == MARATHON && subMode == 2 && goal > 0 &&
        (towerScrollCount + (lengthY - stackHeight)) >= goal) {
        setWin();
    }

    // 1. Shift the board down
    for (int y = lengthY - 1; y > 0; y--) {
        for (int x = 0; x < lengthX; x++) {
            board[y][x] = board[y - 1][x];
            disappearTimers[y][x] = disappearTimers[y - 1][x];
        }
        bitboard[y] = bitboard[y - 1];
    }

    // 2. Clear the top line
    memset32_fast(board[0], 0, 10);
    if (disappearing)
        memset32_fast(disappearTimers[0], 0, 10);
    bitboard[0] = 0;

    // 3. Update stack and column heights
    stackHeight++;
    for (int x = 0; x < lengthX; x++) {
        columnHeights[x] = max(columnHeights[x] - 1, 0);
    }

    if (stackHeight == lengthY) {
        score += 3500 * level;
        sounds.clear = 1;
        previousClear = Score(0, 0, -1, 0, 1, 0, 0, Drop());
    }

    refresh = 1;
}

int Game::checkIntegrity() {
    // Simple flood fill to check for air bubbles
    u32 visited[40] = {0};
    std::vector<u32> toCheck;

    // Start from stackHeight - 1
    for (int x = 0; x < lengthX; x++) {
        if (board[stackHeight - 1][x] == 0) {
            toCheck.push_back(x + ((stackHeight - 1) << 16));
        }
    }

    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};

    while (!toCheck.empty()) {
        const u32 pair = toCheck.back();
        toCheck.pop_back();
        const int x = pair & 0xffff;
        const int y = (pair >> 16) & 0xffff;

        if (visited[y] & (1 << x))
            continue;

        visited[y] |= 1 << x;

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx >= 0 && nx < lengthX && ny >= stackHeight && ny < lengthY) {
                if (board[ny][nx] == 0 && (visited[ny] & (1 << nx)) == 0) {
                    toCheck.push_back(nx + (ny << 16));
                }
            }
        }
    }

    // If any empty block was not visited, it's an enclosed bubble -> Loss
    for (int y = stackHeight; y < lengthY; y++) {
        for (int x = 0; x < lengthX; x++) {
            if (board[y][x] == 0 && (visited[y] & (1 << x)) == 0) {
                return y;
            }
        }
    }

    return -1;
}

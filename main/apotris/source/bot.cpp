#include "blockEngine.hpp"
#include "def.h"
#include "logging.h"
#include <posprintf.h>
#include <string>
#include <tuple>

int botThinkingSpeed = 11;
int botSleepDuration = 1;
int botStepMax = 10;

using namespace BlockEngine;

void showTestBoard(u16* testBoard);

INLINE bool bitGet(u16* bitboard, int x, int y) {
    return (bitboard[y] >> x) & 0b1;
}

u8 bitcount[] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

INLINE int countBits(u16 n) {
    return bitcount[n & 0xff] + bitcount[(n >> 8) & 0xff];
}

INLINE int countTransitions(u16 a) { return countBits((a >> 1) ^ a); }

IWRAM_CODE Values evaluate(u16* board, int* columnHeights, int lengthX,
                           int lengthY, int startY) {
    int start = max(1, startY - 1);

    int rowTrans = 0;
    int colTrans = 0;
    int holes = 0;
    int wells = 0;
    int quadWell = 0;

    //~300-600 cycles
    for (int i = start; i < lengthY; i++) {
        if (board[i]) {
            rowTrans += countTransitions(board[i]);
        }
    }

    for (int j = 0; j < lengthX; j++) {
        bool prev = false;
        for (int i = lengthY - columnHeights[j]; i < lengthY; i++) {
            // profile_start();
            bool n = bitGet(board, j, i);

            colTrans += (n != prev);

            holes += (n != prev) && !n;

            prev = n;
        }
    }

    int lowestX = 0;
    int lowestCount = 255;

    //~1200-1400 cycles
    for (int x = 0; x < lengthX; x++) {
        int count = 0;
        int n = columnHeights[x];

        if (n < lowestCount) {
            lowestCount = n;
            lowestX = x;
        }

        while (((x == 0 || n < columnHeights[x - 1]) &&
                (x == lengthX - 1 || n < columnHeights[x + 1]))) {
            n++;
            count += count + 1;
        }

        wells += count;
    }

    int i = lengthY - columnHeights[lowestX] - 1;
    while (countBits(board[i]) == 9 && i >= 0) {
        i--;
        quadWell++;
    }

    Values result;
    result.rowTrans = rowTrans;
    result.colTrans = colTrans;
    result.holes = holes;
    result.wells = wells;
    result.quadWell = quadWell;
    result.wellX = lowestX;

    return result;
}

int countClears(u16* board, int lengthX, int lengthY, int startY) { // row
    int clears = 0;
    if (startY < 0)
        startY = 0;
    const int len = min(startY + 4, lengthY);
    for (int i = startY; i < len; i++) {
        if (board[i] == 0x3ff)
            clears++;
    }

    return clears;
}

void Bot::run() {
    if (--sleepTimer > 0)
        return;
    if (thinking) {
        if (game->clearLock || game->dropping) {
            return;
        }

        if (thinkingI == -6) {
            if (checking == 0 && exploring == 0) {
                if (testGame != nullptr)
                    delete testGame;
                testGame = new Game(game);

                if (game->pawn.current == -1) {
                    testGame->next();
                }
            }
        }

        if (thinkingI < 6) {
            for (int i = 0; i < botThinkingSpeed; i++) {
                if (thinkingI >= 6)
                    break;
                if (thinkingJ < 4) {
                    while (!findBestDrop(thinkingI, thinkingJ, testGame, moves,
                                         weights)) {
                        if (thinkingJ < 3) {
                            thinkingJ++;
                        } else {
                            thinkingJ = 0;
                            if (thinkingI < 6)
                                thinkingI++;
                            else
                                break;
                        }
                    }
                    thinkingJ++;
                } else {
                    thinkingJ = 0;
                    thinkingI++;
                }
            }

            if (thinkingI >= 6) {
                thinkingI = -6;
                thinkingJ = 0;
                sleepTimer = botSleepDuration;

                if (checking == 1) {
                    stopThinking();
                } else {
                    checking++;

                    if (testGame->canHold) {
                        if (testGame->held != -1)
                            testGame->pawn.current = testGame->held;
                        else
                            testGame->pawn.current = testGame->queue.front();

                        testGame->pawn.setBlock(testGame->rotationSystem);

                    } else {
                        stopThinking();
                    }
                }
            }
        }
    }

    if (!thinking) // not in an else block because "thinking" can change during
                   // the above block and I want this to run in the same call if
                   // possible
        move();
}

void Bot::stopThinking() {
    if (SEARCH_DEPTH <= 1) {
        thinking = false;
        checking = 0;
        exploring = 0;

        current = moves.front();

        if (testGame != nullptr)
            delete testGame;
        testGame = new Game(game);

        moves.clear();

        return;
    }

    if (exploring == MAX_MOVELIST_SIZE) {
        thinking = false;
        checking = 0;

        // current = firstMoves[0];
        current = firstMoves[bestFuture];
        exploring = 0;

        // log("best move: " + std::to_string(current.dx) + " with score " +
        // std::to_string(current.score));

        if (bestFuture != 0) {
            log("best future: " + std::to_string(bestFuture) +
                ", using piece: " +
                std::to_string(firstMoves[bestFuture].piece));
            log("move: " + std::to_string(bestMoves[bestFuture].dx) + " " +
                std::to_string(bestMoves[bestFuture].rotation) + " " +
                std::to_string(bestMoves[bestFuture].score));
        }
        // else
        //     log("no better move found");

        if (testGame != nullptr)
            delete testGame;
        testGame = new Game(game);
    } else {
        if (exploring == 0) { // populate first moves array (best moves from
                              // search depth = 0)
            int i = 0;
            auto it = moves.begin();
            while (it != moves.end() && i < MAX_MOVELIST_SIZE) {
                firstMoves[i] = *it;
                ++it;
                ++i;
            }
        } else {
            // populate/replace bestMoves with the results from the previous
            // search
            if (exploring == 1 || moves.front().score > bestMoves[0].score) {
                int i = 0;
                auto it = moves.begin();
                while (it != moves.end() && i < MAX_MOVELIST_SIZE) {
                    bestMoves[i] = *it;
                    ++it;
                    ++i;
                }
                bestFuture = exploring - 1;
            }

            // reset testGame to current game to prepare to execute the next
            // move. Only necessary the second time since testGame is the same
            // as current game initially
            if (testGame != nullptr)
                delete testGame;
            testGame = new Game(game);
        }

        executeMove(firstMoves[exploring]);

        exploring++;
        checking = 0;
    }

    moves.clear();
}

void Bot::executeMove(Move move) {
    if (testGame->pawn.current != move.piece) {
        if (testGame->held == move.piece ||
            testGame->queue.front() == move.piece) {
            testGame->hold(1);
            testGame->hold(0);
        }
    }

    if (testGame->pawn.rotation != move.rotation) {
        while (testGame->pawn.rotation != move.rotation) {
            testGame->rotateCW(1);
            testGame->rotateCW(0);
        }
    }

    int step = 0;
    int botStepMax = 20;
    int dx = 0;
    if (dx > move.dx) {
        while (dx != move.dx && step++ < botStepMax) {
            testGame->keyLeft(1);
            testGame->keyLeft(0);
            dx--;
        }
    } else if (dx < move.dx) {
        while (dx != move.dx && step++ < botStepMax) {
            testGame->keyRight(1);
            testGame->keyRight(0);
            dx++;
        }
    }

    if (dx == move.dx && testGame->pawn.rotation == move.rotation) {
        testGame->keyDrop(1);
    }

    if (testGame->clearLock) {
        testGame->removeClearLock();
        log("removed clearlock");
    }
}

void Bot::move() {
    if (game->dropping || game->pawn.current == -1)
        return;
    if (--sleepTimer > 0)
        return;
    else
        sleepTimer = maxSleep;

    if (game->pawn.current != current.piece) {
        // log("current: " + std::to_string(game->pawn.current) + " best: " +
        // std::to_string(current.piece));
        if (game->held == current.piece ||
            game->queue.front() == current.piece) {
            game->hold(1);
            game->hold(0);
        } else {
            log("error hold!");
        }
        return;
    }

    if (game->pawn.rotation != current.rotation) {
        while (game->pawn.rotation != current.rotation) {
            game->rotateCW(1);
            game->rotateCW(0);
            // rotation++;
        }
    }

    int step = 0;
    if (dx > current.dx) {
        while (dx != current.dx && step++ < botStepMax) {
            game->keyLeft(1);
            game->keyLeft(0);
            dx--;
        }
    } else if (dx < current.dx) {
        while (dx != current.dx && step++ < botStepMax) {
            game->keyRight(1);
            game->keyRight(0);
            dx++;
        }
    }

    if (dx == current.dx && game->pawn.rotation == current.rotation) {
        // log("dx: " + std::to_string(game->pawn.x) + " dy: " +
        // std::to_string(game->pawn.rotation)); log("drop");
        game->keyDrop(1);
        dx = 0;
        rotation = 0;
        thinking = true;
        sleepTimer = maxSleep;

        return;
    }
}

int findBestDrop(int ii, int jj, Game* game, std::list<Move>& moves,
                 Weights weights) {
    if (!game->checkRotation(ii, 0, jj))
        return 0;

    // std::string str;
    // profile_start();

    int prevR = game->pawn.rotation;
    int prevX = game->pawn.x;
    game->pawn.rotation = jj;
    game->pawn.x += ii;
    int lowest = game->lowest();

    game->pawn.rotation = prevR;
    game->pawn.x = prevX;

    const int height = 32;

    uint16_t testBoard[40];
    int columnHeights[10];
    memcpy16_fast(testBoard, &game->bitboard[8], height);
    memcpy32_fast(columnHeights, game->columnHeights, 10);

    // for(int i = 0; i < game->lengthY; i++)
    //     testBoard[i] = game->bitboard[i];
    // memcpy32_fast(testBoard[i],game->board[i+20],10);

    int minH = -1;
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int y = lowest + i;
        for (int j = 0; j < 4; j++) {
            if (game->pawn.board[jj][i][j] == 0)
                continue;

            if (minH == -1)
                minH = i;

            int x = game->pawn.x + ii + j;

            testBoard[y - 8] |= 1 << x;
            if (height - (y - 8) > columnHeights[x])
                columnHeights[x] = height - (y - 8);

            if (++count == 4)
                break;
        }
        if (count == 4)
            break;
    }

    // showTestBoard(testBoard);

    int clears =
        countClears(testBoard, game->lengthX, height, lowest + minH - 8);
    int afterHeight = min(game->stackHeight, lowest + minH) + clears;

    Values v = evaluate(testBoard, columnHeights, game->lengthX, height,
                        afterHeight - 8);

    // int wellDiff = v.wells - currentValues.wells;
    int wellDiff = v.wells;

    int score = (clears * clears) * weights.clears + v.holes * (weights.holes) +
                (game->lengthY - lowest) * weights.lowest +
                wellDiff * (weights.well) + v.rowTrans * (weights.row) +
                v.colTrans * (weights.col) + v.quadWell * (weights.quadWell);

    // char buff[64];

    // posprintf(buff,"%d %d %d %d %d %d %d %d", clears, v.holes,
    // game->lengthY-lowest, wellDiff, v.rowTrans, v.colTrans, v.quadWell,
    // v.wellX);

    // log(buff);

    // log( "score " + std::to_string(score) + " best score: " +
    // std::to_string(best.score));
    Move m;
    m.score = score;
    m.dx = ii;
    m.rotation = jj;
    m.piece = game->pawn.current;

    // ~2500 cycles
    if (moves.empty()) {
        moves.push_back(m);
    } else {
        // log("checking: " + std::to_string(m.score));
        auto it = moves.begin();
        while (it != moves.end()) {
            // log("comparing: " + std::to_string(it->score)  + " " +
            // std::to_string(m.score));
            if (it->score < m.score) {
                moves.insert(it, m);
                break;
            }
            ++it;
        }

        if (it == moves.end()) {
            moves.push_back(m);
        }

        if (moves.size() > MAX_MOVELIST_SIZE) {
            moves.pop_back();
        }
    }

    // print move list

    // std::string str;
    // for(auto const& move : moves){
    //     str += std::to_string(move.score) + " ";
    // }
    // log(str);

    return 1;
}

void showTestBoard(u16* testBoard) {
    log("testboard: ");
    for (int i = 0; i < 20; i++) {
        std::string str;

        for (int j = 0; j < 10; j++) {
            str += std::to_string((testBoard[i + 12] >> j) & 0b1);
        }

        log(str);
    }

    // OBJ_ATTR* enemyBoardSprite = &obj_buffer[20];
    // obj_unhide(enemyBoardSprite, ATTR0_AFF_DBL);
    // obj_set_attr(enemyBoardSprite, ATTR0_TALL | ATTR0_AFF_DBL, ATTR1_SIZE(2)
    // | ATTR1_AFF_ID(6), 0); enemyBoardSprite->attr2 = ATTR2_BUILD(360, 0, 1);
    // obj_set_pos(enemyBoardSprite, 43, 24);
    // obj_aff_identity(&obj_aff_buffer[6]);
    // obj_aff_scale(&obj_aff_buffer[6], float2fx(0.5), float2fx(0.5));

    // TILE* dest2;

    // for(int height = 10; height < 20; height++){
    //     for (int j = 0; j < 10; j++) {
    //         dest2 = (TILE*)&tile_mem[4][360 + ((height) / 8) * 2 + (j) / 8];

    //         if ((testBoard[30+height] >> j) & 0b1)
    //             dest2->data[(height) % 8] |= (4 + 2 *
    //             savefile->settings.lightMode) << ((j % 8) * 4);
    //         else
    //             dest2->data[(height) % 8] &= ~(0xffff << ((j % 8) * 4));
    //     }
    // }
}

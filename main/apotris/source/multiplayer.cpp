#include "def.h"

int initialPlayerCount = 0;

#ifdef GBA
#include "LinkUniversal.hpp"
#endif
#include "blockEngine.hpp"
#include "logging.h"
#include "multiplayerClasses.h"
#include "rumble.h"
#include "scene.hpp"
#include "text.h"
#include <queue>
#include <string>

using namespace BlockEngine;

void drawEnemyBoard();
u8 weightedRandomPlayerSelection();
u8 getInitialId(u8 realId);

u8 enemyBoard[4][20][10];
u32 enemyPower[4] = {1, 1, 1, 1};
u8 attackAnimationTimers[4];

const int animationMax = 15;

u32 ownPower = 1;
u8 currentPlayerId = -1;
std::queue<u16> sendQueue;

PlayerStateMutex* playAgainMutex = new PlayerStateMutex();

class BoardUpdate {
public:
    int row = 0;
    int playerId = 0;

    BoardUpdate(int r, int id) {
        row = r;
        playerId = id;
    }
};

std::vector<BoardUpdate> enemyBoardUpdates;

std::map<u8, bool> loseIndex = {
    {0, false}, {1, false}, {2, false}, {3, false}, {4, false}};

OBJ_ATTR* enemyBoardSprite;

bool multiplayerStart = false;
int rank = 0;
int connected = 0;
int currentScanHeight = 0;

Game* botGame;
int botIncomingHeight = 0;

const int boardTile = 512 - 32 * 3 - 64 - 8;

#define PLAY_AGAIN 1
#define CHANGE_STATE 2
#define SEND_ATTACK 3
#define ACK_ATTACK 4
#define SEND_ROW 5
#define ENCODE(x) ((x) << 13)

void clearEnemyBoard() {
    memset32_fast((u32*)enemyBoard, 0, sizeof(enemyBoard) / 4);
    clearSpriteTiles(boardTile, 8, 8);

    for (auto& timer : attackAnimationTimers)
        timer = 0;
}

void sendGameUpdates() {
#ifdef GBA
    // Process up to three sends per frame
    int sentCounter = 3;
    while (!sendQueue.empty() && --sentCounter) {
        u16 payload = sendQueue.front();
        bool sent = false;

        if (linkConnection->getMode() == LinkUniversal::Mode::LINK_CABLE) {
            linkConnection->send(payload);
            sent = true;
        } else if (linkConnection->getLinkWireless()->send(payload))
            sent = true;

        if (sent)
            sendQueue.pop();
    }

#endif
}

// returns true if we should terminate the game
int GameScene::handleMultiplayer(bool duringGame) {
#ifdef GBA
    if (!multiplayer)
        return 0;

    // Update remote state and fetch incoming data
    linkConnection->sync();
    u8 currentPlayerCount = linkConnection->playerCount();
    currentPlayerId = linkConnection->currentPlayerId();

    // Exit early for a later update if bad state (e.g. some player disconnects)
    if (!linkConnection->isConnected() ||
        currentPlayerCount < initialPlayerCount) {
        // If reconnect returns false, we got a connection back with all players
        if (reconnect()) {
            // Get potential new playerId
            linkConnection->sync();
            u8 newPlayerId = linkConnection->currentPlayerId();
            // Send out power and ID or lose state to map to new if changed
            u32 power = ownPower;
            if (game->lost)
                power = 1023;
            sendQueue.push(ENCODE(CHANGE_STATE) + power);
            currentPlayerId = newPlayerId;
            return 0;
        } else {
            return 1;
        }
    }

    // Game start
    if (multiplayerStart) {
        multiplayerStart = false;
        loseIndex = {
            {0, false}, {1, false}, {2, false}, {3, false}, {4, false}};
        for (auto& i : enemyPower)
            i = 1;
        playAgain = true;
        ended = false;
        clearEnemyBoard();
        rank = 0;
        ownPower = 1;
        return 0;
    }

    // Handle remote player updates
    for (int remotePlayerId = 0; remotePlayerId < initialPlayerCount;
         remotePlayerId++) {
        // Skip self
        if (remotePlayerId == currentPlayerId)
            continue;

        // Process remote player's packets
        while (linkConnection->canRead(remotePlayerId))
            ProcessMultiplayerPacket(linkConnection->read(remotePlayerId),
                                     RemotePlayerId(remotePlayerId));

        if (game->lost) {
            if (duringGame) {
                sendQueue.push((u16)ENCODE(CHANGE_STATE) + 1023);
                updateRank();
            }
        } else {
            // Attacks take priority over block state
            if (!game->attackQueue.empty()) {
                // Grab next attack
                BlockEngine::Garbage atck = game->attackQueue.front();
                // Pop it out of pending attack queue
                game->attackQueue.pop_front();
                // Encode and send out attack
                u8 targetId = getInitialId(weightedRandomPlayerSelection());
                sendQueue.push(
                    (u16)(ENCODE(SEND_ATTACK) + (targetId << 4) + atck.amount));
                ownPower += atck.amount;
            } else {
                // Rotate over the 20 rows
                if (currentScanHeight < 19)
                    currentScanHeight++;
                else
                    currentScanHeight = 0;

                // Store row as each bit on or off for the presence of each
                // block
                u16 row_bitfield = 0;
                for (int i = 0; i < 10; i++)
                    if (game->board[currentScanHeight + 20][i])
                        row_bitfield += 1 << i;

                if (duringGame)
                    // Note that board updates do not use the send queue as they
                    // are best-effort
                    linkConnection->send(
                        (u16)(ENCODE(SEND_ROW) +
                              ((currentScanHeight & 0x1f) << 10) +
                              (row_bitfield & 0x3ff)));
            }
        }
    }

    sendGameUpdates();
    drawEnemyBoard();
#endif

    return 0;
}

u8 weightedRandomPlayerSelection() {
#ifdef GBA
    // Skip 2P mode
    if (linkConnection->playerCount() == 2)
        return 0;

    // TODO: The rest of this function appears to be subtly broken at runtime,
    // hence 3+ player mode being disabled

    // Calculate the total attack power
    u32 totalAttackPower = 0;
    for (int i = 0; i < initialPlayerCount; ++i) {
        if (loseIndex[i])
            continue;
        totalAttackPower += enemyPower[i];
    }

    // Pick randomly if no power
    if (!totalAttackPower)
        return randNext() % initialPlayerCount;

    // Generate a random number between 0 and totalAttackPower
    u32 r = randNext() % totalAttackPower;

    // Determine which player is selected
    u32 accumulatedPower = 0;
    for (int i = 0; i < initialPlayerCount; ++i) {
        if (loseIndex[i])
            continue;
        accumulatedPower += enemyPower[i];
        if (r < accumulatedPower)
            return i; // Return the player ID
    }

    // This point should never be reached if the enemyBoard is not empty
#endif
    return -1; // Return an invalid player ID
}

void acknowledgeGarbage(BlockEngine::Game* sourceGame,
                        BlockEngine::Garbage garbage) {
    if (multiplayer) {
#ifdef GBA
        sendQueue.push(ENCODE(ACK_ATTACK) + (garbage.id << 4) + garbage.amount);
#endif
    } else {
        if (sourceGame == botGame) {
            attackAnimationTimers[0] = animationMax * garbage.amount;
        }
    }
}

void GameScene::updateRank() {
    rank = 0;
    for (int playerId = 0; playerId < initialPlayerCount; playerId++)
        if (!loseIndex[playerId])
            rank++;
}

// Never returns own ID
u8 getInitialId(u8 realId) {
    if (realId >= currentPlayerId) {
        realId += 1;
    }
    return realId;
}

// Function to process multiplayer packets
void GameScene::ProcessMultiplayerPacket(u16 data,
                                         RemotePlayerId remotePlayerId) {
    // Extract the command from the upper 3 bits of the data
    int command = data >> 13;
    // Extract the value from the lower 13 bits of the data
    int value = data & 0x1FFF;

    // If the command is 1, restart the game with a new seed
    if (command == PLAY_AGAIN) {
        if (value == 0x1FFF) {
            multiplayerStart = true;
            playAgainMutex->clear();
        } else {
            playAgainMutex->updatePlayerReady(remotePlayerId.id, true);
            nextSeed = value;
        }
    }
    // Command 2 means special updates
    else if (command == CHANGE_STATE)
    // If the value is 0x123, an opponent has lost
    {
        const u8 remotePlayerIndex = remotePlayerId.getIndex().index;
        if (value == 1023) {
            loseIndex[remotePlayerIndex] = true;
            if (game->lost)
                return;
            updateRank();
            if (rank == 1)
                game->setWin();
        }
        // Otherwise the value is player power level, and we were in a reconnect
        // state, so update
        else
            enemyPower[remotePlayerIndex] = value;
    }
    // If the command is between 3 and 7, process the game update
    else if (command >= SEND_ATTACK && command <= 7)
        ProcessGameUpdate(command, value, remotePlayerId);
}

// Function to process game updates
void GameScene::ProcessGameUpdate(int command, int value,
                                  RemotePlayerId remotePlayerId) {
    const u8 remotePlayerIndex = remotePlayerId.getIndex().index;
    // If the command is 3, receive an attack
    if (command == SEND_ATTACK) {
        int targetId = (value >> 4) & 0xFF;
        int attackValue = value & 0xF;
        // Record this attack to judge relative multiplayer rank
        enemyPower[remotePlayerIndex] += attackValue;
        // Add the attack to the garbage queue if it's for us
        if (targetId == currentPlayerId)
            game->addToGarbageQueue(targetId, value & 0xF);
    } else if (command == ACK_ATTACK) {
        // The player just had garbage placed on their board
        // int garbageSenderPlayerId = (value >> 4) & 0xFF;
        int attackValue = value & 0xf;
        attackAnimationTimers[remotePlayerIndex] = animationMax * attackValue;
    }
    // If the command is between 5 and 7, update the enemy board
    else
        UpdateEnemyBoard(command, value, remotePlayerId);
}

// Function to update the enemy board
void GameScene::UpdateEnemyBoard(int command, int value,
                                 RemotePlayerId remotePlayerId) {
    // Calculate the base height based on the command
    int baseHeight = (command - 5) * 8;
    // Calculate the row to be updated
    int row = (value >> 10) + baseHeight;
    // If the row is within the board's height
    if (row < 20) {
        auto id = remotePlayerId.getIndex().index;
        // Loop through each column in the row
        for (int j = 0; j < 10; j++)
            // Update the cell in the enemy board based on the value
            enemyBoard[id][row][j] = (value >> j) & 0x1;

        enemyBoardUpdates.emplace_back(row, id);
    }
}

void startMultiplayerGame(int seed) {
    clearSpriteTiles(boardTile, 2, 4);

    for (int i = 0; i < 20; i++)
        for (int j = 0; j < 10; j++)
            for (auto& k : enemyBoard)
                k[i][j] = 0;

    delete game;
    game = new Game(BATTLE, seed & 0x1fff, false);

    if (previousGameOptions == nullptr)
        previousGameOptions = new BlockEngine::Options();

    game->setOptions(*previousGameOptions);
    game->setTuning(getTuning());
    game->setLevel(1);
    game->setGoal(100);
    multiplayer = true;
    clearEnemyBoard();

    nextSeed = 0;
}

void drawEnemyBoard() {
    if (game->won || game->lost) {
        return;
    }

    bool zoom = false;
    if (multiplayer) {
#ifdef GBA
        if (linkConnection->isConnected() &&
            linkConnection->playerCount() <= 2) {
            zoom = true;
        }
#endif
    } else {
        zoom = true;
    }

    int x = 43 - 32;
    int y = 20 - 32;

    if (zoom) {
        x += 32;
        y += 32;
    }

    enemyBoardSprite = &obj_buffer[25];
    sprite_unhide(enemyBoardSprite, ATTR0_AFF_DBL);
    sprite_set_attr(enemyBoardSprite, ShapeSquare, 3, boardTile, 15, 1);
    sprite_enable_affine(enemyBoardSprite, 25, true);
    sprite_set_pos(enemyBoardSprite, x, y);
    sprite_set_size(enemyBoardSprite, int2fx(1) >> zoom, 25);

    const int margin = 3;

    int count = 1;
    if (multiplayer) {
#ifdef GBA
        if (linkConnection->isConnected()) {
            count = linkConnection->playerCount() - 1;
        }
#endif
    }

    for (int i = 0; i < count; i++) {
        OBJ_ATTR* boardFrame = &obj_buffer[26 + i];

        const int offsetX = (i % 2) * (10 + margin) + (25) * !zoom;
        const int offsetY = (i / 2) * (20 + margin) + (17) * !zoom;

        sprite_set_attr(boardFrame, ShapeTall, 2, boardTile + 64,
                        14 + (attackAnimationTimers[i] > 0), 1);
        sprite_unhide(boardFrame, ATTR0_AFF_DBL);
        sprite_enable_affine(boardFrame, 6, true);
        sprite_set_pos(boardFrame, x + offsetX - 2, y + offsetY - 2);
        sprite_set_size(boardFrame, int2fx(1) >> zoom, 6);

        if (attackAnimationTimers[i])
            attackAnimationTimers[i]--;
    }

    for (auto const& update : enemyBoardUpdates) {
        const int offsetX = (update.playerId % 2) * (10 + margin);
        const int offsetY = (update.playerId / 2) * (20 + margin);

        const int yy = update.row + offsetY;

        for (int j = 0; j < 10; j++) {
            const int xx = j + offsetX;

            if (enemyBoard[update.playerId][update.row][j])
                setSpritePixel(boardTile, xx / 8, yy / 8, 8, xx % 8, yy % 8,
                               ((enemyBoard[update.playerId][update.row][j]) +
                                1 + savefile->settings.lightMode));
            else
                setSpritePixel(boardTile, xx / 8, yy / 8, 8, xx % 8, yy % 8, 0);
        }
    }

    enemyBoardUpdates.clear();
}

int botClearTimer = 0;

void handleBotGame() {

    if (botIncomingHeight++ > 19) {
        botIncomingHeight = 0;
    }

    if (!botGame->attackQueue.empty()) {
        BlockEngine::Garbage atck = botGame->attackQueue.front();
        game->addToGarbageQueue(atck.id, atck.amount);
        botGame->clearAttack(atck.id);
    }

    if (!game->attackQueue.empty()) {
        BlockEngine::Garbage atck = game->attackQueue.front();
        botGame->addToGarbageQueue(atck.id, atck.amount);
        game->clearAttack(atck.id);
    }

    const int id = 0;

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 10; j++) {
            enemyBoard[id][i][j] = (botGame->board[i + 20][j] > 0) * 1;
        }
    }

    enemyBoardUpdates.emplace_back(botIncomingHeight, id);
    drawEnemyBoard();

    if (botGame->clearLock) {
        if (++botClearTimer > botGame->maxClearDelay) {
            botClearTimer = 0;
            botGame->removeClearLock();
        }
    }

    if (botGame->lost) {
        game->won = true;
    }
}

// returns true on reconnect
bool GameScene::reconnect() {
#ifdef GBA
    pauseSong();
    game->liftKeys();

    resetSmallText();
    clearText();

    for (auto& wordSprite : wordSprites)
        wordSprite.hide();

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

    naprint("Connection Lost", 8, 8);

    std::string str = "Attempting to Reconnect...";
    naprint(str, 120 - getVariableWidth(str) / 2, 80);

    int timer = 0;

    std::string exit = "Exit";

    while (linkConnection->playerCount() != initialPlayerCount && closed()) {
        vsync();
        key_poll();

        if (timer == 120) {
            naprint(exit, 120 - getVariableWidth(exit) / 2, 144);
        }

        if (timer >= 120) {
            if (key_hit(k.confirm)) {
                multiplayer = false;
                linkConnection->deactivate();
                connected = -1;

                sfx(SFX_MENUCANCEL);
                enableBlend(prevBld);
                playSongRandom(0);
                buildBG(3, 0, 27, 0, 1, true);
                changeScene(new TitleScene(), Transitions::FADE);
                return false;
            }

            cursorFloat += 6;
            if (cursorFloat >= 512)
                cursorFloat = 0;
            int offset = (sinLut(cursorFloat) * 2) >> 12;
            FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

            for (int i = 0; i < 2; i++) {
                sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
                sprite_enable_affine(cursorSprites[i], i, true);
                sprite_set_size(cursorSprites[i], scale, i);

                int x = 240 / 2 -
                        ((getVariableWidth(exit) + 8) / 2 + offset + 4) *
                            ((i) ? -1 : 1) -
                        8;

                sprite_set_pos(cursorSprites[i], x, 144 - 5);
            }
        }

        showSprites(128);
        linkConnection->sync();
        timer++;
    }

    clearTilemap(27);
    clearTilemap(26);
    buildBG(3, 0, 27, 0, 1, true);

    enableBlend(prevBld);

    for (auto& wordSprite : wordSprites)
        wordSprite.hide();

    for (auto& cursorSprite : cursorSprites)
        sprite_hide(cursorSprite);

    drawFrame(0);
    if (game->zoneTimer)
        frameSnow(0);

    showBackground(0);
    resetSmallText();
    clearText();
    setSmallTextArea(110, 3, 7, 9, 10);
    showText();

    if (!(game->won || game->lost)) {
        countdown();
    } else {
        endScreenSetup();
    }

    resumeSong();

#endif

    return true;
}

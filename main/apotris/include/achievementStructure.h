#include "blockEngine.hpp"
#include "def.h"

#pragma once

struct StatusData {
    [[maybe_unused]] const unsigned int primaryMagic = 1689123404;
    [[maybe_unused]] const unsigned int secondaryMagic = 2035498713;
    // Handling Settings
    [[maybe_unused]] BlockEngine::Tuning tuning;
    // In-game Settings
    [[maybe_unused]] bool *BigMode = nullptr, *ProMode = nullptr,
                          *Paused = nullptr;
    [[maybe_unused]] int *GoalLine = nullptr, *RotationSystem = nullptr,
                         *PeekAbove = nullptr, *UnpauseCountdown = nullptr;
    [[maybe_unused]] int *Randomizer = nullptr, *PlayerLost = nullptr,
                         *PlayerWon = nullptr, *Combo = nullptr;
    [[maybe_unused]] int *ZoneScore = nullptr, *GameMode = nullptr,
                         *SubMode = nullptr;
    [[maybe_unused]] unsigned int* Score = nullptr;
    // Stats
    [[maybe_unused]] BlockEngine::Stats* Stats = nullptr;
    [[maybe_unused]] int* ElapsedTime = nullptr;
    [[maybe_unused]] int* LinesSent = nullptr;
    [[maybe_unused]] BlockEngine::Options* Options;
    [[maybe_unused]] int gradeIndex;
    [[maybe_unused]] std::string gradeString;
    [[maybe_unused]] int* ZoneCharge = nullptr;
    [[maybe_unused]] int* ZoneLines = nullptr;
    [[maybe_unused]] int* ZoneTimer = nullptr;
    // Requested data
    [[maybe_unused]] int* GameLevel = nullptr;
    [[maybe_unused]] int* LinesCleared = nullptr;
    [[maybe_unused]] int* FinesseFaults = nullptr;
    [[maybe_unused]] int* FinesseStreak = nullptr;
    [[maybe_unused]] int* StackHeight = nullptr;
    [[maybe_unused]] int* GarbageHeight = nullptr;
    [[maybe_unused]] int* ActivePiece = nullptr;
    [[maybe_unused]] int* Held = nullptr;
    [[maybe_unused]] int* PawnX = nullptr;
    [[maybe_unused]] int* PawnY = nullptr;
    [[maybe_unused]] int* PawnType = nullptr;
    [[maybe_unused]] int* PawnCurrent = nullptr;
    [[maybe_unused]] int* PawnRotation = nullptr;

    // Methods for updating
    void update(Settings* settings) {
        tuning = getTuning();
        UnpauseCountdown = reinterpret_cast<int*>(&settings->pauseCountdown);
        Randomizer = &settings->randomizer;
    }
    void startGame() {
        if (game) {
            GoalLine = &game->goal;
            RotationSystem = &game->rotationSystem;
            PeekAbove = &game->peek;
            PlayerLost = &game->lost;
            PlayerWon = &game->won;
            Combo = &game->comboCounter;
            Score = &game->score;
            ZoneScore = &game->zoneScore;
            GameMode = &game->gameMode;
            SubMode = &game->subMode;
            Stats = &game->statTracker;
            ElapsedTime = &game->inGameTimer;
            LinesSent = &game->linesSent;
            ZoneCharge = &game->zoneCharge;
            ZoneLines = &game->zonedLines;
            ZoneTimer = &game->zoneTimer;
            GameLevel = &game->level;
            LinesCleared = &game->linesCleared;
            FinesseFaults = &game->finesseFaults;
            FinesseStreak = &game->finesseStreak;
            StackHeight = &game->stackHeight;
            GarbageHeight = &game->garbageHeight;
            ActivePiece = &game->activePiece;
            Held = &game->held;
            PawnX = &game->pawn.x;
            PawnY = &game->pawn.y;
            PawnType = &game->pawn.type;
            PawnCurrent = &game->pawn.current;
            PawnRotation = &game->pawn.rotation;
        }
        Paused = &paused;
        BigMode = &bigMode;
        ProMode = &proMode;
    }

} __attribute((aligned(4)));

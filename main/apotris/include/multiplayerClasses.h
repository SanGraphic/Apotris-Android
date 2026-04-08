#pragma once

#include "def.h"
#include <memory>

class RemotePlayerId;
class RemotePlayerIndex;

class RemotePlayerId {
public:
    u8 id;
    explicit RemotePlayerId(u8 playerId);
    [[nodiscard]] RemotePlayerIndex getIndex() const;
};

class RemotePlayerIndex {
public:
    u8 index;
    u8 originalId;
    RemotePlayerIndex(u8 index, u8 originalId);
    [[nodiscard]] RemotePlayerId getOriginalId() const;
};

class PlayerStateMutex {
public:
    std::map<u8, bool> locks;
    void clear();
    void updatePlayerReady(u8 remotePlayerId, bool readyState);
    int getReadyCount();
    bool checkAllReady();
    bool checkCurrentReady();
};

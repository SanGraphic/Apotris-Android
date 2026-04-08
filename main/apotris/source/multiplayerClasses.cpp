#include "multiplayerClasses.h"

RemotePlayerId::RemotePlayerId(u8 playerId) { id = playerId; }

u8 getEnemyIndex(u8 id) {
    if (id > currentPlayerId)
        id -= 1;
    return id;
}

RemotePlayerIndex RemotePlayerId::getIndex() const {
    return {getEnemyIndex(id), id};
}

RemotePlayerIndex::RemotePlayerIndex(u8 index, u8 originalId) {
    this->index = index;
    this->originalId = originalId;
}

RemotePlayerId RemotePlayerIndex::getOriginalId() const {
    return RemotePlayerId(originalId);
}

void PlayerStateMutex::clear() { this->locks.clear(); }

void PlayerStateMutex::updatePlayerReady(u8 remotePlayerId, bool readyState) {
    this->locks[remotePlayerId] = readyState;
}

bool PlayerStateMutex::checkAllReady() {
    return this->getReadyCount() == initialPlayerCount;
}

bool PlayerStateMutex::checkCurrentReady() {
    return this->locks[currentPlayerId];
}

int PlayerStateMutex::getReadyCount() {
    int readyCount = 0;
    for (int playerId = 0; playerId < initialPlayerCount; playerId++)
        if (this->locks[playerId])
            readyCount++;
    return readyCount;
}

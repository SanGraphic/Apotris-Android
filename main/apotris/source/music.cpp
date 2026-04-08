#include "def.h"
#include "logging.h"

int currentMenu = 0;
int currentlyPlayingSong = -1;

void playSong(int menuId, int songId) {
    int song = 0;

    std::list<int>* songList;

    if (menuId == 0) {
        songList = &songs.menu;
    } else if (menuId == 1) {
        songList = &songs.game;
    } else {
        return;
    }

    if (songId > (int)songList->size())
        return;

    auto it = songList->begin();
    std::advance(it, songId);

    song = *it;

    currentMenu = menuId;
    currentlyPlayingSong = songId;

    setMusicVolume(512 * ((float)savefile->settings.volume / 10));
    startSong(song, (savefile->settings.cycleSongs == 0));
}

void playSongRandom(int menuId) {

    int max = 0;

    if (menuId == 0) {
        max = (int)songs.menu.size();
    } else if (menuId == 1) {
        max = (int)songs.game.size();
    }

    if (max <= 0)
        return;

    int index = 0;
    int count = 0;

    do {
        index = (int)(randNext() % max);
    } while (
        (getSongState(menuId, (int)index) || index == currentlyPlayingSong) &&
        count++ < 1000);

    if (count >= 1000)
        return;

    playSong(menuId, (int)index);
}

void playNextSong() {
    int max = 0;

    int menuId = currentMenu;

    if (menuId == 0) {
        max = (int)songs.menu.size();
    } else if (menuId == 1) {
        max = (int)songs.game.size();
    } else {
        return;
    }

    if (max <= 0)
        return;

    int index = std::max(currentlyPlayingSong, 0);
    int count = 0;

    do {
        if (++index >= max)
            index = 0;
    } while (getSongState(menuId, index) && count++ < 1000);

    if (count >= 1000)
        return;

    playSong(menuId, (int)index);
}

void toggleSong(int group, int index, bool state) {
    int* list = nullptr;

    if (group == 0) { // MENU
        list = savefile->settings.songDisabling.menuBits;
    } else if (group == 1) { // IN-GAME
        list = savefile->settings.songDisabling.gameBits;
    }

    if (!list)
        return;

    if (state)
        list[index / 32] |= 1 << (index % 32);
    else
        list[index / 32] &= ~(1 << (index % 32));
}

bool getSongState(int group, int index) {
    int* list = nullptr;

    if (group == 0) { // MENU
        list = savefile->settings.songDisabling.menuBits;
    } else if (group == 1) { // IN-GAME
        list = savefile->settings.songDisabling.gameBits;
    }

    if (!list)
        return true;

    return (list[index / 32] >> (index % 32)) & 1;
}

void checkSongs() {
    SongDisabling* set = &savefile->settings.songDisabling;

    if ((u8)songs.menu.size() != set->menu ||
        (u8)songs.game.size() != set->game) {
        memset32_fast(set->gameBits, 0, 4);
        memset32_fast(set->menuBits, 0, 4);

        set->menu = (u8)songs.menu.size();
        set->game = (u8)songs.game.size();
    }
}

void stopMusic() {
    currentlyPlayingSong = -1;
    stopSong();
}

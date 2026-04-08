#include "logging.h"
#include "sceneAudio.hpp"

bool TrackListElement::action() {
    sfx(SFX_MENUCONFIRM);
    changeScene(new TrackListScene(), Transitions::FADE);
    return true;
}

void SongElement::select() {
    if (index < 0)
        return;
    playSong(menu, index);
}

void TrackListScene::init() {
    OptionListScene::init();

    auto it = elementList.begin();

    do {
        std::advance(it, 1);
    } while ((*it)->getCurrentOption() == "\n");

    SongElement* song = (SongElement*)*it;

    song->select();
}

bool TrackListScene::control() {
    int previous = selection;
    if (OptionListScene::control())
        return true;

    if (key_hit(savefile->settings.menuKeys.special3)) {
        setMusicTempo(slow ? 1024 : 512);
        slow = !slow;
    }

    if (selection != previous) {
        auto it = elementList.begin();
        std::advance(it, selection);

        SongElement* song = (SongElement*)*it;

        song->select();
    }

    return false;
}

std::string TrackListScene::getDescription() {
    SongElement* song;
    {
        auto it = elementList.begin();
        std::advance(it, selection);

        song = (SongElement*)*it;
    }

    std::list<int>* songList;

    if (song->menu == 0) {
        songList = &songs.menu;
    } else if (song->menu == 1) {
        songList = &songs.game;
    } else {
        return {};
    }

    if (song->index > (int)songList->size())
        return {};

    auto it = songList->begin();
    std::advance(it, song->index);

    int songIndex = *it;
    return getSongName(songIndex);
}

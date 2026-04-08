#include "platform.hpp"

#if defined(PC) || defined(PORTMASTER) || defined(WEB) || defined(SWITCH)

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iostream>

#include "soloud.h"
#include "soloud_openmpt.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

#include "def.h"

SoLoud::Soloud gSoloud;
SoLoud::Queue musicQueue;

SoLoud::Wav soundEffects[SFX_COUNT];

std::vector<SoLoud::AudioSource*> musicArray;

SoLoud::handle currentSongHandle;

std::vector<std::string> songList;
std::vector<std::string> songNameList;

int currentSong = -1;
int sfxVolume = 1024;
float musicVolume = 1;
bool looping = false;

Songs songs;

bool hasSuffix(const std::string& s, const std::string& suffix) {
    return (s.size() >= suffix.size()) &&
           equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

void loadAudio(std::string pathPrefix) {
    gSoloud.init();

    for (int i = 0; i < SFX_COUNT; i++) {
        std::string str = pathPrefix + (std::string)SoundEffectPaths[i];

        soundEffects[i].load(str.c_str());
    }

    int counter = 0;

    std::string path = pathPrefix + "assets";

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        log("couldn't open dir");
        return;
    }

    dirent* entry;
    std::vector<std::string> songPaths;
    while ((entry = readdir(dir)) != nullptr) {
        if (hasSuffix(entry->d_name, ".it") ||
            hasSuffix(entry->d_name, ".mod") ||
            hasSuffix(entry->d_name, ".s3m") ||
            hasSuffix(entry->d_name, ".S3M") ||
            hasSuffix(entry->d_name, ".mp3") ||
            hasSuffix(entry->d_name, ".ogg") ||
            hasSuffix(entry->d_name, ".flac") ||
            hasSuffix(entry->d_name, ".wav") ||
            hasSuffix(entry->d_name, ".xm")) {

            bool found = false;
            if (hasSuffix(entry->d_name, ".wav")) {
                for (int i = 0; i < SFX_COUNT; i++) {
                    std::string str = (std::string)SoundEffectPaths[i];

                    if (hasSuffix(str, entry->d_name)) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                songPaths.push_back(entry->d_name);
            }
        }
    }

    closedir(dir);

    sort(songPaths.begin(), songPaths.end());

    for (auto str : songPaths) {
        songList.push_back(path + "/" + str);
        std::string asciiName = str;
        std::replace_if(
            asciiName.begin(), asciiName.end(),
            [](char c) { return c < 32 || c >= 127; }, '?');
        songNameList.push_back(std::move(asciiName));

        if (str.find("MENU_") != std::string::npos) {
            songs.menu.push_back(counter++);
        } else {
            songs.game.push_back(counter++);
        }
    }

    for (int i = 0; i < counter; i++) {
        SoLoud::result loaded;
        std::string songPath = songList.at(i);

        if (hasSuffix(songPath, ".mp3") || hasSuffix(songPath, ".ogg") ||
            hasSuffix(songPath, ".flac") || hasSuffix(songPath, ".wav")) {
            SoLoud::WavStream* wavstream = new SoLoud::WavStream;
            loaded = wavstream->load(songPath.c_str());
            musicArray.push_back((SoLoud::AudioSource*)wavstream);
        } else {
            SoLoud::Openmpt* openmpt = new SoLoud::Openmpt;
            loaded = openmpt->load(songPath.c_str());
            musicArray.push_back((SoLoud::AudioSource*)openmpt);
        }

        if (loaded != SoLoud::SO_NO_ERROR) {
            log("couldn't load: " + songList.at(i) + " " +
                std::to_string(loaded));
        }
    }
}

void freeAudio() {
    for (auto source : musicArray) {
        delete source;
    }

    gSoloud.deinit();
}

void songEndHandler() {
    if (musicVolume == 0)
        return;

    if (currentSong != -1 &&
        !musicQueue.isCurrentlyPlaying(*musicArray.at(currentSong))) {
        if (savefile->settings.cycleSongs == 1) { // CYCLE
            playNextSong();
        } else if (savefile->settings.cycleSongs == 2) { // SHUFFLE
            playSongRandom(currentMenu);
        }
    }
}

void sfx(int n) {
    soundEffects[n].setVolume((float)savefile->settings.sfxVolume / 10);
    gSoloud.play(soundEffects[n]);
}

void startSong(int song, bool loop) {
    if (song == currentSong && !gSoloud.getPause(currentSongHandle) &&
        (currentSong != -1 &&
         musicQueue.isCurrentlyPlaying(*musicArray.at(currentSong)))) {
        return;
    }

    if (currentSong != -1 &&
        musicQueue.isCurrentlyPlaying(*musicArray.at(currentSong))) {
        musicQueue.stop();
        musicQueue.skip();
    }

    if (musicVolume > 0) {
        musicQueue.setParams(44100, 2);
        currentSongHandle = gSoloud.play(musicQueue);
        musicArray.at(song)->setLooping(loop);
        musicQueue.play(*musicArray.at(song));
    }

    currentSong = song;
    looping = loop;

    gSoloud.setVolume(currentSongHandle, musicVolume);
}

void stopSong() {
    if (currentSong == -1)
        return;

    musicQueue.stop();

    musicQueue.skip();

    currentSong = -1;
}

void resumeSong() { gSoloud.setPause(currentSongHandle, 0); }

void setMusicVolume(int volume) {
    musicVolume = volume / (512.0 / 2);

    gSoloud.setVolume(currentSongHandle, musicVolume);
}

void setMusicTempo(int tempo) {
    float nt = tempo / 1024.0;

    log(std::to_string(nt));

    gSoloud.setRelativePlaySpeed(currentSongHandle, tempo / 1024.0);
}

void sfxRate(int n, float rate) {
    soundEffects[n].setVolume((float)savefile->settings.sfxVolume / 10);
    int s = gSoloud.play(soundEffects[n]);
    gSoloud.setRelativePlaySpeed(s, rate);
}

void pauseSong() { gSoloud.setPause(currentSongHandle, 1); }

std::string getSongName(int song) { return songNameList[song]; }

#endif

#pragma once

#include "flashSaves.h"
#include "scene.hpp"
#ifdef GBA
#include "gba_flash.h"
#endif

class SaveTypeElement : public Element {
public:
    std::string getLabel() override { return "Save Type"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        std::string result;

#ifdef GBA

        if (bootleg_type)
            result = "ROM";
        else if (flashInfo.size)
            result = "FLASH";
        else if (sramInfo.size)
            result = "SRAM";
        else
            result = "ERROR!";

#endif

        return result;
    }
};

class SaveSizeElement : public Element {
public:
    std::string getLabel() override { return "Chip Size"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        std::string result;

#ifdef GBA

        if (flashInfo.size == FLASH_SIZE_64KB || sramInfo.size == 0xFFFF)
            result = "64KB";
        else if (flashInfo.size == FLASH_SIZE_128KB)
            result = "128KB";
        else if (sramInfo.size == 0x7FFF)
            result = "32KB";
        else
            result = std::to_string(sizeof(Save)) + "B";

#endif

        return result;
    }
};

class AutoSaveElement : public Element {
public:
    std::string getLabel() override { return "Autosave"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.autosave > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.autosave < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.autosave < 2) {
                savefile->settings.autosave++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.autosave > 0) {
                savefile->settings.autosave--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.autosave)
            return std::to_string(savefile->settings.autosave * 5) + "MIN";
        else
            return "OFF";
    }
};

class SavingOptionScene : public OptionListScene {
public:
    std::string name() { return "Saving"; };

    std::list<Element*> getElementList() {
        std::list<Element*> list;

#ifdef GBA
        list.push_back(new SaveTypeElement());
        list.push_back(new SaveSizeElement());
#endif

        list.push_back(new AutoSaveElement());

        return list;
    };

    Scene* previousScene() { return new SettingsScene(); };
};

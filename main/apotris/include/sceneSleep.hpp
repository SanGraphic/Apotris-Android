#pragma once

#include "scene.hpp"

#ifdef GBA

class AutoSleepElement : public Element {
public:
    std::string getLabel() override { return "Autosleep Time"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.sleep.autosleepDelay > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.sleep.autosleepDelay < 14)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.sleep.autosleepDelay < 14) {
                savefile->settings.sleep.autosleepDelay++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.sleep.autosleepDelay > 0) {
                savefile->settings.sleep.autosleepDelay--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.sleep.autosleepDelay)
            return std::to_string(get_autosleep_mins(savefile->settings.sleep.autosleepDelay)) + "MIN";
        else
            return "OFF";
    }
};

class AnyButtonWakesElement : public Element {
public:
    std::string getLabel() override { return "Autosleep Wake"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.sleep.autosleepWakeCombo > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.sleep.autosleepWakeCombo < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.sleep.autosleepWakeCombo < 2) {
                savefile->settings.sleep.autosleepWakeCombo++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.sleep.autosleepWakeCombo > 0) {
                savefile->settings.sleep.autosleepWakeCombo--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.sleep.autosleepWakeCombo) {
        case 0:
            return "Sleep Combo";
        case 1:
            return "Any Button";
        case 2:
            return "Any but L/R";
        default:
            return "";
        }
    }
};

class SleepComboElement : public Element {
public:
    std::string getLabel() override { return "Sleep Combo"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.sleep.sleepWakeCombo > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.sleep.sleepWakeCombo < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.sleep.sleepWakeCombo < 2) {
                savefile->settings.sleep.sleepWakeCombo++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.sleep.sleepWakeCombo > 0) {
                savefile->settings.sleep.sleepWakeCombo--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.sleep.sleepWakeCombo) {
        case 0:
            return "None";
        case 1:
            return "L+R+Select";
        case 2:
            return "Start+Select";
        default:
            return "";
        }
    }
};

class SleepOptionScene : public OptionListScene {
public:
    std::string name() { return "Sleep"; };
    std::list<Element*> getElementList() {
        std::list<Element*> list;

        list.push_back(new AutoSleepElement());
        list.push_back(new AnyButtonWakesElement());
        list.push_back(new SleepComboElement());

        return list;
    };

    Scene* previousScene() { return new SettingsScene(); };
};

#endif

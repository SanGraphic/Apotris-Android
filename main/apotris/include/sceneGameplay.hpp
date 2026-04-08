#pragma once

#include "scene.hpp"

class PreviewsElement : public Element {
public:
    std::string getLabel() override { return "Previews"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.maxQueue > 1)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.maxQueue < 5)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.maxQueue < 5) {
                savefile->settings.maxQueue++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.maxQueue > 1) {
                savefile->settings.maxQueue--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->settings.maxQueue);
    }
};

class BigModeElement : public Element {
public:
    std::string getLabel() override { return "Big Mode"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.big = !savefile->settings.big;
        bigMode = savefile->settings.big;
        initFallingBlocks();

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.big)
            return "ON";
        else
            return "OFF";
    }
};

class ProModeElement : public Element {
public:
    std::string getLabel() override { return "Pro Mode"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.pro = !savefile->settings.pro;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.pro)
            return "ON";
        else
            return "OFF";
    }
};

class GoalLineElement : public Element {
public:
    std::string getLabel() override { return "Goal Line"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.goalLine > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.goalLine < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.goalLine < 2) {
                savefile->settings.goalLine++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.goalLine > 0) {
                savefile->settings.goalLine--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.goalLine) {
        case 1:
            return "DOTTED";
        case 2:
            return "SOLID";
        default:
            return "OFF";
        }
    }
};

class RotationSystemElement : public Element {
public:
    std::string getLabel() override { return "Rotation System"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.rotationSystem > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.rotationSystem < BlockEngine::SDRS)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.rotationSystem < BlockEngine::SDRS) {
                savefile->settings.rotationSystem++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.rotationSystem > 0) {
                savefile->settings.rotationSystem--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.rotationSystem) {
        case BlockEngine::NRS:
            return "NRS";
        case BlockEngine::ARS:
            return "ARS";
        case BlockEngine::BARS:
            return "BARS";
        case BlockEngine::SDRS:
            return "SDRS";
        default:
            return "SRS";
        }
    }
};

class RandomizerElement : public Element {
public:
    std::string getLabel() override { return "Randomizer"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.randomizer > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.randomizer < BlockEngine::BAG_35)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.randomizer < BlockEngine::BAG_35) {
                savefile->settings.randomizer++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.randomizer > 0) {
                savefile->settings.randomizer--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.randomizer) {
        case BlockEngine::BAG_7:
            return "7-BAG";
        case BlockEngine::BAG_35:
            return "35-BAG";
        default:
            return "RANDOM";
        }
    }
};

class PeekElement : public Element {
public:
    std::string getLabel() override { return "Peek Above"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.peek = !savefile->settings.peek;

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.peek)
            return "ON";
        else
            return "OFF";
    }
};

class PauseCountdownElement : public Element {
public:
    std::string getLabel() override { return "Countdown on Unpause"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.pauseCountdown = !savefile->settings.pauseCountdown;

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.pauseCountdown)
            return "ON";
        else
            return "OFF";
    }
};

class InterruptCountdownElement : public Element {
public:
    std::string getLabel() override { return "Interrupt Countdown"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.interruptCountdown = !savefile->settings.interruptCountdown;

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.interruptCountdown)
            return "ON";
        else
            return "OFF";
    }
};

class SpawnElement : public Element {
public:
    std::string getLabel() override { return "Spawn Preview"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.showSpawn = !savefile->settings.showSpawn;

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.showSpawn)
            return "ON";
        else
            return "OFF";
    }
};

class ResetGameplayElement : public Element {
public:
    std::string getLabel() override { return "Reset Options"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        setDefaultGameplay(savefile);

        sfx(SFX_MENUCONFIRM);
        return false;
    }

    std::string getCurrentOption() override { return "_"; }
};

class GameplayOptionScene : public OptionListScene {
public:
    std::string name() { return "Gameplay"; };
    std::list<Element*> getElementList() {
        return {
            new PreviewsElement(),       new ProModeElement(),
            new PeekElement(),           new GoalLineElement(),
            new SpawnElement(),          new RotationSystemElement(),
            new RandomizerElement(),     new BigModeElement(),
            new PauseCountdownElement(), new InterruptCountdownElement(),
            new ResetGameplayElement(),
        };
    };

    Scene* previousScene() { return new SettingsScene(); };
};

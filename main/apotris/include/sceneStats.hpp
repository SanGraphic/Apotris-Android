#pragma once

#include "scene.hpp"

class PlayTimeElement : public Element {
public:
    std::string getLabel() override { return "Play Time"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return timeToStringHours(savefile->stats.timePlayed);
    }
};

class GamesStartedElement : public Element {
public:
    std::string getLabel() override { return "Games Started"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gamesStarted);
    }
};

class GamesCompletedElement : public Element {
public:
    std::string getLabel() override { return "Games Completed"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gamesCompleted);
    }
};

class TotalLinesElement : public Element {
public:
    std::string getLabel() override { return "Lines Cleared"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.totalLines);
    }
};

class SinglesElement : public Element {
public:
    std::string getLabel() override { return "Singles"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.clears[0]);
    }
};

class DoublesElement : public Element {
public:
    std::string getLabel() override { return "Doubles"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.clears[1]);
    }
};

class TriplesElement : public Element {
public:
    std::string getLabel() override { return "Triples"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.clears[2]);
    }
};

class QuadsElement : public Element {
public:
    std::string getLabel() override { return "Quads"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.clears[3]);
    }
};

class TSpinsElement : public Element {
public:
    std::string getLabel() override { return "T-Spins"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.tspins);
    }
};

class PerfectClearsElement : public Element {
public:
    std::string getLabel() override { return "Perfect Clears"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.perfectClears);
    }
};

class TimesHeldElement : public Element {
public:
    std::string getLabel() override { return "Times Held"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.holds);
    }
};

class MaxStreakElement : public Element {
public:
    std::string getLabel() override { return "Max Streak"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.maxStreak);
    }
};

class MaxComboElement : public Element {
public:
    std::string getLabel() override { return "Max Combo"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.maxCombo);
    }
};

class MaxZonedLinesElement : public Element {
public:
    std::string getLabel() override { return "Max MultiClear"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.maxZonedLines);
    }
};

class SecretGradeElement : public Element {
public:
    std::string getLabel() override { return "Max Secret Grade"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.gameStats.secretGrade);
    }
};

class HighestLevelElement : public Element {
public:
    std::string getLabel() override { return "Highest Level"; }

    std::string getCursor(std::string text) override {
        return " " + text + " ";
    }

    std::string getCurrentOption() override {
        return std::to_string(savefile->stats.maxLevel);
    }
};

class StatScene : public OptionListScene {
public:
    std::string name() { return "Stats"; };

    std::list<Element*> getElementList() {
        return {
            new PlayTimeElement(),       new GamesStartedElement(),
            new GamesCompletedElement(), new TotalLinesElement(),
            new SinglesElement(),        new DoublesElement(),
            new TriplesElement(),        new QuadsElement(),
            new TSpinsElement(),         new PerfectClearsElement(),
            new MaxStreakElement(),      new MaxComboElement(),
            new MaxZonedLinesElement(),  new SecretGradeElement(),
            new HighestLevelElement(),
        };
    };

    Scene* previousScene() { return new MainMenuScene(); };
};

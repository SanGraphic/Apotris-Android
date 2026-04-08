#pragma once

#include "scene.hpp"

class DASElement : public Element {
public:
    std::string getLabel() override { return "DAS"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (!(savefile->settings.das == 1 && savefile->settings.customDas))
            result += "<";
        else
            result += " ";

        result += text;

        if (!(savefile->settings.das == 8 && !savefile->settings.customDas))
            result += ">";

        return result;
    }

    void action(int dir) override {
        int prev = savefile->settings.das;
        int n = 0;

        if (savefile->settings.customDas) {
            n = savefile->settings.das - 30;
        } else {
            switch (savefile->settings.das) {
            case 16:
                n = 1;
                break;
            case 11:
                n = 2;
                break;
            case 9:
                n = 3;
                break;
            case 8:
                n = 4;
                break;
            }
        }

        if (n + dir <= 4 && n + dir >= -29)
            n += dir;

        if (n > 0) {
            switch (n) {
            case 1:
                n = 16;
                break;
            case 2:
                n = 11;
                break;
            case 3:
                n = 9;
                break;
            case 4:
                n = 8;
                break;
            }
            savefile->settings.customDas = false;
            savefile->settings.das = n;
        } else {
            n = 30 + n;
            savefile->settings.customDas = true;
            savefile->settings.das = n;
        }

        if (prev != savefile->settings.das)
            sfx(SFX_MENUMOVE);
    }

    std::string getCurrentOption() override {
        int n = savefile->settings.das;

        if (savefile->settings.customDas)
            return std::to_string(n) + "f";

        switch (savefile->settings.das) {
        case 16:
            return "SLOW";
            break;
        case 11:
            return "MID";
            break;
        case 9:
            return "FAST";
            break;
        case 8:
            return "V.FAST";
            break;
        default:
            return "ERROR";
            break;
        }
    }
};

class ARRElement : public Element {
public:
    std::string getLabel() override { return "ARR"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.arr < 3)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.arr > -1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.arr > -1) {
                savefile->settings.arr--;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.arr < 3) {
                savefile->settings.arr++;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.arr) {
        case -1:
            return "INSTANT";
            break;
        case 0:
            return "V.FAST";
            break;
        case 1:
            return "FAST";
            break;
        case 2:
            return "MID";
            break;
        case 3:
            return "SLOW";
            break;
        default:
            return "ERROR";
            break;
        }
    }
};

class SDFElement : public Element {
public:
    std::string getLabel() override { return "SDF"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.sfr < 3)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.sfr > -1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.sfr > -1) {
                savefile->settings.sfr--;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.sfr < 3) {
                savefile->settings.sfr++;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.sfr) {
        case -1:
            return "INSTANT";
            break;
        case 0:
            return "V.FAST";
            break;
        case 1:
            return "FAST";
            break;
        case 2:
            return "MID";
            break;
        case 3:
            return "SLOW";
            break;
        default:
            return "ERROR";
            break;
        }
    }
};

class DelaySoftDropElement : public Element {
public:
    std::string getLabel() override { return "Delay Soft Drop"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.delaySoftDrop = !savefile->settings.delaySoftDrop;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.delaySoftDrop)
            return "ON";
        else
            return "OFF";
    }
};

class DropProtectionElement : public Element {
public:
    std::string getLabel() override { return "Drop Protection"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.dropProtectionFrames > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.dropProtectionFrames < 20)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.dropProtectionFrames < 20) {
                savefile->settings.dropProtectionFrames++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.dropProtectionFrames > 0) {
                savefile->settings.dropProtectionFrames--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.dropProtectionFrames)
            return std::to_string(savefile->settings.dropProtectionFrames) +
                   "f";
        else
            return "OFF";
    }
};

class DirectionalDelayElement : public Element {
public:
    std::string getLabel() override { return "Directional Delay"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.directionalDas = !savefile->settings.directionalDas;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.directionalDas)
            return "ON";
        else
            return "OFF";
    }
};

class DisableDiagonalsElement : public Element {
public:
    std::string getLabel() override { return "Disable Diagonals"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.diagonalType < 2) {
                savefile->settings.diagonalType++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.diagonalType > 0) {
                savefile->settings.diagonalType--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.diagonalType) {
        case 0:
            return "OFF";
            break;
        case 1:
            return "SOFT";
            break;
        case 2:
            return "STRICT";
            break;
        default:
            return "ERROR";
            break;
        }
    }
};

class IHSElement : public Element {
public:
    std::string getLabel() override { return "Initial Hold"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.ihs = !savefile->settings.ihs;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.ihs)
            return "ON";
        else
            return "OFF";
    }
};

class IRSElement : public Element {
public:
    std::string getLabel() override { return "Initial Rotation"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.irs = !savefile->settings.irs;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.irs)
            return "ON";
        else
            return "OFF";
    }
};

class InitialSystemElement : public Element {
public:
    std::string getLabel() override { return "Initial System"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.initialType = !savefile->settings.initialType;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.initialType)
            return "TYPE B";
        else
            return "TYPE A";
    }
};

class ResetHandlingElement : public Element {
public:
    std::string getLabel() override { return "Reset Handling"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        setDefaultHandling(savefile);

        sfx(SFX_MENUCONFIRM);
        return false;
    }

    std::string getCurrentOption() override { return "_"; }
};

class HandlingOptionScene : public OptionListScene {
public:
    std::string name() { return "Handling"; };

    std::list<Element*> getElementList() {
        return {
            new DASElement(),
            new ARRElement(),
            new SDFElement(),
            new DelaySoftDropElement(),
            new DropProtectionElement(),
            new DirectionalDelayElement(),
            new DisableDiagonalsElement(),
            new IHSElement(),
            new IRSElement(),
            new InitialSystemElement(),
            new ResetHandlingElement(),
        };
    };

    Scene* previousScene() { return new SettingsScene(); };
    bool control();
    void init();
    void deinit();
};

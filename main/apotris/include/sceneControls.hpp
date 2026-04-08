#pragma once

#ifdef GBA
#include "gba-rumble-cart.h"
#endif

#include "scene.hpp"

class ABHoldElement : public Element {
public:
    std::string getLabel() override { return "A+B to Hold"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.abHold = !savefile->settings.abHold;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.abHold)
            return "ON";
        else
            return "OFF";
    }
};

class QuickResetElement : public Element {
public:
    std::string getLabel() override { return "Quick Restart"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (!savefile->settings.resetHoldToggle)
            result += "<";
        else
            result += " ";

        result += text;

        if (!savefile->settings.resetHoldType)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.resetHoldToggle) {
                savefile->settings.resetHoldToggle = 0;
                sfx(SFX_MENUMOVE);
            } else if (!savefile->settings.resetHoldType) {
                savefile->settings.resetHoldType = 1;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (!savefile->settings.resetHoldType) {
                savefile->settings.resetHoldToggle = 1;
                sfx(SFX_MENUMOVE);
            } else if (savefile->settings.resetHoldType) {
                savefile->settings.resetHoldType = 0;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.resetHoldToggle)
            return "OFF";

        if (savefile->settings.resetHoldType)
            return "HOLD";

        return "PRESS";
    }
};

class ResetControlsElement : public Element {
public:
    std::string getLabel() override { return "Reset Controls"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        MenuKeys temp = menuKeys;
        setDefaultKeys();
        menuKeys = savefile->settings.menuKeys;
        savefile->settings.menuKeys = temp;

        sfx(SFX_MENUCONFIRM);
        return false;
    }

    std::string getCurrentOption() override { return "_"; }
};

class RebindElement : public Element {
public:
    std::string label;
    int index = 0;
    int menu = 0;

    int* keys = nullptr;

    bool reading = false;

    int max = 0;

    std::string getLabel() override { return label; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override {
        std::list<int> foundKeys;

        reading = true;

        bool exit = false;

        while (closed()) {
            canDraw = 1;
            vsync();
            key_poll();

            if (exit)
                break;

            // if(key_hit(savefile->settings.menuKeys.pause)){
            //     sfx(SFX_MENUCANCEL);
            //     exit = true;
            //     continue;
            // }

#if defined(GBA) || defined(N3DS)

            u32 key = key_hit(KEY_FULL);

            if (key != 0) {
                std::list<int> currentKeys;

                u32 temp = key;

                int counter = 0;
                do {
                    if (temp & (1 << counter)) {
                        currentKeys.push_back(1 << counter);
                        temp &= ~(1 << counter);
                    }

                    counter++;
                } while (temp != 0);

                for (int i = 0; i < max; i++) {
                    foundKeys.clear();

                    u32 k = keys[i];
                    counter = 0;
                    do {
                        if (k & (1 << counter)) {
                            foundKeys.push_back(1 << counter);
                            k &= ~(1 << counter);
                        }

                        counter++;
                    } while (k != 0);

                    for (auto const& cur : currentKeys) {
                        if (std::find(foundKeys.begin(), foundKeys.end(),
                                      (int)cur) != foundKeys.end())
                            keys[i] -= cur;
                    }
                }

                keys[index] = key;
                sfx(SFX_MENUCONFIRM);
                exit = true;
                continue;
            }

            // #elif defined(PC)

            // bool found = false;
            // for(int i = 0; i < sf::Keyboard::KeyCount; i++){
            //     if(key_hit(i)){
            //         keys[index] = i;
            //         sfx(SFX_MENUCONFIRM);
            //         found = true;

            //         for(int j = 0; j < max; j++){
            //             if(j == index)
            //                 continue;

            //             if(keys[j] == i)
            //                 keys[j] = KEY_FULL - 2;
            //         }

            //         break;
            //     }
            // }

            // if(found){
            //     exit = true;
            //     continue;
            // }

#elif defined(PC)
            u32 key = key_first();

            if (key != KEY_FULL - 1) {
                setKey(keys[index], key);

                for (int j = 0; j < max; j++) {
                    if (j == index)
                        continue;

                    unbindDuplicateKey(keys[j], key);
                }

                sfx(SFX_MENUCONFIRM);
                exit = true;
                continue;
            }

#else
            u32 key = key_first();

            if (key != KEY_FULL - 1) {
                keys[index] = key;

                for (int j = 0; j < max; j++) {
                    if (j == index)
                        continue;

                    if (keys[j] == key)
                        keys[j] = KEY_FULL - 2;
                }

                sfx(SFX_MENUCONFIRM);
                exit = true;
                continue;
            }

#endif
        }

        reading = false;

        return false;
    }

    std::string getCurrentOption() override {
        if (reading) {
            return "...";
#ifdef N3DS
        } else if (keys[index] == UNBOUND) {
#else
        } else if (keys[index] < 0) {
#endif
            return "";
        }

        return getStringFromKey(keys[index]);
    }

    void reset() {
        if (menu == 0) {
            MenuKeys k = getDefaultMenuKeys();

            int* src = (int*)&k;
            keys[index] = src[index];
        } else if (menu == 1) {
            GameKeys k = getDefaultGameKeys();

            int* src = (int*)&k;
            keys[index] = src[index];
        }

        for (int i = 0; i < max; i++) {
            if (i == index)
                continue;

            if (keys[i] == keys[index])
                keys[i] = KEY_FULL - 2;
        }

        sfx(SFX_MENUCONFIRM);
    }

    RebindElement(int* _keys, int _menu, int _index, std::string _label,
                  int _max) {
        keys = _keys;
        menu = _menu;
        index = _index;
        label = _label;
        max = _max;
    }
};

class GBPSupportElement : public Element {
public:
    std::string getLabel() override { return "Enable GBP Support"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.gameBoyPlayerSupport = !savefile->settings.gameBoyPlayerSupport;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.gameBoyPlayerSupport)
            return "ON";
        else
            return "OFF";
    }
};

class RumbleStyleElement : public Element {
public:
    std::string getLabel() override { return "Rumble Style"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.rumbleStyle > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.rumbleStyle < 1)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (savefile->settings.rumbleStyle > 1) {
            savefile->settings.rumbleStyle = 1;
            sfx(SFX_MENUMOVE);
            return;
        }
        if (dir > 0) {
            if (savefile->settings.rumbleStyle < 1) {
                savefile->settings.rumbleStyle++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.rumbleStyle > 0) {
                savefile->settings.rumbleStyle--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.rumbleStyle) {
            case 0:
                return "SIMPLE";
            case 1:
                return "DYNAMIC";
            default:
                return "ERROR";
        }
    }
};

class TestRumbleElement : public Element {
public:
    std::string getLabel() override { return "Test Rumble"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        if (dir > 0) {
            if (rumbleTestPattern < 9) {
                rumbleTestPattern++;
                sfx(SFX_MENUMOVE);
            }
        } else if (rumbleTestPattern) {
            if (rumbleTestPattern > 0) {
                rumbleTestPattern--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    bool action() override {
#ifdef GBA
        initRumbleCart();
#endif

        switch (rumbleTestPattern) {
        case 0:
            rumblePattern(RUMBLE_PLACE);
            break;
        case 1:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_DOUBLE) : rumblePattern(RUMBLE_CLASSIC_CLEAR);
            break;
        case 2:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_DOUBLE) : rumblePattern(RUMBLE_CLASSIC_CLEAR);
            break;
        case 3:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_TRIPLE) : rumblePattern(RUMBLE_CLASSIC_CLEAR);
            break;
        case 4:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_QUAD) : rumblePattern(RUMBLE_CLASSIC_CLEAR);
            break;
        case 5:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_QUAD_B2B) : rumblePattern(RUMBLE_CLASSIC_CLEAR, 1);
            break;
        case 6:
            savefile->settings.rumbleStyle == 1 ? rumblePattern(RUMBLE_OCTORIS) : rumblePattern(RUMBLE_CLASSIC_CLEAR);
            break;
        case 7:
            if (savefile->settings.zoneRumbleMode == 1) {
                rumblePattern(RUMBLE_LONG_ZONE);
            } else if (savefile->settings.zoneRumbleMode == 2) {
                rumblePattern(RUMBLE_ZONE_RIPPLE, 0);
            }
            break;
        case 8:
            if (savefile->settings.zoneRumbleMode == 1) {
                rumblePattern(RUMBLE_LONG_ZONE_2);
            } else if (savefile->settings.zoneRumbleMode == 2) {
                rumblePattern(RUMBLE_ZONE_RIPPLE, 1);
            }
            break;
        case 9:
            if (savefile->settings.zoneRumbleMode == 1) {
                rumblePattern(RUMBLE_LONG_ZONE_3);
            } else if (savefile->settings.zoneRumbleMode == 2) {
                rumblePattern(RUMBLE_ZONE_RIPPLE, 2);
            }
            break;

        default:
            rumblePattern(RUMBLE_PLACE);
        }

        sfx(SFX_MENUCONFIRM);

        return false;
    }

    std::string getCurrentOption() override {
        return "PATT #" + std::to_string(rumbleTestPattern + 1);
    }
private:
    u8 rumbleTestPattern = 0;
};

class ZoneRumbleElement : public Element {
public:
    std::string getLabel() override { return "Zone Rumble Mode"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.zoneRumbleMode > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.zoneRumbleMode < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (savefile->settings.zoneRumbleMode > 2) {
            savefile->settings.zoneRumbleMode = 2;
            sfx(SFX_MENUMOVE);
            return;
        }
        if (dir > 0) {
            if (savefile->settings.zoneRumbleMode < 2) {
                savefile->settings.zoneRumbleMode++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.zoneRumbleMode > 0) {
                savefile->settings.zoneRumbleMode--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.zoneRumbleMode) {
            case 0:
                return "OFF";
            case 1:
                return "CONT.";
            case 2:
                return "RIPPLE";

            default:
                return "ERROR";
        }
    }
};
#ifdef GBA
class RumbleCartTypeElement : public Element {
public:
    std::string getLabel() override { return "Rumble Cart Type"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.rumbleCartType > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.rumbleCartType < 2)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (savefile->settings.rumbleCartType > 2) {
            savefile->settings.rumbleCartType = 2;
            sfx(SFX_MENUMOVE);
            return;
        }
        if (dir > 0) {
            if (savefile->settings.rumbleCartType < 2) {
                savefile->settings.rumbleCartType++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.rumbleCartType > 0) {
                savefile->settings.rumbleCartType--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.rumbleCartType) {
            case 0:
                return "TYPE A";
            case 1:
                return "TYPE B";
            case 2:
                return "TYPE C";

            default:
                return "ERROR";
        }
    }
};
class RumbleUpdateRateElement : public Element {
public:
    std::string getLabel() override { return "Patt. Update Rate"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.rumbleUpdateRate > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.rumbleUpdateRate < 4)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (savefile->settings.rumbleUpdateRate > 4) {
            savefile->settings.rumbleUpdateRate = 4;
            sfx(SFX_MENUMOVE);
            return;
        }
        if (dir > 0) {
            if (savefile->settings.rumbleUpdateRate < 4) {
                savefile->settings.rumbleUpdateRate++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.rumbleUpdateRate > 0) {
                savefile->settings.rumbleUpdateRate--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.rumbleUpdateRate) {
            case 0:
                return "60Hz";
            case 1:
                return "128Hz";
            case 2:
                return "256Hz";
            case 3:
                return "512Hz";
            case 4:
                return "1024Hz";
            default:
                return "ERROR";
        }
    }
};
class EZFlashStrengthElement : public Element {
public:
    std::string getLabel() override { return "EZFlash Motor"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.ezFlash3in1Strength > EZFLASH_RUMBLE_IGNORE)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.ezFlash3in1Strength < EZFLASH_MAX_RUMBLE)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (savefile->settings.ezFlash3in1Strength < EZFLASH_RUMBLE_IGNORE || savefile->settings.ezFlash3in1Strength > EZFLASH_MAX_RUMBLE) {
            savefile->settings.ezFlash3in1Strength = EZFLASH_RUMBLE_IGNORE;
            sfx(SFX_MENUMOVE);
            return;
        }
        if (dir > 0) {
            if (savefile->settings.ezFlash3in1Strength < EZFLASH_MAX_RUMBLE) {
                savefile->settings.ezFlash3in1Strength++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.ezFlash3in1Strength > EZFLASH_RUMBLE_IGNORE) {
                savefile->settings.ezFlash3in1Strength--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        switch (savefile->settings.ezFlash3in1Strength) {
            case EZFLASH_MIN_RUMBLE:
                return "MIN";
            case EZFLASH_MED_RUMBLE:
                return "MED";
            case EZFLASH_MAX_RUMBLE:
                return "MAX";

            case EZFLASH_RUMBLE_IGNORE:
                return "IGNORE";

            default:
                return "ERROR";
        }
    }
};
#endif
class RumbleStrengthElement : public Element {
public:
    std::string getLabel() override { return "Rumble Strength"; }

    std::string getCursor(std::string text) override {
        std::string result;
        if (savefile->settings.rumbleStrength > 0)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.rumbleStrength < 8)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0) {
            if (savefile->settings.rumbleStrength < 8) {
                savefile->settings.rumbleStrength++;
                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.rumbleStrength > 0) {
                savefile->settings.rumbleStrength--;
                sfx(SFX_MENUMOVE);
            }
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.rumbleStrength)
            return std::to_string(savefile->settings.rumbleStrength) + "/8";
        else
            return "OFF";
    }
};

class RumbleMenuElement : public Element {
public:
    std::string getLabel() override { return "Rumble"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override;

    std::string getCurrentOption() override { return "..."; }
};

class AdvancedRumbleMenuElement : public Element {
public:
    std::string getLabel() override { return "Adv. Rumble"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    bool action() override;

    std::string getCurrentOption() override { return "..."; }
};

class ControlOptionScene : public OptionListScene {
public:
    std::string name() { return "Controls"; };

    std::list<Element*> getElementList() {
        std::list<Element*> list;

#if defined(PC) || defined(WEB)
        list.push_back(new LabelElement("In-Game:"));
#endif

        int counter = 0;
        for (auto it = controlOptions.begin(); it != controlOptions.end(); ++it)
            list.push_back(new RebindElement((int*)&savefile->settings.keys, 1,
                                             counter++, *it, 9));

        list.push_back(new ABHoldElement());
        list.push_back(new QuickResetElement());

#if defined(GBA) || defined(SWITCH) || defined(MM) || defined(PORTMASTER) || defined(PC)
        list.push_back(new RumbleMenuElement());
#endif

#if defined(PC) || defined(WEB)

        list.push_back(new LabelElement("Menu:"));

        counter = 0;
        for (auto it = menuControlOptions.begin();
             it != menuControlOptions.end(); ++it)
            list.push_back(
                new RebindElement((int*)&menuKeys, 0, counter++, *it, 11));

#endif

        list.push_back(new ResetControlsElement());

        return list;
    };

    bool control();

    Scene* previousScene() { return new SettingsScene(); };
};

#if defined(GBA)
class AdvancedRumbleOptionScene : public OptionListScene {
public:
    std::string name() { return "Adv. Rumble"; };
    std::list<Element*> getElementList() {
        std::list<Element*> list;

    list.push_back(new TestRumbleElement());
    list.push_back(new RumbleStrengthElement());
    list.push_back(new ZoneRumbleElement());
    list.push_back(new RumbleStyleElement());

#if defined(GBA)
        list.push_back(new GBPSupportElement());
        list.push_back(new RumbleCartTypeElement());
        list.push_back(new EZFlashStrengthElement());
        list.push_back(new RumbleUpdateRateElement());
#endif

        return list;
    };

    Scene* previousScene() { return new ControlOptionScene(); };
};
#endif

class RumbleOptionScene : public OptionListScene {
public:
    std::string name() { return "Rumble"; };
    std::list<Element*> getElementList() {
        std::list<Element*> list;

    list.push_back(new TestRumbleElement());
    list.push_back(new RumbleStrengthElement());
    list.push_back(new ZoneRumbleElement());
    list.push_back(new RumbleStyleElement());

#if defined(GBA)
        list.push_back(new GBPSupportElement());
        list.push_back(new AdvancedRumbleMenuElement());
#endif

        return list;
    };

    Scene* previousScene() { return new ControlOptionScene(); };
};

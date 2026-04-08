#pragma once

#include "scene.hpp"

#if defined(PC) || defined(PORTMASTER)
#include "liba_pc.h"
#include "shader.h"
#include <sstream>
#include <string>
#endif

#ifdef N3DS

class MainScreenElement : public Element {
public:
    std::string getLabel() override { return "Main Screen"; }

    std::string getCursor(std::string text) override {
        return "<" + text + ">";
    }

    void action(int dir) override {
        savefile->settings.n3dsMainScreenIsTop =
            !savefile->settings.n3dsMainScreenIsTop;
        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.n3dsMainScreenIsTop)
            return "Top";
        else
            return "Bottom";
    }
};

class ScaleSelectElement : public Element {
    const n3ds::ScaleMode scale;

public:
    ScaleSelectElement(n3ds::ScaleMode scale) : scale{scale} {}

    std::string getLabel() override {
        switch (scale) {
        case n3ds::ScaleMode::UNSCALED:
            return "1x";
        case n3ds::ScaleMode::SCALED_LINEAR:
            return "1.5x smooth";
        case n3ds::ScaleMode::SCALED_SHARP:
            return "1.5x sharp";
        case n3ds::ScaleMode::SCALED_ULTRA:
            return "1.5x ultra";
        case n3ds::ScaleMode::NUM_ENTRIES:
        default:
            return "?";
        }
    }

    std::string getCursor(std::string text) override {
        if (savefile->settings.n3dsScaleMode == scale) {
            return ">" + text + "<";
        } else if (scale == n3ds::ScaleMode::SCALED_ULTRA &&
                   !n3ds::wideModeSupported) {
            return " " + text + " ";
        } else {
            return "[" + text + "]";
        }
    }

    bool action() override {
        if (savefile->settings.n3dsScaleMode != scale) {
            if (scale == n3ds::ScaleMode::SCALED_ULTRA &&
                !n3ds::wideModeSupported) {
                sfx(SFX_INVALID);
            } else {
                savefile->settings.n3dsScaleMode = scale;
                sfx(SFX_MENUCONFIRM);
            }
        }
        return false;
    }

    std::string getCurrentOption() override {
        if (savefile->settings.n3dsScaleMode == scale) {
            return "In use";
        } else if (scale == n3ds::ScaleMode::SCALED_ULTRA &&
                   !n3ds::wideModeSupported) {
            return "-";
        } else {
            return ">";
        }
    }
};

#else /* N3DS */

class IntegerScaleElement : public Element {
public:
    std::string getLabel() override { return "Integer Scale"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.integerScale = !savefile->settings.integerScale;
        sfx(SFX_MENUCONFIRM);

        if (savefile->settings.integerScale && savefile->settings.zoom > -1) {
            savefile->settings.zoom =
                (savefile->settings.zoom / 10) * 10; // round to 10s
        }

#if defined(PC) || defined(WEB) || defined(PORTMASTER) || defined(SWITCH)
        refreshWindowSize();
#endif
    }

    std::string getCurrentOption() override {
        if (savefile->settings.integerScale)
            return "ON";
        else
            return "OFF";
    }
};

class ScaleElement : public Element {
public:
    const int min = -1;
    const int max = 90;
    std::string getLabel() override { return "Scale"; }

    std::string getCursor(std::string text) override {
        std::string result;

        if (savefile->settings.zoom > min)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.zoom < max)
            result += ">";

        return result;
    }

    void action(int dir) override {
        int add = 1 + 9 * (savefile->settings.integerScale &&
                           savefile->settings.zoom > -1);
        if (dir > 0) {
            if (savefile->settings.zoom < max) {
                savefile->settings.zoom += add;

                if (savefile->settings.zoom > max)
                    savefile->settings.zoom = max;

                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.zoom > min) {
                savefile->settings.zoom -= add;

                if (savefile->settings.zoom < min)
                    savefile->settings.zoom = min;

                sfx(SFX_MENUMOVE);
            }
        }

#if defined(PC) || defined(WEB) || defined(PORTMASTER) || defined(SWITCH)
        refreshWindowSize();
#endif
    }

    std::string getCurrentOption() override {
        if (savefile->settings.zoom > -1) {
            return "x" + std::to_string((savefile->settings.zoom / 10) + 1) +
                   "." + std::to_string(savefile->settings.zoom % 10);
        } else {
            return "AUTO";
        }
    }
};

#endif /* N3DS */

class FPSElement : public Element {
public:
    std::string getLabel() override { return "Show FPS"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.showFPS = !savefile->settings.showFPS;

        if (!savefile->settings.showFPS)
            clearText();

        sfx(SFX_MENUCONFIRM);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.showFPS)
            return "ON";
        else
            return "OFF";
    }
};

#ifdef PC
class FullscreenElement : public Element {
public:
    std::string getLabel() override { return "Fullscreen"; }

    std::string getCursor(std::string text) override {
        return "[" + text + "]";
    }

    void action(int dir) override {
        savefile->settings.fullscreen = !savefile->settings.fullscreen;
        sfx(SFX_MENUCONFIRM);

        setFullscreen(savefile->settings.fullscreen);
    }

    std::string getCurrentOption() override {
        if (savefile->settings.fullscreen)
            return "ON";
        else
            return "OFF";
    }
};
#endif

#if defined(PC) || defined(PORTMASTER)
class ShaderElement : public Element {
public:
    const std::vector<std::string> shaders = findShaders();
    const int min = 0;
    const int max = shaders.size();

    std::string getLabel() override { return "Shaders"; }

    std::string getCursor(std::string text) override {
        if (savefile->settings.shaders != 0 && shaderStatus != ShaderStatus::OK)
            return "<" + text;

        std::string result;

        if (savefile->settings.shaders > min)
            result += "<";
        else
            result += " ";

        result += text;

        if (savefile->settings.shaders < max)
            result += ">";

        return result;
    }

    void action(int dir) override {
        if (dir > 0 && savefile->settings.shaders != 0 &&
            shaderStatus != ShaderStatus::OK) {
            sfx(SFX_MENUCANCEL);
            return;
        }

        if (dir > 0) {
            if (savefile->settings.shaders < max) {
                savefile->settings.shaders++;

                if (savefile->settings.shaders > max)
                    savefile->settings.shaders = max;

                sfx(SFX_MENUMOVE);
            }
        } else {
            if (savefile->settings.shaders > min) {
                savefile->settings.shaders--;

                if (savefile->settings.shaders < min)
                    savefile->settings.shaders = min;

                sfx(SFX_MENUMOVE);
            }
        }

        if (savefile->settings.shaders) {
            shaderInit(savefile->settings.shaders);
            refreshWindowSize();
        } else {
            shaderDeinit();
        }
    }

    std::string getCurrentOption() override {
        if (savefile->settings.shaders != 0) {
            switch (shaderStatus) {
            case ShaderStatus::NOT_INITED:
                break;
            case ShaderStatus::NO_GL:
                return "NO GL";
            case ShaderStatus::NO_SHADER_FILES:
                return "NONE";
            case ShaderStatus::SHADER_FILE_ERROR:
                return "ERROR";
            case ShaderStatus::OK:
                break;
            }
        }

        if (savefile->settings.shaders) {
            std::stringstream shaderPath;
            shaderPath << shaders.at(savefile->settings.shaders - 1);

            std::string name;

            while (getline(shaderPath, name, '/'))
                ;

            return remove_extension(name);
        } else {
            return "OFF";
        }
    }

private:
    std::string remove_extension(const std::string& filename) {
        size_t lastdot = filename.find_last_of(".");
        if (lastdot == std::string::npos)
            return filename;
        return filename.substr(0, lastdot);
    }
};
#endif

class WindowOptionScene : public OptionListScene {
public:
    std::string name() { return "Video"; };
    std::list<Element*> getElementList() {
        std::list<Element*> list;

#ifdef PC
        list.push_back(new FullscreenElement);
#endif

#ifdef N3DS
        list.push_back(new MainScreenElement());
        list.push_back(new LabelElement("Scaling mode:"));
        list.push_back(new ScaleSelectElement(n3ds::ScaleMode::UNSCALED));
        list.push_back(new ScaleSelectElement(n3ds::ScaleMode::SCALED_LINEAR));
        list.push_back(new ScaleSelectElement(n3ds::ScaleMode::SCALED_SHARP));
        list.push_back(new ScaleSelectElement(n3ds::ScaleMode::SCALED_ULTRA));
        list.push_back(new LabelElement("Diagnostics:"));
#else
        list.push_back(new IntegerScaleElement());
        list.push_back(new ScaleElement());
#endif
        list.push_back(new FPSElement());

#if defined(PC) || defined(PORTMASTER)
        list.push_back(new ShaderElement());
#endif

        return list;
    };

#ifdef N3DS
private:
    int previousAspectRatio = savefile->settings.aspectRatio;

public:
    void draw() {
        if (previousAspectRatio != savefile->settings.aspectRatio) {
            previousAspectRatio = savefile->settings.aspectRatio;
            // HACK: Redraw the path to make it match the aspect
            // ratio.
            showPath();
            // Clear the FPS text region:
            aprintClearArea(30 - 5 - 2, 0, 5 + 2, 1);
        }
        OptionListScene::draw();
    }
#endif

    Scene* previousScene() { return new SettingsScene(); };
};

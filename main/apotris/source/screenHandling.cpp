#include "sceneHandling.hpp"

bool HandlingOptionScene::control() {
    OptionListScene::control();

    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.special2)) {
        handlingTest = true;

        BlockEngine::Options options;
        options.tuning = getTuning();

        startGame(options, (int)randNext());

        // remove 'Handling' from path
        path.pop_back();

        changeScene(new GameScene(), Transitions::FADE);
    }

    return false;
}

void HandlingOptionScene::init() {
    OptionListScene::init();

    int offset = 0;
    if (savefile->settings.aspectRatio == 1) {
        offset = (240 - 214) / 2;
    }

    setSmallTextArea(110, 0, 17, 15, 20);
    clearText();

    std::string test =
        "Test: " + getStringFromKey(savefile->settings.menuKeys.special2);

    aprints(test, 1 + offset, 8, 2);
}

void HandlingOptionScene::deinit() {
    resetSmallText();
    OptionListScene::deinit();
}

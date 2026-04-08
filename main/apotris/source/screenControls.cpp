#include "sceneControls.hpp"

bool ControlOptionScene::control() {
    OptionListScene::control();

    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.special3)) {
        auto it = elementList.begin();
        std::advance(it, selection);

        RebindElement* el = (RebindElement*)*it;

        el->reset();

        refreshOption = true;
    }

    return false;
}

bool RumbleMenuElement::action() {
    sfx(SFX_MENUCONFIRM);
    changeScene(new RumbleOptionScene(), Transitions::FADE);

    return true;
}

bool AdvancedRumbleMenuElement::action() {
    sfx(SFX_MENUCONFIRM);
    if (path.size())
        path.pop_back();

#ifdef GBA
    changeScene(new AdvancedRumbleOptionScene(), Transitions::FADE);
#endif

    return true;
}

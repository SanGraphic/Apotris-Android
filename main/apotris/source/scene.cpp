#include "scene.hpp"
#include "def.h"
#include "logging.h"
#include <list>
#include <string>

#include "sprite41tiles_bin.h"
#include "sprites.h"

void SimpleListScene::init() {
    reset();

    clearSprites(128);

    optionList = getOptionList();
    options = (int)optionList.size();

    int index = -1;
    int counter = 0;
    for (auto option : optionList) {
        if (option == previousElement) {
            index = counter;
            break;
        }
        counter++;
    }

    if (index >= 0) {
        selection = index;

        while (selection > listStart + elementMax)
            listStart++;
    } else {
        selection = 0;
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    startY = 40;

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i] = new WordSprite(i, 64 + i * 3, 256 + i * 12);

    for (int i = 0; i < 3; i++)
        scrollingText[i] =
            new WordSprite(MAX_WORD_SPRITES + i,
                           64 + MAX_WORD_SPRITES * 3 + i * 3, 672 + i * 12);

    loadSpriteTiles(1, sprite41tiles_bin, 2, 1);
    for (int i = 0; i < 2; i++) {
        arrowSprites[i] = &obj_buffer[3 + i];
        sprite_set_attr(arrowSprites[i], ShapeWide, 0, 1, 14, 0);

        if (i)
            sprite_enable_flip(arrowSprites[1], false, true);

        sprite_hide(arrowSprites[i]);
    }

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[1 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    for (int i = 0; i < 2; i++) {
        scrollSideSprites[i] = &obj_buffer[10 + i];
        sprite_set_attr(scrollSideSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
        sprite_enable_affine(scrollSideSprites[i], i + 2, true);
        sprite_set_pos(scrollSideSprites[i],
                       240 / 2 + ((i * 2) - 1) * (width / 2 + 16), 152 - 4);
        sprite_set_size(scrollSideSprites[i], float2fx(0.8), i + 2);
        sprite_hide(scrollSideSprites[i]);
    }

    enableBlend((0b101111 << 8) + (1 << 6) + (1 << 3));

    if (!name().empty()) {
        bool found = false;
        unsigned int count = 0;
        auto it = path.begin();
        while (it != path.end()) {
            if (*it == name()) {
                found = true;
                break;
            }
            count++;
            it++;
        }

        if (found) {
            unsigned int n = path.size() - (count + 1);
            for (int i = 0; i < n; i++)
                path.pop_back();
        } else {
            path.push_back(name());
        }
    }

    std::string p;
    int i = 0;
    auto it = path.begin();
    while (it != path.end()) {
        if (i != 0)
            p += " > ";

        p += *it;

        it++;
        i++;
    }

    int count = (getVariableWidth(p) >> 3) / 12 + 1;

    int wordIndex = MAX_WORD_SPRITES - count - 1;

    wordSprites[wordIndex]->setText(p);

    for (int i = 0; i < count; i++)
        wordSprites[wordIndex + i]->show(
            i * 12 * 8 + 4 + 13 * savefile->settings.aspectRatio, 4, 14);

    clearSpriteTiles(672, 144, 1);

    for (int i = 0; i < 3; i++)
        scrollingText[i]->show(12 * 8 * i, 160 - 8, 15);

    scrollingText[0]->setTextSlow(scrollText);
}

void SimpleListScene::draw() {
    fallingBlocks();
    toggleBG(3, true);
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif

    const int space = 1;

    int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    int verticalOffset = 0;
    if (moving) {
        int temp =
            lerp((movingDirection) * (space + 1) * 8, 0, 64 * movingTimer);
        verticalOffset = lerp(temp, 0, 64 * movingTimer);
    }

    int i = 0;       // drawing index, starts at listStart
    int counter = 0; // element index
    for (auto const& option : optionList) {
        if (counter < listStart) {
            counter++;
            continue;
        }

        if (i > elementMax + 1) { // break if elements drawn reached max
            continue;
        }

        int index = (i + listStart) % (elementMax + 2);

        if (refreshText)
            wordSprites[index]->setText(option);

        int len = getVariableWidth(option);
        int x = (240 - len) / 2;

        int y = startY + 8 * (space + 1) * i + verticalOffset;

        wordSprites[index]->setPriority(0);
        if (counter == selection) {
            currentOption = option;
            for (int i = 0; i < 2; i++) {
                sprite_unhide(cursorSprites[i], 0);
                sprite_enable_affine(cursorSprites[i], i, true);
                sprite_set_size(cursorSprites[i], scale, i);

                int x =
                    240 / 2 - ((len + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

                sprite_set_pos(cursorSprites[i], x, y - 5);
            }
        }

        wordSprites[index]->show(
            x, y,
            15 - (counter != selection) -
                (i == elementMax + 1 || (listStart && i == 0)));

        i++;
        counter++;
    }

    for (int j = i; j < elementMax + 2; j++) {
        int index = (j + listStart) % (elementMax + 2);

        wordSprites[index]->hide();
    }

    if (listStart > 0) {
        sprite_unhide(arrowSprites[0], 0);
        sprite_set_pos(arrowSprites[0], 240 / 2 - 8, startY - 16);
    } else {
        sprite_hide(arrowSprites[0]);
    }

    if (listStart + elementMax + 2 < (int)optionList.size()) {
        sprite_unhide(arrowSprites[1], 0);
        sprite_set_pos(arrowSprites[1], 240 / 2 - 8,
                       (elementMax + 2) * (1 + space) * 8 + startY + 0);
    } else {
        sprite_hide(arrowSprites[1]);
    }

    if (refreshText) {
        scrollText = getDescription();
        clearSpriteTiles(672, 144, 1);
        if (scrollText != "") {
            scrollTextLength = getVariableWidth(scrollText);

            const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

            endDelay = 0;
            scrollOffset = -max((width - scrollTextLength), 0) / 2;
            scrollDelay = 0;
            scrollingText[0]->setTextSlow(scrollText);

#if defined(TE) || defined(N3DS)
            for (int i = 0; i < 2; i++) {
                sprite_unhide(scrollSideSprites[i], ATTR0_AFF);
                sprite_set_pos(scrollSideSprites[i],
                               240 / 2 +
                                   ((i * 2) - 1) *
                                       (min(scrollTextLength, width) / 2) -
                                   16 * (i == 0),
                               152 - 4);
            }
#endif
        } else {
            for (int i = 0; i < 2; i++) {
                sprite_hide(scrollSideSprites[i]);
            }
        }
    }

    refreshText = false;

    showSprites(128);
}

bool SimpleListScene::control() {

    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.confirm)) {
        sfx(SFX_MENUCONFIRM);
        auto it = optionList.begin();
        std::advance(it, selection);

        sceneSwitcher(*it);

        previousElement = "";
        return true;
    }

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        previousElement = name();
        previousSelection = selection;

        if (path.size())
            path.pop_back();

        changeScene(previousScene(), Transitions::FADE);
        return true;
    }

    int prev = listStart;

    if (key_hit(k.up)) {
        movingDirection = -1;

        if (selection == 0)
            selection = options - 1;
        else
            selection--;

        if (selection == options - 1)
            listStart = max(0, selection - elementMax);
        else if (selection < listStart + 1) {
            listStart--;
            if (listStart < 0)
                listStart = 0;
        }

        sfx(SFX_MENUMOVE);
        refreshText = true;
    }

    if (key_hit(k.down)) {
        movingDirection = 1;

        if (selection == options - 1)
            selection = 0;
        else
            selection++;

        if (selection == 0)
            listStart = 0;
        else if (selection > listStart + elementMax)
            listStart++;

        sfx(SFX_MENUMOVE);
        refreshText = true;
    }

    if (key_is_down(k.up) || key_is_down(k.down)) {
        if (dasVer < maxDas) {
            dasVer++;
        } else {
            int dir = (key_is_down(k.up) ? -1 : 1);

            if (selection + dir >= 0 && selection + dir <= options - 1 &&
                arr++ > maxArr) {
                arr = 0;

                movingDirection = dir;

                selection += dir;

                if (dir < 0) {
                    if (selection == options - 1)
                        listStart = max(0, selection - elementMax);
                    else if (selection < listStart + 1) {
                        listStart--;
                        if (listStart < 0)
                            listStart = 0;
                    }
                } else {
                    if (selection == 0)
                        listStart = 0;
                    else if (selection > listStart + elementMax)
                        listStart++;
                }

                refreshText = true;
                sfx(SFX_MENUMOVE);
            }
        }
    } else {
        dasVer = 0;
    }

    if (prev != listStart) {
        moving = true;
    }

    return false;
}

void SimpleListScene::update() {
    canDraw = 1;
    key_poll();

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    if (moving) {
        if (++movingTimer > 4) {
            moving = false;
            movingHor = false;
            movingTimer = 0;
            movingDirection = 0;
        }
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    if (endDelay) {
        endDelay--;
        if (!endDelay) {
            scrollOffset = 0;
            scrollDelay = 0;
            clearSpriteTiles(672, scrollTextLength / 8 + 1, 1);
            scrollingText[0]->setTextSlow(scrollText);
        }
    } else if (scrollDelay < scrollDelayMax) {
        scrollDelay++;
    } else if (scrollTextLength > width) {
        if (++scrollTimer >= scrollTimerMax) {
            scrollTimer = 0;
            if (++scrollOffset >= max(scrollTextLength - width, 0)) {
                endDelay = endDelayMax;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        scrollingText[i]->setScroll(scrollOffset);
        scrollingText[i]->show(12 * 8 * i + 13 * savefile->settings.aspectRatio,
                               160 - 8, 14);
    }

    control();
}

void SimpleListScene::deinit() {

    clearSprites(128);
    showSprites(128);

    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);
    clearSpriteTiles(2, 100, 1);
    clearSpriteTiles(256, 256, 1);

    clearText();

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        delete wordSprites[i];

    for (int i = 0; i < 3; i++)
        delete scrollingText[i];

#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}

Scene* ModeListScene::previousScene() { return new MainMenuScene(); }

void OptionListScene::init() {
    loadTiles(2, 102, sprite37tiles_bin, sprite37tiles_bin_size / 32);
    loadTiles(2, 105, sprite41tiles_bin, sprite41tiles_bin_size / 32);

    clearSprites(128);

    elementList = getElementList();
    options = (int)elementList.size();

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    startY = 24;
    if (options < 7) {
        startY += ((7 - options) * 8) & ~0xf;
    }

    selection = 0;

    auto it = elementList.begin();
    while (!elementList.empty() && !(*it)->isSelectable()) {
        selection++;
        it++;
    }

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i] = new WordSprite(i, 32 + i * 3, 256 + i * 12);

    for (int i = 0; i < 7; i++)
        labels[i] = new WordSprite(MAX_WORD_SPRITES + i,
                                   32 + 3 * MAX_WORD_SPRITES + i * 5,
                                   512 + i * 20, true);

    for (int i = 0; i < 3; i++)
        scrollingText[i] =
            new WordSprite(MAX_WORD_SPRITES + 7 + i, 23 + i * 3, 672 + i * 12);

    loadSpriteTiles(1, sprite41tiles_bin, 2, 1);
    for (int i = 0; i < 2; i++) {
        arrowSprites[i] = &obj_buffer[3 + i];
        sprite_set_attr(arrowSprites[i], ShapeWide, 0, 1, 14, 0);

        if (i)
            sprite_enable_flip(arrowSprites[1], false, true);

        sprite_hide(arrowSprites[i]);
    }

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[1 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    for (int i = 0; i < 2; i++) {
        scrollSideSprites[i] = &obj_buffer[8 + i];
        sprite_set_attr(scrollSideSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
        sprite_enable_affine(scrollSideSprites[i], i + 2, true);
        sprite_set_pos(scrollSideSprites[i], -16 + i * (width + 16), 152 - 4);
        sprite_set_size(scrollSideSprites[i], float2fx(0.8), i + 2);
        sprite_hide(scrollSideSprites[i]);
    }

    enableBlend((0b101110 << 8) + (1 << 6) + (1 << 3));

    if (name() != "") {
        bool found = false;
        int count = 0;
        auto it = path.begin();
        while (it != path.end()) {
            if (*it == name()) {
                found = true;
                break;
            }
            count++;
            it++;
        }

        if (found) {
            unsigned int n = path.size() - (count + 1);
            for (int i = 0; i < n; i++)
                path.pop_back();
        } else {
            path.push_back(name());
        }
    }

    showPath();

    draw();

    for (int i = 0; i < 3; i++)
        scrollingText[i]->show(12 * 8 * i, 160 - 8, 15);

    scrollingText[0]->setTextSlow(scrollText);
}

void OptionListScene::draw() {
    fallingBlocks();

    const int space = 1;
    const int startX = 3 * 8;
    const int endX = 30 - 4;

    int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    int verticalOffset = 0;
    if (moving) {
        int temp =
            lerp((movingDirection) * (space + 1) * 8, 0, 64 * movingTimer);
        verticalOffset = lerp(temp, 0, 64 * movingTimer);
#ifdef N3DS
        // Scroll falling blocks to top of screen when in 1x scale:
        setLayerScroll(2, 0, -verticalOffset + 40);
#else
        setLayerScroll(2, 0, -verticalOffset);
#endif
    } else {
#ifdef N3DS
        // Scroll falling blocks to top of screen when in 1x scale:
        setLayerScroll(2, 0, 40);
#else
        setLayerScroll(2, 0, 0);
#endif
    }

    int i = 0;       // drawing index, starts at listStart
    int counter = 0; // element index
    for (auto& option : elementList) {
        if (counter < listStart) {
            counter++;
            continue;
        }

        if (i > elementMax + 1) { // break if elements drawn reached max
            continue;
        }

        std::string opt = option->getCurrentOption();

        bool offset = false;

        if (opt == "\n") {
            offset = true;
            opt = "";
        }

        const int index = (i + listStart) % (elementMax + 2);

        if (refreshOption || refreshText) {
            if (counter == selection)
                wordSprites[index]->setText(
                    option->getCursor(opt)); // draw option with text cursor
            else
                wordSprites[index]->setText(
                    " " + opt); // draw option without text cursor
        }

        if (refreshText)
            labels[index]->setText(option->getLabel());

        const int x = (endX) * 8 - getVariableWidth(opt);
        const int y = startY + 8 * (space + 1) * i + verticalOffset;

        const int color = 15 - ((counter != selection) +
                                (i == elementMax + 1 || (listStart && i == 0)));

        wordSprites[index]->setPriority(0);
        wordSprites[index]->show(x, y, color);

        labels[index]->show(startX - 8 * offset, y, color);

        if (counter == selection) { // draw sprite cursor
            currentOption = option->getLabel();
            for (int i = 0; i < 2; i++) {
                sprite_unhide(cursorSprites[i], 0);
                sprite_enable_affine(cursorSprites[i], i, true);
                sprite_set_size(cursorSprites[i], scale, i);

                int x = startX - ((8) + offset + 4) - 4;

                sprite_set_pos(cursorSprites[i], x, y - 5);
            }
        }

        i++;
        counter++;
    }

    for (int j = i; j < elementMax + 2; j++) {
        int index = (j + listStart) % (elementMax + 2);

        wordSprites[index]->hide();
        labels[index]->hide();
    }

    if (listStart > 0) {
        sprite_unhide(arrowSprites[0], 0);
        sprite_set_pos(arrowSprites[0], 240 / 2 - 8, startY - 11);
    } else {
        sprite_hide(arrowSprites[0]);
    }

    if (listStart + elementMax + 2 < (int)elementList.size()) {
        sprite_unhide(arrowSprites[1], 0);
        sprite_set_pos(arrowSprites[1], 240 / 2 - 8,
                       (elementMax + 2) * (1 + space) * 8 + startY + 0);
    } else {
        sprite_hide(arrowSprites[1]);
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    if (refreshText) {
        scrollText = getDescription();
        clearSpriteTiles(672, 144, 1);
        if (scrollText != "") {
            scrollTextLength = getVariableWidth(scrollText);

            endDelay = 0;
            scrollOffset = -max((width - scrollTextLength), 0) / 2;
            scrollDelay = 0;
            scrollingText[0]->setTextSlow(scrollText);

#if defined(TE) || defined(N3DS)
            for (int i = 0; i < 2; i++) {
                sprite_unhide(scrollSideSprites[i], ATTR0_AFF);
                sprite_set_pos(scrollSideSprites[i],
                               240 / 2 +
                                   ((i * 2) - 1) *
                                       (min(scrollTextLength, width) / 2) -
                                   16 * (i == 0),
                               152 - 4);
            }
#endif
        } else {
            scrollingText[0]->setTextSlow(scrollText);

            for (int i = 0; i < 2; i++) {
                sprite_hide(scrollSideSprites[i]);
            }
        }
    }

    showSprites(128);

    refreshText = false;
    refreshOption = false;
}

bool OptionListScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        previousElement = name();
        previousSelection = selection;

        if (path.size())
            path.pop_back();

        changeScene(previousScene(), Transitions::FADE);
        return true;
    }

    if (key_hit(k.left) && !elementList.empty()) {
        auto it = elementList.begin();
        std::advance(it, selection);

        (*it)->action(-1);

        refreshOption = true;
    }

    if (key_hit(k.right) && !elementList.empty()) {

        auto it = elementList.begin();
        std::advance(it, selection);

        (*it)->action(1);

        refreshOption = true;
    }

    if (key_hit(k.confirm) && !elementList.empty()) {
        auto it = elementList.begin();
        std::advance(it, selection);

        refreshText = true;

        if ((*it)->action())
            return true;

        refreshOption = true;
    }

    int prev = listStart;

    if (key_hit(k.up)) {
        movingDirection = -1;

        auto it = elementList.begin();
        std::advance(it, selection);

        do {
            if (selection == 0)
                selection = options - 1;
            else
                selection--;

            if (selection == options - 1)
                listStart = max(0, selection - elementMax);
            else if (selection < listStart + 1) {
                listStart--;
                if (listStart < 0)
                    listStart = 0;
            }

            if (it == elementList.begin()) {
                it = elementList.end();
            }

            --it;
        } while (!(*it)->isSelectable());

        sfx(SFX_MENUMOVE);
        refreshText = true;
    }

    if (key_hit(k.down)) {
        movingDirection = 1;

        auto it = elementList.begin();
        std::advance(it, selection);

        do {
            if (selection == options - 1)
                selection = 0;
            else
                selection++;

            if (selection == 0)
                listStart = 0;
            else if (selection > listStart + elementMax)
                listStart++;

            if (++it == elementList.end())
                it = elementList.begin();
        } while (!(*it)->isSelectable());

        sfx(SFX_MENUMOVE);
        refreshText = true;
    }

    if (key_is_down(k.up) || key_is_down(k.down)) {
        if (dasVer < maxDas) {
            dasVer++;
        } else {
            int dir = (key_is_down(k.up) ? -1 : 1);

            if (selection + dir >= 0 && selection + dir <= options - 1 &&
                arr++ > maxArr) {
                arr = 0;

                movingDirection = dir;

                int n = selection + dir;

                auto it = elementList.begin();
                std::advance(it, n);

                if (!(*it)->isSelectable()) {
                    if (n + dir >= 0 && n + dir <= options - 1) {
                        n += dir;
                    } else {
                        n -= dir;
                    }
                }

                selection = n;

                if (dir < 0) {
                    if (selection == options - 1)
                        listStart = max(0, selection - elementMax);
                    else if (selection < listStart + 1) {
                        listStart--;
                        if (listStart < 0)
                            listStart = 0;
                    }
                } else {
                    if (selection == 0)
                        listStart = 0;
                    else if (selection > listStart + elementMax)
                        listStart++;
                }

                refreshText = true;
                sfx(SFX_MENUMOVE);
            }
        }
    } else {
        dasVer = 0;
    }

    if (key_is_down(k.left) || key_is_down(k.right)) {
        if (dasHor < maxDas) {
            dasHor++;
        } else {
            int dir = (key_is_down(k.left) ? -1 : 1);
            if (arr++ > maxArr) {
                arr = 0;

                auto it = elementList.begin();
                std::advance(it, selection);

                (*it)->action(dir);

                refreshOption = true;
            }
        }
    } else {
        dasHor = 0;
    }

    if (prev != listStart) {
        moving = true;
    }

    return false;
}

void OptionListScene::update() {
    canDraw = 1;
    key_poll();

    toggleBG(3, true);

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    if (moving) {
        if (++movingTimer > 4) {
            moving = false;
            movingHor = false;
            movingTimer = 0;
            movingDirection = 0;
            refreshText = true;
        }
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    if (endDelay) {
        endDelay--;
        if (!endDelay) {
            scrollOffset = 0;
            scrollDelay = 0;
            clearSpriteTiles(672, scrollTextLength / 8 + 1, 1);
            scrollingText[0]->setTextSlow(scrollText);
        }
    } else if (scrollDelay < scrollDelayMax) {
        scrollDelay++;
    } else if (scrollTextLength > width) {
        if (++scrollTimer >= scrollTimerMax) {
            scrollTimer = 0;
            if (++scrollOffset >= max(scrollTextLength - width, 0)) {
                endDelay = endDelayMax;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        scrollingText[i]->setScroll(scrollOffset);
        scrollingText[i]->show(12 * 8 * i + 13 * savefile->settings.aspectRatio,
                               160 - 8, 14);
    }

    control();
}

void OptionListScene::deinit() {

    clearSprites(128);
    showSprites(128);

    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);
    clearSpriteTiles(2, 100, 1);
    clearSpriteTiles(256, 256, 1);

    clearText();
    clearSmallText();
    resetSmallText();

    auto it = elementList.begin();

    while (it != elementList.end()) {
        delete *it;
        it++;
    }

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        delete wordSprites[i];

    for (int i = 0; i < 7; i++)
        delete labels[i];

    for (int i = 0; i < 3; i++)
        delete scrollingText[i];

#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}

void OptionListScene::showPath() {
    std::string p;
    int i = 0;
    auto it = path.begin();
    while (it != path.end()) {
        if (i != 0)
            p += " > ";

        p += *it;

        it++;
        i++;
    }

    int width = getVariableWidth(p) >> 3;
    int count = (width) / 12 + 1;

    int wordIndex = MAX_WORD_SPRITES - count - 1;

    wordSprites[wordIndex]->setText(p);

    for (int i = 0; i < count; i++)
        wordSprites[wordIndex + i]->show(
            i * 12 * 8 + 4 + 13 * savefile->settings.aspectRatio, 4, 14);
}

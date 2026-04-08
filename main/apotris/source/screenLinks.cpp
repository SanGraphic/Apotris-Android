#include "logging.h"
#include "scene.hpp"
#include "sprites.h"

#include "site_qr_bin.h"
#ifndef MULTIBOOT
void QRScene::init() {
    resetSmallText();
    clearText();

    clearSprites(128);

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i] = new WordSprite(i, 64 + i * 5, 256 + i * 20, true);

    enableBlend((0b101111 << 8) + (1 << 6) + (1 << 3));

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
            log(count);
            int n = path.size() - (count + 1);
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

    int width = getVariableWidth(p) >> 3;
    int count = (width) / 12 + 1;

    int wordIndex = MAX_WORD_SPRITES - count - 1;

    wordSprites[wordIndex]->setText(p);

    for (int i = 0; i < count; i++)
        wordSprites[wordIndex + i]->show(i * 12 * 8 + 4, 4, 14);

    qr = &obj_buffer[0];

    const int qrTile = 300;

    clearSpriteTiles(qrTile, 8, 8);

    sprite_unhide(qr, 0);
    sprite_set_attr(qr, ShapeSquare, 3, qrTile, 0, 1);
    sprite_enable_affine(qr, 8, true);
    sprite_set_size(qr, 1 << 7, 8);
    sprite_set_pos(qr, 120 - qrX, 80 - 16);

    u16* src = (u16*)getData();

    for (int i = 0; i < qrY; i++) {
        for (int j = 0; j < qrX; j++) {
            int color = (src[i * qrX + j] != 0) ? 7 : 6;
            setSpritePixel(qrTile, j / 8, i / 8, 8, j % 8, i % 8, color);
        }
    }

    std::string link = getLink();

    naprint(link, 15 * 8 - getVariableWidth(link) / 2, 5 * 8);
}

void QRScene::draw() {
    fallingBlocks();
    toggleBG(3, true);
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif

    showSprites(128);
}

bool QRScene::control() {

    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        previousElement = name();

        if (path.size())
            path.pop_back();

        changeScene(previousScene(), Transitions::FADE);
    }

    return false;
}

void QRScene::update() {
    canDraw = 1;
    key_poll();

    control();
}

void QRScene::deinit() {
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

#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}
#endif


#include "logging.h"
#include "scene.hpp"
#include "sprites.h"

#ifndef MULTIBOOT

std::list<std::string> creditsText = {
    "",
    "",
    "",
    "",
    "Menu Music:",
    "  - veryshorty-extended",
    "        by supernao",
    "",
    "  - optikal innovation",
    "        by substance",
    "",
    "  - space taxi",
    "        by schmid",
    "",
    "",
    "In-Game Music:",
    "  - Thirno",
    "        by Nikku4211",
    "",
    "  - oh my god!",
    "        by kb-zip",
    "",
    "  - unsuspected <h>",
    "        by curt cool",
    "",
    "  - Warning Infected!",
    "        by Basq",
    "",
    "  - __detersive__",
    "        by xtd",
    "",
    "  - schizofrenium_long",
    "        by panther, karl",
    "",
    "",
    "",
    "Made with:",
    " - Meson",
    "",
    " - SDL & SDL_mixer",
    "",
    " - Xiph audio",
    "",
    " - Tilengine",
    "",
    " - SoLoud",
    "",
    " - meson-gba",
    "",
    " - sdk-seven",
    "",
    " - gba-link-connection",
    "",
    " - gba-rumble",
    "",
    "",
    " - GBA ROM Saving code",
    "   from Lesserkuma's",
    "   goombacolor mod",
    "",
    " - posprintf",
    "",
    " - libsavgba",
    "",
    " - nanotime",
    "",
    " - libctru (N3DS)",
    "",
    " - Citro3D (N3DS)",
    "",
    " - libxmp (N3DS)",
    "",
    "",
    "",
    "Code Contributors:",
    " - Toolchain",
    " - Wireless multiplayer",
    "  by Isaac Aronson",
    "     (@luigi___)",
    "",
    " - Custom rotation",
    "   system (BARS, SDRS)",
    "  by CreeperCraft",
    "",
    " - Explore option",
    "   prototype",
    "  by tanwk",
    "",
    " - Wireless bugfixes",
    "  by Rodrigo Alfonso",
    "     (@afska)",
    "",
    " - Linux Makefile",
    "   improvements",
    "  by abouvier",
    "",
    " - N3DS port",
    "  by Alvin",
    "",
    "",
    "",
    "Special thanks to:",
    " - all donators",
    "",
    " - everyone who tested",
    "   early versions to",
    "   help find bugs",
    "",
    " - the members of the",
    "   discord server",
    "",
    " - you ;)",
};

void CreditsScene::init() {
    resetSmallText();
    clearText();
    clearSprites(128);

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

#ifdef TE
    // letterbox on pc
    buildBG(0, 0, 29, 0, 0, true);
    TILE tile;
    memset32_fast(&tile, 0xeeeeeeee, 8);
    loadTiles(0, 50, &tile, 1);

    setTiles(29, 20 * 64, 44 * 64, 50);
#endif

#ifdef N3DS
    // letterbox on N3DS
    TILE tile;
    memset32_fast(&tile, 0xeeeeeeee, 8);
    int allocTile = textAllocateTile();
    loadTiles(2, allocTile, &tile, 1);
    setTiles(29, 20 * 32, 12 * 32, tileBuild(allocTile, false, false, 0));

    // Force letterbox to fill horizontally (FPS counter also repeated)
    n3ds::setSbbFillScreen(29);
#endif

    for (int i = 0; i < MAX_WORD_SPRITES; i++) {
        wordSprites[i] = new WordSprite(i, 64 + i * 5, 256 + i * 20, true);
        wordSprites[i]->setPriority(1);
    }

    enableBlend((0b101111 << 8) + (1 << 6) + (1 << 3));
}

void CreditsScene::draw() {
    fallingBlocks();
    toggleBG(3, true);
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif

    int startX = 30;
    int counter = 0;
    int height = 0;
    int i = 0;
    int gaps = 0;

    for (auto option = creditsText.begin(); option != creditsText.end();
         ++option) {
        if (counter < listStart) {
            counter++;
            if (*option == "")
                gaps++;

            continue;
        }

        if (*option == "") {
            height += space;
            continue;
        }

        if (i >= maxShow)
            break;

        int index = (i + listStart - gaps) % maxShow;

        wordSprites[index]->setText(*option);

        int y = height - scrollOffset;

        if (y > SCREEN_HEIGHT)
            break;

        wordSprites[index]->show(startX, height - scrollOffset, 15);

        i++;
        height += space;
    }

    for (int j = i; j < maxShow; j++) {
        int index = (j + listStart - gaps) % maxShow;

        wordSprites[index]->hide();
    }

    showSprites(128);
}

bool CreditsScene::control() {

    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        previousElement = name();

        changeScene(previousScene(), Transitions::FADE);
    } else if (key_is_down(k.confirm)) {
        scrollTimerMax = 1;
    } else if (key_is_down(KEY_FULL)) {
        scrollTimerMax = 0xffff;
    } else {
        scrollTimerMax = 4;
    }

    return false;
}

void CreditsScene::update() {
    canDraw = 1;
    key_poll();

    if (delay < delayMax) {
        delay++;
    } else if (scrollTimer++ >= scrollTimerMax) {
        scrollTimer = 0;
        if (++scrollOffset >= space) {
            scrollOffset = 0;
            listStart++;
        }
    }

    if (listStart > creditsText.size()) {
        changeScene(previousScene(), Transitions::FADE);
        return;
    }

    control();
}

void CreditsScene::deinit() {
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

    buildBG(0, 2, 29, 0, 0, true);

#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}

#endif

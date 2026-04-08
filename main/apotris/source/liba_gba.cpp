#include "platform.hpp"

#ifdef GBA
#include "gba-rumble.h"
#include "LinkCableMultiboot.hpp"
#include "LinkUniversal.hpp"
#include "achievementStructure.h"
#include "aeabi.h"
#include "agbabi.h"
#include "detectEmulators.h"
#include "flashSaves.h"
#include "gba_flash.h"
#include "gba_sram.h"
#include "gbfs.h"
#include "gbp_logo.hpp"
#include "logging.h"
#include "maxmod.h"
#include "rumble.h"
#include "rumblePatterns.hpp"
#include "scene.hpp"
#include "text.h"

// #include <string>
// #include "scene.hpp"
// #include "main.hpp"
bool detect_gbp();
void loadAudio();
void onKeyInterrupt();

int spriteVOffset = 0;

u8* soundbank = nullptr;

bool sortSprites = false;
int musicVolume = 0;

bool disableHBlank = true;
bool disableDMA = true;

bool inaccurateEmulator = false;
bool emulatorPrompted = false;

Songs songs;

u32 sleep_combo = 0;

u16 get_sleep_combo(u32 index) {
    switch (index) {
        case 0: // Disabled
            return 0;
        case 1:
            return KEY_L | KEY_R | KEY_SELECT;
        case 2:
            return KEY_START | KEY_SELECT;
        default:
            return 0;
    }
}


u32 get_autosleep_mins(u32 index) {
    switch (index) {
        case 0: // Disabled
            return 0;
        case 1:
            return 1;
        case 2:
            return 3;
        default: // 5 minute intervals, starting from 5
            return (index - 2) * 5;
    }
}

void await_key_release(u32 keys, int timeout_secs) {
    int timer = 0;
    int timeout_frames = timeout_secs * 60;
    do {
        if (timer > timeout_frames && timeout_frames != 0) {
            break;
        }
        VBlankIntrWait();
        key_poll();
        timer++;
    } while ( key_curr_state() != ((~keys) & KEY_FULL) );
}

bool sleep_combo_check(u32 keys) {
    if (game && !paused && !demo)
        return false;
    if (multiplayer)
        return false;

    if (sleep_combo && keys == sleep_combo) {
        sleep();
        return true;
    } else {
        return false;
    }
}

void onHBlank() {
    if (REG_VCOUNT < 160)
        pal_bg_mem[0] = gradientTable[REG_VCOUNT];
}

LinkUniversal* linkUniversal;
LinkUniversal* linkConnection;

OBJ_AFFINE* obj_aff_buffer = (OBJ_AFFINE*)obj_buffer;

mm_word myEventHandler(mm_word msg, mm_word param) {
    if (msg == MMCB_SONGMESSAGE)
        return 0;

    if (savefile->settings.cycleSongs == 1) { // CYCLE
        playNextSong();
    } else if (savefile->settings.cycleSongs == 2) { // SHUFFLE
        playSongRandom(currentMenu);
    }

    return 0;
}

void toggleBG(int layer, bool state) {
    if (state) {
        REG_DISPCNT |= ((1 << layer) << 8);
    } else {
        REG_DISPCNT &= ~((1 << layer) << 8);
    }
}

void buildBG(int layer, int cbb, int sbb, int size, int prio, int mos) {

    *(vu16*)(REG_BASE + 0x0008 + 0x0002 * layer) =
        BG_CBB(cbb) | BG_SBB(sbb) | BG_SIZE(size) | BG_PRIO(prio) |
        BG_MOSAIC * mos;
}

void toggleSprites(bool state) {
    if (state) {
        REG_DISPCNT |= DCNT_OBJ;
    } else {
        REG_DISPCNT &= ~DCNT_OBJ;
    }
}

void vsync() { VBlankIntrWait(); }

void showSprites(int count) {
    if (sortSprites) {
        int counters[4] = {0, 0, 0, 0};

        int positions[count];

        for (int i = 0; i < count; i++) {
            const int priority = (obj_buffer[i].attr2 >> 0xa) & 0b11;
            positions[i] = counters[priority]++ + (priority << 8);
        }

        int offsets[4] = {0, 0, 0, 0};

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < i; j++) {
                offsets[i] += counters[j];
            }
        }

        for (int i = 0; i < count; i++) {
            const int priority = positions[i] >> 8;
            const int position = positions[i] & 0xff;
            const int offset = offsets[priority];

            oam_mem[offset + position] = obj_buffer[i];
        }
    } else {
        oam_copy(oam_mem, obj_buffer, count);
    }

    obj_aff_copy(obj_aff_mem, obj_aff_buffer, 32);
}

unsigned int calculateRemoteTimeout(u32 interval, u32 framesToWait) {
    // Calculate the duration of one transfer in milliseconds
    float transferDurationMs = (float)interval * 61.04f / 1000.0f;
    // Calculate the total duration for the specified number of frames in
    // milliseconds
    float totalDurationMs = (float)framesToWait * 16.67f;
    // Calculate how many transfers can occur in the total duration
    u32 transfers = (u32)((float)totalDurationMs / transferDurationMs);
    return transfers;
}

extern struct FlashInfo gFlashInfo;
struct FlashInfo flashInfo;
struct SramInfo sramInfo;

void testSaveMedia() {
    const int testRegion = 0x7FFF;
    const int testMirror = 0xFFFF;
    flashInfo.size = 0;
    int err;
    // Test SRAM
    err = sram_write(testRegion, (u8*)"S", 1);
    if (!err) {
        // Is SRAM
        char a = '\0';
        // Write to mirror region and read lower mirror half
        __agbabi_memcpy1(sram_mem + testMirror, (u8*)"R", 1);
        __agbabi_memcpy1((u8*)(&a), sram_mem + testRegion, 1);
        // See if it was overwritten (mirrored) or persisted
        switch (a) {
        case 'R':
            sramInfo.size = testRegion;
            break;
        case 'S':
            sramInfo.size = testMirror;
            break;
        default:
            sramInfo.size = testRegion;
            break;
        }
        // Detect bootlegs
        if (ENABLE_FLASH_SAVE)
            detect_rom_backed_flash();
        return;
    }

    // Test Flash
    err = flash_init(FLASH_SIZE_AUTO); // catches some but not all flash chips
    if (err)
        err = flash_init(FLASH_SIZE_64KB); // try smaller first for safety, some
                                           // 128KiB will trip this one
    if (err)
        err = flash_init(FLASH_SIZE_128KB); // last resort detection, may
                                            // actually be a 64KiB chip!
    if (err)
        return; // EEPROM or no save media, no luck
    flashInfo = gFlashInfo;
}

void platformInit() {
    // Guard against EZ Flash and Everdrive jumping to the ROM at some random
    // vsync interval and breaking maxmod
    while (REG_VCOUNT != 160)
        ;
    while (REG_VCOUNT != 161)
        ;

    testSaveMedia();

#ifndef NO_DIAGNOSE
    logInitMgba();
#endif

    interrupt_init();
    interrupt_unmask();
    REG_DISPSTAT |= (1 << 3) | (1 << 4);


#ifndef NO_DIAGNOSE
    REG_KEYCNT = KEY_A | KEY_B | KEY_SELECT | KEY_START | BIT(14) | BIT(15);
    interrupt_set_handler(INTR_KEYPAD, onKeyInterrupt);
    interrupt_enable(INTR_KEYPAD);
#endif

    interrupt_set_handler(INTR_VBLANK, onVBlank);

    interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
    interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);

    // interrupt_add(INTR_HBLANK, onHBlank);

    interrupt_enable(INTR_VBLANK);
    interrupt_enable(INTR_SERIAL);
    interrupt_enable(INTR_TIMER3);

    loadAudio();

    mmInitDefault((mm_addr)soundbank, 12);
    mmSetEventHandler((mm_callback)myEventHandler);

    REG_DISPCNT = DCNT_OBJ_1D | DCNT_MODE0;

    REG_BLDALPHA = BLD_EVA(31) | BLD_EVB(5);

    REG_WAITCNT = 0x4317;

    setUpLinkUniversal(false);
}

void setUpLinkUniversal(bool detectEmus) {
    const bool inaccurateEmulator =
        detectEmus ? detect_inaccurate_emulator() : true;
    const u32 wirelessTimeout = inaccurateEmulator ? 45 : 15;
    const u32 wirelessInterval =
        150; // 50 usually works but some devices needed 150...
    const u32 wiredInterval =
        300; // 150 usually works but some devices needed 300...
    delete linkUniversal;
    linkUniversal = new LinkUniversal(
        LinkUniversal::Protocol::WIRELESS_AUTO, "Apotris",
        (LinkUniversal::CableOptions){
            .baudRate =
                LinkCable::BaudRate::BAUD_RATE_3, // Baud 3 is required for
                                                  // enough b/w to send garbage
            .timeout = LINK_CABLE_DEFAULT_TIMEOUT,
            .interval = wiredInterval,
            .sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID},
        (LinkUniversal::WirelessOptions){
            .forwarding = true,
            .retransmission = true,
            .maxPlayers = LINK_WIRELESS_MAX_PLAYERS,
            .timeout = wirelessTimeout,
            .interval = wirelessInterval,
            .sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID});
    linkConnection = linkUniversal;
}

void deinitialize() {}

// static int rumbleTimer = 0;

static u32 autosleepTimer = 0;

void sleep_checks() {
    if (multiplayer)
        return;

    const u32 autosleep_wait_frames = get_autosleep_mins(savefile->settings.sleep.autosleepDelay) * FRAMES_PER_MIN;

    const bool no_user_input = savefile->settings.sleep.autosleepWakeCombo == 2 ? !key_is_down(KEY_FULL & ~(KEY_L | KEY_R)) : !key_is_down(KEY_FULL);

    if (no_user_input) {
        autosleepTimer++;

        if (autosleep_wait_frames && autosleepTimer > autosleep_wait_frames) {
            sleep();
            autosleepTimer = 0;
        }
    } else if (sleep_combo && key_curr_state() == sleep_combo) {
        autosleepTimer = 0;
        sleep_combo_check(key_curr_state());
    } else {
        autosleepTimer = 0;
    }
}

bool closed() {
    sleep_checks();

    gba_rumble_loop();
    // if (rumbleTimer > 0) {
    //     rumbleTimer--;

    //     rumble_set_state(rumble_start);
    // } else {
    //     rumble_set_state(rumble_hard_stop);
    // }
    return true;
}

IWRAM_CODE void memcpy16_fast(void* dest, const void* src, int hwCount) {
    __agbabi_memcpy2(dest, src, hwCount * 2);
}

IWRAM_CODE void memcpy32_fast(void* dest, const void* src, int wCount) {
    __aeabi_memcpy4(dest, src, wCount * 4);
}

IWRAM_CODE void memset32_fast(void* dest, int word, int wCount) {
    __aeabi_memset4(dest, wCount * 4, word);
}

void loadPalette(int palette, int index, const void* src, int count) {
    memcpy16_fast(&pal_bg_mem[16 * palette + index], src, count);
}

void loadTiles(int tileset, int index, const void* src, int count) {
    memcpy32_fast(&tile_mem[tileset][index], src, count * 8);
}

void loadTilemap(int tilemap, int index, const void* src, int count) {
    u16* dst = (u16*)&se_mem[tilemap];
    memcpy16_fast(&dst[index], src, count);
}

void clearTilemap(int tilemap) { memset16(&se_mem[tilemap], 0, 32 * 32); }

void clearSpriteTiles(int index, int lengthX, int lengthY) {
    memset32_fast(&tile_mem[4][index], 0, lengthX * lengthY * 8);
}

void clearSpriteTile(int index, int tx, int ty, int width) {
    memset32_fast(&tile_mem[4][index + ty * width + tx], 0, 8);
}

void loadSpriteTiles(int index, const void* src, int lengthX, int lengthY) {
    memcpy32_fast(&tile_mem[4][index], src, lengthX * lengthY * 8);
}

void loadSpriteTilesPartial(int index, const void* src, int tx, int ty,
                            int lengthX, int lengthY, int rowLength) {
    memcpy32_fast(&tile_mem[4][index + ty * rowLength + tx], src,
                  lengthX * lengthY * 8);
}

void setPaletteColor(int palette, int index, u16 color, int count) {
    memset16(&pal_bg_mem[palette * 16 + index], color, count);
}

void sprite_hide(OBJ_ATTR* sprite) { obj_hide(sprite); }

void sprite_unhide(OBJ_ATTR* sprite, int mode) { obj_unhide(sprite, mode); }

void sprite_set_pos(OBJ_ATTR* sprite, int x, int y) {
    obj_set_pos(sprite, x, y);
}

void sprite_set_attr(OBJ_ATTR* sprite, int shape, int size, int tile_start,
                     int palette, int priority) {
    obj_set_attr(sprite, ATTR0_SHAPE(shape), ATTR1_SIZE(size),
                 ATTR2_BUILD(tile_start, palette, priority));
}

void setLayerScroll(int layer, int x, int y) {
    switch (layer) {
    case 0:
        REG_BG0HOFS = x;
        REG_BG0VOFS = y;
        break;
    case 1:
        REG_BG1HOFS = x;
        REG_BG1VOFS = y;
        break;
    case 2:
        REG_BG2HOFS = x;
        REG_BG2VOFS = y;
        break;
    case 3:
        REG_BG3HOFS = x;
        REG_BG3VOFS = y;
        break;
    }
}

void enableLayerBlend(int layer) {
    REG_BLDCNT = (1 << 6) + (1 << 0xb) + (1 << layer);
    REG_BLDALPHA = BLD_EVA(31) | BLD_EVB(15);

    // TLN_SetLayerBlendMode(2,BLEND_ADD,0);
}

void color_fade_palette(int palette, int index, const COLOR* src, COLOR color,
                        int count, u32 alpha) {
    clr_fade(src, color, &pal_bg_mem[palette * 16 + index], count, alpha);
}

void color_fade(COLOR* dst, const COLOR* src, COLOR color, int count,
                u32 alpha) {
    clr_fade(src, color, dst, count, alpha);
}

void color_adj_brightness(int palette, int index, const COLOR* src, u32 count,
                          FIXED alpha) {
    clr_adj_brightness(&pal_bg_mem[palette * 16 + index], src, count, alpha);
}

void color_adj_MEM(COLOR* dst, const COLOR* src, u32 count, u32 alpha) {
    clr_adj_brightness(dst, src, count, alpha);
}

void color_blend(COLOR* dst, const COLOR* srca, const COLOR* srcb, int nclrs,
                 u32 alpha) {
    clr_blend(srca, srcb, dst, nclrs, alpha);
}

void addColorToPalette(int palette, int index, COLOR color, int count) {
    for (int i = 0; i < count; i++) {
        pal_bg_mem[palette * 16 + index + i] += color;
    }
}

INLINE bool usingVBlankForRumblePatternLoop() {
    return savefile->settings.rumbleUpdateRate == 0;
}

void initRumbleCart() {
    interrupt_disable(INTR_TIMER2);
    REG_TM2CNT = 0;

    enum GBARumbleCartType cart_type = gba_rumble_cart_uninitialized;

    switch (savefile->settings.rumbleCartType) {
        case 0: // "Type A", ROM GPIO
            cart_type = gba_rumble_cart_rio;
            break;
        case 1: // "Type B", DS Rumble Pak
            cart_type = gba_rumble_cart_ds;
            break;
        case 2: // "Type C", EZ-Flash Omega DE/3-in-1
            cart_type = gba_rumble_cart_ezflash;
            break;
        default:
            return;
    }
    gba_rumble_init_cart(cart_type);

    if (savefile->settings.ezFlash3in1Strength >= EZFLASH_MIN_RUMBLE && savefile->settings.ezFlash3in1Strength <= EZFLASH_MAX_RUMBLE) {
        gba_rumble_ezflash_write_rumble(savefile->settings.ezFlash3in1Strength);
    }

    if (!usingVBlankForRumblePatternLoop()) { // True if rumbleUpdateRate == 0
        // Setup rumble PWM with one of the hardware timers (any of 0-3)
        interrupt_set_handler(INTR_TIMER2, rumblePatternLoop);

        u16 timerReloadVal = 8 << 1; // 1024hz, further shifts will half the rate.

        if (savefile->settings.rumbleUpdateRate >= 1 && savefile->settings.rumbleUpdateRate <= 4) { // Expecting vals 0-4, but 0-4 are the only valid ones.
            u16 timerShiftFactor = 4 - savefile->settings.rumbleUpdateRate;
            timerReloadVal <<= timerShiftFactor;
        }

        REG_TM2D   = -timerReloadVal; // Set negative val in timer reload reg
        REG_TM2CNT = TM_FREQ_1024 | TM_IRQ | TM_ENABLE; // 1024-cycle prescaler -> 16.384 kHz, 61.04 μs per timer tick

        interrupt_enable(INTR_TIMER2);
    } else {
        // We'll be relying on VBlank
        // interrupt_disable(INTR_TIMER2);
    }
}

void initRumble() {
    if (savefile->settings.gameBoyPlayerSupport) {
        RegisterRamReset(RESET_VRAM);
        REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
        *((volatile u16*)0x4000008) = 0x0088;

        if (detect_gbp()) {
            GBARumbleGBPConfig conf{[](void (*rumble_isr)(void)) {
                interrupt_enable(INTR_SERIAL);
                interrupt_set_handler(INTR_SERIAL, rumble_isr);
            }};

            gba_rumble_init_gbp(conf);
        } else {
            initRumbleCart();
        }
        gbpChecked = true;
    } else {
        initRumbleCart();
    }

    RegisterRamReset(RESET_VRAM);

    REG_DISPCNT = DCNT_OBJ_1D | DCNT_MODE0;
    REG_BLDALPHA = BLD_EVA(31) | BLD_EVB(5);
    REG_WAITCNT = 0x4317;
}

void rumbleOutput(uint16_t strength) {
    if (strength) {
        gba_rumble_start();
    } else {
        gba_rumble_stop();
    }
}

void rumbleStop() {
    gba_rumble_stop();
}

bool detect_gbp() {
    bool gbp_detected = false;

    memcpy16_fast((u16*)0x6008000, gbp_logo_pixels,
                  (sizeof gbp_logo_pixels) / 2);
    memcpy16_fast((u16*)0x6000000, gbp_logo_tiles, (sizeof gbp_logo_tiles) / 2);
    memcpy16_fast(pal_bg_mem, gbp_logo_palette, (sizeof gbp_logo_palette) / 2);

    static volatile u16* keys = (volatile u16*)0x04000130;

    for (int i = 0; i < 120; i++) {
        if (*keys == 0x030f) {
            gbp_detected = true;
        }
        VBlankIntrWait();
    }

    return gbp_detected;
}

void sleep() {
    int timer = 0;

    const u32 autosleep_wait_frames = get_autosleep_mins(savefile->settings.sleep.autosleepDelay) * FRAMES_PER_MIN;
    const bool is_autosleep = (autosleep_wait_frames && autosleepTimer > autosleep_wait_frames);

    const bool use_combo_for_wake = !is_autosleep || (is_autosleep && sleep_combo && savefile->settings.sleep.autosleepWakeCombo == 0);

    // Wait for keys to be released so the console doesn't
    // insta-wake due to the interrupt combo already being active.
    await_key_release(KEY_FULL, 2);

    SoundBias(0);

    int display_value = REG_DISPCNT;

    bool prevGradient = gradientEnabled;
    interrupt_disable(INTR_VBLANK);
    gradient(0);

    int stat_value = REG_SNDSTAT;
    int dsc_value = REG_SNDDSCNT;
    int dmg_value = REG_SNDDMGCNT;

    REG_DISPCNT |= 0x0080;
    REG_SNDSTAT = 0;
    REG_SNDDSCNT = 0;
    REG_SNDDMGCNT = 0;

    bool isLinked = linkConnection->isActive();

    if (isLinked)
        linkConnection->deactivate();

    interrupt_disable(INTR_TIMER3);
    interrupt_disable(INTR_SERIAL);

    interrupt_disable(INTR_TIMER2);
    rumblePatternStop();

    if (use_combo_for_wake) {
        // Using the same keys used to put manually to sleep.
        REG_P1CNT = sleep_combo | BIT(14) | BIT(15);
    } else if (savefile->settings.sleep.autosleepWakeCombo == 2) {
        // Any key except shoulder L and R (bits 8 and 9) should be able to wake the console.
        REG_P1CNT = static_cast<cu16>(~(BIT(15) | BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) | BIT(13)));
    } else {
        // Setting bit 15 to 0 allows any single key in the active mask to wake the console.
        // 10-13 are unused bits in the register
        REG_P1CNT = static_cast<cu16>(~(BIT(15) | BIT(10) | BIT(11) | BIT(12) | BIT(13)));
    }

    interrupt_enable(INTR_KEYPAD);
    interrupt_set_handler(INTR_KEYPAD, nullptr);

    // while (REG_SNDBIAS != 0) {}

    Stop();

    interrupt_disable(INTR_KEYPAD);
    REG_P1CNT = 0;

    REG_DISPCNT = display_value;
    SoundBias(0x200);
    REG_SNDSTAT = stat_value;
    REG_SNDDSCNT = dsc_value;
    REG_SNDDMGCNT = dmg_value;

    if (isLinked)
        linkConnection->activate();

    interrupt_enable(INTR_TIMER3);
    interrupt_enable(INTR_SERIAL);
    interrupt_enable(INTR_VBLANK);

    if (!usingVBlankForRumblePatternLoop()) {
        interrupt_enable(INTR_TIMER2);
    }

    if (prevGradient)
        gradient(1);

    // Wait for waking keys to be released so they don't trigger menu/game actions.
    await_key_release(KEY_FULL, 3);
}

void sfx(int s) {

    int id = SoundEffectIds[s];

    if (id == -1)
        return;

    mm_sfxhand h = mmEffect(id);
    mmEffectVolume(h, 255 * (float)savefile->settings.sfxVolume / 10);
}

void sfxRate(int sound, float rate) {
    int id = SoundEffectIds[sound];

    if (id == -1)
        return;

    mm_sound_effect s = {
        {(mm_word)id},
        (mm_hword)((rate) * (1 << 10)),
        0,
        (u8)(255 * (float)savefile->settings.sfxVolume / 10),
        128,
    };

    mmEffectEx(&s);
}

void stopDMA() { disableDMA = true; }

void setMosaic(int sx, int sy) { REG_MOSAIC = MOS_BUILD(sx, sy, sx, sy); }

void clearSprites(int count) { oam_init(obj_buffer, count); }

void clearTiles(int tileset, int index, int count) {
    memset32_fast(&tile_mem[tileset][index], 0, 8 * count);
}

IWRAM_CODE void onVBlank() {

    mmVBlank();
    LINK_UNIVERSAL_ISR_VBLANK();

    if (usingVBlankForRumblePatternLoop()) {
        rumblePatternLoop();
    }

    if (disableHBlank) {
        interrupt_disable(INTR_HBLANK);
        disableHBlank = false;
    }

    if (disableDMA) {
        REG_DMA0CNT &= ~(1 << 0x1f);
        disableDMA = false;
    }

    if (gradientEnabled)
        DMA_TRANSFER(&pal_bg_mem[0], gradientTable, 1, 0, DMA_HDMA);

    if (canDraw) {
        canDraw = false;

        scene->draw();
    }

    frameCounter++;
    mmFrame();
}

void sprite_enable_affine(OBJ_ATTR* sprite, int affineId, bool doubleSize) {
    sprite->attr0 |= ATTR0_AFF + doubleSize * ATTR0_AFF_DBL_BIT;
    sprite->attr1 |= ATTR1_AFF_ID(affineId);
}

void sprite_enable_mosaic(OBJ_ATTR* sprite) { sprite->attr0 |= ATTR0_MOSAIC; }

void sprite_set_id(OBJ_ATTR* sprite, int id) { sprite = &obj_buffer[id]; }

void sprite_set_size(OBJ_ATTR* sprite, FIXED size, int aff_id) {
    obj_aff_identity(&obj_aff_buffer[aff_id]);
    obj_aff_scale(&obj_aff_buffer[aff_id], size, size);
}

void sprite_rotscale(OBJ_ATTR* sprite, FIXED sizex, FIXED sizey, int angle,
                     int aff_id) {
    obj_aff_identity(&obj_aff_buffer[aff_id]);
    obj_aff_rotscale(&obj_aff_buffer[aff_id], sizex, sizey, angle);
}

void setMusicTempo(int tempo) { mmSetModuleTempo(tempo); }

void setMusicVolume(int volume) {
    musicVolume = volume;
    mmSetModuleVolume(volume);
}

void startSong(int song, bool loop) {
    int n = song;

    if (n < 0)
        return;

    mmStart(n, (!loop) ? MM_PLAY_ONCE : MM_PLAY_LOOP);
    mmSetModuleVolume(musicVolume);
}

void stopSong() { mmStop(); }

void setTiles(int tilemap, int index, int count, u32 tile) {
    u16* dst = (u16*)&se_mem[tilemap];
    memset16(&dst[index], tile, count);
}

void pauseSong() { mmPause(); }

void resumeSong() { mmResume(); }

std::string getSongName(int song) { return {}; }

void clearTilemapEntries(int tilemap, int index, int count) {
    u16* dst = (u16*)&se_mem[tilemap];

    memset16(&dst[index], 0, count);
}

void loadSavefile() {
    delete savefile;

    savefile = new Save();

    if (flashInfo.size != 0) {
        flash_read(0, (u8*)savefile, sizeof(Save));
    } else {
        __agbabi_memcpy1((u8*)savefile, sram_mem, sizeof(Save));
    }
}

void saveSavefile() {
    if (flashInfo.size != 0) {
        flash_write(0, (u8*)savefile, sizeof(Save));
    } else {
        __agbabi_memcpy1(sram_mem, savefile, sizeof(Save));

        if (ENABLE_FLASH_SAVE)
            save_sram_flash();
    }
    status.update(&savefile->settings);
}

void mirrorPalettes(int index, int count) {
    memcpy16_fast(&pal_bg_mem[index], &pal_obj_mem[index], (8 * 16));
}

void toggleHBlank(bool state) {
    if (state) {
        interrupt_set_handler(INTR_HBLANK, onHBlank);
        interrupt_enable(INTR_HBLANK);
    } else {
        disableHBlank = true;
    }
}

void savePalette(COLOR* dst) { memcpy32_fast(dst, pal_bg_mem, 256); }

void enableBlend(int info) { REG_BLDCNT = info; }

void sprite_enable_blend(OBJ_ATTR* sprite) { sprite->attr0 |= ATTR0_BLEND; }

int prevDispCnt = 0;

void toggleRendering(bool state) {
    if (state) {
        REG_DISPCNT = prevDispCnt;
    } else {
        prevDispCnt = REG_DISPCNT;
        REG_DISPCNT = 0;
    }
}

void sprite_enable_flip(OBJ_ATTR* sprite, bool flipX, bool flipY) {
    sprite->attr1 |= flipX << 0xc;
    sprite->attr1 |= flipY << 0xd;
}

void quit() {}

void findSoundbank() {
    const GBFS_FILE* gbfsFile =
        find_first_gbfs_file(reinterpret_cast<const void*>(AGB_ROM));
    const void* myFile = gbfs_get_obj(gbfsFile, "soundbank.bin", NULL);

    if (myFile == NULL) {
        log("couldn't find soundbank.bin");
        return;
    }

    log("found soundbank.bin");

    soundbank = (u8*)myFile;
}

void buildEffectLocations() {

    const GBFS_FILE* gbfsFile =
        find_first_gbfs_file(reinterpret_cast<const void*>(AGB_ROM));
    const void* myFile = gbfs_get_obj(gbfsFile, "effect_locations.bin", NULL);

    u32* file = (u32*)myFile;

    if (file[0] != 0x51) {
        log("couldn't find effect location file");
        return;
    }

    log("found location file");

    file++;

    int count = *file++;
    for (int i = 0; i < count; i++) {
        SoundEffectIds[i] = *file++;
    }

    count = *file++;
    log("found " + std::to_string(count) + " menu songs");
    for (int i = 0; i < count; i++) {
        songs.menu.push_back(*file++);
    }

    count = *file++;
    log("found " + std::to_string(count) + " game songs");
    for (int i = 0; i < count; i++) {
        songs.game.push_back(*file++);
    }
}

void loadAudio() {
    log("Trying to load from appended files...");

    findSoundbank();
    buildEffectLocations();
}

void enableLayerWindow(int layer, int x1, int y1, int x2, int y2, bool invert) {
}

void disableLayerWindow(int layer) {}

std::map<int, std::string> keyToString = {
    {KEY_LEFT, "Left"},     {KEY_RIGHT, "Right"}, {KEY_UP, "Up"},
    {KEY_DOWN, "Down"},     {KEY_A, "A"},         {KEY_B, "B"},
    {KEY_L, "L"},           {KEY_R, "R"},         {KEY_START, "Start"},
    {KEY_SELECT, "Select"},
};

void refreshWindowSize() {}

void toggleSpriteSorting(bool state) { sortSprites = state; }

IWRAM_CODE void onKeyInterrupt() {
    // Soft reset on bootleg carts will destroy the save
    if (bootleg_type)
        return;
    mmStop();
    REG_IME = 0;
    REG_DMA0CNT_H &= ~DMA_ENABLE;
    REG_DMA1CNT_H &= ~DMA_ENABLE;
    REG_DMA2CNT_H &= ~DMA_ENABLE;
    REG_DMA3CNT_H &= ~DMA_ENABLE;
    REG_TM0D = 0;
    REG_TM1D = 0;
    REG_TM2D = 0;
    REG_TM3D = 0;
    SoftReset();
}

#endif

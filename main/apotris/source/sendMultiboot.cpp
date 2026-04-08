#include "sendMultiboot.h"
#include "Multiboot.h"

LinkCableMultiboot* linkCableMultiboot = new LinkCableMultiboot();

LinkCableMultiboot::Result sendMultiboot() {
    return linkCableMultiboot->sendRom(Apotris_multi_gba,
                                       Apotris_multi_gba_size, []() {
                                           u16 keys = ~REG_KEYS & KEY_ANY;
                                           return keys & KEY_START;
                                       });
}

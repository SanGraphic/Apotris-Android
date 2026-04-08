#ifdef GBA
#include "interrupt.h"
#include <seven/irq.h>

void interrupt_init(void) { irqInitDefault(); }

void interrupt_set_handler(interrupt_index index, interrupt_vector function) {
    irqHandlerSet(1 << index, reinterpret_cast<void (*)(uint16_t)>(function));
}

interrupt_vector interrupt_get_handler(interrupt_index index) {
    interrupt_vector handler;
    irqHandlerGet(1 << index, reinterpret_cast<void (**)(uint16_t)>(&handler));
    return handler;
}

void interrupt_enable(interrupt_index index) { irqEnable(1 << index); }

void interrupt_disable(interrupt_index index) { irqDisable(1 << index); }

void interrupt_unmask() { irqUnmask(); }

#endif
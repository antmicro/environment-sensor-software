#include "fram.h"

#include <zephyr/drivers/eeprom.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace fram {
const device* const fram = DEVICE_DT_GET_ONE(fujitsu_mb85rcxx);

void init() {
    if (!device_is_ready(fram)) {
        printk("warning: fram: not ready\n");
        return;
    }
}

void clear() {
    const uint8_t buf[64]{};
    for (addr_t addr = 0x0; addr < fram_size; addr += sizeof(buf)) {
        write_raw(addr, reinterpret_cast<const void*>(buf), sizeof(buf));
    }
}

void write_raw(addr_t addr, const void* data, size_t size) {
    if (int rc = eeprom_write(fram, addr, data, size); rc < 0) {
        while (1) {
            printk("Error: Couldn't write to eeprom, err: %d\n", rc);
            k_sleep(K_MSEC(1000));
        }
    }
}

void read_raw(addr_t addr, void* data, size_t size) {
    if (int rc = eeprom_read(fram, addr, data, size); rc < 0) {
        while (1) {
            printk("Error: Couldn't read eeprom, err: %d\n", rc);
            k_sleep(K_MSEC(1000));
        }
    }
}
}

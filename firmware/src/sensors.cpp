#include "sensors.h"
#include "rtc.h"

#include <zephyr/sys/printk.h>

namespace sensors {
const device* const bme = DEVICE_DT_GET_ONE(bosch_bme280);
const device* const sht = DEVICE_DT_GET_ONE(sensirion_sht4x);

void init() {
    if (!device_is_ready(bme)) {
        printk("warning: bme280: not ready\n");
        return;
    }

    if (!device_is_ready(sht)) {
        printk("warning: sht43: not ready\n");
        return;
    }
}

data_point get_data() {
    data_point p;

    sensor_sample_fetch(bme);
    sensor_sample_fetch(sht);

    sensor_channel_get(bme, SENSOR_CHAN_AMBIENT_TEMP, &p.bme_temperature);
    sensor_channel_get(bme, SENSOR_CHAN_PRESS, &p.bme_pressure);
    sensor_channel_get(bme, SENSOR_CHAN_HUMIDITY, &p.bme_humidity);

    sensor_channel_get(sht, SENSOR_CHAN_AMBIENT_TEMP, &p.sht_temperature);
    sensor_channel_get(sht, SENSOR_CHAN_HUMIDITY, &p.sht_humidity);

    p.timestamp = rtc::get_current_time();

    return p;
}

void data_point::print_value(const sensor_value& sv) const {
    // decimal part can be negative so simply printing val1 + . + val2 wouldn't work in cases like val1=12 and val2=-0.5
    // where final value is 11.5
    constexpr auto factor = 1000000;
    int64_t val = sv.val1 * factor + sv.val2;
    int32_t integer_part = val / factor;
    int32_t decimal_part = abs(val % factor);

    printk("%d.%06d", integer_part, decimal_part);
}

void data_point::print() const {
    rtc::print_time(timestamp);
    auto print_comma_prepend_value = [&](sensor_value val) {
        printk(",");
        print_value(val);
    };
    print_comma_prepend_value(bme_temperature);
    print_comma_prepend_value(bme_pressure);
    print_comma_prepend_value(bme_humidity);
    print_comma_prepend_value(sht_temperature);
    print_comma_prepend_value(sht_humidity);
    printk("\n");

    k_sleep(K_MSEC(10));
}
}

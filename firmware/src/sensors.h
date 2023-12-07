#ifndef ANTENVSENS_SENSORS_H
#define ANTENVSENS_SENSORS_H
#include <zephyr/drivers/sensor.h>

#include <ctime>

namespace sensors {

struct data_point {
    time_t timestamp{};
    sensor_value bme_temperature{};
    sensor_value bme_pressure{};
    sensor_value bme_humidity{};
    sensor_value sht_temperature{};
    sensor_value sht_humidity{};

    void print() const;
    void print_value(const sensor_value& sv) const;
};

void init();
data_point get_data();
}

#endif

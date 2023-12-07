#include "fram.h"
#include "fram_buffer.h"
#include "rtc.h"
#include "sensors.h"
#include "user_config.h"

#include <zephyr/console/console.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>

using namespace std::literals;

constexpr auto logger_stack_size = 4096;
constexpr auto io_stack_size = 4096;

#define LED0_NODE DT_ALIAS(led0)
const gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

constexpr auto logger_priority = 2;
constexpr auto io_priority = 1;

static K_THREAD_STACK_DEFINE(logger_stack_area, logger_stack_size);
static K_THREAD_STACK_DEFINE(io_stack_area, io_stack_size);

static k_thread logger_thread_data;
static k_thread io_thread_data;

constexpr std::string_view default_name = "antenvsens";
constexpr uint32_t default_period = 1;
static user_config* config;

static k_mutex main_buffer_mtx;
static k_mutex secondary_buffer_mtx;
static k_sem logger_sleep_smph;

// Printing data from main buffer, waiting for acknowledge before removal and removing data from main buffer is done
// concurrently to logging data in another thread. To prevent a situation where the logger thread overwrites the entry
// that was going to be read (and break chronology as a result) because buffer was full (and buffer's head catched up
// with read position in "get data"), second smaller buffer is used during "get data" execution. After "get data"
// finishes data from secondary_buffer is moved to main_buffer and subsequent entries from logger thread are pushed into
// main_buffer
using fram_buffer_t = fram_buffer<sensors::data_point>;
static fram_buffer_t* main_f_buffer = nullptr;
static fram_buffer_t* secondary_f_buffer = nullptr;

void logger(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1) {
        gpio_pin_set_dt(&led, 1);

        k_mutex_lock(&secondary_buffer_mtx, K_FOREVER);

        const sensors::data_point p = sensors::get_data();
        if (k_mutex_lock(&main_buffer_mtx, K_NO_WAIT) == 0) {
            secondary_f_buffer->pop_all([&](const sensors::data_point& p) { main_f_buffer->push(p); });
            main_f_buffer->push(p);
            k_mutex_unlock(&main_buffer_mtx);
        } else {
            // use secondary buffer when data in main buffer is in read-acknowledge-remove phase
            secondary_f_buffer->push(p);
        }

        k_mutex_unlock(&secondary_buffer_mtx);

        gpio_pin_set_dt(&led, 0);
        k_sem_take(&logger_sleep_smph, K_SECONDS(config->get_period()));
    }
}

struct command {
    using handler_func = void(std::string_view params);

    std::string_view name;
    std::string_view description;
    handler_func* handler = nullptr;
};

static void print_help();

static void factory_reset_dialog() {
    printk("set new name (default = \"%s\"): ", default_name.data());
    std::string_view name = console_getline();
    if (name.empty()) {
        config->set_name(default_name);
    } else {
        config->set_name(name);
    }
    printk("set new period (default = %d): ", default_period);
    std::string_view period = console_getline();
    if (period.empty()) {
        config->set_period(default_period);
    } else {
        config->set_period(atoi(period.data()));
    }
}

static const command commands[] = {
    {.name = "get data"sv,
     .description = "- prints stored data"sv,
     .handler =
         [](std::string_view params) {
             k_mutex_lock(&main_buffer_mtx, K_FOREVER);
             time_t last_timestamp = 0;
             main_f_buffer->peek_all([&](const sensors::data_point& p) {
                 p.print();
                 last_timestamp = p.timestamp;
             });
             k_mutex_unlock(&main_buffer_mtx);
             printk("remove printed data from the device? (y/N): ");
             std::string_view s = console_getline();
             if (s == "y"sv) {
                 k_mutex_lock(&main_buffer_mtx, K_FOREVER);
                 main_f_buffer->clear_peeked();
                 k_mutex_unlock(&main_buffer_mtx);
             }
         }},
    {.name = "clear data"sv,
     .description = "- clears stored data"sv,
     .handler =
         [](std::string_view params) {
             k_mutex_lock(&main_buffer_mtx, K_FOREVER);
             k_mutex_lock(&secondary_buffer_mtx, K_FOREVER);
             main_f_buffer->clear();
             secondary_f_buffer->clear();
             k_mutex_unlock(&main_buffer_mtx);
             k_mutex_unlock(&secondary_buffer_mtx);
             printk("data cleared\n");
         }},
    {.name = "set time"sv,
     .description = "<time> - sets time"sv,
     .handler =
         [](std::string_view params) {
             if (rtc::set_current_time(params.data()) == -EINVAL) {
                 printk("invalid time\n");
             } else {
                 printk("time set\n");
             }
         }},
    {.name = "get time"sv,
     .description = "- prints time"sv,
     .handler =
         [](std::string_view params) {
             rtc::print_time(rtc::get_current_time());
             printk("\n");
         }},
    {.name = "set period"sv,
     .description = "<period> - sets period"sv,
     .handler =
         [](std::string_view params) {
             config->set_period(atoi(params.data()));
             k_sem_give(&logger_sleep_smph);
             printk("period set\n");
         }},
    {.name = "get period"sv,
     .description = "- prints period"sv,
     .handler = [](std::string_view params) { printk("%u\n", config->get_period()); }},
    {.name = "set name"sv,
     .description = "<name> - sets name"sv,
     .handler =
         [](std::string_view params) {
             config->set_name(params);
             printk("name set\n");
         }},
    {.name = "get name"sv,
     .description = "- prints name"sv,
     .handler = [](std::string_view params) { printk("%s\n", config->get_name().data()); }},
    {.name = "get status"sv,
     .description = "- prints device status"sv,
     .handler =
         [](std::string_view params) {
             printk("name: %s\n"
                    "period: %d\n",
                    config->get_name().data(),
                    config->get_period());
             printk("time: ");
             rtc::print_time(rtc::get_current_time());

             sensors::data_point p = sensors::get_data();
             auto print_sensor_value = [&](std::string_view name, sensor_value val) {
                 printk("  %s: ", name.data());
                 p.print_value(val);
                 printk("\n");
             };
             printk("\nsht45:\n");
             print_sensor_value("temperature"sv, p.sht_temperature);
             print_sensor_value("humidity"sv, p.sht_humidity);
             printk("bme280:\n");
             print_sensor_value("temperature"sv, p.bme_temperature);
             print_sensor_value("humidity"sv, p.bme_humidity);
             print_sensor_value("pressure"sv, p.bme_pressure);
         }},
    {.name = "factory reset"sv,
     .description = "- performs factory reset"sv,
     .handler =
         [](std::string_view params) {
             k_mutex_lock(&main_buffer_mtx, K_FOREVER);
             k_mutex_lock(&secondary_buffer_mtx, K_FOREVER);
             main_f_buffer->clear();
             secondary_f_buffer->clear();
             fram::clear();
             factory_reset_dialog();
             k_sem_give(&logger_sleep_smph);
             k_mutex_unlock(&main_buffer_mtx);
             k_mutex_unlock(&secondary_buffer_mtx);
             printk("reset complete\n");
         }},
    {.name = "help"sv, .description = "- prints help"sv, .handler = [](std::string_view params) { print_help(); }}};

static void print_help() {
    printk("available commands:\n");
    for (const auto& cmd : commands) {
        if (!cmd.name.empty()) {
            printk("%s", cmd.name.data());
            if (!cmd.description.empty()) {
                printk(" %s\n", cmd.description.data());
            } else {
                printk("\n");
            }
        }
    }
}

static void handle_command(std::string_view s) {
    for (const auto& cmd : commands) {
        if (s.starts_with(cmd.name)) {
            if (s.size() == cmd.name.size()) {
                if (cmd.handler) {
                    cmd.handler(""sv);
                }
                return;
            } else if (s[cmd.name.size()] == ' ') {
                s.remove_prefix(cmd.name.size() + 1);
                if (cmd.handler) {
                    cmd.handler(s);
                }
                return;
            }
        }
    }
    for (auto ch : s) {
        if (!std::isspace(ch)) {
            printk("invalid command\n");
            return;
        }
    }
}

void io(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1) {
        handle_command(console_getline());
    }
}

int main(void) {
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    console_getline_init();
    sensors::init();
    fram::init();
    rtc::init();

    static fram_buffer_t f_main_buf{fram::memory_map::env_main_buffer.begin(), fram::memory_map::env_main_buffer.end()};
    main_f_buffer = &f_main_buf;

    static fram_buffer_t f_secondary_buf{fram::memory_map::env_secondary_buffer.begin(),
                                         fram::memory_map::env_secondary_buffer.end()};
    secondary_f_buffer = &f_secondary_buf;

    static user_config conf{fram::memory_map::device_name.begin(), fram::memory_map::period.begin()};
    config = &conf;

    k_mutex_init(&main_buffer_mtx);
    k_mutex_init(&secondary_buffer_mtx);
    k_sem_init(&logger_sleep_smph, 0, 1);

    k_thread_create(&logger_thread_data,
                    logger_stack_area,
                    K_THREAD_STACK_SIZEOF(logger_stack_area),
                    logger,
                    nullptr,
                    nullptr,
                    nullptr,
                    logger_priority,
                    0,
                    K_NO_WAIT);

    k_thread_create(&io_thread_data,
                    io_stack_area,
                    K_THREAD_STACK_SIZEOF(io_stack_area),
                    io,
                    nullptr,
                    nullptr,
                    nullptr,
                    io_priority,
                    0,
                    K_NO_WAIT);
}

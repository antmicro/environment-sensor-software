#ifndef ANTENVSENS_USER_CONFIG_H
#define ANTENVSENS_USER_CONFIG_H
#include "fram.h"

#include <string_view>

class user_config {
    constexpr static uint32_t min_period = 1;

    fram::addr_t m_name_addr;
    fram::addr_t m_period_addr;

    char m_name[fram::memory_map::device_name.size()];
    size_t m_name_len;
    uint32_t m_period;

  public:
    user_config(fram::addr_t name_addr, fram::addr_t period_addr);

    void set_period(uint32_t period);
    uint32_t get_period() const;
    void set_name(std::string_view name);
    std::string_view get_name() const;
};

#endif
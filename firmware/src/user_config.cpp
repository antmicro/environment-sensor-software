#include "user_config.h"

#include <algorithm>
#include <cstring>

user_config::user_config(fram::addr_t name_addr, fram::addr_t period_addr)
    : m_name_addr{name_addr}, m_period_addr(period_addr) {
    fram::read(m_name_addr, m_name);
    m_name[sizeof(m_name) - 1] = '\0';
    m_name_len = strlen(m_name);
    fram::read(m_period_addr, m_period);
    m_period = std::max(m_period, min_period);
}

void user_config::set_period(uint32_t period) {
    m_period = std::max(period, min_period);
    fram::write(m_period_addr, period);
}

uint32_t user_config::get_period() const { return m_period; }

void user_config::set_name(std::string_view name) {
    m_name_len = std::min(name.size() + 1, sizeof(m_name)); // +1 to include \0
    memcpy(m_name, name.data(), m_name_len);
    m_name[sizeof(m_name) - 1] = '\0';
    fram::write(m_name_addr, m_name);
}

std::string_view user_config::get_name() const { return std::string_view{m_name, m_name_len - 1}; }

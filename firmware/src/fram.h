#ifndef ANTENVSENS_FRAM_H
#define ANTENVSENS_FRAM_H
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace fram {
using addr_t = uint32_t;

constexpr addr_t fram_size = 0x20000;

class memory_block {
    addr_t m_begin;
    addr_t m_size;

  public:
    constexpr memory_block(addr_t begin, addr_t size) : m_begin{begin}, m_size{size} {}
    constexpr addr_t begin() const { return m_begin; }
    constexpr addr_t end() const { return m_begin + m_size; }
    constexpr addr_t size() const { return m_size; }
};

namespace memory_map {
constexpr memory_block device_name = {0, 256};
constexpr memory_block period = {device_name.end(), 4};
constexpr memory_block env_secondary_buffer = {period.end(), 5100};
constexpr memory_block env_main_buffer = {env_secondary_buffer.end(), fram_size - env_secondary_buffer.end()};
}

void init();
void clear();
void write_raw(addr_t addr, const void* data, size_t size);
void read_raw(addr_t addr, void* data, size_t size);

template <typename T>
void read(addr_t addr, T& t)
    requires std::is_trivially_copyable_v<T>
{
    read_raw(addr, reinterpret_cast<void*>(&t), sizeof(t));
}

template <typename T>
T read(addr_t addr)
    requires std::is_trivially_copyable_v<T>
{
    T t;
    read(addr, t);
    return t;
}

template <typename T>
void write(addr_t addr, const T& t)
    requires std::is_trivially_copyable_v<T>
{
    write_raw(addr, reinterpret_cast<const void*>(&t), sizeof(t));
}
}

#endif

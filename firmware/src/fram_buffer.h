#ifndef ANTENVSENS_FRAM_BUFFER_H
#define ANTENVSENS_FRAM_BUFFER_H
#include "fram.h"

#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>

#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>

/*
fram_buffer is a circular buffer stored in fram.
Size of each entry in buffer is constant and equal to sizeof(T) + size of
metadata (header and checksum). Each entry starts with header used to find ends
of data block after startup (this is more robust than keeping positons of ends
of data block separately in fram as after power loss we will lose at most one
entry), proceeded by user data and ends with crc32 checksum of user data.
*/
template <typename T>
    requires std::is_trivially_copyable_v<T> // buffer is stored in nonreferenceable address space
class fram_buffer {
    using addr_t = fram::addr_t;
    using crc_t = uint32_t;

    // range of fram memory available to buffer
    addr_t m_buf_begin;
    addr_t m_buf_end;
    // range of user data stored in buffer
    addr_t m_data_begin;
    addr_t m_data_end;

    struct entry_header {
        constexpr static auto valid_signature = 0x75;
        uint8_t signature : 7 = valid_signature;
        bool peeked : 1 = false;
        bool is_valid() const { return signature == valid_signature; }
    };
    constexpr static auto entry_size = sizeof(entry_header) + sizeof(T) + sizeof(crc_t);

    using it_func = addr_t (fram_buffer::*)(addr_t) const;
    // finds first entry for which predicate @pred returns true, uses @advance to
    // iterate through buffer starting from
    // @start_entry
    addr_t find_entry(const addr_t start_entry, it_func advance, std::invocable<entry_header> auto&& pred) {
        const addr_t first_entry = (this->*advance)(start_entry);
        addr_t next_entry;
        for (addr_t entry = first_entry; entry != start_entry; entry = next_entry) {
            if (pred(fram::read<entry_header>(entry))) {
                return entry;
            }
            next_entry = (this->*advance)(entry);
        }
        return m_buf_begin;
    }

    // returns address of next entry, assumes that @entry is valid entry address
    addr_t next_entry(addr_t entry) const {
        if (entry == m_buf_begin + (capacity() - 1) * entry_size) {
            return m_buf_begin;
        } else {
            return entry + entry_size;
        }
    }

    // returns address of previous entry, assumes that @entry is valid entry
    // address
    addr_t prev_entry(addr_t entry) const {
        if (entry == m_buf_begin) {
            return m_buf_begin + (capacity() - 1) * entry_size;
        } else {
            return entry - entry_size;
        }
    }

    std::optional<T> read_entry(addr_t entry) {
        const entry_header header = fram::read<entry_header>(entry);
        const T elem = fram::read<T>(entry + sizeof(entry_header));
        const crc_t read_crc = fram::read<crc_t>(entry + entry_size - sizeof(crc_t));
        const crc_t calc_crc = entry_crc(header, elem);
        if (read_crc == calc_crc) {
            return elem;
        }
        return {};
    }

    crc_t entry_crc(entry_header header, const T& elem) {
        crc_t crc = crc32_ieee(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
        crc = crc32_ieee_update(crc, reinterpret_cast<const uint8_t*>(&elem), sizeof(elem));
        return crc;
    }

    void invalidate_entry(addr_t entry) { fram::write(entry, entry_header{.signature = 0}); }

  public:
    fram_buffer(addr_t begin, addr_t end) : m_buf_begin{begin}, m_buf_end{end} {
        // restores m_data_begin and m_data_end using data from fram
        if (fram::read<entry_header>(begin).is_valid()) {
            m_data_end =
                find_entry(m_buf_begin, &fram_buffer::next_entry, [](entry_header h) { return !h.is_valid(); });
            m_data_begin = next_entry(
                find_entry(m_buf_begin, &fram_buffer::prev_entry, [](entry_header h) { return !h.is_valid(); }));
        } else {
            m_data_end = next_entry(
                find_entry(m_buf_begin, &fram_buffer::prev_entry, [](entry_header h) { return h.is_valid(); }));
            m_data_begin =
                find_entry(m_buf_begin, &fram_buffer::next_entry, [](entry_header h) { return h.is_valid(); });
        }
    }

    size_t capacity() const { return (m_buf_end - m_buf_begin) / entry_size; }

    // adds element to the buffer
    void push(const T& elem) {
        const addr_t next = next_entry(m_data_end);
        const entry_header next_header = fram::read<entry_header>(next);
        if (next_header.is_valid()) {
            // front of buffer reached back of buffer, ends of buffer have to be
            // separated by an empty entry to properly restore backup from fram
            // after restart
            invalidate_entry(next);
            m_data_begin = next_entry(next);
        }

        const entry_header header;
        const crc_t crc = entry_crc(header, elem);
        fram::write(m_data_end, header);
        fram::write(m_data_end + sizeof(entry_header), elem);
        fram::write(m_data_end + entry_size - sizeof(crc), crc);

        m_data_end = next;
    }

    // invokes func for every valid element in buffer in chronological order,
    // returns amount of entries with checksum mismatch
    uint16_t peek_all(std::invocable<T&> auto&& func) {
        uint16_t invalid_entries = 0;
        for (addr_t entry = m_data_begin; entry != m_data_end; entry = next_entry(entry)) {
            const std::optional<T> elem = read_entry(entry);
            if (elem) {
                func(*elem);
                const entry_header header{.peeked = true};
                const crc_t crc = entry_crc(header, *elem);
                fram::write(entry, header);
                fram::write(entry + entry_size - sizeof(crc), crc);
            } else {
                invalid_entries++;
            }
        }

        return invalid_entries;
    }

    // clears entries visited during peek_all() invocation
    void clear_peeked() {
        for (addr_t entry = m_data_begin; entry != m_data_end; entry = next_entry(entry)) {
            const entry_header header = fram::read<entry_header>(entry);
            if (header.is_valid()) {
                if (header.peeked) {
                    invalidate_entry(entry);
                } else {
                    return;
                }
            }
            m_data_begin = next_entry(entry);
        }
    }

    // invokes func for every valid element in buffer in chronological order and
    // removes elements from buffer, returns amount of entries with checksum
    // mismatch
    uint16_t pop_all(std::invocable<T&> auto&& func) {
        uint16_t invalid_entries = 0;
        for (addr_t entry = m_data_begin; entry != m_data_end; entry = next_entry(entry)) {
            const std::optional<T> elem = read_entry(entry);
            if (elem) {
                func(*elem);
            } else {
                invalid_entries++;
            }
            invalidate_entry(entry);
        }

        m_data_end = m_buf_begin;
        m_data_begin = m_buf_begin;

        return invalid_entries;
    }

    void clear() {
        addr_t entry = m_buf_begin;
        do {
            invalidate_entry(entry);
            entry = next_entry(entry);
        } while (entry != m_buf_begin);

        m_data_begin = m_buf_begin;
        m_data_end = m_buf_begin;
    }

    // default ones create shallow copies
    fram_buffer(const fram_buffer&) = delete;
    fram_buffer& operator=(const fram_buffer&) = delete;
};

#endif
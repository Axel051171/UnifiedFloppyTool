#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdint>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class bit_array
{
private:
    std::vector<uint8_t>    m_array_data;
    size_t                  m_bit_length;
    size_t                  m_stream_pos;
    bool                    m_wraparound;

    inline size_t to_byte_pos(size_t bit_pos) { return bit_pos >> 3; }
    inline uint8_t to_bit_pos(size_t bit_pos) { return 0x80u >> (bit_pos & 0x07u); }

    inline size_t to_alloc_unit(size_t size) { return ((((size)/256)+1)*256); }
public:

    bit_array() : m_bit_length(0), m_stream_pos(0), m_wraparound(false) {
        m_array_data.clear();
    }

    void set_array(std::vector<uint8_t>& buf, int bit_length=-1);
    void set_array(size_t size);
    void clear_with_0(void);
    void extend_buffer(int new_buf_size_in_bit);
    std::vector<uint8_t> get_array(void) const;
    void clear_array(void);
    size_t get_bit_length(void) const;
    size_t size(void) const;
    void resize(size_t bit_length);
    void reserve(size_t bit_length);
    inline bool is_wraparound(void) { return m_wraparound; }
    inline void clear_wraparound_flag(void) { m_wraparound = false; }

    void    set(int index, uint8_t value, bool elastic=true);
    uint8_t get(int index);

    bool set_stream_pos(int position);
    size_t get_stream_pos(void);
    void write_stream(uint8_t value, bool elastic = false);
    void advance_stream_pos(bool elastic = false);
    uint8_t read_stream(void);
    size_t distance_to_next_pulse(void);
    void fill_stream(int start_pos, int end_pos, uint8_t data);

    void dump(size_t start = 0, size_t size = 0);

    void save(std::string file_name);
    void load(std::string file_name);
};

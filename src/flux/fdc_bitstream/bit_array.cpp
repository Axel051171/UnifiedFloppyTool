/**
 * @file bit_array.cpp
 * @author Yasunori Shimura (yasu0710@gmail.com)
 * @brief Bit array class to support bit granularity data access for FDC. The bit array supports the ring-buffer and elastic buffer modes.
 * @version 0.1
 * @date 2022-08-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "bit_array.h"

#include <string.h>

/**
 * @brief Set a bit array data.
 * 
 * @param buf New array (std::vector<uint8_t>)
 */
void bit_array::set_array(std::vector<uint8_t>& buf, int bit_length) {
    m_array_data = buf;
    if(bit_length >= 0 && bit_length <= buf.size() * 8) {
        m_bit_length = bit_length;
    } else {
        m_bit_length = buf.size() * 8;
    }
}

/**
 * @brief Set an empty array. The new bit array will be filled with 0.
 * 
 * @param size_in_bits Size of the new empty bit array.
 */
void bit_array::set_array(size_t size) {
    m_array_data.resize(size);
    memset(m_array_data.data(), 0, size);
    m_bit_length = size * 8;
}


void bit_array::clear_with_0(void) {
    std::fill(m_array_data.begin(), m_array_data.end(), 0);
}

void bit_array::extend_buffer(int new_buf_size_in_bit) {
    if(new_buf_size_in_bit <= m_bit_length) return;

    size_t byte_count = to_byte_pos(new_buf_size_in_bit);
    if(byte_count >= m_array_data.size()) {
        m_array_data.resize(to_alloc_unit(byte_count));  // Extend the buffer if the new buffer size is larger than the preserved memory buffer size
    }
    fill_stream(m_bit_length, m_array_data.size()*8, 0); // clear extend part
    m_bit_length = new_buf_size_in_bit;
}


/**
 * @brief Get an copy of the bit array data.
 * 
 * @return std::vector<uint8_t> Bit array data.
 */
std::vector<uint8_t> bit_array::get_array(void) const {
    return m_array_data;
}

/**
 * @brief Clear bit array data. The length of the array will be 0.
 * 
 */
void bit_array::clear_array(void) {
    m_array_data.clear();
    m_bit_length = 0;
    m_stream_pos = 0;
}

/**
 * @brief Get bit length of the bit array data.
 * 
 * @return size_t Buffer length in bit unit.
 */
size_t bit_array::get_bit_length(void) const{
    return m_bit_length;
}

/**
 * @brief Get the size (==length) of the bit array data.
 * 
 * @return size_t Buffer length in bit unit.
 */
size_t bit_array::size(void) const {
    return m_bit_length;
}

/**
 * @brief Resize bit array data.
 * 
 * @param bit_length New length in bit unit.
 */
void bit_array::resize(size_t bit_length) {
    if(bit_length > m_bit_length) {
        extend_buffer(bit_length);
    } else if(bit_length < m_bit_length) {
        size_t new_size_in_bytes = to_alloc_unit(to_byte_pos(bit_length));
        m_array_data.resize(new_size_in_bytes);
        m_bit_length = bit_length;
    } else {
        // bit_length == m_bit_length. nothing to do
    }
}

/**
 * @brief Reserve bit array capacity for future array extention.
 * 
 * @param bit_length Capacity in bit to reserve.
 */
void bit_array::reserve(size_t bit_length) {
    size_t new_size_in_bytes = to_byte_pos(bit_length) + 1;
    m_array_data.reserve(new_size_in_bytes);
}

//inline bool bit_array::is_wraparound(void) { return m_wraparound; }
//inline void bit_array::clear_wraparound_flag(void) { m_wraparound = false; }

/**
 * @brief Set a bit data to the bit array.
 * 
 * @param index Bit position to set the data.
 * @param value Data to set.
 */
void bit_array::set(int index, uint8_t value, bool elastic) {
    while(index < 0) {
        index += static_cast<int>(m_bit_length);
    }
    if(elastic==false && index >= m_bit_length) {
        index = index % m_bit_length;
    }

    size_t  byte_pos = to_byte_pos(index);
    uint8_t bit_pos = to_bit_pos(index);

    if (byte_pos >= m_array_data.size()) {
        extend_buffer(index+1);
    }
    if(index >= m_bit_length) {
        m_bit_length = index;
    }

    if (value != 0) {
        m_array_data[byte_pos] |= bit_pos;
    } else {
        m_array_data[byte_pos] &= ~bit_pos;
    }
}

/**
 * @brief Get a bit data from the bit array.
 * 
 * @param index Bit position to read.
 * @return bool Read data.
 */
uint8_t bit_array::get(int index) {
    if(m_array_data.size() == 0) {
        m_wraparound = true;
        return 0;
    }
    while(index < 0) {
        index += static_cast<int>(m_bit_length);
    }
    if(index >= m_bit_length) {
        index = index % m_bit_length;
    }

    size_t  byte_pos = to_byte_pos(index);
    uint8_t bit_pos = to_bit_pos(index);
    uint8_t res = (m_array_data[byte_pos] & bit_pos) ? 1 : 0;
    return res;
}


/**
 * @brief Set read/write pointer for streaming operations.
 * 
 * @param position Bit position to read and write the bit array as bit stream.
 * @return true 
 * @return false 
 */
bool bit_array::set_stream_pos(int position) {
    while(position < 0) {
        position += static_cast<int>(m_bit_length);
    }
    if(position >= m_bit_length) {
        position = position % m_bit_length;
    }
#if 0
    if (position < m_bit_length) {
        m_stream_pos = position;
    }
    else {
        position = m_bit_length - 1;
    }
#endif
    m_stream_pos = position;
    m_wraparound = false;
    return true;
}

/**
 * @brief Get stream pointer position.
 * 
 * @return size_t Pointer position.
 */
size_t bit_array::get_stream_pos(void) {
    return m_stream_pos;
}



/**
 * @brief Write a bit data sequentially (stream mode)
 * 
 * @param value Data to write.
 * @param elastic Operation mode (false=non-elastic and ring-buffer mode. The pointer will wrap around at the end of the buffer / true=elastic, buffer will extend if pointer go over the end of the buffer )
 */
void bit_array::write_stream(uint8_t value, bool elastic) {
    set(m_stream_pos++, value, elastic);
    if (m_stream_pos >= m_bit_length && elastic == false) {     // reached to the end of the bit array
        m_stream_pos = 0;           // wrap around only when elastic==false
        m_wraparound = true;
    }
}

/**
 * @brief Advance the stream read/write pointer for 1 bit (without actual read/write operation)
 * 
 */
void bit_array::advance_stream_pos(bool elastic) {
    if(m_array_data.size() == 0) {
        m_stream_pos = 0;
        m_wraparound = true;
        return;
    }
    m_stream_pos++;
    if (m_stream_pos >= m_bit_length) {     // wrap around
        if (elastic == false) {
            m_stream_pos = 0;
            m_wraparound = true;
        } else {
            size_t byte_pos = to_byte_pos(m_stream_pos);
            m_array_data.resize(byte_pos + 10, 0);      // Extend the buffer
            m_bit_length = m_stream_pos + 1;
        }
    } 
}

/**
 * @brief Read a bit data sequentially (stream mode). The streaming pointer will wrap around when the last bit is read.
 * 
 * @return uint8_t Read data.
 */
uint8_t bit_array::read_stream(void) {
    if(m_array_data.size() == 0) {
        m_wraparound = true;
        return 0;
    }
    uint8_t val = get(m_stream_pos++);
    if (m_stream_pos >= m_bit_length) {     // wrap around
        m_stream_pos = 0;
        m_wraparound = true;
    }
    return val;
}

/**
 * @brief Calculate the distance from the current stream position to the next bit 1.
 * 
 * @return size_t Distance to next bit 1 (in bit unit).
 */
size_t bit_array::distance_to_next_pulse(void) {
    if(m_array_data.size() == 0) {
        m_wraparound = true;
        return 0;
    }
    size_t distance = 0;
    uint8_t val;
    if (m_bit_length == 0) return 0;
    do {
        val = read_stream();
        if (distance++ >= m_bit_length) {       // distance exceeded the entire bit array length
            return 0;
        }
    } while (val == 0);
    return distance;
}

/**
 * @brief Fill bit array buffer from the current position with the specified value.
 * 
 * @param data Data to fill.
 * @param length Number of bits to fill.
 */
void bit_array::fill_stream(int start_pos, int end_pos, uint8_t data) {
    if(start_pos >= m_bit_length) start_pos = m_bit_length - 1;
    if(end_pos   >= m_bit_length) end_pos   = m_bit_length - 1;
    if(start_pos >= end_pos) return;
    for (int i = start_pos; i < end_pos; i++) {
        set(i, data);
    }
}

/**
 * @brief Dump bit array data.
 * 
 * @param start Start position (default=0).
 * @param size Number of bits to dump (default=size of bit array)
 */
void bit_array::dump(size_t start, size_t size) {
    if (size == 0) {
        size = m_bit_length - start;
    }
    set_stream_pos(start);
    size_t count = 0;
    while (count++ < size) {
        uint8_t val = read_stream();
        std::cout << (val != 0 ? '1' : '0');
        if (count % 8 == 0) {
            std::cout << ' ';
        }
        if (count % 64 == 0) {
            std::cout << std::endl;
        }
    }
}

/**
 * @brief Save bit arry data to a file.
 * 
 * @param file_name File name.
 */
void bit_array::save(std::string file_name) {
    std::ofstream ofs;
    ofs.open(file_name, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<char*>(&m_bit_length), sizeof(size_t));                  // bit length
    ofs.write(reinterpret_cast<char*>(m_array_data.data()), m_array_data.size());       // data
    std::cout << m_bit_length << std::endl;
    ofs.close();
}

/**
 * @brief Load bit array from a file.
 * 
 * @param file_name File name.
 */
void bit_array::load(std::string file_name) {
    std::ifstream ifs;
    ifs.open(file_name, std::ios::in | std::ios::binary);
    if (ifs.is_open() == false) {
#ifdef DEBUG
        std::cerr << "Failed to open a file'" << file_name << "'." << std::endl;
#endif
        clear_array();
        return;
    }
    ifs.seekg(0, std::ios_base::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    ifs.read(reinterpret_cast<char*>(&m_bit_length), sizeof(size_t));
    std::cout << m_bit_length << std::endl;
    size -= sizeof(size_t);
    set_array(size);
    ifs.read(reinterpret_cast<char*>(m_array_data.data()), size);
    m_stream_pos = 0;
}
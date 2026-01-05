/**
 * @file fdc_bitstream.cpp
 * @brief MFM Floppy FDC emulator
 * @author Yasunori Shimura
 * 
 * @details A FDC emulator that handles 'raw' (uncooked) MFM/IBM-PC format floppy disk bit stream data as a track data.
 *
 * @copyright Copyright (c) 2022
 */

#include <vector>

#include <string.h>

#include "fdc_misc.h"
#include "fdc_bitstream.h"

//#define DEBUG

/**
 * @brief Construct a new fdc bitstream::fdc bitstream object.
 * 
 */
fdc_bitstream::fdc_bitstream() : m_state(fdc_state::IDLE), m_sampling_rate(4e6), m_data_bit_rate(500e3) {
    m_crcgen.reset();
    m_codec.reset();
    set_fdc_params(m_sampling_rate, m_data_bit_rate);

    m_rand_engine = std::default_random_engine(std::default_random_engine());
    m_rand_dist = std::normal_distribution<>(0.0, 0.5);
};


/**
* @brief Set FDC parameters.
* 
* @param[in] sampling_rate The sampling rate of the floppy image (track data) in MHz unit. (e.g. 4e6 == 4MHz)
* @param[in] data_bit_rate The FDC bit rate in bit/sec uint. (MFM/2D == 500e3 == 500HKz)
* @param[in] bit_window_ratio The ratio of the bit window period to the entire bit cell width (<1.0). In general, the wider the bit window, the more stable the bit reading.  Default in the specification is 0.5 but 0.75 is recommended for stable reading. 
* @return none
*/
void fdc_bitstream::set_fdc_params(size_t sampling_rate, size_t data_bit_rate, double bit_window_ratio) {
    m_sampling_rate = sampling_rate;
    m_data_bit_rate = data_bit_rate;
    m_codec.set_data_bit_rate(m_data_bit_rate);
    m_codec.set_sampling_rate(m_sampling_rate);

    if(bit_window_ratio<0.2 || bit_window_ratio>0.8) {
        bit_window_ratio = 0.5f;
    }
    //size_t bit_cell_size = m_sampling_rate / m_data_bit_rate;
    //m_codec.set_cell_size(bit_cell_size, bit_window_ratio);
}

/**
* @brief Read track. Reads track data from the current position until the pointer reaches to the end of the track (no wrap around).
* 
* @param[in] none
* @return std::vector<uint8_t> Read track data (Decoded MFM data)
*/
std::vector<uint8_t> fdc_bitstream::read_track(void) {
    uint8_t read_data;
    bool missing_clock;
    std::vector<uint8_t> track_data;
    if (m_codec.is_track_ready() == false) return track_data;

    m_codec.clear_wraparound();
    while (m_codec.is_wraparound() == false) {
        m_codec.mfm_read_byte(read_data, missing_clock, false, false);
#ifdef DEBUG
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(read_data) << " ";
#endif
        track_data.push_back(read_data);
    }
    return track_data;
}

/**
* @brief Read track. Reads track data with missing clock information. (debug/analyze purpose)
* 
* @param[in] none
* @return std::vector<std::vector<size_t>> Read track data (Decoded MFM data) missing clock mark, and bit position [[mfm,mc,pos],[mfm,mc,pos],...]
*/
std::vector<std::vector<size_t>> fdc_bitstream::read_track_ex(void) {   
    std::vector<std::vector<size_t>> res;
    size_t pos;
    uint8_t read_data;
    bool missing_clock;
    double error;
    if (m_codec.is_track_ready() == false) return std::vector<std::vector<size_t>>();

    m_codec.clear_wraparound();
    while (m_codec.is_wraparound() == false) {
        pos = m_codec.get_real_pos();
        if(pos>get_track_length()) pos = 0;         // exceptional case handling
        m_codec.mfm_read_byte(read_data, missing_clock, error, false, false);
#ifdef DEBUG
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(read_data) << " ";
#endif
        res.push_back(std::vector<size_t>{static_cast<size_t>(read_data), static_cast<size_t>(missing_clock?1:0), pos, static_cast<size_t>(error * 1000)});
    }
    return res;
}


/**
* @brief Write track. Start writing from the current position until all data is written. 
*        The track buffer is treated as a ring buffer, so the pointer will wrap around if the write data goes over the buffer's end. 
*        Write track recognizes MB8876/FDC179x compatible special codes such as $f5, $f6, $f7, $fb, $fe, and so on.
* 
* @param[in] track_buf Write track data
* @return none
*/
void fdc_bitstream::write_track(const std::vector<uint8_t>& track_buf) {
    uint16_t crc_val;
    for (auto it = track_buf.begin(); it != track_buf.end(); ++it) {
        uint8_t data = *it;
        switch (data) {
        case 0xf5:      // marker A1
            write_data(data, true);
            m_crcgen.data(0xa1);
            break;
        case 0xf6:      // marker C2
            write_data(data, true);
            m_crcgen.data(0xc2);
            break;
        case 0xf7:      // write CRC (two bytes)
            m_crcgen.data(0x00);
            m_crcgen.data(0x00);
            crc_val = m_crcgen.get();
            write_data(crc_val >> 8);
            write_data(crc_val & 0xffu);
            break;
        case 0xf8:      // data address marks (reset CRC)
        case 0xf9:      // data address marks (reset CRC)
        case 0xfa:      // data address marks (reset CRC)
        case 0xfb:      // data address marks (reset CRC)
        case 0xfe:      // ID address marks (reset CRC)
            m_crcgen.reset();
            write_data(data);
            m_crcgen.data(data);
            break;
        default:
            write_data(data);
            m_crcgen.data(data);
            break;
        }
    }
    m_codec.set_track_status_forcibly(true);
}

/**
* @brief Read an sector ID. Start reading from the current position and read the 1st found sector ID.
*
* @param[in] none
* @param[out] id_field read sector ID data
* @param[out] crc_error crc error status
* @return size_t Track position of the read_id() started reading.

*/
size_t fdc_bitstream::read_id(std::vector<uint8_t>& id_field, bool& crc_error) {
    uint8_t read_data, mc_byte = 0;
    bool missing_clock;
    crc_error = false;
    size_t read_count = 0;
    id_field.clear();
    if (m_codec.is_track_ready() == false) return 0;
    size_t total_read_count = 0;
    size_t read_start_pos = 0;
    size_t track_length_byte = m_codec.get_track_length() / ((m_sampling_rate/m_data_bit_rate)*2*8);
    while (true) {
        switch (m_state) {
        case fdc_state::IDLE:
            m_codec.mfm_read_byte(read_data, missing_clock, false, false);
            if (missing_clock) {
                m_state = fdc_state::CHECK_MARK;
                mc_byte = read_data;
            }
            break;
        case fdc_state::CHECK_MARK:
            read_start_pos = m_codec.get_pos();
            m_codec.mfm_read_byte(read_data, missing_clock, false, false);
            if (missing_clock) {
                mc_byte = read_data;
            }
            else if (mc_byte == 0xa1 && (read_data == 0xfc || read_data == 0xfd || read_data == 0xfe || read_data == 0xff /* IDAMs*/)) {
                id_field.clear();
                id_field.push_back(read_data);
                m_crcgen.reset();
                m_crcgen.data(read_data);
                read_count = 4 + 2;    // ID+CRC
                m_state = fdc_state::READ_IDAM;
            }
            else {
                m_state = fdc_state::IDLE;
            }
            break;
        case fdc_state::READ_IDAM:
            m_codec.mfm_read_byte(read_data, missing_clock, true, true);
            id_field.push_back(read_data);
            m_crcgen.data(read_data);
            if (--read_count == 0) {
                crc_error = ((m_crcgen.get() == 0) ? false : true);
                id_field.erase(id_field.begin());       // remove the ID mark on the top
                m_state = fdc_state::OPERATION_COMPLETED;
            }
            break;
        default:
            m_state = fdc_state::IDLE;
            break;
        }

        if (m_state == fdc_state::OPERATION_COMPLETED) {
            break;
        }
        if (++total_read_count > track_length_byte) { 
            break;
        }
    }
    return read_start_pos;
}

/**
 * @brief Reads all IDAM in the track buffer.
 * 
 * @return std::vector<fdc_bitstream::id_field> Read ID field data. 
 */
std::vector<fdc_bitstream::id_field> fdc_bitstream::read_all_idam(void) {
    std::vector<id_field> result;
    id_field item;
    memset(&item, 0, sizeof(id_field));
    std::vector<uint8_t> sect_id;
    bool crc_error = false;
    if (m_codec.is_track_ready() == false) return result;
    clear_wraparound();
    set_pos(0);
    while(true) {
        sect_id.clear();
        size_t pos = read_id(sect_id, crc_error);
        if (is_wraparound() == true) break;
        if (sect_id.size()>0) {
            item.C = sect_id[0];
            item.H = sect_id[1];
            item.R = sect_id[2];
            item.N = sect_id[3];
            item.crc_val = (sect_id[4] << 8) + sect_id[5];
            item.crc_sts = crc_error;
            item.pos = pos;
            item.end_pos = m_codec.get_real_pos();
            result.push_back(item);
        }
    }
    return result;
}




/**
* @brief Read a sector body with byte position data.  
*        Read sector data from the next nearest DAM/DDAM mark from the current position. 
*        This function doesn't care about the sector ID and simply reads the subsequent sector data.
*        Combine the read_id() function to find the desired sector ID and call this function to read the sector body data.
* 
* @param[in] sect_length Sector length code (0-3)
* @param[out] sect_data Read sector data buffer
* @param[out] pos The start bit position of each byte (in bit_array position). Ignored if the 
* @param[out] crc_error CRC error flag (true=error)
* @param[out] dam_type (true=DDAM/false=DAM)
* @param[out] record_not_found (true=Record-not-found, false=no error)
* @param[in] time out enable flag (true=time out when DAM can't find in 43 bytes of data read / false = no time out)
* @return size_t Track position of the read_sector() started reading.
*/
size_t fdc_bitstream::read_sector_body_ex(size_t sect_length_code, std::vector<uint8_t>& sect_data, std::vector <size_t> &pos, bool& crc_error, bool& dam_type, bool &record_not_found, bool timeout) {
    std::vector<size_t> sector_length_table{ 128, 256, 512, 1024 };

    sect_data.clear();
    crc_error = false;
    dam_type = false;
    record_not_found = false;

    size_t read_count = 0;
    size_t total_read_count = 0;
    size_t read_start_pos = 0;
    int time_out_count;
    time_out_count = timeout ? 43 : -1;     // Time out (=record not found) in 43 bytes

    uint8_t read_data, mc_byte = 0;
    bool missing_clock;

    while (true) {
        switch (m_state) {
        case fdc_state::IDLE:
            m_codec.mfm_read_byte(read_data, missing_clock, false, false);
            if (missing_clock) {
                m_state = fdc_state::CHECK_MARK;
                mc_byte = read_data;
            }
            break;
        case fdc_state::CHECK_MARK:
            read_start_pos = m_codec.get_pos();
            m_codec.mfm_read_byte(read_data, missing_clock, false, false);
            if (missing_clock) {
                mc_byte = read_data;
            }
            else if (mc_byte == 0xa1 && (read_data == 0xfa || read_data == 0xfb || read_data == 0xf8 || read_data == 0xf9)) {
                if (read_data == 0xfa || read_data == 0xfb) {     // Normal DAM
                    dam_type = false;
                }
                else if (read_data == 0xf8 || read_data == 0xf9) {     // DDAM
                    dam_type = true;
                }
                m_crcgen.reset();
                m_crcgen.data(read_data);
                read_count = sector_length_table[sect_length_code & 0b0011u] + 2;   // sector size+CRC
                time_out_count = -1;                                                // hereafter, no time out
                m_state = fdc_state::READ_SECT;
            }
            else {
                m_state = fdc_state::IDLE;
            }
            break;
        case fdc_state::READ_SECT:
            pos.push_back(m_codec.get_real_pos());
            m_codec.mfm_read_byte(read_data, missing_clock, true, true);
            m_crcgen.data(read_data);
            sect_data.push_back(read_data);
            if (--read_count == 0) {
                crc_error = ((m_crcgen.get() == 0) ? false : true);
                sect_data.pop_back();                     // remove CRC field
                sect_data.pop_back();                     // remove CRC field
                m_state = fdc_state::OPERATION_COMPLETED;
            }
            break;
        default:
            m_state = fdc_state::IDLE;
            break;
        }

        if (m_state == fdc_state::OPERATION_COMPLETED) {
            break;
        }
        if (time_out_count >0 ) {
            if (--time_out_count == 0) {        // DAM/DDAM didn't appear in 43 bytes
                record_not_found = true;
                return read_start_pos;
            }
        }
        total_read_count++;
        if (total_read_count > m_codec.get_track_length()) {
            break;
        }
    }
    return read_start_pos;
}

/**
* @brief Read a sector body. 
*        Read sector data from the next nearest DAM/DDAM mark from the current position. 
*        This function doesn't care about the sector ID and simply reads the subsequent sector data.
*        Combine the read_id() function to find the desired sector ID and call this function to read the sector body data.
* 
* @param[in] sect_length Sector length code (0-3)
* @param[out] sect_data Read sector data buffer
* @param[out] crc_error CRC error flag (true=error)
* @param[out] dam_type (true=DDAM/false=DAM)
* @param[out] record_not_found (true=Record-not-found, false=no error)
* @param[in] time out enable flag (true=time out when DAM can't find in 43 bytes of data read / false = no time out)
* @return size_t Track position of the read_sector() started reading.
*/
size_t fdc_bitstream::read_sector_body(size_t sect_length_code, std::vector<uint8_t>& sect_data, bool& crc_error, bool& dam_type, bool &record_not_found, bool timeout) {
    std::vector<size_t> sector_length_table{ 128, 256, 512, 1024 };
    
    std::vector<size_t> dummy_pos;
    return read_sector_body_ex(sect_length_code, sect_data, dummy_pos, crc_error, dam_type, record_not_found, timeout);
}



/**
* @brief Write a sector.
*        Write sector data from the current position.
*        Please be aware that this function follows and emulates the write sequence of the FD179x/MB8876 FDC device.
*        This means this function will skip certain bytes from the start position and then start actual data writing that includes SYNC bytes and the data address marks.
*        To allow the user program to emulate the 'force interrupt' command of FD179x/MB8876, this function has an option not to write the CRC bytes and the last 0xff byte.
* 
* @param[in] write_data The sector data to write.
* @param[in] dam_type Specify the type of data address mark (true:DDAM, false:DAM(normal))
* @param[in] write_crc The flag to specify whether write the last CRC + $FF, or not.
*/
// dam_type : false=DAM, true=DDAM
void fdc_bitstream::write_sector_body(std::vector<uint8_t> write_data, bool dam_type, bool write_crc) {
    for (int i = 0; i < 2; i++) m_codec.mfm_write_byte(0x00, false, false);     // WG==false == dummy write
    for (int i = 0; i < 8; i++) m_codec.mfm_write_byte(0x00, false, false);     // WG==false == dummy write
    for (int i = 0; i < 1; i++) m_codec.mfm_write_byte(0x00, false, false);     // WG==false == dummy write
    for (int i = 0; i < 11; i++) m_codec.mfm_write_byte(0x00, false, false);     // WG==false == dummy write
    for (int i = 0; i < 12; i++) m_codec.mfm_write_byte(0x00);                   // 0x00 x 12
    for (int i = 0; i < 3; i++) m_codec.mfm_write_byte(0xf5, true);             // 0xf5 (0xa1*) x 3

                                                                                    // reset CRC
    m_crcgen.reset();

    // write DAM/DDAM
    if (dam_type == false) {
        m_codec.mfm_write_byte(0xfb);     // 0xfb (normal DAM) x 1
        m_crcgen.data(0xfb);
    }
    else {
        m_codec.mfm_write_byte(0xf8);     // 0xf8 (Deleted DAM) x 1
        m_crcgen.data(0xf8);
    }

    // write sector data
    for (auto it = write_data.begin(); it != write_data.end(); ++it) {
        uint8_t data = *it;
        m_codec.mfm_write_byte(data);
        m_crcgen.data(data);
    }

    // write CRC
    if (write_crc == true) {
        m_crcgen.data(0x00);
        m_crcgen.data(0x00);
        uint16_t crc_val = m_crcgen.get();
        m_codec.mfm_write_byte(crc_val >> 8);
        m_codec.mfm_write_byte(crc_val & 0xffu);

        m_codec.mfm_write_byte(0xff);           // write 0xff (or 0x4f)
    }
}




/**
* @brief Set current bit position in the track buffer.
*        The position is the bit position in the track buffer.
*        When the sampling rate of the track buffer is 4MHz, one bit means 250ns (1/4MHz).
*        When you set the position to 10 and the sampling rate is 4MHz, the new position will be 2.5us from the top of the track buffer. (1/4MHz * 10 = 2.5us)
*
* @param[in] bit_pos New position
*/
void fdc_bitstream::set_pos(size_t bit_pos) {
    m_codec.set_pos(bit_pos);
    clear_wraparound();
}

/**
* @brief Get current position in the track buffer.
* 
* @param[in] none
* @return size_t Current bit position in the track buffer.
*/
size_t fdc_bitstream::get_pos(void) {
    return m_codec.get_pos();
}


/**
* @brief Set real current bit position in the track buffer.
*        Difference from set_pos is this function also updates m_distance_to_next_pulse so that the real position
*        becomes bit_pos.
*
* @param[in] bit_pos New position
*/
void fdc_bitstream::set_real_pos(size_t bit_pos)
{
    m_codec.set_real_pos(bit_pos);
    clear_wraparound();
}


/**
* @brief Get current position minus distance to the next pulse
*        Current position is always at a pulse.
*        Real position is m_codec.m_distance_to_next_pulse before the current position.
* 
* @param[in] track_data New track data.
* @return none
*/
size_t fdc_bitstream::get_real_pos(void)
{
	return m_codec.get_real_pos();
}

/**
 * @brief Read 1 byte from track buffer (Interface to mfm_codec)
 * 
 * @param[out] data Read data of 1 byte. 
 * @param[out] missing_clock Missing clock flag (true=missing clock). 
 * @param ignore_missing_clock Flag to ignore missing clock pattern during decoding (true=ignore).
 * @param ignore_sync_field Flag to ignore SYNC pattern during decoding (true=ignore).
 * @return true: Result is valid.
 * @return false: Track bit array data is not set. Returned values are invalid.
*/
bool fdc_bitstream::read_byte(uint8_t& data, bool& missing_clock, double &error,bool ignore_missing_clock, bool ignore_sync_field)
{
	return m_codec.mfm_read_byte(data,missing_clock,error,ignore_missing_clock,ignore_sync_field);
}

/**
* @brief Set a new track data.
*        Set a new track data in a new bit_array.
* 
* @param[in] track_data New track data.
* @return none
*/
void fdc_bitstream::set_track_data(bit_array track_data) {
    // Inspect track data and determine initial cell size for VFO for easy synchronization
    //if(track_data.get_length>0) {
    //    std::vector<size_t> dist_freq = fdc_misc::get_frequent_distribution(track_data);
    //    std::vector<size_t> peaks = fdc_misc::find_peaks(dist_freq);
    //    m_codec.set_vfo_cell_size(peaks[0]/2.f);
    //}

    track_data.set_stream_pos(0);
    m_codec.set_track_data(track_data);
}

/**
* @brief Get the track data in bit_array.
*
* @return bit_array Track dta in bit_array.
*/
bit_array fdc_bitstream::get_track_data(void) {
    return m_codec.get_track_data();
}

/**
* @brief Write a byte data to the track buffer at the current position and advance the position for one byte.
*        This function writes a byte data to the track buffer. The data wil be written at the current position, and the pointer will advance for one byte.
*        This function also supports FD179x/MB8877 special codes. When the 'mode' is set to 'true', the special codes are recognized and handled respectively.
*        When the 'write_gate' is set to 'false', this function will not perform actual data writing but advance the pointer. You can use this feature for dummy data writing (or just skip certain bytes).
*
* @param[in] data A byte data to write.
* @param[in] mode Special code handling mode (true=special, false=normal)
* @param[in] write_gate Flag to decide whether the actual write is performed or not.
*/
void fdc_bitstream::write_data(uint8_t data, bool mode, bool write_gate) {
    m_codec.mfm_write_byte(data, mode, write_gate);
}

/**
* @brief Read a byte from the current position in the track buffer and advance the position for one byte.
*        When reading an ID field and a block of sector data body, ignore_missing_clock and ignore_sync_field must be set to 'true'. 
*        Otherwise, unexpected patten match to the missing-clock-pattern or sync-field-pattern may happen.
*        If this situation occurs, the data separator will perform forcible phase correction and this causes read data error.
*        Adversely, you must set ignore_missing_clock and ignore_sync_field to 'false' when you are searching for address markers (such as IDAM, A1* A1* A1* FE).
* 
* @param[out] data Read data
* @param[out] missing_clock Missing clock status (true=the read data had missing clock).
* @param[in] ignore_missing_clock Flag to ignore missing clock on reading a data (true=ignore missing clock pattern).
* @param[in] ignore_sync_field Flag to ignore SYNC field on reading a data (true=ignore SYNC field pattern).
* @return none
*/
void fdc_bitstream::read_data(uint8_t& data, bool& missing_clock, bool ignore_missing_clock, bool ignore_sync_field) {
    m_codec.mfm_read_byte(data, missing_clock, ignore_missing_clock, ignore_sync_field);
}


/**
* @brief FD189x/MB8876 compatible TYPE-II READ SECTOR operation
 * 
 * @param trk Track number.
 * @param sct Sector number.
 * @return fdc_bitstream::sector_data Read sector data.
 */
fdc_bitstream::sector_data fdc_bitstream::read_sector(int trk, int sct) {
    size_t index_hole_count = 0;
    sector_data sect_data;
    std::vector<uint8_t> sect_id;
    std::vector<uint8_t> sect_body_data;
    std::vector<size_t> sect_byte_pos;
    memset(&sect_data, 0, sizeof(sector_data));
    bool crc_error = false;
    bool dam_type = false;
    bool record_not_found = false;
    while (true) {
        if (is_wraparound()) {
            clear_wraparound();
            if (++index_hole_count >= 4) {                   // FD179x/MB8876 needs to find the desired sector ID within 4 spins.
                sect_data.record_not_found = true;
                return sect_data;
            }
        }
        size_t pos = read_id(sect_id, crc_error);
        sect_data.id_pos = pos;
        if(sect_id.size()==0) continue;                     // Failed to read ID
        if (sect_id[0] == trk && sect_id[2] == sct) {       // FD179x/MB8876 won'nt compare 'head' field
            if (crc_error == false) {                       // Skip reading sector body if ID has a CRC error
                pos = read_sector_body_ex(sect_id[3], sect_body_data, sect_byte_pos, crc_error, dam_type, record_not_found);
                sect_data.data_pos = pos;
                sect_data.data_end_pos = m_codec.get_real_pos();
                size_t distance;
                if (sect_data.data_pos >= sect_data.id_pos) {
                    distance = pos - sect_data.id_pos;
                }
                else {
                    distance = pos - sect_data.id_pos + get_track_length();     // Compensation for wrap around
                }
                if (distance > (43 + 9) * (m_sampling_rate / m_data_bit_rate) * 8 * 2) {       // DAM/DDAM must be found in 43 bytes (MFM, FDC179x/MB8876). +10 for IDAM field, *8 for 1 byte, *2 for (CLK+DATA).
                    sect_data.record_not_found = true;
                }
                else {  // Record not found
                    sect_data.data             = sect_body_data;
                    sect_data.pos              = sect_byte_pos;
                    sect_data.record_not_found = record_not_found;
                    sect_data.dam_type         = dam_type;
                    sect_data.crc_sts          = crc_error;
                    return sect_data;
                }
            }
        }
    }
}


/**
 * @brief FD189x/MB8876 compatible TYPE-II WRITE SECTOR operation
 * 
 * @param trk Track number.
 * @param sct Sector number.
 * @param dam_type Data address mark type (false=DAM, true=DDAM)
 * @param write_data Sector data.
 * @param fluctuate Flag to control write start timing (false=deterministic timing, true=write start timing will fluctuate)
 * @return true No error
 * @return false Record-not-found error 
 */
bool fdc_bitstream::write_sector(int trk, int sct, bool dam_type, std::vector<uint8_t> &write_data, bool fluctuate) {
    size_t index_hole_count = 0;
    std::vector<uint8_t> sect_id;
    bool crc_error = false;
    while (true) {
        if (is_wraparound()) {
            clear_wraparound();
            if (++index_hole_count >= 4) {              // FD179x/MB8876 needs to find the desired sector ID within 4 spins.
                return false;                           // record not found error
            }
        }
        read_id(sect_id, crc_error);
        if(sect_id.size() == 0) continue;
        if (sect_id[0] == trk && sect_id[2] == sct) {       // FD179x/MB8876 won'nt compare 'head' field
            if (crc_error == false) {
                if (fluctuate) {
                    // Fluctuate write start timing
                    int64_t pos = static_cast<int64_t>(get_pos());
                    pos += normal_distribution_random();   // +-3
                    if (pos < 0) pos = 0;
                    if (pos >= get_track_length()) pos = static_cast<int64_t>(get_track_length()) - 1;
                    set_pos(static_cast<size_t>(pos));
                }
                write_sector_body(write_data, dam_type);
                return true;
            }
        }
    }
}

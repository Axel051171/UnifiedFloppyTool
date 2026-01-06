/**
 * @file fdc_misc.cpp
 * @author your name (you@domain.com)
 * @brief Miscellaneous functions for FDC lib (format data generator)
 * @version 0.1
 * @date 2022-08-12
 * 
 * @copyright Copyright (c) 2022
 * 
 * @note FIXED R18: All #includes MUST be outside namespace blocks to avoid
 *       polluting the namespace with std:: symbols (causes "fdc_misc::std" errors)
 */

#include <algorithm>

// Platform-specific includes - MUST be before namespace block
#ifdef _WIN32
    #include <windows.h>
#else
    #include <iostream>
    #include <sstream>
#endif

#include "fdc_misc.h"

namespace fdc_misc {

#ifdef _WIN32
void color(size_t col) {
    //if(col>7) col=7;
    size_t flags = FOREGROUND_INTENSITY;
    if (col & 0b10000) {
        flags = 0;
        if (col & 0b0001) flags |= BACKGROUND_INTENSITY | BACKGROUND_BLUE;
        if (col & 0b0010) flags |= BACKGROUND_INTENSITY | BACKGROUND_RED;
        if (col & 0b0100) flags |= BACKGROUND_INTENSITY | BACKGROUND_GREEN;
    } else {
        if (col & 0b0001) flags |= FOREGROUND_INTENSITY | FOREGROUND_BLUE;
        if (col & 0b0010) flags |= FOREGROUND_INTENSITY | FOREGROUND_RED;
        if (col & 0b0100) flags |= FOREGROUND_INTENSITY | FOREGROUND_GREEN;
    }
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(console_handle, flags);
}
#else
void color(size_t col) {
    const size_t col_tbl[8] { 30, 34, 31, 35, 32, 36, 33, 37 };
    if(col>7) col=7;
    std::stringstream ss;
    ss << "\e[" << std::dec << col_tbl[col] << "m";
    std::cout << ss.str();
}
#endif

/**
 * @brief Generate a list of interleaved sector numbers.
 * 
 * @param num_sector Number of sectors.
 * @param interleave Interleave distance to the next sector. Default=1.
 * @return std::vector<int> Generated sector number.
 */
std::vector<int> generate_interleaved_sector_list(size_t num_sector, int interleave = 1) {
    std::vector<int> sector_list(num_sector, 0);
    int pos = 0;
    for (int i = 1; i <= num_sector; i++) {
        while (sector_list[pos] != 0) {
            if (++pos >= num_sector) pos = 0;
        }
        sector_list[pos] = i;
        pos += interleave;
        if (pos >= num_sector) pos = 0;
    }
    return sector_list;
}

/**
* @brief Format data generator. Generates format data from the given parameters.
* 
* @param[in] track_n Track number.
* @param[in] side_n Side number.
* @param[in] num_sector Number of sectors in the track (starting from 1).
* @param[in] sect_len_code Sector length code (0-3 -> 128/256/512/1024).
* @param[in] interleave Interleave parameter. Meaning the distance to the next sector. 1=no interleave. 1>interleave.
* @param[in] format_type 0:IBM format, 1:ECMA/ISO format (default).
* @return std::vector<uint8_t> Generated format data.
*/
std::vector<uint8_t> generate_format_data(size_t track_n, size_t side_n, size_t num_sector, size_t sect_len_code, size_t interleave, size_t format_type) {
    std::vector<int> sector_list = generate_interleaved_sector_list(num_sector, interleave);
    std::vector<uint8_t> format_data;
    std::vector<size_t> sector_length_table{ 128, 256, 512, 1024 };
    switch (format_type) {
    case 0: { /** IBM format */
        std::vector<size_t> gap3_table{ 54,54,84,116 };
        std::vector<size_t> gap4b_table{ 152,152,182,94 };

        for (size_t i = 0; i < 80; i++) format_data.push_back(0x4e);     //Gap4a
        for (size_t i = 0; i < 12; i++) format_data.push_back(0x00);     //SYNC
        for (size_t i = 0; i < 3; i++) format_data.push_back(0xf6);      // IAM, f6->c2
        format_data.push_back(0xfc);
        for (size_t i = 0; i < 50; i++) format_data.push_back(0x4e);     //Gap1

        for (size_t n = 0; n < num_sector; n++) {
            for (size_t i = 0; i < 12; i++) format_data.push_back(0x00); //SYNC
            for (size_t i = 0; i < 3; i++) format_data.push_back(0xf5);  //IDAM f5->a1
            format_data.push_back(0xfe);
            format_data.push_back(track_n);                 //C
            format_data.push_back(side_n);                  //H
            format_data.push_back(sector_list[n]);          //R
            format_data.push_back(sect_len_code & 0x03u);   //N
            format_data.push_back(0xf7);                    //CRC f7->crc
            for (size_t i = 0; i < 22; i++) format_data.push_back(0x4e);        //Gap2

            for (size_t i = 0; i < 12; i++) format_data.push_back(0x00);        //SYNC
            for (size_t i = 0; i < 3; i++) format_data.push_back(0xf5);  //DAM f5->a1
            format_data.push_back(0xfb);
            for (size_t i = 0; i < sector_length_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x00);  //Sector data
            format_data.push_back(0xf7);                    //CRC f7->crc
            for (size_t i = 0; i < gap3_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x4e);        //Gap3
        }
        for (size_t i = 0; i < gap4b_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x4e);        //Gap4b
        }
        break;
    case 1: { /** ECMA/ISO format */
        std::vector<size_t> gap3_table{ 54,54,84,116 };
        std::vector<size_t> gap4_table{ 226,226,296,208 };

        for (size_t i = 0; i < 32; i++) format_data.push_back(0x4e);     //Gap1

        for (size_t n = 0; n < num_sector; n++) {
            for (size_t i = 0; i < 12; i++) format_data.push_back(0x00); //SYNC
            for (size_t i = 0; i < 3; i++) format_data.push_back(0xf5);  //IDAM f5->a1
            format_data.push_back(0xfe);
            format_data.push_back(track_n);                 //C
            format_data.push_back(side_n);                  //H
            format_data.push_back(sector_list[n]);          //R
            format_data.push_back(sect_len_code & 0x03u);   //N
            format_data.push_back(0xf7);                    //CRC f7->crc
            for (size_t i = 0; i < 22; i++) format_data.push_back(0x4e);        //Gap2

            for (size_t i = 0; i < 12; i++) format_data.push_back(0x00);        //SYNC
            for (size_t i = 0; i < 3; i++) format_data.push_back(0xf5);  //DAM f5->a1
            format_data.push_back(0xfb);
            for (size_t i = 0; i < sector_length_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x00);  //Sector data
            format_data.push_back(0xf7);                    //CRC f7->crc
            for (size_t i = 0; i < gap3_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x4e);        //Gap3
        }
        for (size_t i = 0; i < gap4_table[sect_len_code & 0x03u]; i++) format_data.push_back(0x4e);        //Gap4
        break;
        }
    default:
        /** Wrong type */
        break;
    }
    return format_data;
}

// ------------------------------------------------------------------------------------------

void display_histogram(const std::vector<size_t> &dist_array, bool color_flag) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    size_t max_count = *std::max_element(dist_array.begin(), dist_array.end());
    double scale = 100.f / static_cast<double>(max_count);
    size_t max_item = 0;
    for(size_t i=0; i<dist_array.size(); i++) {
        if (dist_array[i] != 0) max_item = i;
    }

    // display histogram
    for(size_t i=0; i<=max_item; i++) {
        std::cout << std::setw(4) << std::setfill(' ') << i << " : ";
        std::cout << std::setw(8) << std::setfill(' ') << dist_array[i] << " : ";
        size_t bar_length = static_cast<size_t>(static_cast<double>(dist_array[i]) * scale); // normalize
        if (color_flag) color(4);
        std::cout << std::string(bar_length, '*') << std::endl;
        if (color_flag) color(7);
    }
    std::cout.flags(flags_saved);
}

std::vector<size_t> get_frequent_distribution(bit_array barray) {
    barray.clear_wraparound_flag();
    std::vector<size_t> freq_dist;
    do {
        size_t dist = barray.distance_to_next_pulse();
        if(freq_dist.size() <= dist) {
            freq_dist.resize(dist+1);
        }
        freq_dist[dist]++;
    } while(!barray.is_wraparound());
    return freq_dist;
}

std::vector<size_t> find_peaks(const std::vector<size_t> &dist_freq) {
    // LPF
    std::vector<size_t> avg(dist_freq.size());
    const int filt[5] { 1,2,3,2,1 };
    const int flt_sum = 1+2+3+2+1;
    for(int i = 0; i<dist_freq.size(); i++) {
        size_t avg_tmp = 0;
        for(int j=-2; j<=2; j++) {
            size_t dt=0;
            if (i+j < 0) {
                dt = 0;
            } else if(i+j >= dist_freq.size()) {
                dt = 0;
            } else {
                dt = dist_freq[i+j];
            }
            avg_tmp += dt * filt[j+2];
        }
        avg[i] = avg_tmp / flt_sum;
    }
    //fdc_misc::display_histogram(avg);       // for debug purpose

    // detect peaks
    std::vector<std::pair<size_t, size_t>> peaks;
    bool flat_peak = false;
    size_t lower_edge = 0;
    for(size_t i=1; i<avg.size()-1; i++) {
        if(avg[i-1] < avg[i] && avg[i+1] < avg[i]) {    // a sharp peak
            peaks.push_back(std::pair<size_t, size_t>(i, avg[i]));
            flat_peak = false;
        } else if(avg[i-1] < avg[i] && avg[i+1] == avg[i])  { // lower edge of a possible flat peak
            flat_peak = true;
            lower_edge = i;
        } else if(avg[i-1] == avg[i] && avg[i+1] < avg[i] && flat_peak) {   // upper edge of a flat peak
            peaks.push_back(std::pair<size_t, size_t>((i+lower_edge)/2, avg[i]));
            flat_peak = false;
        } else if(avg[i-1] == avg[i] && avg[i+1] == avg[i] && flat_peak) {       // top of flat peak
            // nop
        } else {        // neither sharp peak, top of flat peak, nor edge of flat peak.
            flat_peak = false;
        }
    }
    if(flat_peak==true) {
        size_t tmp = (avg.size()-1 + lower_edge) / 2;
        peaks.push_back(std::pair<size_t, size_t>(tmp, avg[tmp]));
    }

    // sort peaks by value (frequency)
    std::sort(peaks.begin(), peaks.end(), [](const std::pair<size_t, size_t> &left, const std::pair<size_t, size_t> &right) { return left.second > right.second; });

    if (peaks.size()<3) {
        for(size_t i=0; i<3-peaks.size(); i++) {
            peaks.push_back(std::pair<size_t, size_t>(9999,1));     // stuff dummy
        }
    }
    std::vector<size_t> result;
    for(auto it=peaks.begin(); it != peaks.begin()+3; ++it) {
        result.push_back((*it).first);
    }

    // sort peaks by index
    std::sort(result.begin(), result.end(), [](const size_t &left, const size_t &right) { return left < right; });

    return result;
}

std::vector<size_t> convert_to_dist_array(bit_array track) {
    track.clear_wraparound_flag();
    std::vector<size_t> dist_array;
    do {
        size_t dist = track.distance_to_next_pulse();
        dist_array.push_back(dist);
    } while(!track.is_wraparound());
    return dist_array;
}


void dump_buf(uint8_t* ptr, size_t size, bool line_feed/*=true*/, size_t cols/*=64*/, size_t rows/*=32*/, bool disp_ofst/*=false*/, uint8_t *marker/*=nullptr*/, bool ascii_dump/*=false*/) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    size_t row_count = 0;
    std::string ascii_buf;

    for (size_t i = 0; i < size; i++) {
        if (i % cols == 0) {
            ascii_buf = std::string(cols, '.');
            if (disp_ofst) {
                if (row_count % rows == 0) {
                    std::cout << std::endl << "-OFST- : ";
                    for(size_t j=0; j<cols; j++) {
                        std::cout << std::hex << std::setw(2) << std::setfill('0') << j << " ";
                    }
                    std::cout << std::endl << std::string(9 + 3*cols, '-') << std::endl;;
                }
                std::cout << std::hex << std::setw(6) << std::setfill('0') << i << " : ";
            }
        }
        if (marker != nullptr) if(marker[i] == 1) color(4);
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ptr[i]) << " ";
        color(7);
        // ascii buffer
        ascii_buf[i % cols] = (ptr[i] >= 0x20 && ptr[i] <= 0x7e) ? ptr[i] : '.';
        if (i % cols == cols-1) {
            if(ascii_dump) {
                std::cout << ascii_buf;
            }
            std::cout << std::endl;
            row_count++;
        }
    }
    if (line_feed) std::cout << std::endl;
    std::cout.flags(flags_saved);
}

// Specialized dump function for 'tr' command in analyzer
void dump_buf2(const std::vector<std::vector<size_t>> &data) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    size_t row_count = 0;
    constexpr size_t rows = 16;
    constexpr size_t cols = 32;
    for (size_t i=0; i<data.size(); i++) {
        size_t dt = data[i][0];
        size_t mc = data[i][1];
        size_t pos = data[i][2];
        size_t err = data[i][3];
        // display ruler
        if (i % cols == 0) {
            if (row_count % rows == 0) {
                std::cout << std::endl << "-OFST- : -BitPos- : ";
                for(size_t j=0; j<cols; j++) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << j << " ";
                }
                std::cout << std::endl << std::string(19 + 3*cols, '-') << std::endl;;
            }
            std::cout << std::hex << std::setw(6) << std::setfill('0') << i << " : ";   // ofst adrs
            std::cout << std::dec << std::setw(8) << std::setfill(' ') << pos << " : "; // bit pos
        }
#if 1
        // Color bit error
        if(err > 16000) color(6);
        if(err > 32000) color(3);
        if(err > 64000) color(2);
#endif
        // Color AMs
        if(mc != 0) color(4 + 0x10);

        std::cout << std::hex << std::setw(2) << std::setfill('0') << dt << " ";
        color(7);
        if (i % cols == cols-1) {
            std::cout << std::endl;
            row_count++;
        }
    }
    std::cout << std::endl;
    std::cout.flags(flags_saved);
}

void bit_dump(const uint64_t data, size_t bit_width, size_t spacing /*= 0*/, bool line_feed /*= true*/) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    size_t count = 0;
    for (uint64_t bit_pos = 1 << (bit_width - 1); bit_pos != 0; bit_pos >>= 1) {
        std::cout << ((data & bit_pos) ? 1 : 0);
        count++;
        if (spacing > 0) {
            if (count % spacing == 0) {
                std::cout << " ";
            }
        }
    }
    if (line_feed == true) {
        std::cout << std::endl;
    }
    std::cout.flags(flags_saved);
}

void bit_dump(bit_array &data, size_t bit_width /*= 0*/, size_t spacing /*= 0*/, bool line_feed /*=true*/) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    size_t count = 0;
    size_t length = (bit_width == 0) ? data.get_bit_length() : bit_width;
    for (uint64_t i = 0; i < length; length++) {
        std::cout << (data.get(i) ? 1 : 0);
        count++;
        if (spacing > 0) {
            if (count % spacing == 0) {
                std::cout << " ";
            }
        }
    }
    if (line_feed == true) {
        std::cout << std::endl;
    }
    std::cout.flags(flags_saved);
}


void display_sector_data(const fdc_bitstream::sector_data &sect_data, bool color_flag) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    std::cout << std::dec << std::setw(4) << std::setfill(' ');
    if(color_flag && sect_data.data.size()==0) {
        color(2);
    }
    std::cout << sect_data.data.size() << " ";
    color(7);
    if(sect_data.dam_type) {
        if(color_flag) color(3);
        std::cout << "DDAM ";
        color(7);
    } else {
        std::cout << "DAM  ";
    }
    if(sect_data.crc_sts) {
        if(color_flag) color(2);
        std::cout << "DT-CRC_ERR ";
        color(7);
     } else {
        std::cout << "DT-CRC OK  ";
     }
    if(sect_data.record_not_found) { 
        if(color_flag) color(2);
        std::cout << "RNF_ERR ";
        color(7);
    } else {
        std::cout << "RNF_OK  ";
    }
    std::cout << std::dec << std::setw(8);
    std::cout << "IDAM_POS=" << std::setw(8) << sect_data.id_pos << " ";
    std::cout << "DAM_POS="  << std::setw(8) << sect_data.data_pos << " ";
    std::cout.flags(flags_saved);
}

void display_id(const fdc_bitstream::id_field &id, bool color_flag) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(id.C) << " ";
    std::cout << std::setw(2) << static_cast<int>(id.H) << " ";
    std::cout << std::setw(2) << static_cast<int>(id.R) << " ";
    std::cout << std::setw(2) << static_cast<int>(id.N) << " ";
    std::cout << std::setw(4) << static_cast<int>(id.crc_val) << " ";
    if (id.crc_sts) { 
        if (color_flag) color(2);
        std::cout << "ID-CRC_ERR ";
        color(7);
    } else {
        std::cout << "ID-CRC_OK  ";
    }
    std::cout.flags(flags_saved);
}

void display_id_list(const std::vector<fdc_bitstream::id_field> &id_fields, bool color_flag) {
    std::ios::fmtflags flags_saved = std::cout.flags();
    std::cout << std::hex << std::setw(2) << std::setfill('0');
    for (int i = 0; i < id_fields.size(); i++) {
        std::cout << std::dec << std::setw(2) << std::setfill(' ') << i+1 << " ";
        display_id(id_fields[i], color_flag);
        std::cout << std::endl;
    }
    std::cout.flags(flags_saved);
}

size_t str2val(const std::string &hexstr) {
    size_t res = 0;
    if (hexstr.size()==0) return 0;
    if (hexstr[0] == '$') {
        for(auto it = hexstr.begin(); it != hexstr.end(); ++it) {
            if (*it >='0' && *it <= '9') {
                res = (res<<4) | ((*it) - '0');
            } else if (*it>='a' && *it <= 'f') {
                res = (res<<4) | ((*it) - 'a' + 10);
            }
        } 
    } 
    else if (hexstr[0] >= '0' && hexstr[0] <= '9') {
        res = std::stoi(hexstr);
    }
    //std::cout << hexstr << " " << res << std::endl;
    return res;
}

} // namespace fdc_misc

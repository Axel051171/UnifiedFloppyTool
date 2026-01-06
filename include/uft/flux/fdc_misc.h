#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>

#include "fdc_bitstream.h"
#include "bit_array.h"

namespace fdc_misc {

void color(size_t col);

std::vector<int> generate_interleaved_sector_list(int num_sector, int interleave = 1);
std::vector<uint8_t> generate_format_data(size_t track_n, size_t side_n, size_t num_sector, size_t sect_len_code, size_t interleave = 1, size_t format_type = 1);

void display_histogram(const std::vector<size_t> &dist_array, bool color_flag=false);
std::vector<size_t> get_frequent_distribution(bit_array barray);
std::vector<size_t> find_peaks(const std::vector<size_t> &vec);
std::vector<size_t> convert_to_dist_array(bit_array track);

void dump_buf(uint8_t* ptr, size_t size, bool line_feed=true, size_t cols=64, size_t rows=32, bool disp_ofst=false, uint8_t *marker=nullptr, bool ascii_dump=false);
void dump_buf2(const std::vector<std::vector<size_t>> &data);

void bit_dump(const uint64_t data, size_t bit_width, size_t spacing = 0, bool line_feed = true);
void bit_dump(bit_array &data, size_t bit_width = 0, size_t spacing = 0, bool line_feed = true);
void display_sector_data(const fdc_bitstream::sector_data &sect_data, bool color_flag=false);
void display_id(const fdc_bitstream::id_field &id, bool color_flag=false);
void display_id_list(const std::vector<fdc_bitstream::id_field> &id_fields, bool color_flag=false);
size_t str2val(const std::string &hexstr);

} // namespace fdc_misc

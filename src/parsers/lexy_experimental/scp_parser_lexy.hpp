/**
 * @file scp_parser_lexy.hpp
 * @brief SCP Format Parser using lexy
 * 
 * EXPERIMENTAL - Proof of Concept for lexy integration
 * 
 * Requires: C++17, lexy library
 */

#ifndef UFT_SCP_PARSER_LEXY_HPP
#define UFT_SCP_PARSER_LEXY_HPP

#ifdef UFT_USE_LEXY

#include <lexy/dsl.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/input/buffer.hpp>
#include <optional>
#include <cstdint>

namespace uft::scp::lexy_parser {

namespace dsl = lexy::dsl;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct SCPHeader {
    uint8_t version;
    uint8_t disk_type;
    uint8_t revolutions;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint8_t bit_cell_encoding;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
    
    // Derived properties
    int track_count() const { return end_track - start_track + 1; }
    bool is_single_sided() const { return heads == 0; }
    bool has_index_marks() const { return flags & 0x01; }
    bool is_96_tpi() const { return flags & 0x02; }
    bool has_360_rpm() const { return flags & 0x04; }
    bool is_normalized() const { return flags & 0x08; }
    bool is_read_write() const { return flags & 0x10; }
    bool has_footer() const { return flags & 0x20; }
};

struct SCPTrackHeader {
    uint32_t track_offset;
    // Track data follows at offset
};

struct SCPRevolution {
    uint32_t index_time;
    uint32_t track_length;
    uint32_t data_offset;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lexy Grammar Rules
 * ═══════════════════════════════════════════════════════════════════════════════ */

// SCP Magic bytes: "SCP"
struct scp_magic {
    static constexpr auto rule = dsl::lit_b<'S', 'C', 'P'>;
    static constexpr auto value = lexy::noop;
};

// Main header parser
struct scp_header {
    static constexpr auto rule = [] {
        // Magic "SCP"
        auto magic = dsl::p<scp_magic>;
        
        // Single byte fields
        auto version = dsl::integer<uint8_t>;
        auto disk_type = dsl::integer<uint8_t>;
        auto revolutions = dsl::integer<uint8_t>;
        auto start_track = dsl::integer<uint8_t>;
        auto end_track = dsl::integer<uint8_t>;
        auto flags = dsl::integer<uint8_t>;
        auto encoding = dsl::integer<uint8_t>;
        auto heads = dsl::integer<uint8_t>;
        auto resolution = dsl::integer<uint8_t>;
        
        // 32-bit checksum (little endian)
        auto checksum = dsl::little_endian<dsl::integer<uint32_t>>;
        
        return magic >> 
               version + disk_type + revolutions +
               start_track + end_track + flags +
               encoding + heads + resolution + checksum;
    }();
    
    static constexpr auto value = lexy::callback<SCPHeader>(
        [](uint8_t ver, uint8_t dtype, uint8_t revs,
           uint8_t start, uint8_t end, uint8_t flags,
           uint8_t enc, uint8_t heads, uint8_t res,
           uint32_t csum) {
            return SCPHeader{
                ver, dtype, revs, start, end, flags,
                enc, heads, res, csum
            };
        }
    );
};

// Track offset entry (in header)
struct track_offset {
    static constexpr auto rule = 
        dsl::little_endian<dsl::integer<uint32_t>>;
    
    static constexpr auto value = lexy::construct<SCPTrackHeader>;
};

// Revolution header
struct revolution_header {
    static constexpr auto rule = [] {
        auto index_time = dsl::little_endian<dsl::integer<uint32_t>>;
        auto length = dsl::little_endian<dsl::integer<uint32_t>>;
        auto offset = dsl::little_endian<dsl::integer<uint32_t>>;
        
        return index_time + length + offset;
    }();
    
    static constexpr auto value = lexy::callback<SCPRevolution>(
        [](uint32_t idx, uint32_t len, uint32_t off) {
            return SCPRevolution{idx, len, off};
        }
    );
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parser API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse SCP header from raw bytes
 * @param data Raw byte data
 * @param len Data length (must be >= 16)
 * @return Parsed header or nullopt on error
 */
inline std::optional<SCPHeader> parse_header(const uint8_t* data, size_t len) {
    if (len < 16) return std::nullopt;
    
    auto input = lexy::buffer<lexy::byte_encoding>(
        reinterpret_cast<const char*>(data), len);
    
    auto result = lexy::parse<scp_header>(input, lexy::noop);
    
    if (result.has_value()) {
        return result.value();
    }
    return std::nullopt;
}

/**
 * @brief Validate SCP magic bytes
 * @param data Raw byte data  
 * @param len Data length (must be >= 3)
 * @return true if magic is valid
 */
inline bool validate_magic(const uint8_t* data, size_t len) {
    if (len < 3) return false;
    return data[0] == 'S' && data[1] == 'C' && data[2] == 'P';
}

} // namespace uft::scp::lexy_parser

#endif // UFT_USE_LEXY

#endif // UFT_SCP_PARSER_LEXY_HPP

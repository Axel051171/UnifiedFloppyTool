/**
 * @file scp_parser_complete.hpp
 * @brief Complete SCP Format Parser using lexy
 * 
 * P1: Full SCP format support including:
 * - Header parsing
 * - Track headers
 * - Revolution data
 * - Flux timing extraction
 * 
 * Requires: C++17, lexy library
 */

#ifndef UFT_SCP_PARSER_COMPLETE_HPP
#define UFT_SCP_PARSER_COMPLETE_HPP

#ifdef UFT_USE_LEXY

#include <lexy/dsl.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/scan.hpp>
#include <lexy/callback.hpp>
#include <lexy/input/buffer.hpp>
#include <optional>
#include <vector>
#include <cstdint>
#include <string>

namespace uft::scp {

namespace dsl = lexy::dsl;

/* ═══════════════════════════════════════════════════════════════════════════════
 * SCP Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

constexpr uint8_t SCP_MAGIC[] = {'S', 'C', 'P'};

// Disk Types
enum class DiskType : uint8_t {
    C64 = 0x00,
    Amiga = 0x04,
    AtariFMSS = 0x10,
    AtariFMDS = 0x11,
    AtariMFMSS = 0x12,
    AtariMFMDS = 0x13,
    AppleII = 0x20,
    AppleIIPro = 0x21,
    Apple400K = 0x22,
    Apple800K = 0x23,
    Apple144 = 0x24,
    PC360K = 0x30,
    PC720K = 0x31,
    PC12M = 0x32,
    PC144M = 0x33,
    TandyTRS80SSSD = 0x40,
    TandyTRS80SSDD = 0x41,
    TandyTRS80DSSD = 0x42,
    TandyTRS80DSDD = 0x43,
    TI994A = 0x50,
    RolandD20 = 0x60,
    AmstradCPC = 0x70,
    Other = 0x80,
    TapeDrive = 0xE0,
    HardDrive = 0xF0
};

// Flags
struct SCPFlags {
    bool hasIndex : 1;      // Bit 0: Index mark stored
    bool is96tpi : 1;       // Bit 1: 96 TPI
    bool is360rpm : 1;      // Bit 2: 360 RPM
    bool isNormalized : 1;  // Bit 3: Normalized flux
    bool isReadWrite : 1;   // Bit 4: Read/Write capable
    bool hasFooter : 1;     // Bit 5: Footer present
    bool hasExtFooter : 1;  // Bit 6: Extended footer
    bool reserved : 1;      // Bit 7: Reserved
    
    static SCPFlags fromByte(uint8_t b) {
        SCPFlags f;
        f.hasIndex = b & 0x01;
        f.is96tpi = b & 0x02;
        f.is360rpm = b & 0x04;
        f.isNormalized = b & 0x08;
        f.isReadWrite = b & 0x10;
        f.hasFooter = b & 0x20;
        f.hasExtFooter = b & 0x40;
        f.reserved = b & 0x80;
        return f;
    }
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct SCPHeader {
    uint8_t version;
    DiskType diskType;
    uint8_t revolutions;
    uint8_t startTrack;
    uint8_t endTrack;
    SCPFlags flags;
    uint8_t bitCellEncoding;
    uint8_t heads;
    uint8_t resolution;
    uint32_t checksum;
    
    int trackCount() const { return endTrack - startTrack + 1; }
    bool isSingleSided() const { return heads == 0; }
    double resolutionNs() const { return (resolution + 1) * 25.0; }
};

struct SCPRevolution {
    uint32_t indexTime;     // Time for index in 25ns/40MHz units
    uint32_t trackLength;   // Number of flux transitions
    uint32_t dataOffset;    // Offset to flux data from track start
    
    double indexTimeUs() const { return indexTime * 0.025; }
    double indexTimeMs() const { return indexTime * 0.000025; }
};

struct SCPTrackHeader {
    char magic[3];          // "TRK"
    uint8_t trackNumber;
    std::vector<SCPRevolution> revolutions;
};

struct SCPTrack {
    uint8_t trackNumber;
    uint8_t side;
    std::vector<SCPRevolution> revolutions;
    std::vector<std::vector<uint16_t>> fluxData;  // Per revolution
    
    // Calculated properties
    double averageRpm() const {
        if (revolutions.empty()) return 0;
        double totalTime = 0;
        for (const auto& rev : revolutions) {
            totalTime += rev.indexTimeUs();
        }
        return 60000000.0 / (totalTime / revolutions.size());
    }
};

struct SCPFile {
    SCPHeader header;
    std::vector<uint32_t> trackOffsets;  // Offset table
    std::vector<SCPTrack> tracks;
    
    // Footer (optional)
    uint32_t manufacturerOffset;
    uint32_t modelOffset;
    uint32_t serialOffset;
    uint32_t creatorOffset;
    uint32_t applicationOffset;
    uint32_t comments;
    uint64_t timestamp;
    
    bool isValid() const { return header.trackCount() > 0; }
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lexy Grammar Rules
 * ═══════════════════════════════════════════════════════════════════════════════ */

// SCP Magic "SCP"
struct scp_magic_rule {
    static constexpr auto rule = dsl::lit_b<'S', 'C', 'P'>;
    static constexpr auto value = lexy::noop;
};

// Little-endian 32-bit integer
struct le_uint32 {
    static constexpr auto rule = dsl::little_endian<dsl::integer<uint32_t>>;
    static constexpr auto value = lexy::forward<uint32_t>;
};

// Little-endian 16-bit integer
struct le_uint16 {
    static constexpr auto rule = dsl::little_endian<dsl::integer<uint16_t>>;
    static constexpr auto value = lexy::forward<uint16_t>;
};

// Revolution header (12 bytes)
struct revolution_rule {
    static constexpr auto rule = [] {
        auto index_time = dsl::p<le_uint32>;
        auto track_length = dsl::p<le_uint32>;
        auto data_offset = dsl::p<le_uint32>;
        return index_time + track_length + data_offset;
    }();
    
    static constexpr auto value = lexy::callback<SCPRevolution>(
        [](uint32_t idx, uint32_t len, uint32_t off) {
            return SCPRevolution{idx, len, off};
        }
    );
};

// Main header parser (16 bytes after magic)
struct header_rule {
    static constexpr auto rule = [] {
        auto magic = dsl::p<scp_magic_rule>;
        auto version = dsl::integer<uint8_t>;
        auto disk_type = dsl::integer<uint8_t>;
        auto revolutions = dsl::integer<uint8_t>;
        auto start_track = dsl::integer<uint8_t>;
        auto end_track = dsl::integer<uint8_t>;
        auto flags = dsl::integer<uint8_t>;
        auto encoding = dsl::integer<uint8_t>;
        auto heads = dsl::integer<uint8_t>;
        auto resolution = dsl::integer<uint8_t>;
        auto checksum = dsl::p<le_uint32>;
        
        return magic >> version + disk_type + revolutions +
               start_track + end_track + flags +
               encoding + heads + resolution + checksum;
    }();
    
    static constexpr auto value = lexy::callback<SCPHeader>(
        [](uint8_t ver, uint8_t dtype, uint8_t revs,
           uint8_t start, uint8_t end, uint8_t flags,
           uint8_t enc, uint8_t heads, uint8_t res,
           uint32_t csum) {
            SCPHeader h;
            h.version = ver;
            h.diskType = static_cast<DiskType>(dtype);
            h.revolutions = revs;
            h.startTrack = start;
            h.endTrack = end;
            h.flags = SCPFlags::fromByte(flags);
            h.bitCellEncoding = enc;
            h.heads = heads;
            h.resolution = res;
            h.checksum = csum;
            return h;
        }
    );
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parser Class
 * ═══════════════════════════════════════════════════════════════════════════════ */

class SCPParser {
public:
    /**
     * @brief Parse SCP header only
     */
    static std::optional<SCPHeader> parseHeader(const uint8_t* data, size_t len) {
        if (len < 16) return std::nullopt;
        
        auto input = lexy::buffer<lexy::byte_encoding>(
            reinterpret_cast<const char*>(data), len);
        
        auto result = lexy::parse<header_rule>(input, lexy::noop);
        
        if (result.has_value()) {
            return result.value();
        }
        return std::nullopt;
    }
    
    /**
     * @brief Parse complete SCP file
     */
    static std::optional<SCPFile> parseFile(const uint8_t* data, size_t len) {
        // Parse header first
        auto headerOpt = parseHeader(data, len);
        if (!headerOpt) return std::nullopt;
        
        SCPFile file;
        file.header = *headerOpt;
        
        // Parse track offset table (starts at offset 16)
        int numTracks = file.header.trackCount() * (file.header.heads + 1);
        size_t offsetTableStart = 16;
        size_t offsetTableSize = numTracks * 4;
        
        if (len < offsetTableStart + offsetTableSize) {
            return std::nullopt;
        }
        
        // Read track offsets
        file.trackOffsets.reserve(numTracks);
        for (int i = 0; i < numTracks; i++) {
            size_t pos = offsetTableStart + i * 4;
            uint32_t offset = data[pos] | (data[pos+1] << 8) | 
                             (data[pos+2] << 16) | (data[pos+3] << 24);
            file.trackOffsets.push_back(offset);
        }
        
        // Parse each track
        for (int i = 0; i < numTracks; i++) {
            uint32_t trackOffset = file.trackOffsets[i];
            if (trackOffset == 0) continue;  // Track not present
            
            if (trackOffset >= len) continue;
            
            auto track = parseTrack(data + trackOffset, len - trackOffset,
                                   file.header.revolutions);
            if (track) {
                track->trackNumber = file.header.startTrack + (i / (file.header.heads + 1));
                track->side = i % (file.header.heads + 1);
                file.tracks.push_back(*track);
            }
        }
        
        return file;
    }
    
    /**
     * @brief Parse single track
     */
    static std::optional<SCPTrack> parseTrack(const uint8_t* data, size_t len,
                                               uint8_t numRevolutions) {
        // Check TRK magic
        if (len < 4 || data[0] != 'T' || data[1] != 'R' || data[2] != 'K') {
            return std::nullopt;
        }
        
        SCPTrack track;
        track.trackNumber = data[3];
        
        // Parse revolution headers (12 bytes each, starting at offset 4)
        size_t revOffset = 4;
        for (int r = 0; r < numRevolutions; r++) {
            if (revOffset + 12 > len) break;
            
            SCPRevolution rev;
            rev.indexTime = data[revOffset] | (data[revOffset+1] << 8) |
                           (data[revOffset+2] << 16) | (data[revOffset+3] << 24);
            rev.trackLength = data[revOffset+4] | (data[revOffset+5] << 8) |
                             (data[revOffset+6] << 16) | (data[revOffset+7] << 24);
            rev.dataOffset = data[revOffset+8] | (data[revOffset+9] << 8) |
                            (data[revOffset+10] << 16) | (data[revOffset+11] << 24);
            
            track.revolutions.push_back(rev);
            revOffset += 12;
        }
        
        // Parse flux data for each revolution
        for (const auto& rev : track.revolutions) {
            std::vector<uint16_t> fluxes;
            fluxes.reserve(rev.trackLength);
            
            size_t fluxOffset = rev.dataOffset;
            for (uint32_t f = 0; f < rev.trackLength && fluxOffset + 1 < len; f++) {
                uint16_t flux = (data[fluxOffset] << 8) | data[fluxOffset + 1];
                fluxes.push_back(flux);
                fluxOffset += 2;
            }
            
            track.fluxData.push_back(std::move(fluxes));
        }
        
        return track;
    }
    
    /**
     * @brief Validate SCP magic
     */
    static bool validateMagic(const uint8_t* data, size_t len) {
        return len >= 3 && data[0] == 'S' && data[1] == 'C' && data[2] == 'P';
    }
    
    /**
     * @brief Calculate SCP checksum
     */
    static uint32_t calculateChecksum(const uint8_t* data, size_t len) {
        if (len < 16) return 0;
        
        uint32_t sum = 0;
        // Checksum is from offset 16 to end
        for (size_t i = 16; i < len; i++) {
            sum += data[i];
        }
        return sum;
    }
    
    /**
     * @brief Verify checksum
     */
    static bool verifyChecksum(const uint8_t* data, size_t len) {
        auto header = parseHeader(data, len);
        if (!header) return false;
        
        uint32_t calculated = calculateChecksum(data, len);
        return calculated == header->checksum;
    }
    
    /**
     * @brief Get disk type name
     */
    static const char* diskTypeName(DiskType type) {
        switch (type) {
            case DiskType::C64: return "Commodore 64";
            case DiskType::Amiga: return "Amiga";
            case DiskType::AtariFMSS: return "Atari FM SS";
            case DiskType::AtariFMDS: return "Atari FM DS";
            case DiskType::AtariMFMSS: return "Atari MFM SS";
            case DiskType::AtariMFMDS: return "Atari MFM DS";
            case DiskType::AppleII: return "Apple II";
            case DiskType::AppleIIPro: return "Apple II Pro";
            case DiskType::Apple400K: return "Apple 400K";
            case DiskType::Apple800K: return "Apple 800K";
            case DiskType::Apple144: return "Apple 1.44M";
            case DiskType::PC360K: return "PC 360K";
            case DiskType::PC720K: return "PC 720K";
            case DiskType::PC12M: return "PC 1.2M";
            case DiskType::PC144M: return "PC 1.44M";
            case DiskType::TandyTRS80SSSD: return "TRS-80 SS/SD";
            case DiskType::TandyTRS80SSDD: return "TRS-80 SS/DD";
            case DiskType::TandyTRS80DSSD: return "TRS-80 DS/SD";
            case DiskType::TandyTRS80DSDD: return "TRS-80 DS/DD";
            case DiskType::TI994A: return "TI-99/4A";
            case DiskType::RolandD20: return "Roland D-20";
            case DiskType::AmstradCPC: return "Amstrad CPC";
            case DiskType::Other: return "Other";
            case DiskType::TapeDrive: return "Tape Drive";
            case DiskType::HardDrive: return "Hard Drive";
            default: return "Unknown";
        }
    }
};

} // namespace uft::scp

#endif // UFT_USE_LEXY

#endif // UFT_SCP_PARSER_COMPLETE_HPP

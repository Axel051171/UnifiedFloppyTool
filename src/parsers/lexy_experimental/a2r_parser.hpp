/**
 * @file a2r_parser.hpp
 * @brief A2R (Apple II Raw) Format Parser
 * 
 * P3: Support for A2R v2 format (Applesauce)
 * - Apple II 5.25" and 3.5" disk images
 * - Flux-level preservation
 * - Timing/meta information
 */

#ifndef UFT_A2R_PARSER_HPP
#define UFT_A2R_PARSER_HPP

#ifdef UFT_USE_LEXY

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <map>

namespace uft::a2r {

/* ═══════════════════════════════════════════════════════════════════════════════
 * A2R Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

constexpr uint8_t A2R_MAGIC[] = {'A', '2', 'R', '2'};  // Version 2
constexpr uint32_t A2R_MAGIC_V2 = 0x32523241;         // "A2R2" little-endian

// Chunk types
enum class ChunkType : uint32_t {
    INFO = 0x4F464E49,  // "INFO"
    STRM = 0x4D525453,  // "STRM"
    META = 0x4154454D,  // "META"
    RWCP = 0x50435752,  // "RWCP" - Raw Capture
    SLVD = 0x44564C53,  // "SLVD" - Solved (decoded)
};

// Disk types
enum class DiskType : uint8_t {
    Disk525_SS = 1,     // 5.25" Single-sided
    Disk525_DS = 2,     // 5.25" Double-sided
    Disk35_SS = 3,      // 3.5" Single-sided (400K)
    Disk35_DS = 4,      // 3.5" Double-sided (800K)
};

// Capture types
enum class CaptureType : uint8_t {
    Timing = 1,
    Bits = 2,
    XT = 3,             // Extended timing
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct A2RInfo {
    uint8_t version;
    std::string creator;
    DiskType diskType;
    bool writeProtected;
    bool synchronized;
    bool hardSectorCount;
    
    int trackCount() const {
        switch (diskType) {
            case DiskType::Disk525_SS: return 35;
            case DiskType::Disk525_DS: return 70;
            case DiskType::Disk35_SS: return 80;
            case DiskType::Disk35_DS: return 160;
            default: return 0;
        }
    }
    
    std::string diskTypeName() const {
        switch (diskType) {
            case DiskType::Disk525_SS: return "5.25\" SS";
            case DiskType::Disk525_DS: return "5.25\" DS";
            case DiskType::Disk35_SS: return "3.5\" SS (400K)";
            case DiskType::Disk35_DS: return "3.5\" DS (800K)";
            default: return "Unknown";
        }
    }
};

struct A2RCapture {
    uint8_t side;
    uint8_t track;
    CaptureType captureType;
    uint32_t dataLength;
    uint32_t tickCount;
    std::vector<uint8_t> data;
    
    // Calculated timing
    double revolutionTimeUs() const {
        // 8MHz clock, typical
        return tickCount / 8.0;
    }
    
    double rpmEstimate() const {
        double revTimeUs = revolutionTimeUs();
        if (revTimeUs > 0) {
            return 60000000.0 / revTimeUs;
        }
        return 0;
    }
};

struct A2RTrack {
    uint8_t trackNum;
    uint8_t side;
    std::vector<A2RCapture> captures;
    
    // Best capture selection
    const A2RCapture* bestCapture() const {
        if (captures.empty()) return nullptr;
        return &captures[0];  // TODO: Score-based selection
    }
};

struct A2RMeta {
    std::string title;
    std::string subtitle;
    std::string publisher;
    std::string developer;
    std::string copyright;
    std::string version;
    std::string language;
    std::string requires;
    std::string side;
    std::string sideNum;
    std::string notes;
    
    std::map<std::string, std::string> custom;
};

struct A2RFile {
    A2RInfo info;
    A2RMeta meta;
    std::vector<A2RTrack> tracks;
    std::map<std::pair<int,int>, size_t> trackIndex;  // (track,side) -> index
    
    bool isValid() const { return !tracks.empty(); }
    
    const A2RTrack* getTrack(int track, int side = 0) const {
        auto it = trackIndex.find({track, side});
        if (it != trackIndex.end()) {
            return &tracks[it->second];
        }
        return nullptr;
    }
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parser Class
 * ═══════════════════════════════════════════════════════════════════════════════ */

class A2RParser {
public:
    /**
     * @brief Validate A2R magic
     */
    static bool validateMagic(const uint8_t* data, size_t len) {
        if (len < 8) return false;
        return data[0] == 'A' && data[1] == '2' && 
               data[2] == 'R' && data[3] == '2';
    }
    
    /**
     * @brief Get A2R version
     */
    static int getVersion(const uint8_t* data, size_t len) {
        if (!validateMagic(data, len)) return -1;
        return data[4];  // Version byte after magic
    }
    
    /**
     * @brief Parse A2R file
     */
    static std::optional<A2RFile> parseFile(const uint8_t* data, size_t len) {
        if (!validateMagic(data, len)) return std::nullopt;
        
        A2RFile file;
        size_t offset = 8;  // Skip magic + header
        
        while (offset + 8 <= len) {
            uint32_t chunkType = readLE32(data + offset);
            uint32_t chunkSize = readLE32(data + offset + 4);
            
            if (offset + 8 + chunkSize > len) break;
            
            const uint8_t* chunkData = data + offset + 8;
            
            switch (static_cast<ChunkType>(chunkType)) {
                case ChunkType::INFO:
                    parseInfoChunk(chunkData, chunkSize, file.info);
                    break;
                    
                case ChunkType::STRM:
                    parseStreamChunk(chunkData, chunkSize, file);
                    break;
                    
                case ChunkType::META:
                    parseMetaChunk(chunkData, chunkSize, file.meta);
                    break;
                    
                case ChunkType::RWCP:
                    parseRawCaptureChunk(chunkData, chunkSize, file);
                    break;
                    
                default:
                    // Skip unknown chunks
                    break;
            }
            
            offset += 8 + chunkSize;
        }
        
        return file;
    }
    
    /**
     * @brief Parse INFO chunk
     */
    static void parseInfoChunk(const uint8_t* data, size_t len, A2RInfo& info) {
        if (len < 60) return;
        
        info.version = data[0];
        
        // Creator string (32 bytes, null-padded)
        char creator[33] = {0};
        memcpy(creator, data + 1, 32);
        info.creator = creator;
        
        info.diskType = static_cast<DiskType>(data[33]);
        info.writeProtected = data[34] != 0;
        info.synchronized = data[35] != 0;
        info.hardSectorCount = data[36];
    }
    
    /**
     * @brief Parse META chunk
     */
    static void parseMetaChunk(const uint8_t* data, size_t len, A2RMeta& meta) {
        // META is a series of key-value pairs separated by tabs and newlines
        std::string metaStr(reinterpret_cast<const char*>(data), len);
        
        size_t pos = 0;
        while (pos < metaStr.size()) {
            size_t eol = metaStr.find('\n', pos);
            if (eol == std::string::npos) eol = metaStr.size();
            
            std::string line = metaStr.substr(pos, eol - pos);
            size_t tab = line.find('\t');
            
            if (tab != std::string::npos) {
                std::string key = line.substr(0, tab);
                std::string value = line.substr(tab + 1);
                
                if (key == "title") meta.title = value;
                else if (key == "subtitle") meta.subtitle = value;
                else if (key == "publisher") meta.publisher = value;
                else if (key == "developer") meta.developer = value;
                else if (key == "copyright") meta.copyright = value;
                else if (key == "version") meta.version = value;
                else if (key == "language") meta.language = value;
                else if (key == "requires") meta.requires = value;
                else if (key == "side") meta.side = value;
                else if (key == "side_name") meta.sideNum = value;
                else if (key == "notes") meta.notes = value;
                else meta.custom[key] = value;
            }
            
            pos = eol + 1;
        }
    }
    
private:
    static uint32_t readLE32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    
    static void parseStreamChunk(const uint8_t* data, size_t len, A2RFile& file) {
        // STRM chunk contains timing/flux data
        size_t offset = 0;
        
        while (offset + 10 <= len) {
            uint8_t location = data[offset];
            uint8_t captureType = data[offset + 1];
            uint32_t dataLen = readLE32(data + offset + 2);
            uint32_t tickCount = readLE32(data + offset + 6);
            
            if (offset + 10 + dataLen > len) break;
            
            A2RCapture cap;
            cap.side = (location >> 7) & 0x01;
            cap.track = location & 0x7F;
            cap.captureType = static_cast<CaptureType>(captureType);
            cap.dataLength = dataLen;
            cap.tickCount = tickCount;
            cap.data.resize(dataLen);
            memcpy(cap.data.data(), data + offset + 10, dataLen);
            
            // Find or create track
            auto key = std::make_pair((int)cap.track, (int)cap.side);
            auto it = file.trackIndex.find(key);
            
            if (it == file.trackIndex.end()) {
                A2RTrack track;
                track.trackNum = cap.track;
                track.side = cap.side;
                track.captures.push_back(cap);
                file.trackIndex[key] = file.tracks.size();
                file.tracks.push_back(track);
            } else {
                file.tracks[it->second].captures.push_back(cap);
            }
            
            offset += 10 + dataLen;
        }
    }
    
    static void parseRawCaptureChunk(const uint8_t* data, size_t len, A2RFile& file) {
        // Similar to STRM but with different structure
        parseStreamChunk(data, len, file);
    }
};

} // namespace uft::a2r

#endif // UFT_USE_LEXY

#endif // UFT_A2R_PARSER_HPP

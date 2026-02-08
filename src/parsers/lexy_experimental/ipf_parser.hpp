/**
 * @file ipf_parser.hpp
 * @brief IPF (Interchangeable Preservation Format) Parser using lexy
 * 
 * P2: Support for CAPS/SPS IPF format
 * - TLV-based container format
 * - Multiple record types
 * - Copy protection preservation
 */

#ifndef UFT_IPF_PARSER_HPP
#define UFT_IPF_PARSER_HPP

#ifdef UFT_USE_LEXY

#include <lexy/dsl.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/input/buffer.hpp>
#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include <map>

namespace uft::ipf {

namespace dsl = lexy::dsl;

/* ═══════════════════════════════════════════════════════════════════════════════
 * IPF Record Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

enum class RecordType : uint32_t {
    CAPS = 0x43415053,  // "CAPS" - File header
    INFO = 0x494E464F,  // "INFO" - Disk info
    IMGE = 0x494D4745,  // "IMGE" - Track image
    DATA = 0x44415441,  // "DATA" - Track data
    CTEI = 0x43544549,  // "CTEI" - CTEI info
    CTEX = 0x43544558,  // "CTEX" - CTEI extension
    TRCK = 0x5452434B,  // "TRCK" - Track info (v5)
    UNKNOWN = 0
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct IPFRecord {
    RecordType type;
    uint32_t length;
    uint32_t crc;
    std::vector<uint8_t> data;
    
    std::string typeName() const {
        switch (type) {
            case RecordType::CAPS: return "CAPS";
            case RecordType::INFO: return "INFO";
            case RecordType::IMGE: return "IMGE";
            case RecordType::DATA: return "DATA";
            case RecordType::CTEI: return "CTEI";
            case RecordType::CTEX: return "CTEX";
            case RecordType::TRCK: return "TRCK";
            default: return "UNKNOWN";
        }
    }
};

struct IPFHeader {
    uint32_t encoderType;
    uint32_t encoderRev;
    uint32_t fileKey;
    uint32_t fileRev;
    uint32_t origin;
    uint32_t minTrack;
    uint32_t maxTrack;
    uint32_t minSide;
    uint32_t maxSide;
    uint32_t creationDate;
    uint32_t creationTime;
    uint32_t platforms;
    uint32_t diskNumber;
    uint32_t creatorId;
    uint32_t reserved[3];
};

struct IPFDiskInfo {
    uint32_t mediaType;
    uint32_t encoderType;
    uint32_t encoderRev;
    uint32_t fileKey;
    uint32_t fileRev;
    uint32_t origin;
    uint32_t minTrack;
    uint32_t maxTrack;
    uint32_t minSide;
    uint32_t maxSide;
    uint32_t creationDate;
    uint32_t creationTime;
    uint32_t platforms;
    uint32_t diskNumber;
    uint32_t creatorId;
    
    int trackCount() const { return maxTrack - minTrack + 1; }
    int sideCount() const { return maxSide - minSide + 1; }
    bool isDoubleSided() const { return maxSide > 0; }
    
    std::string platformName() const {
        std::string result;
        if (platforms & 0x0001) result += "Amiga ";
        if (platforms & 0x0002) result += "AtariST ";
        if (platforms & 0x0004) result += "PC ";
        if (platforms & 0x0008) result += "Amstrad ";
        if (platforms & 0x0010) result += "Spectrum ";
        if (platforms & 0x0020) result += "Sam ";
        if (platforms & 0x0040) result += "Archimedes ";
        if (platforms & 0x0080) result += "C64 ";
        if (platforms & 0x0100) result += "AtariXL ";
        return result.empty() ? "Unknown" : result;
    }
};

struct IPFTrackImage {
    uint32_t track;
    uint32_t side;
    uint32_t density;
    uint32_t signalType;
    uint32_t trackBytes;
    uint32_t startBytePos;
    uint32_t startBitPos;
    uint32_t dataBits;
    uint32_t gapBits;
    uint32_t trackBits;
    uint32_t blockCount;
    uint32_t encoderProcess;
    uint32_t trackFlags;
    uint32_t dataKey;
    uint32_t reserved[3];
};

struct IPFDataBlock {
    uint32_t dataOffset;
    uint32_t dataLength;
    uint32_t gapOffset;
    uint32_t gapLength;
    std::vector<uint8_t> data;
    std::vector<uint8_t> gap;
};

struct IPFTrack {
    IPFTrackImage image;
    std::vector<IPFDataBlock> blocks;
    std::vector<uint8_t> rawData;
    
    bool hasWeakBits() const { return (image.trackFlags & 0x01) != 0; }
    bool hasSpeedVariation() const { return (image.trackFlags & 0x02) != 0; }
    bool isFuzzyBits() const { return (image.trackFlags & 0x04) != 0; }
};

struct IPFFile {
    IPFHeader header;
    IPFDiskInfo diskInfo;
    std::vector<IPFTrack> tracks;
    std::map<std::pair<int,int>, size_t> trackIndex;  // (track,side) -> index
    
    bool isValid() const { return diskInfo.trackCount() > 0; }
    
    const IPFTrack* getTrack(int track, int side) const {
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

class IPFParser {
public:
    /**
     * @brief Parse IPF file header
     */
    static std::optional<IPFHeader> parseHeader(const uint8_t* data, size_t len) {
        if (len < 28) return std::nullopt;
        
        // Check CAPS magic
        if (data[0] != 'C' || data[1] != 'A' || 
            data[2] != 'P' || data[3] != 'S') {
            return std::nullopt;
        }
        
        IPFHeader h;
        h.encoderType = readBE32(data + 12);
        h.encoderRev = readBE32(data + 16);
        h.fileKey = readBE32(data + 20);
        h.fileRev = readBE32(data + 24);
        
        return h;
    }
    
    /**
     * @brief Parse single IPF record
     */
    static std::optional<IPFRecord> parseRecord(const uint8_t* data, size_t len) {
        if (len < 12) return std::nullopt;
        
        IPFRecord rec;
        rec.type = static_cast<RecordType>(readBE32(data));
        rec.length = readBE32(data + 4);
        rec.crc = readBE32(data + 8);
        
        if (rec.length > len) return std::nullopt;
        
        if (rec.length > 12) {
            rec.data.resize(rec.length - 12);
            std::copy(data + 12, data + rec.length, rec.data.begin());
        }
        
        return rec;
    }
    
    /**
     * @brief Parse complete IPF file
     */
    static std::optional<IPFFile> parseFile(const uint8_t* data, size_t len) {
        IPFFile file;
        size_t offset = 0;
        
        while (offset + 12 <= len) {
            auto rec = parseRecord(data + offset, len - offset);
            if (!rec) break;
            
            switch (rec->type) {
                case RecordType::CAPS:
                    // Skip header record, already validated
                    break;
                    
                case RecordType::INFO:
                    if (rec->data.size() >= 80) {
                        parseDiskInfo(rec->data.data(), rec->data.size(), file.diskInfo);
                    }
                    break;
                    
                case RecordType::IMGE:
                    if (rec->data.size() >= 80) {
                        IPFTrack track;
                        parseTrackImage(rec->data.data(), rec->data.size(), track.image);
                        file.tracks.push_back(track);
                        file.trackIndex[{track.image.track, track.image.side}] = 
                            file.tracks.size() - 1;
                    }
                    break;
                    
                case RecordType::DATA:
                    // Data follows IMGE records
                    if (!file.tracks.empty()) {
                        file.tracks.back().rawData = rec->data;
                    }
                    break;
                    
                default:
                    // Skip unknown records
                    break;
            }
            
            offset += rec->length;
            
            // Align to 4 bytes
            offset = (offset + 3) & ~3;
        }
        
        return file;
    }
    
    /**
     * @brief Validate IPF magic
     */
    static bool validateMagic(const uint8_t* data, size_t len) {
        return len >= 4 && 
               data[0] == 'C' && data[1] == 'A' && 
               data[2] == 'P' && data[3] == 'S';
    }
    
    /**
     * @brief Calculate IPF CRC
     */
    static uint32_t calculateCRC(const uint8_t* data, size_t len) {
        uint32_t crc = 0;
        for (size_t i = 0; i < len; i++) {
            crc += data[i];
        }
        return crc;
    }
    
private:
    static uint32_t readBE32(const uint8_t* p) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    
    static void parseDiskInfo(const uint8_t* data, size_t len, IPFDiskInfo& info) {
        if (len < 80) return;
        
        info.mediaType = readBE32(data);
        info.encoderType = readBE32(data + 4);
        info.encoderRev = readBE32(data + 8);
        info.fileKey = readBE32(data + 12);
        info.fileRev = readBE32(data + 16);
        info.origin = readBE32(data + 20);
        info.minTrack = readBE32(data + 24);
        info.maxTrack = readBE32(data + 28);
        info.minSide = readBE32(data + 32);
        info.maxSide = readBE32(data + 36);
        info.creationDate = readBE32(data + 40);
        info.creationTime = readBE32(data + 44);
        info.platforms = readBE32(data + 48);
        info.diskNumber = readBE32(data + 52);
        info.creatorId = readBE32(data + 56);
    }
    
    static void parseTrackImage(const uint8_t* data, size_t len, IPFTrackImage& img) {
        if (len < 80) return;
        
        img.track = readBE32(data);
        img.side = readBE32(data + 4);
        img.density = readBE32(data + 8);
        img.signalType = readBE32(data + 12);
        img.trackBytes = readBE32(data + 16);
        img.startBytePos = readBE32(data + 20);
        img.startBitPos = readBE32(data + 24);
        img.dataBits = readBE32(data + 28);
        img.gapBits = readBE32(data + 32);
        img.trackBits = readBE32(data + 36);
        img.blockCount = readBE32(data + 40);
        img.encoderProcess = readBE32(data + 44);
        img.trackFlags = readBE32(data + 48);
        img.dataKey = readBE32(data + 52);
    }
};

} // namespace uft::ipf

#endif // UFT_USE_LEXY

#endif // UFT_IPF_PARSER_HPP

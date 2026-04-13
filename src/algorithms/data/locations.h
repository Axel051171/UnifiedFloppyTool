#pragma once

struct CylinderHead
{
#if __cplusplus >= 202002L
    bool operator==(const CylinderHead&) const = default;
    std::strong_ordering operator<=>(const CylinderHead&) const = default;
#else
    bool operator==(const CylinderHead& o) const { return cylinder == o.cylinder && head == o.head; }
    bool operator!=(const CylinderHead& o) const { return !(*this == o); }
    bool operator<(const CylinderHead& o) const { return cylinder != o.cylinder ? cylinder < o.cylinder : head < o.head; }
    bool operator>(const CylinderHead& o) const { return o < *this; }
    bool operator<=(const CylinderHead& o) const { return !(o < *this); }
    bool operator>=(const CylinderHead& o) const { return !(*this < o); }
#endif

    unsigned cylinder, head;
};

struct CylinderHeadSector
{
#if __cplusplus >= 202002L
    bool operator==(const CylinderHeadSector&) const = default;
    std::strong_ordering operator<=>(const CylinderHeadSector&) const = default;
#else
    bool operator==(const CylinderHeadSector& o) const { return cylinder == o.cylinder && head == o.head && sector == o.sector; }
    bool operator!=(const CylinderHeadSector& o) const { return !(*this == o); }
    bool operator<(const CylinderHeadSector& o) const {
        if (cylinder != o.cylinder) return cylinder < o.cylinder;
        if (head != o.head) return head < o.head;
        return sector < o.sector;
    }
    bool operator>(const CylinderHeadSector& o) const { return o < *this; }
    bool operator<=(const CylinderHeadSector& o) const { return !(o < *this); }
    bool operator>=(const CylinderHeadSector& o) const { return !(*this < o); }
#endif

    unsigned cylinder, head, sector;
};

struct LogicalLocation
{
#if __cplusplus >= 202002L
    bool operator==(const LogicalLocation&) const = default;
    std::strong_ordering operator<=>(const LogicalLocation&) const = default;
#else
    bool operator==(const LogicalLocation& o) const { return logicalCylinder == o.logicalCylinder && logicalHead == o.logicalHead && logicalSector == o.logicalSector; }
    bool operator!=(const LogicalLocation& o) const { return !(*this == o); }
    bool operator<(const LogicalLocation& o) const {
        if (logicalCylinder != o.logicalCylinder) return logicalCylinder < o.logicalCylinder;
        if (logicalHead != o.logicalHead) return logicalHead < o.logicalHead;
        return logicalSector < o.logicalSector;
    }
    bool operator>(const LogicalLocation& o) const { return o < *this; }
    bool operator<=(const LogicalLocation& o) const { return !(o < *this); }
    bool operator>=(const LogicalLocation& o) const { return !(*this < o); }
#endif

    unsigned logicalCylinder;
    unsigned logicalHead;
    unsigned logicalSector;

    operator std::string() const
    {
        return fmt::format(
            "c{}h{}s{}", logicalCylinder, logicalHead, logicalSector);
    }

    CylinderHead trackLocation() const
    {
        return {logicalCylinder, logicalHead};
    }
};

inline std::ostream& operator<<(std::ostream& stream, LogicalLocation location)
{
    stream << (std::string)location;
    return stream;
}

extern std::vector<CylinderHead> parseCylinderHeadsString(const std::string& s);
extern std::string convertCylinderHeadsToString(
    const std::vector<CylinderHead>& chs);

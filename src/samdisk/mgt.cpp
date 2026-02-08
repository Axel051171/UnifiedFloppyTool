// Miles Gordon Technology format for MGT +D and SAM Coupe disks:
//  http://www.worldofspectrum.org/faq/reference/formats.htm#MGT
//
// Also accepts IMG variant with different track order.

#include "SAMdisk.h"
#include "SAMCoupe.h"

bool ReadMGT(MemFile& file, std::shared_ptr<Disk>& disk)
{
    if (file.size() != MGT_DISK_SIZE)
        return false;

    // Read the first sector, which contains disk information.
    std::array<uint8_t, SECTOR_SIZE> sector0;
    if (!file.rewind() || !file.read(sector0))
        return false;

    MGT_DISK_INFO di{};
    GetDiskInfo(sector0.data(), di);

    // Check the expected chain offset in MGT images (alternating sides).
    std::array<uint8_t, 2> buf;
    auto chain_offset = (di.dir_tracks * MGT_SIDES * MGT_TRACK_SIZE) + SECTOR_SIZE - 2;
    bool mgt = file.seek(chain_offset) && file.read(buf) && buf[0] == di.dir_tracks && buf[1] == 2;

    // Check the expected chain offset in IMG images (tracks on same side).
    chain_offset = (di.dir_tracks * MGT_TRACK_SIZE) + SECTOR_SIZE - 2;
    bool img = file.seek(chain_offset) && file.read(buf) && buf[0] == di.dir_tracks && buf[1] == 2;

    // Also check the file header against the first directory entry.
    std::array<uint8_t, 9> dir_header{};
    std::array<uint8_t, 9> file_header{};
    auto dir_offset = 0xd3;
    auto file_offset = di.dir_tracks * MGT_TRACK_SIZE;
    img |= file.seek(dir_offset) && file.read(dir_header) &&
        !std::equal(dir_header.begin(), dir_header.end(), file_header.begin()) && // zero check
        file.seek(file_offset) && file.read(file_header) &&
        std::equal(dir_header.begin(), dir_header.end(), file_header.begin());

    // If neither signature matched this probably isn't an MGT file.
    // However, accept it if it has a .mgt file extension.
    if (!mgt && !img && !IsFileExt(file.name(), "mgt"))
        return false;

    file.rewind();
    disk->format(Format(RegularFormat::MGT), file.data(), img);
    disk->strType = img ? "IMG" : "MGT";

    return true;
}

bool WriteMGT(FILE* f_, std::shared_ptr<Disk>& disk)
{
    return WriteRegularDisk(f_, *disk, RegularFormat::MGT);
}

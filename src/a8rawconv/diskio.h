#ifndef f_DISKIO_H
#define f_DISKIO_H

void kf_read(RawDisk& raw_disk, int trackcount, int trackstep, const char *basepath, int sidepos, int sidewidth, int sidebase, int countpos, int countwidth, int trackselect, bool use_48tpi);
void scp_read(RawDisk& raw_disk, const char *path, int selected_track, int forced_tpi, int forced_side_step, int forced_tracks, int forced_sides);
void scp_write(const RawDisk& raw_disk, const char *path, int selected_track, int forced_tpi, int forced_side_step);

void scp_direct_read(RawDisk& raw_disk, const char *path, int selected_track, int revs, bool high_density, bool splice);
void scp_direct_write(const RawDisk& raw_disk, const char *path, int selected_track, bool high_density, bool splice);

void script_read(RawDisk& raw_disk, const char *path, int selected_track);

void read_atr(DiskInfo& disk, const char *path, int track_select);
void read_atx(DiskInfo& disk, const char *path, int track);
void write_atx(const char *path, DiskInfo& disk, int track);
void write_atr(const char *path, DiskInfo& disk, int track);

void read_xfd(DiskInfo& disk, const char *path, int track);
void write_xfd(const char *path, DiskInfo& disk, int track);

void read_vfd(DiskInfo& disk, const char *path, int track);
void write_vfd(const char *path, DiskInfo& disk, int track);

void read_adf(DiskInfo& disk, const char *path, int track);
void write_adf(const char *path, DiskInfo& disk, int track);

void read_apple2_dsk(DiskInfo& disk, const char *path, int track, bool useProDOSOrder);
void write_apple2_dsk(const char *path, DiskInfo& disk, int track, bool useProDOSOrder, bool mac_format);
void read_apple2_nib(RawDisk& raw_disk, const char *path, int track);
void write_apple2_nib(const char *path, const DiskInfo& disk, int track);

#endif

struct sector_list {
	int sector;
	int offset;
};

struct phys {
	int (*min_track)(struct phys *);
	int (*max_track)(struct phys *);
	int (*num_tracks)(struct phys *);
	int (*min_side)(struct phys *);
	int (*max_side)(struct phys *);
	int (*num_sides)(struct phys *);
	int (*min_sector)(struct phys *, int, int);
	int (*max_sector)(struct phys *, int, int);
	int (*num_sectors)(struct phys *, int, int);
	int (*tpi)(struct phys *);
	int (*density)(struct phys *);
	unsigned char (*encoding)(struct phys *, int, int, int);
	int (*bitcell)(struct phys *, int, int, int);
	int (*sector_bytes)(struct phys *, int, int, int);
	int (*track_bytes)(struct phys *, int, int);
	int (*physical_track)(struct phys *, int);
	void (*best_read_order)(struct phys *, struct sector_list *, int, int);
	int (*read_sector)(struct phys *, unsigned char *, int, int, int);
	int (*prepare)(struct phys *);
};

int phys_gen_num_tracks(struct phys *this);
int phys_gen_num_sectors(struct phys *this, int track, int side);
int phys_gen_num_sides(struct phys *this);
int phys_gen_track_bytes(struct phys *this, int track, int side);
int phys_gen_physical_track(struct phys *this, int track);
void phys_gen_best_read_order(struct phys *this, struct sector_list *out, int track, int side);
int phys_gen_read_fm(unsigned char *out, int track, int, int side, int sector, int size, int bitcell);
int phys_gen_read_mfm(unsigned char *out, int track, int, int side, int sector, int size, int bitcell);
int phys_gen_read_sector(struct phys *this, unsigned char *out, int track, int side, int sector);
int phys_gen_48tpi(struct phys *this);
int phys_gen_96tpi(struct phys *this);
int phys_gen_100tpi(struct phys *this);
int phys_gen_low_density(struct phys *this);
int phys_gen_high_density(struct phys *this);
int phys_gen_no_prepare(struct phys *this);

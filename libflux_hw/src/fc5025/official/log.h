struct directory_entry {
	char *name;
	mode_t mode;
	time_t mtime;
	off_t size;
};

struct log {
	int (*file_list)(struct phys *, struct log *, struct directory_entry *, int, char *);
	int (*is_path_valid)(struct phys *, struct log *, char *);
	int (*file_size)(struct phys *, struct log *, char *);
	int (*read_file)(struct phys *, struct log *, unsigned char *, char *);
};

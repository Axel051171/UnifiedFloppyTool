struct format_info {
	char *id;
	char *desc;
	char *suffix;
	struct phys *phys;
	struct log *log;
};

struct format_info *get_format_list(void);
struct format_info *format_by_id(char *id);

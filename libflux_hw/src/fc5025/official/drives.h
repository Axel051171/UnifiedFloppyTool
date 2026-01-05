struct drive_info {
	char id[256];
	char desc[256];
	struct usb_device *usbdev;
};

int open_drive(struct drive_info *drive);
int open_drive_by_id(char *id);
char *open_default_drive(void);
int close_drive(void);
struct drive_info *get_drive_list(void);

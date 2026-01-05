static const int FC5025_VID=0x16c0;
static const int FC5025_PID=0x6d6;

static const unsigned char OPCODE_SEEK=0xc0;
static const unsigned char OPCODE_SELF_TEST=0xc1;
static const unsigned char OPCODE_FLAGS=0xc2;
static const unsigned char OPCODE_DRIVE_STATUS=0xc3;
static const unsigned char OPCODE_INDEXES=0xc4;
static const unsigned char OPCODE_READ_FLEXIBLE=0xc6;
static const unsigned char OPCODE_READ_ID=0xc7;

static const unsigned char FORMAT_APPLE_GCR=1;
static const unsigned char FORMAT_COMMODORE_GCR=2;
static const unsigned char FORMAT_FM=3;
static const unsigned char FORMAT_MFM=4;

static const unsigned char READ_FLAG_SIDE=1;
static const unsigned char READ_FLAG_ID_FIELD=2;
static const unsigned char READ_FLAG_ORUN_RECOV=4;
static const unsigned char READ_FLAG_NO_AUTOSYNC=8;
static const unsigned char READ_FLAG_ANGULAR=16;
static const unsigned char READ_FLAG_NO_ADAPTIVE=32;

int fc_bulk_cdb(void *cdb, int length, int timeout, void *csw_out, void *xferbuf, int xferlen, int *xferlen_out);
int fc_recalibrate(void);
int fc_SEEK_abs(int track);
int fc_READ_ID(unsigned char *out, int length, char side, char format, int bitcell, unsigned char idam0, unsigned char idam1, unsigned char idam2);
int fc_FLAGS(int in, int mask, int *out);
int fc_set_density(int density);
int fc5025_open(struct usb_device *dev);
int fc5025_close(void);
int fc5025_find(struct usb_device **devs, int max);


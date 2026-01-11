#ifndef UFT_FORMATS_KCS_CASUTIL_WAVE_H
#define UFT_FORMATS_KCS_CASUTIL_WAVE_H

/*
 *  wave.h
 *
 *  Entry points of wave.c
 *
 *  Library to create or read wave files
 *
 *  Kansas City Standard formatted files are created uncompressed, 8 bit mono,
 *  with a sample rate of 22050.
 *
 *  All functions return errors in errno. This is indicated by
 *  a return value of NULL (open) or a nonzero result.
 */

#include <stdio.h>

#ifdef __GNUC__
#include <stdint.h>
#else
typedef unsigned long  uint32_t;
typedef unsigned short uint16_t;
typedef short          int16_t;  
#endif

/*
 *  Wave file control structure
 */
typedef char ID[ 4 ];

typedef struct _formatChunk {
    ID       chunkId;
    uint32_t chunkSize;
    int16_t  formatTag;
    uint16_t channels;
    uint32_t samplesPerSec;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} FORMAT_CHUNK;

typedef struct _dataChunk {
    ID       chunkId;
    uint32_t chunkSize;
} DATA_CHUNK;

typedef struct _wfile {
    FILE *file;
    int openmode;
    int isEof;
    long amplitude;
    uint32_t position;
    struct _riffHeader {
        ID           fileId;
        uint32_t     fileSize;
        ID           typeId;
        FORMAT_CHUNK formatChunk;
        DATA_CHUNK   dataChunk;
    } header;
    unsigned char sample[ 4 ];
} WFILE;

/*
 *  Open a wave file
 *
 *  openmode is "rb" or "wb"
 *  channels, bitsPerSample and samplesPerSec are used only for writing
 */
WFILE *wopen( char *filename,
              char *openmode,
              uint16_t channels,
              uint16_t bitsPerSample,
              uint32_t  samplesPerSec );

/*
 *  Close file after updating the header
 */
int wclose( WFILE *file );
              
/*
 *  Read samples from the file
 */
int wread( WFILE *file, void *samples, uint32_t numberOfSamples );

/*
 *  Read single sample from the file.
 *
 *  The full sample is written to file->sample in LSB-first byte order.
 *
 *  Returns the value of the first sample, first channel.
 *  The value is upscaled to 32 bits, signed.
 *
 *  Check errno for errors if result == -1!
 */
long readSample( WFILE *file );

/*
 *  Write samples to the file
 */
int wwrite( WFILE *file, void *sample, uint32_t numberOfSamples );

/*
 *  Write a single sample
 *  All channels have the same value
 */
int writeSample( WFILE *file, long value, uint32_t count );

/*
 *  Write a wave with given frequency and number of periods
 *  Amplitude is in percent
 *  phase = 1 inverts signal
 */
int writeWave( WFILE *file, uint32_t freq, uint32_t periods,
               long amplitude, int phase );

/*
 *  Write silence
 */
int writeSilence( WFILE *file, uint32_t samples );

/*
 *  Return file position in milliseconds
 */
uint32_t wtell( WFILE *file );

/*
 *  "Kansas City Standard" (KCS) file control structure
 */
typedef struct _kcsfile {
    WFILE *file;
    int baudrate;
    int bits;
    char parity;
    int stopbits;
    int carrier;
    enum {
        KCS_UNKNOWN,
        KCS_SYNCHING,
        KCS_SYNCHED
    } state;
    int bitLength;
    int wavesPerZero;
    int wavesPerOne;
    int halfWave;
    int singleWave;
    int syncOnZero;
    int zeroWaveLength;
    long syncWaves;
    int raw;
    int lastWave;
    int phase;
    enum {
        FRQ_UNKNOWN,
        FRQ_HIGH,
        FRQ_LOW
    } lastFreq;
    int lastSample;
    long bias;
    int good_count;
    int sampleBufferLength;
    int samplePointer;
    long sampleBuffer[ 1 ];
} KCS_FILE;

#define KCS_ERROR    -1
#define KCS_EOF      -2
#define KCS_BAD_DATA -3
#define KCS_LEAD_IN  0x7FFF
#define KCS_PARITY   0x4000
#define KCS_FRAMING  0x2000

/*
 *  Open a wave file for "Kansas City Standard" (KCS) encoded data
 *
 *  baudrate is 300, 600 or 1200
 *  bits is 7 or 8
 *  parity is one of 'E', 'O' or 'N'
 *  stopbits is 1 or 2
 */
KCS_FILE *kcsOpen( char *filename,
                   char *openmode,
                   int baudrate,
                   int bits,
                   char parity,
                   int stopbits );

/*
 *  Close KCS file
 */
int kcsClose( KCS_FILE *file );

/*
 *  Read a KCS coded bit
 *
 *  Returns
 *     0, 1 - bit successfully decoded
 *     KCS_ERROR    (-1) - file error, check errno
 *     KCS_EOF      (-2) - end of file
 *     KCS_BAD_DATA (-3) - no appropriate wave pattern found
 */
int kcsReadBit( KCS_FILE *file );

/*
 *  Read KCS coded data in uncooked form
 *
 *  Returns
 *     Positive 16 bit word - data successfully read
 *     KCS_ERROR    (-1)    - file error, check errno
 *     KCS_EOF      (-2)    - end of file
 *     KCS_BAD_DATA (-3)    - no appropriate wave pattern found
 */
int16_t kcsReadRaw( KCS_FILE *file );

/*
 *  Read a KCS coded byte with parity and framing info
 *
 *  Returns
 *     Positive 16 bit word - data successfully decoded
 *     Parity is always moved to bit 8
 *     KCS_PARITY  (Bit 14) is set on parity error
 *     KCS_FRAMING (Bit 13) is set on framing error
 *     KCS_LEAD_IN (0x7fff) - lead-in character
 *     KCS_ERROR    (-1)    - file error, check errno
 *     KCS_EOF      (-2)    - end of file
 *     KCS_BAD_DATA (-3)    - no appropriate wave pattern found
 */
int16_t kcsReadByte( KCS_FILE *file );

/*
 *  Read KCS coded data in uncooked ASCII armoured form
 *  - The lower 6 bits are transfered to the lower byte and 0x30 is added
 *  - The remaining bits are transfered to the upper byte and 0x30 is added
 *
 *  Returns
 *     Positive 16 bit word - data successfully read
 *     KCS_ERROR    (-1)    - file error, check errno
 *     KCS_EOF      (-2)    - end of file
 *     KCS_BAD_DATA (-3)    - no appropriate wave pattern found
 */
int16_t kcsReadAscii( KCS_FILE *file );

/*
 *  Write a "Kansas City Standard" (KCS) encoded bit to the file
 */
int kcsWriteBit( KCS_FILE *file, int bit );

/*
 *  Write a KCS encoded byte to the file
 */
int kcsWriteByte( KCS_FILE *file, unsigned char byte );

/*
 *  Write a KCS lead-in sequence of given length (in tens of seconds)
 */
int kcsLeadIn( KCS_FILE *file, unsigned int tensOfSecs );

/*
 *  Write a KCS lead-out sequence of given length (in periods)
 */
int kcsLeadOut( KCS_FILE *file, unsigned int periods );

/*
 *  Make sure that a value is stored LSB first
 */
void storeLsbFirst( uint32_t val, void *dest, int length );

/*
 *  Macro for storeLsbFirst (in place, automatic size)
 */
#define LSB_FIRST( v ) storeLsbFirst( v, &( v ), sizeof( v ) )

/*
 *  Read a value which is stored LSB first
 */
uint32_t getLsbFirst( void *src, int length );

/*
 *  Macro for getLsbFirst (automatic size)
 */
#define GET_LSB_FIRST( v ) getLsbFirst( &( v ), sizeof( v ) )

#endif /* UFT_FORMATS_KCS_CASUTIL_WAVE_H */

#include "amsdos.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cpm.h"
#include "platformdef.h"

static
u16 header_checksum(struct amsdos_header_s *header)
{
    int i;
    u16 sum;

    sum = 0;

    for (i = 0; i < 67; i++) {
        u8 *data = (u8 *) header;
        sum += data[i];
    }

    return sum;
}

void amsdos_new(FILE *fp, struct amsdos_header_s *dest, const char *file_name, u16 entry_addr, u16 exec_addr)
{
    u16 data_length;
    struct cpm_diren_s filename_diren;
    u8 file_type;
    u16 data_location;

    assert(fp);
    assert(dest);
    assert(file_name);

    memset(dest, 0, sizeof(*dest));

    fseek(fp, 0, SEEK_END);
    data_length = (u16) ftell(fp);
    fseek(fp, 0, SEEK_SET);

    denormalize_filename(file_name, &filename_diren);

    file_type = strnicmp((char *) filename_diren.ext, "bas", 3) == 0 ? 0 : 2;

    data_location = 0;

    if (file_type == 0) {
        data_location = 0x170; /* BASIC start address */
    }

    if (entry_addr > 0) {
        data_location = entry_addr;
    }

    dest->user_number = 0;
    memcpy(dest->filename, filename_diren.file_name, sizeof(dest->filename));
    memcpy(dest->extension, filename_diren.ext, sizeof(dest->extension));
    dest->filetype = file_type;
    dest->data_location = data_location;
    dest->first_block = 0;
    dest->logical_length = data_length;
    dest->entry_address = exec_addr;
    memcpy(&dest->_unused_file_length, &data_length, 2);
    dest->check_sum = header_checksum(dest);
}

int amsdos_header_exists(struct amsdos_header_s *header)
{
    assert(header);

    return header_checksum(header) == header->check_sum;
}

static
char *get_filetype(u8 filetype)
{
    if (filetype == 0) {
        return "Basic";
    }

    if (filetype == 1) {
        return "Protected";
    }

    if (filetype == 2) {
        return "Binary";
    }

    return "Unrecognized filetype";
}

void amsdos_print_header(struct amsdos_header_s *header)
{
    assert(header);

    printf("AMSDOS Header\n");
    printf("-------------\n");
    printf("User number          : %d\n", header->user_number);
    printf("Filename             : %.8s\n", header->filename);
    printf("Extension            : %.3s\n", header->extension);
    printf("Filetype             : %s\n", get_filetype(header->filetype));
    printf("Data length          : 0x%.4x (%d)\n", header->data_length, header->data_length);
    printf("Loading address      : 0x%.4x\n", header->data_location);
    printf("Logical length       : 0x%.4x (%d)\n", header->logical_length, header->logical_length);
    printf("Entry adress         : 0x%.4x\n", header->entry_address);
    printf("Checksum:            : 0x%.4x\n", header->check_sum);
}

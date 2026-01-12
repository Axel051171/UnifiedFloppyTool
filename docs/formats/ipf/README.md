# IPF Container Format Support

## Overview

IPF (Interchangeable Preservation Format) is a container format developed by 
the Software Preservation Society (SPS) for preserving copy-protected floppy 
disk images, particularly Amiga games.

## Features

- **Chunk-based container parsing**
- **CRC32 validation**
- **Big/Little endian support**
- **Bounds and overlap checking**
- **Minimal writer for container creation**

## API

### Reader

```c
#include "uft/formats/ipf/uft_ipf.h"

uft_ipf_t ipf;
uft_ipf_err_t err;

// Open and parse
err = uft_ipf_open(&ipf, "disk.ipf");
err = uft_ipf_parse(&ipf);

// Validate (strict mode checks CRC)
err = uft_ipf_validate(&ipf, true);

// Access chunks
size_t n = uft_ipf_chunk_count(&ipf);
for (size_t i = 0; i < n; i++) {
    const uft_ipf_chunk_t *c = uft_ipf_chunk_at(&ipf, i);
    printf("Chunk: %s, size: %u\n", uft_fourcc_str(c->id), c->data_size);
}

// Read chunk data
uint8_t buf[4096];
size_t got;
uft_ipf_read_chunk_data(&ipf, 0, buf, sizeof(buf), &got);

// Cleanup
uft_ipf_close(&ipf);
```

### Writer

```c
uft_ipf_writer_t w;

// Create container (big-endian, 8-byte header)
uft_ipf_writer_open(&w, "output.ipf", UFT_IPF_MAGIC_CAPS, true, 8);

// Add chunks
uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_INFO, metadata, meta_len, false);
uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_DATA, track_data, track_len, false);

// Close
uft_ipf_writer_close(&w);
```

## Note on Track Decoding

This module handles container structure only. Full track decoding requires
a provider library (e.g., CapsImg from SPS). The optional CapsImg adapter
can be enabled with `-DUFT_IPF_ENABLE_CAPSIMG`.

## References

- Software Preservation Society: https://www.softpres.org/
- CAPS/IPF documentation (proprietary)

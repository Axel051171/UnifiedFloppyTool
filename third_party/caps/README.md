# CAPS/SPS Library

Official IPF decoding library from the Software Preservation Society (SPS).

## Contents

- `include/` - C header files
- `lib/linux-x86_64/` - Linux shared library (libcapsimage.so.4.2)
- `lib/windows-x64/` - Windows DLL (CAPSImg.dll)

## License

See LICENSE file. Library is provided by SPS for preservation purposes.

## Usage

```c
#include <caps/capsimage.h>

// Initialize
CAPSInit();

// Add image slot
int id = CAPSAddImage();

// Lock IPF file
CAPSLockImage(id, "game.ipf");

// Get image info
struct CapsImageInfo info;
CAPSGetImageInfo(&info, id);

// Lock track for reading
struct CapsTrackInfoT2 track;
CAPSLockTrack(&track, id, cylinder, head, flags);

// ... use track data ...

// Cleanup
CAPSUnlockAllTracks(id);
CAPSUnlockImage(id);
CAPSRemImage(id);
CAPSExit();
```

## Links

- http://www.softpres.org - Software Preservation Society
- http://www.kryoflux.com - KryoFlux (related hardware)

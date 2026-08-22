#include "stdafx.h"
#include <cmath>

#include "stdafx.cpp"
#include "a8rawconv.cpp"
#include "analyze.cpp"
#include "compensation.cpp"
#include "diskadf.cpp"
#include "diskatr.cpp"
#include "diskatx.cpp"
#include "diska2.cpp"
#include "diskvfd.cpp"
#include "diskxfd.cpp"
#include "binary.cpp"
#include "checksum.cpp"
#include "disk.cpp"
#include "encode.cpp"
#include "globals.cpp"
#include "interleave.cpp"
#include "os.cpp"
#include "rawdiskkf.cpp"
#include "rawdiskscp.cpp"
#include "rawdiskscpdirect.cpp"
#include "rawdiskscript.cpp"
#include "reporting.cpp"
#include "scp.cpp"
#include "sectorparser.cpp"

#if defined(_WIN32)
	#include "serial_win32.cpp"
#elif defined(__linux__)
	#include "serial_linux.cpp"
#else
	#include "serial_null.cpp"
#endif

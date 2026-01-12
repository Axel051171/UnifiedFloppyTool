#pragma once

#include "uft_sam.h"

#ifdef _WIN32

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4091)   // Ignore for 8.1 SDK + VS2015 warning
#endif

#include <DbgHelp.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

LONG WINAPI CrashDumpUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo);

#endif // _WIN32

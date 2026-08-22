#ifndef f_REPORTING_H
#define f_REPORTING_H

#ifdef _DEBUG
	#define A8RC_RT_ASSERT(condition) if (!(condition)) { printf("Runtime assertion failed!\n" __FILE__ "(%u): "#condition"\n", __LINE__); __debugbreak(); } else ((void)0)
#else
	#define A8RC_RT_ASSERT(condition) if (!(condition)) { fatalf("Runtime assertion failed!\n" __FILE__ "(%u): "#condition"\n", __LINE__); } else ((void)0)
#endif

[[noreturn]] void fatal(const char *msg);
[[noreturn]] void fatalf(const char *msg, ...);
[[noreturn]] void fatal_read();

#endif

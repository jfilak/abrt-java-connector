#ifndef __ABRT_CHECKER__
#define __ABRT_CHECKER__

/* Macros used to convert __LINE__ into string */
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define __UNUSED_VAR __attribute__ ((unused))

#ifdef VERBOSE
# define VERBOSE_PRINT(...) do { fprintf(stdout, __VA_ARGS__); } while(0)
#else // !VERBOSE
# define VERBOSE_PRINT(...) do { } while (0)
#endif // VERBOSE

#endif // __ABRT_CHECKER__



/*
 * finito
 */

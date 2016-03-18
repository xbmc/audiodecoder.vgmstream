/*
 * streamtypes.h - widely used type definitions
 */


#ifndef _STREAMTYPES_H
#define _STREAMTYPES_H

#include <stdint.h>
#ifdef _MSC_VER
//#include <pstdint.h>
#define inline _inline
#define strcasecmp _stricmp
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

typedef int16_t sample;

#endif

#ifndef NUMERIC_TYPES_HXX
#define NUMERIC_TYPES_HXX

#include <cstdint>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef u8 ubyte;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef s8 sbyte;

typedef float f32;
typedef double f64;

typedef size_t usize;

constexpr u8 ptrsize = sizeof(void *);

constexpr bool is_64bit = (ptrsize == 8u);
constexpr bool is_32bit = (ptrsize == 4u);
constexpr bool is_16bit = (ptrsize == 2u);

typedef intptr_t sptr;
typedef uintptr_t uptr;

#endif // NUMERIC_TYPES_HXX

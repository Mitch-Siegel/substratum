#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define U64_MAX UINT64_MAX
#define U32_MAX UINT32_MAX
#define U16_MAX UINT16_MAX
#define U8_MAX UINT8_MAX

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

#define I64_MAX INT64_MAX
#define I32_MAX INT32_MAX
#define I16_MAX INT16_MAX
#define I8_MAX INT8_MAX

#define false 0
#define true 1

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// TODO: replace this with sizeof(size_t): effectively XLEN
#define MACHINE_REGISTER_SIZE_BYTES 8

#define sprintedNumberLength 32

#define OUT_OBJECT_POINTER_NAME "__out_obj_pointer"

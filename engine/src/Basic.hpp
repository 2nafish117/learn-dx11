#include <memory>

//#ifdef __STDC_LIB_EXT1__
//#define __STDC_WANT_LIB_EXT1__
//#endif
#include <cstdlib>
#include <cstdint>

#include <vector>
#include <array>
#include <unordered_map>
#include <map>

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

using byte = unsigned char;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;
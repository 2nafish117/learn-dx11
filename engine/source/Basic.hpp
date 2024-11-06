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
#include <memory>
#include <limits>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define NAMEOF(x) (#x)

#ifdef _DEBUG
#define DEBUGBREAK() __debugbreak()
#else
#define DEBUGBREAK()
#endif

#define ASSERT(condition, msg) {	\
	assert((condition) && (msg));	\
	if(!(condition)) DEBUGBREAK();	\
}

#define UNIMPLEMENTED() ASSERT(false, msg)
#define UNREACHABLE(msg) ASSERT(false, msg)

using byte = unsigned char;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using uint = unsigned int;

using f32 = float;
using f64 = double;

using unique_defer = std::unique_ptr<void, std::function<void(void*)>>;

#define defer(name, expressions) unique_defer name((void*)1, [&](...){ {expressions} })
// usage
// int main(void)
// {
// 	int x = 0;
// 	defer(_d1, 
// 		spdlog::info(", World!");
// 		spdlog::info(", yo works!");
// 		x += 1;

// 		spdlog::info("Hello x={}", x);
// 	);
// 	spdlog::info("Hello x={}", x);
// }

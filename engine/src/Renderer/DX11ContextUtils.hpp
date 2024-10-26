#pragma once

#include "Basic.hpp"
#include <comdef.h>

static void dxlog(spdlog::level::level_enum lvl, const char* file, int line, HRESULT hr) {
	_com_error err(hr);
	spdlog::log(lvl, "[DX] {}:{} : {} ({:x})", file, line, err.ErrorMessage(), (u32)hr);
}

// @TODO: use the levels correctly
#define DXCRIT(hr)	dxlog(spdlog::level::critical, __FILE__, __LINE__, hr)
#define DXERROR(hr) dxlog(spdlog::level::err, __FILE__, __LINE__, hr)
#define DXWARN(hr)	dxlog(spdlog::level::warn, __FILE__, __LINE__, hr)
#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_ABI_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_ABI_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace screenai::backend::abi {

using GetSizeCallback = uint32_t(__cdecl*)(const char*);
using ReadCallback = void(__cdecl*)(const char*, uint32_t, char*);
using SetFileContentFunctionsFn = void(__cdecl*)(GetSizeCallback, ReadCallback);
using InitOcrFn = bool(__cdecl*)();
using UninitOcrFn = void(__cdecl*)();
using GetMaxDimensionFn = uint32_t(__cdecl*)();
using PerformOcrFn = char*(__cdecl*)(const void*, uint32_t&);
using FreeResultFn = void(__cdecl*)(char*);

// Recovered ABI view used by PerformOCR in the tested Chrome 153.4 build.
struct SkBitmapAbiView {
	void* pixel_ref;
	const void* pixels;
	size_t row_bytes;
	void* color_space;
	int32_t color_type;
	int32_t alpha_type;
	int32_t width;
	int32_t height;
};

static_assert(offsetof(SkBitmapAbiView, pixels) == 0x08);
static_assert(offsetof(SkBitmapAbiView, color_type) == 0x20);
static_assert(offsetof(SkBitmapAbiView, width) == 0x28);
static_assert(offsetof(SkBitmapAbiView, height) == 0x2c);
static_assert(sizeof(SkBitmapAbiView) == 0x30);

struct Exports {
	SetFileContentFunctionsFn set_file_content_functions = nullptr;
	InitOcrFn initialize = nullptr;
	UninitOcrFn uninitialize = nullptr;
	GetMaxDimensionFn get_max_dimension = nullptr;
	PerformOcrFn perform_ocr = nullptr;
	FreeResultFn free_result = nullptr;
};

} // namespace screenai::backend::abi

#endif

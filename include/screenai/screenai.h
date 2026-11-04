#ifndef SCREENAI_SCREENAI_H_
#define SCREENAI_SCREENAI_H_

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(SCREENAI_BUILD)
#define SCREENAI_API __declspec(dllexport)
#else
#define SCREENAI_API __declspec(dllimport)
#endif
#define SCREENAI_CALL __cdecl
#else
#define SCREENAI_API
#define SCREENAI_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct screenai_recognizer screenai_recognizer;

typedef enum screenai_status {
	SCREENAI_STATUS_OK = 0,
	SCREENAI_STATUS_INVALID_ARGUMENT = 1,
	SCREENAI_STATUS_RUNTIME_ERROR = 2,
	SCREENAI_STATUS_IMAGE_ERROR = 3,
	SCREENAI_STATUS_OCR_ERROR = 4,
	SCREENAI_STATUS_OUT_OF_MEMORY = 5,
	SCREENAI_STATUS_INTERNAL_ERROR = 6
} screenai_status;

typedef enum screenai_pixel_format {
	SCREENAI_PIXEL_FORMAT_BGRA8 = 1,
	SCREENAI_PIXEL_FORMAT_RGBA8 = 2
} screenai_pixel_format;

SCREENAI_API screenai_status SCREENAI_CALL screenai_create(
	const wchar_t* runtime_dir,
	screenai_recognizer** out_recognizer,
	char* error_message,
	size_t error_capacity
);

SCREENAI_API void SCREENAI_CALL screenai_destroy(
	screenai_recognizer* recognizer
);

SCREENAI_API screenai_status SCREENAI_CALL screenai_get_max_image_dimension(
	screenai_recognizer* recognizer,
	uint32_t* out_dimension,
	char* error_message,
	size_t error_capacity
);

SCREENAI_API screenai_status SCREENAI_CALL screenai_recognize_file(
	screenai_recognizer* recognizer,
	const wchar_t* image_path,
	char** out_utf8_text,
	size_t* out_text_length,
	char* error_message,
	size_t error_capacity
);

SCREENAI_API screenai_status SCREENAI_CALL screenai_recognize_pixels(
	screenai_recognizer* recognizer,
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	screenai_pixel_format pixel_format,
	char** out_utf8_text,
	size_t* out_text_length,
	char* error_message,
	size_t error_capacity
);

SCREENAI_API void SCREENAI_CALL screenai_free_buffer(char* buffer);

#ifdef __cplusplus
}
#endif

#endif

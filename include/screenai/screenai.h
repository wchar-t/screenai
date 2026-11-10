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

typedef struct screenai_bounding_box_view {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	float angle;
} screenai_bounding_box_view;

typedef struct screenai_line_view {
	const char* text;
	size_t text_length;
	screenai_bounding_box_view bounding_box;
	int32_t has_bounding_box;
	float confidence;
	int32_t has_confidence;
} screenai_line_view;

typedef struct screenai_block_view {
	const screenai_line_view* lines;
	size_t line_count;
} screenai_block_view;

typedef struct screenai_ocr_result_view {
	const char* text;
	size_t text_length;
	uint32_t image_width;
	uint32_t image_height;
	const screenai_block_view* blocks;
	size_t block_count;
} screenai_ocr_result_view;

typedef struct screenai_ocr_result screenai_ocr_result;

SCREENAI_API screenai_status SCREENAI_CALL screenai_create(
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

SCREENAI_API screenai_status SCREENAI_CALL screenai_recognize_file_result(
	screenai_recognizer* recognizer,
	const wchar_t* image_path,
	screenai_ocr_result** out_result,
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

SCREENAI_API screenai_status SCREENAI_CALL screenai_recognize_pixels_result(
	screenai_recognizer* recognizer,
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	screenai_pixel_format pixel_format,
	screenai_ocr_result** out_result,
	char* error_message,
	size_t error_capacity
);

/* The returned view and its nested arrays remain valid until the result is destroyed. */
SCREENAI_API const screenai_ocr_result_view* SCREENAI_CALL screenai_ocr_result_get_view(
	const screenai_ocr_result* result
);

SCREENAI_API void SCREENAI_CALL screenai_ocr_result_destroy(
	screenai_ocr_result* result
);

SCREENAI_API void SCREENAI_CALL screenai_free_buffer(char* buffer);

#ifdef __cplusplus
}
#endif

#endif

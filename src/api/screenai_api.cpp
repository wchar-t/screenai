#include "screenai/screenai.h"

#include "backend/chrome_screen_ai/chrome_backend.h"
#include "core/recognizer.h"
#include "image/wic_decoder.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

struct screenai_recognizer {
	screenai_recognizer()
		: implementation(std::make_unique<screenai::core::Recognizer>()) {}

	std::unique_ptr<screenai::core::Recognizer> implementation;
	std::mutex mutex;
};

namespace {

void SetError(char* output, size_t capacity, const char* message) noexcept {
	if (!output || capacity == 0)
		return;

	const char* source = message ? message : "Unknown error.";
	const size_t source_length = std::strlen(source);
	const size_t length = capacity - 1 < source_length ? capacity - 1 : source_length;

	std::memcpy(output, source, length);
	output[length] = '\0';
}


void SetError(char* output, size_t capacity, const std::string& message) noexcept {
	SetError(output, capacity, message.c_str());
}


screenai_status CopyText(
	const std::string& text,
	char** output,
	size_t* output_length,
	char* error_message,
	size_t error_capacity
) {
	if (!output || !output_length) {
		SetError(error_message, error_capacity, "Output text pointers are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*output = nullptr;
	*output_length = 0;

	char* buffer = new (std::nothrow) char[text.size() + 1];
	if (!buffer) {
		SetError(error_message, error_capacity, "Could not allocate the OCR result buffer.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	}

	std::memcpy(buffer, text.data(), text.size());
	buffer[text.size()] = '\0';
	*output = buffer;
	*output_length = text.size();

	return SCREENAI_STATUS_OK;
}


screenai_status HandleProcessingError(
	const std::exception& error,
	screenai_status status,
	char* error_message,
	size_t error_capacity
) {
	SetError(error_message, error_capacity, error.what());

	return status;
}

} // namespace

extern "C" {

screenai_status SCREENAI_CALL screenai_create(
	screenai_recognizer** out_recognizer,
	char* error_message,
	size_t error_capacity
) {
	if (!out_recognizer) {
		SetError(error_message, error_capacity, "An output recognizer pointer is required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*out_recognizer = nullptr;

	try {
		*out_recognizer = new screenai_recognizer();

		return SCREENAI_STATUS_OK;
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate a recognizer.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const std::exception& error) {
		SetError(error_message, error_capacity, error.what());

		return SCREENAI_STATUS_RUNTIME_ERROR;
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while initializing ScreenAI.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


void SCREENAI_CALL screenai_destroy(screenai_recognizer* recognizer) {
	delete recognizer;
}


screenai_status SCREENAI_CALL screenai_get_max_image_dimension(
	screenai_recognizer* recognizer,
	uint32_t* out_dimension,
	char* error_message,
	size_t error_capacity
) {
	if (!recognizer || !out_dimension) {
		SetError(error_message, error_capacity, "Recognizer and output dimension are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		*out_dimension = recognizer->implementation->GetMaxImageDimension();

		return SCREENAI_STATUS_OK;
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while reading the image limit.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


screenai_status SCREENAI_CALL screenai_recognize_file(
	screenai_recognizer* recognizer,
	const wchar_t* image_path,
	char** out_utf8_text,
	size_t* out_text_length,
	char* error_message,
	size_t error_capacity
) {
	if (!recognizer || !image_path || !*image_path || !out_utf8_text || !out_text_length) {
		SetError(error_message, error_capacity, "Recognizer, image path, and output pointers are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*out_utf8_text = nullptr;
	*out_text_length = 0;

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		const auto result = recognizer->implementation->RecognizeFile(image_path);

		return CopyText(
			result.text,
			out_utf8_text,
			out_text_length,
			error_message,
			error_capacity
		);
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate the OCR result.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const screenai::image::DecodeError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_IMAGE_ERROR,
			error_message,
			error_capacity
		);
	} catch (const screenai::backend::OcrError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_OCR_ERROR,
			error_message,
			error_capacity
		);
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while processing the image.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


screenai_status SCREENAI_CALL screenai_recognize_pixels(
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
) {
	if (!recognizer || !pixels || !out_utf8_text || !out_text_length) {
		SetError(error_message, error_capacity, "Recognizer, pixels, and output pointers are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}
	if (width == 0 || height == 0 || stride < static_cast<uint64_t>(width) * 4 ||
		static_cast<uint64_t>(stride) * height > pixel_buffer_size) {
		SetError(error_message, error_capacity, "Pixel dimensions, stride, or buffer size are invalid.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}
	if (pixel_format != SCREENAI_PIXEL_FORMAT_BGRA8 && pixel_format != SCREENAI_PIXEL_FORMAT_RGBA8) {
		SetError(error_message, error_capacity, "Pixel format must be BGRA8 or RGBA8.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*out_utf8_text = nullptr;
	*out_text_length = 0;

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		const auto result = recognizer->implementation->RecognizePixels(
			pixels,
			pixel_buffer_size,
			width,
			height,
			stride,
			pixel_format == SCREENAI_PIXEL_FORMAT_RGBA8
		);

		return CopyText(
			result.text,
			out_utf8_text,
			out_text_length,
			error_message,
			error_capacity
		);
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate the OCR result.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const screenai::image::DecodeError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_IMAGE_ERROR,
			error_message,
			error_capacity
		);
	} catch (const screenai::backend::OcrError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_OCR_ERROR,
			error_message,
			error_capacity
		);
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while processing the pixel buffer.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


void SCREENAI_CALL screenai_free_buffer(char* buffer) {
	delete[] buffer;
}

} // extern "C"

#ifndef SCREENAI_SCREENAI_HPP_
#define SCREENAI_SCREENAI_HPP_

#include "screenai/screenai.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace screenai {

class Error : public std::runtime_error {
public:
	explicit Error(const std::string& message)
		: std::runtime_error(message) {}
};

class Recognizer {
public:
	Recognizer() {
		char error[1024]{};

		const screenai_status status = screenai_create(
			&this->recognizer_,
			error,
			sizeof(error)
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "Could not initialize ScreenAI.");
	}

	~Recognizer() {
		screenai_destroy(this->recognizer_);
	}

	Recognizer(const Recognizer&) = delete;
	Recognizer& operator=(const Recognizer&) = delete;

	Recognizer(Recognizer&& other) noexcept
		: recognizer_(std::exchange(other.recognizer_, nullptr)) {}

	Recognizer& operator=(Recognizer&& other) noexcept {
		if (this != &other) {
			screenai_destroy(this->recognizer_);
			this->recognizer_ = std::exchange(other.recognizer_, nullptr);
		}

		return *this;
	}

	uint32_t GetMaxImageDimension() const {
		uint32_t dimension = 0;
		char error[1024]{};

		const screenai_status status = screenai_get_max_image_dimension(
			this->recognizer_,
			&dimension,
			error,
			sizeof(error)
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "Could not read the maximum image dimension.");

		return dimension;
	}

	std::string RecognizeFile(const std::wstring& image_path) const {
		char* text = nullptr;
		size_t length = 0;
		char error[1024]{};

		const screenai_status status = screenai_recognize_file(
			this->recognizer_,
			image_path.c_str(),
			&text,
			&length,
			error,
			sizeof(error)
		);

		std::unique_ptr<char, decltype(&screenai_free_buffer)> text_owner(
			text,
			&screenai_free_buffer
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "OCR failed.");

		return std::string(text_owner.get(), length);
	}

	std::string RecognizePixels(
		const uint8_t* pixels,
		size_t pixel_buffer_size,
		uint32_t width,
		uint32_t height,
		uint32_t stride,
		screenai_pixel_format pixel_format
	) const {
		char* text = nullptr;
		size_t length = 0;
		char error[1024]{};

		const screenai_status status = screenai_recognize_pixels(
			this->recognizer_,
			pixels,
			pixel_buffer_size,
			width,
			height,
			stride,
			pixel_format,
			&text,
			&length,
			error,
			sizeof(error)
		);

		std::unique_ptr<char, decltype(&screenai_free_buffer)> text_owner(
			text,
			&screenai_free_buffer
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "OCR failed.");

		return std::string(text_owner.get(), length);
	}

private:
	screenai_recognizer* recognizer_ = nullptr;
};

} // namespace screenai

#endif

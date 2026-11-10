#ifndef SCREENAI_SCREENAI_HPP_
#define SCREENAI_SCREENAI_HPP_

#include "screenai/ocr_result.hpp"
#include "screenai/screenai.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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
		return this->RecognizeFileResult(image_path).text;
	}

	OcrResult RecognizeFileResult(const std::wstring& image_path) const {
		screenai_ocr_result* raw_result = nullptr;
		char error[1024]{};

		const screenai_status status = screenai_recognize_file_result(
			this->recognizer_,
			image_path.c_str(),
			&raw_result,
			error,
			sizeof(error)
		);

		std::unique_ptr<screenai_ocr_result, decltype(&screenai_ocr_result_destroy)> result_owner(
			raw_result,
			&screenai_ocr_result_destroy
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "OCR failed.");

		const screenai_ocr_result_view* result_view = screenai_ocr_result_get_view(result_owner.get());

		if (!result_view)
			throw Error("Could not read the OCR result.");

		return this->CopyResult(*result_view);
	}

	std::string RecognizePixels(
		const uint8_t* pixels,
		size_t pixel_buffer_size,
		uint32_t width,
		uint32_t height,
		uint32_t stride,
		screenai_pixel_format pixel_format
	) const {
		return this->RecognizePixelsResult(
			pixels,
			pixel_buffer_size,
			width,
			height,
			stride,
			pixel_format
		).text;
	}

	OcrResult RecognizePixelsResult(
		const uint8_t* pixels,
		size_t pixel_buffer_size,
		uint32_t width,
		uint32_t height,
		uint32_t stride,
		screenai_pixel_format pixel_format
	) const {
		screenai_ocr_result* raw_result = nullptr;
		char error[1024]{};

		const screenai_status status = screenai_recognize_pixels_result(
			this->recognizer_,
			pixels,
			pixel_buffer_size,
			width,
			height,
			stride,
			pixel_format,
			&raw_result,
			error,
			sizeof(error)
		);

		std::unique_ptr<screenai_ocr_result, decltype(&screenai_ocr_result_destroy)> result_owner(
			raw_result,
			&screenai_ocr_result_destroy
		);

		if (status != SCREENAI_STATUS_OK)
			throw Error(error[0] ? error : "OCR failed.");

		const screenai_ocr_result_view* result_view = screenai_ocr_result_get_view(result_owner.get());

		if (!result_view)
			throw Error("Could not read the OCR result.");

		return this->CopyResult(*result_view);
	}

private:
	static std::string CopyString(const char* value, size_t length) {
		if (!value || length == 0)
			return {};

		return std::string(value, length);
	}

	static std::optional<BoundingBox> CopyBoundingBox(
		const screenai_bounding_box_view& value,
		int32_t is_present
	) {
		if (!is_present)
			return std::nullopt;

		return BoundingBox{
			value.x,
			value.y,
			value.width,
			value.height,
			value.angle,
		};
	}

	static OcrResult CopyResult(const screenai_ocr_result_view& source) {
		if (source.block_count > 0 && !source.blocks)
			throw Error("The OCR result contains an invalid block array.");

		OcrResult result;
		result.text = CopyString(source.text, source.text_length);
		result.image_width = source.image_width;
		result.image_height = source.image_height;

		for (size_t block_index = 0; block_index < source.block_count; ++block_index) {
			const screenai_block_view& source_block = source.blocks[block_index];
			OcrBlock block;

			if (source_block.line_count > 0 && !source_block.lines)
				throw Error("The OCR result contains an invalid line array.");

			for (size_t line_index = 0; line_index < source_block.line_count; ++line_index) {
				const screenai_line_view& source_line = source_block.lines[line_index];
				OcrLine line;
				line.text = CopyString(source_line.text, source_line.text_length);
				line.bounding_box = CopyBoundingBox(
					source_line.bounding_box,
					source_line.has_bounding_box
				);
				if (source_line.has_confidence)
					line.confidence = source_line.confidence;

				block.lines.push_back(std::move(line));
			}

			result.blocks.push_back(std::move(block));
		}

		return result;
	}

	screenai_recognizer* recognizer_ = nullptr;
};

} // namespace screenai

#endif

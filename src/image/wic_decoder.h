#ifndef SCREENAI_IMAGE_WIC_DECODER_H_
#define SCREENAI_IMAGE_WIC_DECODER_H_

#include "core/image.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace screenai::image {

class DecodeError : public std::runtime_error {
public:
	explicit DecodeError(const std::string& message)
		: std::runtime_error(message) {}
};

core::Image DecodeFile(const std::filesystem::path& image_path, uint32_t max_dimension);
core::Image NormalizePixels(
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	bool rgba,
	uint32_t max_dimension
);

} // namespace screenai::image

#endif

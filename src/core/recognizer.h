#ifndef SCREENAI_CORE_RECOGNIZER_H_
#define SCREENAI_CORE_RECOGNIZER_H_

#include "backend/chrome_screen_ai/chrome_backend.h"
#include "core/image.h"
#include "core/result.h"

#include <filesystem>

namespace screenai::core {

class Recognizer {
public:
	Recognizer();

	uint32_t GetMaxImageDimension() const;
	OcrResult RecognizeFile(const std::filesystem::path& image_path);
	OcrResult RecognizePixels(
		const uint8_t* pixels,
		size_t pixel_buffer_size,
		uint32_t width,
		uint32_t height,
		uint32_t stride,
		bool rgba
	);

private:
	backend::ChromeBackend backend_;
};

} // namespace screenai::core

#endif

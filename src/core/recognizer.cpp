#include "core/recognizer.h"

#include "image/wic_decoder.h"

namespace screenai::core {

Recognizer::Recognizer(const std::optional<std::filesystem::path>& runtime_directory)
	: backend_(runtime_directory) {}


uint32_t Recognizer::GetMaxImageDimension() const {
	return this->backend_.GetMaxImageDimension();
}


OcrResult Recognizer::RecognizeFile(const std::filesystem::path& image_path) {
	const Image image = image::DecodeFile(image_path, this->GetMaxImageDimension());

	return this->backend_.PerformOcr(image);
}


OcrResult Recognizer::RecognizePixels(
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	bool rgba
) {
	const Image image = image::NormalizePixels(
		pixels,
		pixel_buffer_size,
		width,
		height,
		stride,
		rgba,
		this->GetMaxImageDimension()
	);

	return this->backend_.PerformOcr(image);
}

} // namespace screenai::core

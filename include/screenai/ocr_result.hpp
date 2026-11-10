#ifndef SCREENAI_OCR_RESULT_HPP_
#define SCREENAI_OCR_RESULT_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace screenai {

struct BoundingBox {
	// Coordinates are in processed-image pixels; angle is clockwise in degrees.
	int32_t x = 0;
	int32_t y = 0;
	int32_t width = 0;
	int32_t height = 0;
	float angle = 0.0f;
};

struct OcrLine {
	std::string text;
	std::optional<BoundingBox> bounding_box;
	std::optional<float> confidence;
};

struct OcrBlock {
	std::vector<OcrLine> lines;
};

struct OcrResult {
	std::string text;
	uint32_t image_width = 0;
	uint32_t image_height = 0;
	std::vector<OcrBlock> blocks;
};

} // namespace screenai

#endif

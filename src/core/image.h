#ifndef SCREENAI_CORE_IMAGE_H_
#define SCREENAI_CORE_IMAGE_H_

#include <cstdint>
#include <vector>

namespace screenai::core {

struct Image {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> bgra;
};

} // namespace screenai::core

#endif

#include "backend/chrome_screen_ai/annotation_decoder.h"

#include <cstdint>

namespace screenai::backend {
namespace {

bool ReadVarint(std::string_view bytes, size_t& offset, uint64_t& value) {
	value = 0;

	for (unsigned index = 0; index < 10; ++index) {
		if (offset >= bytes.size())
			return false;

		const uint8_t byte = static_cast<uint8_t>(bytes[offset++]);
		if (index == 9 && byte > 1)
			return false;

		value |= static_cast<uint64_t>(byte & 0x7f) << (index * 7);
		if ((byte & 0x80) == 0)
			return true;
	}

	return false;
}


bool ReadLengthDelimited(
	std::string_view bytes,
	size_t& offset,
	std::string_view& value
) {
	uint64_t length = 0;

	if (!ReadVarint(bytes, offset, length) || length > bytes.size() - offset)
		return false;

	value = bytes.substr(offset, static_cast<size_t>(length));
	offset += static_cast<size_t>(length);

	return true;
}


bool SkipField(std::string_view bytes, size_t& offset, uint32_t wire_type) {
	uint64_t ignored = 0;

	switch (wire_type) {
	case 0:
		return ReadVarint(bytes, offset, ignored);
	case 1:
		if (bytes.size() - offset < 8)
			return false;

		offset += 8;

		return true;
	case 2: {
		std::string_view skipped;

		return ReadLengthDelimited(bytes, offset, skipped);
	}
	case 5:
		if (bytes.size() - offset < 4)
			return false;

		offset += 4;

		return true;
	default:
		return false;
	}
}


bool ParseLineBox(std::string_view bytes, std::string& text) {
	size_t offset = 0;

	while (offset < bytes.size()) {
		uint64_t tag = 0;

		if (!ReadVarint(bytes, offset, tag) || tag == 0)
			return false;

		const uint32_t field_number = static_cast<uint32_t>(tag >> 3);
		const uint32_t wire_type = static_cast<uint32_t>(tag & 7);

		if (field_number == 3 && wire_type == 2) {
			std::string_view line;

			if (!ReadLengthDelimited(bytes, offset, line))
				return false;
			if (!line.empty()) {
				if (!text.empty())
					text.push_back('\n');
				text.append(line.data(), line.size());
			}
		} else if (!SkipField(bytes, offset, wire_type)) {
			return false;
		}
	}

	return true;
}

} // namespace


bool ParseVisualAnnotation(std::string_view bytes, std::string& text) {
	text.clear();
	size_t offset = 0;

	while (offset < bytes.size()) {
		uint64_t tag = 0;

		if (!ReadVarint(bytes, offset, tag) || tag == 0)
			return false;

		const uint32_t field_number = static_cast<uint32_t>(tag >> 3);
		const uint32_t wire_type = static_cast<uint32_t>(tag & 7);

		if (field_number == 2 && wire_type == 2) {
			std::string_view line_box;

			if (!ReadLengthDelimited(bytes, offset, line_box) || !ParseLineBox(line_box, text))
				return false;
		} else if (!SkipField(bytes, offset, wire_type)) {
			return false;
		}
	}

	return true;
}

} // namespace screenai::backend

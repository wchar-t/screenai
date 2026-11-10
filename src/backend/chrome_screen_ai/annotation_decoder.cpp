#include "backend/chrome_screen_ai/annotation_decoder.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace screenai::backend {
namespace {

using screenai::BoundingBox;
using screenai::OcrBlock;
using screenai::OcrLine;

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


bool ReadInt32(std::string_view bytes, size_t& offset, int32_t& value) {
	uint64_t encoded_value = 0;

	if (!ReadVarint(bytes, offset, encoded_value))
		return false;

	const uint32_t value_bits = static_cast<uint32_t>(encoded_value);

	std::memcpy(&value, &value_bits, sizeof(value));

	return true;
}


bool ReadFloat(std::string_view bytes, size_t& offset, float& value) {
	if (offset > bytes.size() || bytes.size() - offset < sizeof(uint32_t))
		return false;

	uint32_t value_bits = 0;

	std::memcpy(&value_bits, bytes.data() + offset, sizeof(value_bits));
	std::memcpy(&value, &value_bits, sizeof(value));

	offset += sizeof(value_bits);

	return std::isfinite(value);
}


bool ReadLengthDelimited(
	std::string_view bytes,
	size_t& offset,
	std::string_view& value
) {
	uint64_t length = 0;

	if (offset > bytes.size() || !ReadVarint(bytes, offset, length) ||
		length > bytes.size() - offset) {
		return false;
	}

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
		if (offset > bytes.size() || bytes.size() - offset < 8)
			return false;

		offset += 8;

		return true;
	case 2: {
		std::string_view skipped;

		return ReadLengthDelimited(bytes, offset, skipped);
	}
	case 5:
		if (offset > bytes.size() || bytes.size() - offset < 4)
			return false;

		offset += 4;

		return true;
	default:
		return false;
	}
}


bool ParseBoundingBox(std::string_view bytes, BoundingBox& bounding_box) {
	size_t offset = 0;

	while (offset < bytes.size()) {
		uint64_t tag = 0;

		if (!ReadVarint(bytes, offset, tag) || tag == 0)
			return false;

		const uint32_t field_number = static_cast<uint32_t>(tag >> 3);
		const uint32_t wire_type = static_cast<uint32_t>(tag & 7);

		int32_t* coordinate = nullptr;

		if (wire_type == 0) {
			switch (field_number) {
			case 1:
				coordinate = &bounding_box.x;
				break;
			case 2:
				coordinate = &bounding_box.y;
				break;
			case 3:
				coordinate = &bounding_box.width;
				break;
			case 4:
				coordinate = &bounding_box.height;
				break;
			default:
				break;
			}
		}

		if (coordinate) {
			if (!ReadInt32(bytes, offset, *coordinate))
				return false;
		} else if (field_number == 5 && wire_type == 5) {
			if (!ReadFloat(bytes, offset, bounding_box.angle))
				return false;
		} else if (!SkipField(bytes, offset, wire_type)) {
			return false;
		}
	}

	return true;
}


bool ParseLineBox(
	std::string_view bytes,
	OcrLine& line,
	std::optional<int32_t>& block_id
) {
	size_t offset = 0;

	while (offset < bytes.size()) {
		uint64_t tag = 0;

		if (!ReadVarint(bytes, offset, tag) || tag == 0)
			return false;

		const uint32_t field_number = static_cast<uint32_t>(tag >> 3);
		const uint32_t wire_type = static_cast<uint32_t>(tag & 7);

		if (field_number == 2 && wire_type == 2) {
			std::string_view encoded_bounding_box;
			BoundingBox bounding_box;

			if (!ReadLengthDelimited(bytes, offset, encoded_bounding_box) ||
				!ParseBoundingBox(encoded_bounding_box, bounding_box)) {
				return false;
			}

			line.bounding_box = bounding_box;
		} else if (field_number == 3 && wire_type == 2) {
			std::string_view text;

			if (!ReadLengthDelimited(bytes, offset, text))
				return false;

			line.text.assign(text.data(), text.size());
		} else if (field_number == 5 && wire_type == 0) {
			int32_t value = 0;

			if (!ReadInt32(bytes, offset, value))
				return false;

			block_id = value;
		} else if (field_number == 10 && wire_type == 5) {
			float confidence = 0.0f;

			if (!ReadFloat(bytes, offset, confidence))
				return false;

			line.confidence = confidence;
		} else if (!SkipField(bytes, offset, wire_type)) {
			return false;
		}
	}

	return true;
}


void AddLineToResult(
	OcrLine line,
	std::optional<int32_t> block_id,
	core::OcrResult& result,
	std::unordered_map<int32_t, size_t>& block_indices
) {
	if (!line.text.empty()) {
		if (!result.text.empty())
			result.text.push_back('\n');

		result.text.append(line.text);
	}

	size_t block_index = result.blocks.size();

	if (block_id) {
		const auto found = block_indices.find(*block_id);

		if (found != block_indices.end()) {
			block_index = found->second;
		} else {
			block_indices.emplace(*block_id, block_index);
		}
	}

	if (block_index == result.blocks.size())
		result.blocks.push_back({});

	result.blocks[block_index].lines.push_back(std::move(line));
}

} // namespace


bool ParseVisualAnnotation(std::string_view bytes, core::OcrResult& result) {
	core::OcrResult parsed_result;
	std::unordered_map<int32_t, size_t> block_indices;
	size_t offset = 0;

	while (offset < bytes.size()) {
		uint64_t tag = 0;

		if (!ReadVarint(bytes, offset, tag) || tag == 0)
			return false;

		const uint32_t field_number = static_cast<uint32_t>(tag >> 3);
		const uint32_t wire_type = static_cast<uint32_t>(tag & 7);

		if (field_number == 2 && wire_type == 2) {
			std::string_view encoded_line;
			OcrLine line;
			std::optional<int32_t> block_id;

			if (!ReadLengthDelimited(bytes, offset, encoded_line) ||
				!ParseLineBox(encoded_line, line, block_id)) {
				return false;
			}

			AddLineToResult(std::move(line), block_id, parsed_result, block_indices);
		} else if (!SkipField(bytes, offset, wire_type)) {
			return false;
		}
	}

	result = std::move(parsed_result);

	return true;
}


bool ParseVisualAnnotation(std::string_view bytes, std::string& text) {
	core::OcrResult result;

	if (!ParseVisualAnnotation(bytes, result))
		return false;

	text = std::move(result.text);

	return true;
}

} // namespace screenai::backend

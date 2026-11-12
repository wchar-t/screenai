#ifndef SCREENAI_WORKER_PROTOCOL_H_
#define SCREENAI_WORKER_PROTOCOL_H_

#include "screenai/ocr_result.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace screenai::worker {

constexpr uint32_t MESSAGE_MAGIC = 0x53414957;
constexpr uint32_t MESSAGE_VERSION = 1;
constexpr uint64_t MAX_MESSAGE_SIZE = 64ULL * 1024 * 1024;

constexpr uint32_t MESSAGE_RECOGNIZE_FILE = 1;
constexpr uint32_t MESSAGE_SHUTDOWN = 2;
constexpr uint32_t MESSAGE_READY = 3;
constexpr uint32_t MESSAGE_RESULT = 4;
constexpr uint32_t MESSAGE_ERROR = 5;
constexpr uint32_t MESSAGE_STOPPED = 6;

struct MessageHeader {
	uint32_t magic = MESSAGE_MAGIC;
	uint32_t version = MESSAGE_VERSION;
	uint32_t type = 0;
	uint32_t status = 0;
	uint64_t payload_size = 0;
};


template <typename T>
void AppendValue(std::vector<uint8_t>& bytes, const T& value) {
	static_assert(std::is_trivially_copyable_v<T>);

	const auto* value_bytes = reinterpret_cast<const uint8_t*>(&value);
	bytes.insert(bytes.end(), value_bytes, value_bytes + sizeof(value));
}


inline void AppendString(std::vector<uint8_t>& bytes, const std::string& value) {
	const uint64_t length = value.size();
	AppendValue(bytes, length);
	bytes.insert(bytes.end(), value.begin(), value.end());
}


inline bool ReadBytes(
	const std::vector<uint8_t>& bytes,
	size_t& offset,
	void* output,
	size_t length
) {
	if (offset > bytes.size() || length > bytes.size() - offset)
		return false;

	if (length > 0)
		std::memcpy(output, bytes.data() + offset, length);

	offset += length;

	return true;
}


template <typename T>
bool ReadValue(const std::vector<uint8_t>& bytes, size_t& offset, T& value) {
	static_assert(std::is_trivially_copyable_v<T>);

	return ReadBytes(bytes, offset, &value, sizeof(value));
}


inline bool ReadString(const std::vector<uint8_t>& bytes, size_t& offset, std::string& value) {
	uint64_t length = 0;

	if (!ReadValue(bytes, offset, length) || length > bytes.size() - offset)
		return false;

	value.assign(
		reinterpret_cast<const char*>(bytes.data() + offset),
		static_cast<size_t>(length)
	);
	offset += static_cast<size_t>(length);

	return true;
}


inline std::vector<uint8_t> SerializeResult(const OcrResult& result) {
	std::vector<uint8_t> bytes;
	AppendValue(bytes, result.image_width);
	AppendValue(bytes, result.image_height);
	AppendString(bytes, result.text);

	const uint64_t block_count = result.blocks.size();
	AppendValue(bytes, block_count);

	for (const OcrBlock& block : result.blocks) {
		const uint64_t line_count = block.lines.size();
		AppendValue(bytes, line_count);

		for (const OcrLine& line : block.lines) {
			AppendString(bytes, line.text);

			const uint32_t has_bounding_box = line.bounding_box.has_value();
			AppendValue(bytes, has_bounding_box);

			if (line.bounding_box) {
				AppendValue(bytes, line.bounding_box->x);
				AppendValue(bytes, line.bounding_box->y);
				AppendValue(bytes, line.bounding_box->width);
				AppendValue(bytes, line.bounding_box->height);
				AppendValue(bytes, line.bounding_box->angle);
			}

			const uint32_t has_confidence = line.confidence.has_value();
			AppendValue(bytes, has_confidence);

			if (line.confidence)
				AppendValue(bytes, *line.confidence);
		}
	}

	if (bytes.size() > MAX_MESSAGE_SIZE)
		throw std::length_error("The worker OCR result is too large.");

	return bytes;
}


inline bool DeserializeResult(const std::vector<uint8_t>& bytes, OcrResult& result) {
	if (bytes.size() > MAX_MESSAGE_SIZE)
		return false;

	size_t offset = 0;
	OcrResult decoded;
	uint64_t block_count = 0;

	if (!ReadValue(bytes, offset, decoded.image_width) ||
		!ReadValue(bytes, offset, decoded.image_height) ||
		!ReadString(bytes, offset, decoded.text) ||
		!ReadValue(bytes, offset, block_count) ||
		block_count > bytes.size() / sizeof(uint64_t)) {
		return false;
	}

	decoded.blocks.reserve(static_cast<size_t>(block_count));

	for (uint64_t block_index = 0; block_index < block_count; ++block_index) {
		uint64_t line_count = 0;
		OcrBlock block;

		if (!ReadValue(bytes, offset, line_count) ||
			line_count > bytes.size() / sizeof(uint64_t)) {
			return false;
		}

		block.lines.reserve(static_cast<size_t>(line_count));

		for (uint64_t line_index = 0; line_index < line_count; ++line_index) {
			OcrLine line;
			uint32_t has_bounding_box = 0;
			uint32_t has_confidence = 0;

			if (!ReadString(bytes, offset, line.text) ||
				!ReadValue(bytes, offset, has_bounding_box) || has_bounding_box > 1) {
				return false;
			}

			if (has_bounding_box) {
				BoundingBox bounding_box;

				if (!ReadValue(bytes, offset, bounding_box.x) ||
					!ReadValue(bytes, offset, bounding_box.y) ||
					!ReadValue(bytes, offset, bounding_box.width) ||
					!ReadValue(bytes, offset, bounding_box.height) ||
					!ReadValue(bytes, offset, bounding_box.angle)) {
					return false;
				}

				line.bounding_box = bounding_box;
			}

			if (!ReadValue(bytes, offset, has_confidence) || has_confidence > 1)
				return false;

			if (has_confidence) {
				float confidence = 0.0f;

				if (!ReadValue(bytes, offset, confidence))
					return false;

				line.confidence = confidence;
			}

			block.lines.push_back(std::move(line));
		}

		decoded.blocks.push_back(std::move(block));
	}

	if (offset != bytes.size())
		return false;

	result = std::move(decoded);

	return true;
}

} // namespace screenai::worker

#endif

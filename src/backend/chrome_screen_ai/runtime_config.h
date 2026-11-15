#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_RUNTIME_CONFIG_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_RUNTIME_CONFIG_H_

// Compile-time tuning for the bundled ScreenAI recognizer.
//
// The two values are written into the recognizer config protobuf
// (gocr_mobile_und_config.pb) before chrome_screen_ai.dll parses it.  They do
// not change the model or the output; they only control how the TFLite
// interpreter schedules the work.
//
// SCREENAI_INTERPRETER_THREADS:
//   Threads used by the TFLite/XNNPACK interpreter inside each process.
//   Higher values reduce single-image latency by using more cores, at the cost
//   of more total CPU per image.  The native default is one thread.  On an
//   8-core/16-thread CPU, 4-8 is the useful range.
//
// SCREENAI_CHUNKS_PER_BATCH:
//   Number of 32x168 line chunks packed into one recognizer invocation.
//   Higher values amortize the per-invocation overhead; too high increases
//   peak memory and can reduce throughput.  The native default is 8.  On the
//   21-page test corpus, 32-64 was best and 96+ was slower.
//
// These are intentionally compile-time constants for now.  They can become
// runtime options once the best defaults are settled.

#define SCREENAI_INTERPRETER_THREADS 6
#define SCREENAI_CHUNKS_PER_BATCH 64

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace screenai::backend::runtime_config {
namespace detail {

struct Field {
	uint32_t number = 0;
	uint8_t wire = 0;
	std::vector<uint8_t> raw;
	std::vector<uint8_t> value;
};


inline uint64_t ReadVarint(const std::vector<uint8_t>& data, size_t& offset) {
	uint64_t value = 0;
	unsigned shift = 0;

	while (true) {
		if (offset >= data.size() || shift >= 64)
			throw std::runtime_error("Invalid ScreenAI config varint.");

		const uint8_t byte = data[offset++];
		value |= static_cast<uint64_t>(byte & 0x7f) << shift;

		if ((byte & 0x80) == 0)
			return value;

		shift += 7;
	}
}


inline void AppendVarint(std::vector<uint8_t>& output, uint64_t value) {
	while (true) {
		uint8_t byte = static_cast<uint8_t>(value & 0x7f);
		value >>= 7;

		if (value != 0) {
			output.push_back(static_cast<uint8_t>(byte | 0x80));
		} else {
			output.push_back(byte);

			return;
		}
	}
}


inline std::vector<Field> ParseFields(const std::vector<uint8_t>& data) {
	std::vector<Field> fields;
	size_t offset = 0;

	while (offset < data.size()) {
		const size_t start = offset;
		const uint64_t key = ReadVarint(data, offset);
		Field field;
		field.number = static_cast<uint32_t>(key >> 3);
		field.wire = static_cast<uint8_t>(key & 7);

		if (field.wire == 0) {
			ReadVarint(data, offset);
		} else if (field.wire == 2) {
			const uint64_t length = ReadVarint(data, offset);

			if (length > data.size() - offset)
				throw std::runtime_error("Truncated ScreenAI config field.");

			field.value.assign(
				data.begin() + static_cast<std::ptrdiff_t>(offset),
				data.begin() + static_cast<std::ptrdiff_t>(offset + length)
			);
			offset += static_cast<size_t>(length);
		} else if (field.wire == 5) {
			if (data.size() - offset < 4)
				throw std::runtime_error("Truncated ScreenAI config fixed32 field.");

			offset += 4;
		} else if (field.wire == 1) {
			if (data.size() - offset < 8)
				throw std::runtime_error("Truncated ScreenAI config fixed64 field.");

			offset += 8;
		} else {
			throw std::runtime_error("Unsupported ScreenAI config wire type.");
		}

		field.raw.assign(
			data.begin() + static_cast<std::ptrdiff_t>(start),
			data.begin() + static_cast<std::ptrdiff_t>(offset)
		);
		fields.push_back(std::move(field));
	}

	return fields;
}


inline std::vector<uint8_t> EncodeFields(const std::vector<Field>& fields) {
	std::vector<uint8_t> output;

	for (const Field& field : fields)
		output.insert(output.end(), field.raw.begin(), field.raw.end());

	return output;
}


inline std::vector<uint8_t> WrapField(uint32_t number, const std::vector<uint8_t>& payload) {
	std::vector<uint8_t> output;

	AppendVarint(output, (static_cast<uint64_t>(number) << 3) | 2);
	AppendVarint(output, payload.size());
	output.insert(output.end(), payload.begin(), payload.end());

	return output;
}


inline std::vector<uint8_t> ReplaceField(
	const std::vector<Field>& fields,
	uint32_t number,
	const std::vector<uint8_t>& replacement
) {
	std::vector<uint8_t> output;

	for (const Field& field : fields) {
		if (field.number == number)
			output.insert(output.end(), replacement.begin(), replacement.end());
		else
			output.insert(output.end(), field.raw.begin(), field.raw.end());
	}

	return output;
}


inline std::vector<uint8_t> SetVarint(
	std::vector<Field> fields,
	uint32_t number,
	uint32_t value
) {
	std::vector<uint8_t> encoded;

	AppendVarint(encoded, (static_cast<uint64_t>(number) << 3) | 0);
	AppendVarint(encoded, value);

	for (Field& field : fields) {
		if (field.number == number) {
			field.raw = encoded;

			return EncodeFields(fields);
		}
	}

	std::vector<uint8_t> output = EncodeFields(fields);
	output.insert(output.end(), encoded.begin(), encoded.end());

	return output;
}


inline const std::vector<uint8_t>& FindField(
	const std::vector<Field>& fields,
	uint32_t number
) {
	for (const Field& field : fields) {
		if (field.number == number)
			return field.value;
	}

	throw std::runtime_error("Missing ScreenAI config field.");
}

} // namespace detail


// Rewrites the two scheduling fields in gocr_mobile_und_config.pb:
//   TfliteModelPooledRunnerConfig.interpreter_num_threads (field 3)
//   GocrLineRecognizerConfig.num_chunk_per_batch          (field 47)
inline std::vector<uint8_t> PatchRecognizerConfig(
	const uint8_t* data,
	size_t size,
	uint32_t interpreter_threads,
	uint32_t chunks_per_batch
) {
	using namespace detail;

	const std::vector<uint8_t> config(data, data + size);
	const std::vector<Field> top = ParseFields(config);
	const std::vector<Field> any = ParseFields(FindField(top, 2));
	const std::vector<Field> recognizer = ParseFields(FindField(any, 2));
	const std::vector<Field> model_runner = ParseFields(FindField(recognizer, 13));
	const std::vector<Field> pooled_runner = ParseFields(FindField(model_runner, 3));

	const std::vector<uint8_t> pooled_runner_bytes = SetVarint(
		pooled_runner,
		3,
		interpreter_threads
	);
	const std::vector<uint8_t> model_runner_bytes = ReplaceField(
		model_runner,
		3,
		WrapField(3, pooled_runner_bytes)
	);
	std::vector<uint8_t> recognizer_bytes = ReplaceField(
		recognizer,
		13,
		WrapField(13, model_runner_bytes)
	);

	recognizer_bytes = SetVarint(
		ParseFields(recognizer_bytes),
		47,
		chunks_per_batch
	);

	const std::vector<uint8_t> any_bytes = ReplaceField(
		any,
		2,
		WrapField(2, recognizer_bytes)
	);

	return ReplaceField(top, 2, WrapField(2, any_bytes));
}

} // namespace screenai::backend::runtime_config

#endif // SCREENAI_BACKEND_CHROME_SCREEN_AI_RUNTIME_CONFIG_H_

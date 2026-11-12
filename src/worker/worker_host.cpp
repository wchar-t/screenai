#include "worker/worker_host.h"

#include "screenai/screenai.hpp"
#include "worker/protocol.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {

bool ReadExact(HANDLE pipe, void* data, size_t size) {
	auto* bytes = static_cast<uint8_t*>(data);
	size_t offset = 0;

	while (offset < size) {
		const DWORD request_size = static_cast<DWORD>(
			(size - offset) < 64 * 1024 ? size - offset : 64 * 1024
		);
		DWORD bytes_read = 0;

		if (!ReadFile(pipe, bytes + offset, request_size, &bytes_read, nullptr) ||
			bytes_read == 0) {
			return false;
		}

		offset += bytes_read;
	}

	return true;
}


bool WriteExact(HANDLE pipe, const void* data, size_t size) {
	const auto* bytes = static_cast<const uint8_t*>(data);
	size_t offset = 0;

	while (offset < size) {
		const DWORD request_size = static_cast<DWORD>(
			(size - offset) < 64 * 1024 ? size - offset : 64 * 1024
		);
		DWORD bytes_written = 0;

		if (!WriteFile(pipe, bytes + offset, request_size, &bytes_written, nullptr) ||
			bytes_written == 0) {
			return false;
		}

		offset += bytes_written;
	}

	return true;
}


bool WriteMessage(
	HANDLE pipe,
	uint32_t type,
	screenai_status status,
	const void* payload,
	size_t payload_size
) {
	if (payload_size > screenai::worker::MAX_MESSAGE_SIZE)
		return false;

	screenai::worker::MessageHeader header{};
	header.type = type;
	header.status = static_cast<uint32_t>(status);
	header.payload_size = payload_size;

	if (!WriteExact(pipe, &header, sizeof(header)))
		return false;

	return payload_size == 0 || WriteExact(pipe, payload, payload_size);
}


bool ReadMessage(
	HANDLE pipe,
	screenai::worker::MessageHeader& header,
	std::vector<uint8_t>& payload
) {
	if (!ReadExact(pipe, &header, sizeof(header)))
		return false;

	if (header.magic != screenai::worker::MESSAGE_MAGIC ||
		header.version != screenai::worker::MESSAGE_VERSION ||
		header.payload_size > screenai::worker::MAX_MESSAGE_SIZE) {
		return false;
	}

	payload.resize(static_cast<size_t>(header.payload_size));

	return payload.empty() || ReadExact(pipe, payload.data(), payload.size());
}


bool WriteError(HANDLE output, screenai_status status, const std::string& message) {
	return WriteMessage(
		output,
		screenai::worker::MESSAGE_ERROR,
		status,
		message.data(),
		message.size()
	);
}

} // namespace


int RunScreenAIWorker() {
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	std::unique_ptr<screenai::Recognizer> recognizer;

	try {
		recognizer = std::make_unique<screenai::Recognizer>();
	} catch (const screenai::Error& error) {
		WriteError(output, error.GetStatus(), error.what());

		return 2;
	} catch (const std::bad_alloc&) {
		WriteError(output, SCREENAI_STATUS_OUT_OF_MEMORY, "Could not allocate a batch worker.");

		return 2;
	} catch (const std::exception& error) {
		WriteError(output, SCREENAI_STATUS_RUNTIME_ERROR, error.what());

		return 2;
	} catch (...) {
		WriteError(output, SCREENAI_STATUS_INTERNAL_ERROR, "Unknown batch worker startup error.");

		return 2;
	}

	if (!WriteMessage(output, screenai::worker::MESSAGE_READY, SCREENAI_STATUS_OK, nullptr, 0))
		return 2;

	while (true) {
		screenai::worker::MessageHeader request{};
		std::vector<uint8_t> payload;

		if (!ReadMessage(input, request, payload))
			return 0;

		if (request.type == screenai::worker::MESSAGE_SHUTDOWN) {
			WriteMessage(output, screenai::worker::MESSAGE_STOPPED, SCREENAI_STATUS_OK, nullptr, 0);

			return 0;
		}

		if (request.type != screenai::worker::MESSAGE_RECOGNIZE_FILE ||
			payload.empty() || payload.size() % sizeof(wchar_t) != 0) {
			WriteError(output, SCREENAI_STATUS_INVALID_ARGUMENT, "The worker request is invalid.");

			continue;
		}

		std::wstring image_path(payload.size() / sizeof(wchar_t), L'\0');
		std::memcpy(image_path.data(), payload.data(), payload.size());

		try {
			const screenai::OcrResult result = recognizer->Recognize(image_path);
			const std::vector<uint8_t> result_bytes = screenai::worker::SerializeResult(result);

			if (!WriteMessage(
				output,
				screenai::worker::MESSAGE_RESULT,
				SCREENAI_STATUS_OK,
				result_bytes.data(),
				result_bytes.size()
			)) {
				return 2;
			}
		} catch (const screenai::Error& error) {
			if (!WriteError(output, error.GetStatus(), error.what()))
				return 2;
		} catch (const std::bad_alloc&) {
			if (!WriteError(output, SCREENAI_STATUS_OUT_OF_MEMORY, "Could not allocate the OCR result."))
				return 2;
		} catch (const std::exception& error) {
			if (!WriteError(output, SCREENAI_STATUS_INTERNAL_ERROR, error.what()))
				return 2;
		} catch (...) {
			if (!WriteError(output, SCREENAI_STATUS_INTERNAL_ERROR, "Unknown OCR worker error."))
				return 2;
		}
	}
}

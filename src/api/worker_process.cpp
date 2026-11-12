#include "api/worker_process.h"

#include "worker/protocol.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace screenai::api {
namespace {

constexpr size_t PIPE_BUFFER_SIZE = 64 * 1024;

const char MODULE_ANCHOR = 0;


void CloseHandleIfOpen(HANDLE& handle) noexcept {
	if (handle && handle != INVALID_HANDLE_VALUE) {
		CloseHandle(handle);
		handle = nullptr;
	}
}


std::string GetWin32Error(const char* operation) {
	return std::string(operation) + " failed (Win32 error " +
		std::to_string(GetLastError()) + ").";
}


std::wstring GetWorkerExecutablePath() {
	HMODULE module = nullptr;

	if (!GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&MODULE_ANCHOR),
		&module
	)) {
		throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, GetWin32Error("GetModuleHandleExW"));
	}

	std::vector<wchar_t> module_path(32768);
	const DWORD path_length = GetModuleFileNameW(
		module,
		module_path.data(),
		static_cast<DWORD>(module_path.size())
	);

	if (path_length == 0 || static_cast<size_t>(path_length) >= module_path.size())
		throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, GetWin32Error("GetModuleFileNameW"));

	const std::filesystem::path worker_path =
		std::filesystem::path(module_path.data()).parent_path() / L"screenai_worker.exe";

	if (GetFileAttributesW(worker_path.c_str()) == INVALID_FILE_ATTRIBUTES)
		throw WorkerError(
			SCREENAI_STATUS_RUNTIME_ERROR,
			"The batch worker executable is missing beside screenai.dll."
		);

	return worker_path.wstring();
}


void WriteExact(HANDLE pipe, const void* data, size_t size) {
	const auto* bytes = static_cast<const uint8_t*>(data);
	size_t offset = 0;

	while (offset < size) {
		const DWORD request_size = static_cast<DWORD>(
			(size - offset) < PIPE_BUFFER_SIZE ? size - offset : PIPE_BUFFER_SIZE
		);
		DWORD bytes_written = 0;

		if (!WriteFile(pipe, bytes + offset, request_size, &bytes_written, nullptr) ||
			bytes_written == 0) {
			throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, GetWin32Error("Writing to a batch worker"));
		}

		offset += bytes_written;
	}
}


bool ReadExact(HANDLE pipe, void* data, size_t size) {
	auto* bytes = static_cast<uint8_t*>(data);
	size_t offset = 0;

	while (offset < size) {
		const DWORD request_size = static_cast<DWORD>(
			(size - offset) < PIPE_BUFFER_SIZE ? size - offset : PIPE_BUFFER_SIZE
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


void WriteMessage(
	HANDLE pipe,
	uint32_t type,
	screenai_status status,
	const void* payload,
	size_t payload_size
) {
	if (payload_size > screenai::worker::MAX_MESSAGE_SIZE)
		throw WorkerError(SCREENAI_STATUS_INTERNAL_ERROR, "A batch worker message is too large.");

	screenai::worker::MessageHeader header{};
	header.type = type;
	header.status = static_cast<uint32_t>(status);
	header.payload_size = payload_size;

	WriteExact(pipe, &header, sizeof(header));

	if (payload_size > 0)
		WriteExact(pipe, payload, payload_size);
}


screenai::worker::MessageHeader ReadMessage(HANDLE pipe, std::vector<uint8_t>& payload) {
	screenai::worker::MessageHeader header{};

	if (!ReadExact(pipe, &header, sizeof(header)))
		throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, "The batch worker closed its response pipe.");

	if (header.magic != screenai::worker::MESSAGE_MAGIC ||
		header.version != screenai::worker::MESSAGE_VERSION ||
		header.payload_size > screenai::worker::MAX_MESSAGE_SIZE) {
		throw WorkerError(SCREENAI_STATUS_INTERNAL_ERROR, "The batch worker sent an invalid response.");
	}

	payload.resize(static_cast<size_t>(header.payload_size));

	if (!payload.empty() && !ReadExact(pipe, payload.data(), payload.size()))
		throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, "The batch worker response was incomplete.");

	return header;
}


std::string DecodeMessage(const std::vector<uint8_t>& payload) {
	return std::string(payload.begin(), payload.end());
}

} // namespace


WorkerProcess::WorkerProcess() {
	SECURITY_ATTRIBUTES security_attributes{};
	security_attributes.nLength = sizeof(security_attributes);
	security_attributes.bInheritHandle = TRUE;

	HANDLE child_input = nullptr;
	HANDLE parent_input = nullptr;
	HANDLE parent_output = nullptr;
	HANDLE child_output = nullptr;
	HANDLE child_error = nullptr;
	PROCESS_INFORMATION process_information{};

	try {
		if (!CreatePipe(&child_input, &parent_input, &security_attributes, 0) ||
			!SetHandleInformation(parent_input, HANDLE_FLAG_INHERIT, 0)) {
			throw WorkerError(
				SCREENAI_STATUS_RUNTIME_ERROR,
				GetWin32Error("Creating the worker input pipe")
			);
		}

		if (!CreatePipe(&parent_output, &child_output, &security_attributes, 0) ||
			!SetHandleInformation(parent_output, HANDLE_FLAG_INHERIT, 0)) {
			throw WorkerError(
				SCREENAI_STATUS_RUNTIME_ERROR,
				GetWin32Error("Creating the worker output pipe")
			);
		}

		child_error = CreateFileW(
			L"NUL",
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			&security_attributes,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (!child_error || child_error == INVALID_HANDLE_VALUE)
			throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, GetWin32Error("Opening NUL for the worker"));

		const std::wstring executable_path = GetWorkerExecutablePath();
		std::wstring command_line = L"\"" + executable_path + L"\"";
		STARTUPINFOW startup_info{};
		startup_info.cb = sizeof(startup_info);
		startup_info.dwFlags = STARTF_USESTDHANDLES;
		startup_info.hStdInput = child_input;
		startup_info.hStdOutput = child_output;
		startup_info.hStdError = child_error;

		if (!CreateProcessW(
			executable_path.c_str(),
			command_line.data(),
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			std::filesystem::path(executable_path).parent_path().c_str(),
			&startup_info,
			&process_information
		)) {
			throw WorkerError(SCREENAI_STATUS_RUNTIME_ERROR, GetWin32Error("Starting screenai_worker.exe"));
		}

		CloseHandleIfOpen(child_input);
		CloseHandleIfOpen(child_output);
		CloseHandleIfOpen(child_error);
		CloseHandleIfOpen(process_information.hThread);

		this->process_ = process_information.hProcess;
		process_information.hProcess = nullptr;
		this->input_ = parent_input;
		this->output_ = parent_output;
		parent_input = nullptr;
		parent_output = nullptr;

		std::vector<uint8_t> payload;
		const screenai::worker::MessageHeader response = ReadMessage(this->output_, payload);

		if (response.type == screenai::worker::MESSAGE_ERROR) {
			throw WorkerError(
				static_cast<screenai_status>(response.status),
				DecodeMessage(payload)
			);
		}

		if (response.type != screenai::worker::MESSAGE_READY)
			throw WorkerError(SCREENAI_STATUS_INTERNAL_ERROR, "The batch worker did not become ready.");

		this->ready_ = true;
	} catch (...) {
		CloseHandleIfOpen(child_input);
		CloseHandleIfOpen(parent_input);
		CloseHandleIfOpen(parent_output);
		CloseHandleIfOpen(child_output);
		CloseHandleIfOpen(child_error);
		CloseHandleIfOpen(process_information.hThread);
		CloseHandleIfOpen(process_information.hProcess);
		this->Shutdown();
		throw;
	}
}


WorkerProcess::~WorkerProcess() {
	this->Shutdown();
}


screenai::OcrResult WorkerProcess::RecognizeFile(const std::wstring& image_path) {
	if (image_path.empty())
		throw WorkerError(SCREENAI_STATUS_INVALID_ARGUMENT, "The image path is empty.");

	const size_t path_size = image_path.size() * sizeof(wchar_t);
	if (path_size > screenai::worker::MAX_MESSAGE_SIZE)
		throw WorkerError(SCREENAI_STATUS_INVALID_ARGUMENT, "The image path is too long.");

	std::lock_guard<std::mutex> lock(this->mutex_);
	WriteMessage(
		this->input_,
		screenai::worker::MESSAGE_RECOGNIZE_FILE,
		SCREENAI_STATUS_OK,
		image_path.data(),
		path_size
	);

	std::vector<uint8_t> payload;
	const screenai::worker::MessageHeader response = ReadMessage(this->output_, payload);

	if (response.type == screenai::worker::MESSAGE_ERROR) {
		throw WorkerError(
			static_cast<screenai_status>(response.status),
			DecodeMessage(payload)
		);
	}

	if (response.type != screenai::worker::MESSAGE_RESULT)
		throw WorkerError(
			SCREENAI_STATUS_INTERNAL_ERROR,
			"The batch worker sent an unexpected response."
		);

	screenai::OcrResult result;
	if (!screenai::worker::DeserializeResult(payload, result))
		throw WorkerError(
			SCREENAI_STATUS_INTERNAL_ERROR,
			"The batch worker sent an invalid OCR result."
		);

	return result;
}


void WorkerProcess::Shutdown() noexcept {
	if (this->input_ && this->ready_) {
		try {
			WriteMessage(
				this->input_,
				screenai::worker::MESSAGE_SHUTDOWN,
				SCREENAI_STATUS_OK,
				nullptr,
				0
			);
		} catch (...) {
		}
	}

	this->ready_ = false;
	CloseHandleIfOpen(this->input_);
	CloseHandleIfOpen(this->output_);

	if (this->process_) {
		const DWORD wait_result = WaitForSingleObject(this->process_, 10000);

		if (wait_result == WAIT_TIMEOUT) {
			TerminateProcess(this->process_, 1);
			WaitForSingleObject(this->process_, 5000);
		}

		CloseHandleIfOpen(this->process_);
	}
}

} // namespace screenai::api

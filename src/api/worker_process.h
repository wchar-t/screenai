#ifndef SCREENAI_API_WORKER_PROCESS_H_
#define SCREENAI_API_WORKER_PROCESS_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "screenai/ocr_result.hpp"
#include "screenai/screenai.h"

#include <mutex>
#include <stdexcept>
#include <string>

namespace screenai::api {

class WorkerError : public std::runtime_error {
public:
	WorkerError(screenai_status status, const std::string& message)
		: std::runtime_error(message), status_(status) {}

	screenai_status GetStatus() const noexcept {
		return this->status_;
	}

private:
	screenai_status status_;
};


class WorkerProcess {
public:
	WorkerProcess();
	~WorkerProcess();

	WorkerProcess(const WorkerProcess&) = delete;
	WorkerProcess& operator=(const WorkerProcess&) = delete;

	screenai::OcrResult RecognizeFile(const std::wstring& image_path);

private:
	void Shutdown() noexcept;

	HANDLE process_ = nullptr;
	HANDLE input_ = nullptr;
	HANDLE output_ = nullptr;
	bool ready_ = false;
	std::mutex mutex_;
};

} // namespace screenai::api

#endif

#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_CHROME_BACKEND_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_CHROME_BACKEND_H_

#include "core/image.h"
#include "core/result.h"

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace screenai::backend {

class OcrError : public std::runtime_error {
public:
	explicit OcrError(const std::string& message)
		: std::runtime_error(message) {}
};

class ChromeBackend {
public:
	explicit ChromeBackend(const std::optional<std::filesystem::path>& runtime_directory);
	~ChromeBackend();

	ChromeBackend(const ChromeBackend&) = delete;
	ChromeBackend& operator=(const ChromeBackend&) = delete;

	uint32_t GetMaxImageDimension() const;
	core::OcrResult PerformOcr(const core::Image& image);

private:
	void Release() noexcept;

	HMODULE library_ = nullptr;
	struct State;
	State* state_ = nullptr;
};

} // namespace screenai::backend

#endif

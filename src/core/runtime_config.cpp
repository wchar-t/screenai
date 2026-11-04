#include "core/runtime_config.h"

#include <Windows.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace screenai::core {

std::filesystem::path ResolveRuntimeDirectory(const wchar_t* configured_directory) {
	std::filesystem::path directory;

	if (configured_directory && *configured_directory) {
		directory = configured_directory;
	} else {
		const DWORD required = GetEnvironmentVariableW(L"SCREENAI_RUNTIME_DIR", nullptr, 0);

		if (required == 0)
			throw std::runtime_error("Set SCREENAI_RUNTIME_DIR or pass a runtime directory.");

		std::vector<wchar_t> value(required);
		const DWORD length = GetEnvironmentVariableW(
			L"SCREENAI_RUNTIME_DIR",
			value.data(),
			static_cast<DWORD>(value.size())
		);

		if (length == 0 || length >= value.size())
			throw std::runtime_error("Could not read SCREENAI_RUNTIME_DIR.");

		directory = std::wstring(value.data(), length);
	}

	std::error_code error;
	directory = std::filesystem::absolute(directory, error);

	if (error)
		throw std::runtime_error("Could not resolve the ScreenAI runtime directory: " + error.message());
	if (!std::filesystem::is_directory(directory, error) || error)
		throw std::runtime_error("The ScreenAI runtime path is not a directory.");

	const std::filesystem::path library = directory / L"chrome_screen_ai.dll";

	if (!std::filesystem::is_regular_file(library, error) || error)
		throw std::runtime_error("The runtime directory does not contain chrome_screen_ai.dll.");

	return directory;
}

} // namespace screenai::core

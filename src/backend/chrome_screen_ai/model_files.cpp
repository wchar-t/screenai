#include "backend/chrome_screen_ai/model_files.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>

namespace screenai::backend::model_files {
namespace {

std::mutex g_mutex;
std::filesystem::path g_runtime_directory;
std::string g_callback_error;
bool g_registered = false;

void RecordError(const std::string& message) {
	std::lock_guard<std::mutex> lock(g_mutex);

	if (g_callback_error.empty())
		g_callback_error = message;
}


std::filesystem::path ResolveModelPath(const char* name) {
	if (!name || !*name)
		throw std::runtime_error("ScreenAI requested an empty model filename.");

	const std::filesystem::path relative = std::filesystem::u8path(name);
	if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
		throw std::runtime_error("ScreenAI requested an absolute model path.");

	for (const auto& part : relative) {
		if (part == L"..")
			throw std::runtime_error("ScreenAI requested a model path outside its runtime directory.");
	}

	std::lock_guard<std::mutex> lock(g_mutex);

	if (!g_registered)
		throw std::runtime_error("No ScreenAI runtime directory is registered.");

	return g_runtime_directory / relative;
}

} // namespace


bool RegisterRuntimeDirectory(const std::filesystem::path& directory) {
	std::lock_guard<std::mutex> lock(g_mutex);

	if (g_registered)
		return false;

	g_runtime_directory = directory;
	g_callback_error.clear();
	g_registered = true;

	return true;
}


void UnregisterRuntimeDirectory() {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_runtime_directory.clear();
	g_callback_error.clear();
	g_registered = false;
}


void ClearCallbackError() {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_callback_error.clear();
}


std::string GetCallbackError() {
	std::lock_guard<std::mutex> lock(g_mutex);

	return g_callback_error;
}


uint32_t GetModelSize(const char* name) {
	try {
		const std::filesystem::path path = ResolveModelPath(name);
		std::error_code error;
		const uintmax_t size = std::filesystem::file_size(path, error);

		if (error || size == 0 || size > std::numeric_limits<uint32_t>::max()) {
			throw std::runtime_error(
				"ScreenAI model file is missing, empty, or too large: " +
				std::string(name ? name : "(null)")
			);
		}

		return static_cast<uint32_t>(size);
	} catch (const std::exception& error) {
		RecordError(error.what());

		return 0;
	}
}


void ReadModel(const char* name, uint32_t capacity, char* output) {
	if (!output) {
		RecordError("ScreenAI supplied a null model buffer.");

		return;
	}

	std::memset(output, 0, capacity);

	try {
		const std::filesystem::path path = ResolveModelPath(name);
		std::ifstream file(path, std::ios::binary | std::ios::ate);

		if (!file)
			throw std::runtime_error(
				"Could not open ScreenAI model file: " +
				std::string(name ? name : "(null)")
			);

		const std::streamoff length = file.tellg();

		if (length <= 0 || static_cast<uint64_t>(length) > capacity)
			throw std::runtime_error("ScreenAI model file size did not match its requested buffer.");

		file.seekg(0);
		file.read(output, length);

		if (!file)
			throw std::runtime_error(
				"Could not read ScreenAI model file: " +
				std::string(name ? name : "(null)")
			);
	} catch (const std::exception& error) {
		RecordError(error.what());
	}
}

} // namespace screenai::backend::model_files

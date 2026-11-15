#include "backend/chrome_screen_ai/model_files.h"

#include "backend/chrome_screen_ai/runtime_config.h"
#include "backend/chrome_screen_ai/runtime_package.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace screenai::backend::model_files {
namespace {

std::mutex g_mutex;
std::shared_ptr<const runtime_package::Package> g_runtime_package;
std::string g_callback_error;
bool g_registered = false;

void RecordError(const std::string& message) {
	std::lock_guard<std::mutex> lock(g_mutex);

	if (g_callback_error.empty())
		g_callback_error = message;
}


std::shared_ptr<const runtime_package::Package> GetRuntimePackage() {
	std::lock_guard<std::mutex> lock(g_mutex);

	if (!g_registered || !g_runtime_package)
		throw std::runtime_error("No ScreenAI runtime package is registered.");

	return g_runtime_package;
}


const runtime_package::AssetView* FindPackagedModel(
	const std::shared_ptr<const runtime_package::Package>& package,
	const char* name
) {
	if (!package)
		throw std::runtime_error("No ScreenAI runtime package is registered.");

	if (!name || !*name)
		throw std::runtime_error("ScreenAI requested an empty model filename.");

	const runtime_package::AssetView* asset = package->FindFile(name);

	if (!asset || asset->size == 0)
		throw std::runtime_error(
			"ScreenAI model file is missing or empty: " +
			std::string(name)
		);

	return asset;
}


bool IsRecognizerConfig(const char* name) {
	if (!name)
		return false;

	constexpr std::string_view SUFFIX = "gocr_mobile_und_config.pb";
	const std::string_view path(name);

	return path.size() >= SUFFIX.size() &&
		path.substr(path.size() - SUFFIX.size()) == SUFFIX;
}


const std::vector<uint8_t>& GetPatchedRecognizerConfig(
	const runtime_package::AssetView& asset
) {
	static const std::vector<uint8_t> patched = runtime_config::PatchRecognizerConfig(
		reinterpret_cast<const uint8_t*>(asset.data),
		asset.size,
		SCREENAI_INTERPRETER_THREADS,
		SCREENAI_CHUNKS_PER_BATCH
	);

	return patched;
}

} // namespace


bool RegisterRuntimePackage(std::shared_ptr<const runtime_package::Package> package) {
	if (!package)
		return false;

	std::lock_guard<std::mutex> lock(g_mutex);

	if (g_registered)
		return false;

	g_runtime_package = std::move(package);
	g_callback_error.clear();
	g_registered = true;

	return true;
}


void UnregisterRuntimePackage() {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_runtime_package.reset();
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
		const std::shared_ptr<const runtime_package::Package> package = GetRuntimePackage();
		const runtime_package::AssetView* asset = FindPackagedModel(package, name);

		if (IsRecognizerConfig(name))
			return static_cast<uint32_t>(GetPatchedRecognizerConfig(*asset).size());

		return asset->size;
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
		const std::shared_ptr<const runtime_package::Package> package = GetRuntimePackage();
		const runtime_package::AssetView* asset = FindPackagedModel(package, name);

		if (IsRecognizerConfig(name)) {
			const std::vector<uint8_t>& patched = GetPatchedRecognizerConfig(*asset);

			if (patched.size() > capacity)
				throw std::runtime_error(
					"The patched ScreenAI recognizer config did not fit its requested buffer."
				);

			std::memcpy(output, patched.data(), patched.size());

			return;
		}

		if (asset->size > capacity)
			throw std::runtime_error("ScreenAI model file size did not match its requested buffer.");

		std::memcpy(output, asset->data, asset->size);
	} catch (const std::exception& error) {
		RecordError(error.what());
	}
}

} // namespace screenai::backend::model_files

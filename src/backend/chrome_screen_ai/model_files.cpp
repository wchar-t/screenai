#include "backend/chrome_screen_ai/model_files.h"

#include "backend/chrome_screen_ai/runtime_package.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

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

		if (asset->size > capacity)
			throw std::runtime_error("ScreenAI model file size did not match its requested buffer.");

		std::memcpy(output, asset->data, asset->size);
	} catch (const std::exception& error) {
		RecordError(error.what());
	}
}

} // namespace screenai::backend::model_files

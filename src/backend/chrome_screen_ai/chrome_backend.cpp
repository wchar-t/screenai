#include "backend/chrome_screen_ai/chrome_backend.h"

#include "backend/chrome_screen_ai/abi_v153.h"
#include "backend/chrome_screen_ai/annotation_decoder.h"
#include "backend/chrome_screen_ai/model_files.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace screenai::backend {
namespace {

template <typename T>
T LoadExport(HMODULE library, const char* name) {
	const FARPROC address = GetProcAddress(library, name);

	if (!address)
		throw std::runtime_error(std::string("chrome_screen_ai.dll is missing export ") + name + ".");

	return reinterpret_cast<T>(address);
}


chrome153::Exports LoadExports(HMODULE library) {
	chrome153::Exports exports;

	exports.set_file_content_functions = LoadExport<chrome153::SetFileContentFunctionsFn>(
		library,
		"SetFileContentFunctions"
	);
	exports.initialize = LoadExport<chrome153::InitOcrFn>(library, "InitOCRUsingCallback");
	exports.uninitialize = LoadExport<chrome153::UninitOcrFn>(library, "UninitializeOCR");
	exports.get_max_dimension = LoadExport<chrome153::GetMaxDimensionFn>(
		library,
		"GetMaxImageDimension"
	);
	exports.perform_ocr = LoadExport<chrome153::PerformOcrFn>(library, "PerformOCR");
	exports.free_result = LoadExport<chrome153::FreeResultFn>(
		library,
		"FreeLibraryAllocatedCharArray"
	);

	return exports;
}

} // namespace


struct ChromeBackend::State {
	chrome153::Exports exports;
	uint32_t max_image_dimension = 0;
	bool model_callbacks_registered = false;
	bool initialized = false;
};


ChromeBackend::ChromeBackend(const std::filesystem::path& runtime_directory)
	: state_(new State()) {
	const std::filesystem::path library_path = runtime_directory / L"chrome_screen_ai.dll";

	this->library_ = LoadLibraryExW(
		library_path.c_str(),
		nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
	);

	if (!this->library_) {
		delete this->state_;
		this->state_ = nullptr;

		throw std::runtime_error(
			"Could not load chrome_screen_ai.dll (Win32 error " +
			std::to_string(GetLastError()) + ")."
		);
	}

	try {
		this->state_->exports = LoadExports(this->library_);

		if (!model_files::RegisterRuntimeDirectory(runtime_directory))
			throw std::runtime_error("Only one ScreenAI recognizer may be active in this process.");

		this->state_->model_callbacks_registered = true;

		this->state_->exports.set_file_content_functions(
			&model_files::GetModelSize,
			&model_files::ReadModel
		);
		model_files::ClearCallbackError();

		if (!this->state_->exports.initialize()) {
			const std::string callback_error = model_files::GetCallbackError();
			throw std::runtime_error(
				callback_error.empty()
					? "ScreenAI OCR initialization failed. Check the runtime model files."
					: callback_error
			);
		}

		this->state_->initialized = true;
		this->state_->max_image_dimension = this->state_->exports.get_max_dimension();

		if (this->state_->max_image_dimension == 0 || this->state_->max_image_dimension > 32768)
			throw std::runtime_error("ScreenAI reported an invalid maximum image dimension.");
	} catch (...) {
		this->Release();
		throw;
	}
}


ChromeBackend::~ChromeBackend() {
	this->Release();
}


void ChromeBackend::Release() noexcept {
	if (this->state_) {
		if (this->state_->initialized && this->state_->exports.uninitialize)
			this->state_->exports.uninitialize();
		if (this->state_->model_callbacks_registered)
			model_files::UnregisterRuntimeDirectory();

		delete this->state_;
		this->state_ = nullptr;
	}
	if (this->library_) {
		FreeLibrary(this->library_);
		this->library_ = nullptr;
	}
}


uint32_t ChromeBackend::GetMaxImageDimension() const {
	return this->state_->max_image_dimension;
}


core::OcrResult ChromeBackend::PerformOcr(const core::Image& image) {
	if (image.width == 0 || image.height == 0 || image.bgra.empty())
		throw std::runtime_error("The normalized image is empty.");

	chrome153::SkBitmapAbiView bitmap{};
	bitmap.pixel_ref = nullptr;
	bitmap.pixels = image.bgra.data();
	bitmap.row_bytes = static_cast<size_t>(image.width) * 4;
	bitmap.color_space = nullptr;
	bitmap.color_type = 6;
	bitmap.alpha_type = 1;
	bitmap.width = static_cast<int32_t>(image.width);
	bitmap.height = static_cast<int32_t>(image.height);

	uint32_t result_length = 0;
	char* raw_result = this->state_->exports.perform_ocr(&bitmap, result_length);

	if (!raw_result)
		return {};

	std::unique_ptr<char, chrome153::FreeResultFn> result_owner(
		raw_result,
		this->state_->exports.free_result
	);

	if (result_length > 64 * 1024 * 1024)
		throw OcrError("ScreenAI returned an unexpectedly large OCR response.");

	core::OcrResult result;

	if (!ParseVisualAnnotation(std::string_view(result_owner.get(), result_length), result.text))
		throw OcrError("ScreenAI returned an invalid VisualAnnotation response.");

	return result;
}

} // namespace screenai::backend

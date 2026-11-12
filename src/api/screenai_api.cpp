#include "screenai/screenai.h"

#include "backend/chrome_screen_ai/chrome_backend.h"
#include "core/recognizer.h"
#include "image/wic_decoder.h"
#include "api/worker_process.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

struct screenai_recognizer {
	explicit screenai_recognizer(uint32_t batch_worker_count)
		: implementation(std::make_unique<screenai::core::Recognizer>()) {
		this->worker_processes.reserve(batch_worker_count - 1);

		for (uint32_t worker_index = 1; worker_index < batch_worker_count; ++worker_index)
			this->worker_processes.push_back(std::make_unique<screenai::api::WorkerProcess>());
	}

	std::vector<std::optional<screenai::core::OcrResult>> RecognizeBatch(
		const std::vector<std::wstring>& image_paths
	) {
		std::vector<std::optional<screenai::core::OcrResult>> results(image_paths.size());

		if (image_paths.empty())
			return results;

		if (this->worker_processes.empty()) {
			for (size_t image_index = 0; image_index < image_paths.size(); ++image_index) {
				if (image_paths[image_index].empty())
					continue;

				try {
					results[image_index] = this->implementation->RecognizeFile(image_paths[image_index]);
				} catch (...) {
				}
			}

			return results;
		}

		const size_t worker_count = (std::min)(
			image_paths.size(),
			this->worker_processes.size() + 1
		);
		std::atomic<size_t> next_image_index = 0;
		std::vector<std::thread> workers;
		workers.reserve(worker_count);

		try {
			for (size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
				workers.emplace_back([this, &image_paths, &results, &next_image_index, worker_index]() {
					while (true) {
						const size_t image_index = next_image_index.fetch_add(1);

						if (image_index >= image_paths.size())
							return;

						if (image_paths[image_index].empty())
							continue;

						try {
							if (worker_index == 0) {
								results[image_index] = this->implementation->RecognizeFile(
									image_paths[image_index]
								);
							} else {
								results[image_index] = this->worker_processes[worker_index - 1]->RecognizeFile(
									image_paths[image_index]
								);
							}
						} catch (...) {
							// Leave this slot empty and continue processing the batch.
						}
					}
				});
			}
		} catch (...) {
			for (std::thread& worker : workers)
				worker.join();

			throw;
		}

		for (std::thread& worker : workers)
			worker.join();

		return results;
	}

	std::unique_ptr<screenai::core::Recognizer> implementation;
	std::vector<std::unique_ptr<screenai::api::WorkerProcess>> worker_processes;
	std::mutex mutex;
};


struct screenai_ocr_result {
	explicit screenai_ocr_result(screenai::core::OcrResult result)
		: result_(std::move(result)) {
		this->BuildView();
	}

	const screenai_ocr_result_view* GetView() const noexcept {
		return &this->view_;
	}

private:
	static screenai_bounding_box_view ToView(const screenai::BoundingBox& bounding_box) {
		return {
			bounding_box.x,
			bounding_box.y,
			bounding_box.width,
			bounding_box.height,
			bounding_box.angle,
		};
	}

	void BuildView() {
		size_t line_count = 0;

		for (const screenai::OcrBlock& block : this->result_.blocks)
			line_count += block.lines.size();

		this->block_views_.reserve(this->result_.blocks.size());
		this->line_views_.reserve(line_count);

		for (const screenai::OcrBlock& block : this->result_.blocks) {
			screenai_block_view block_view{};
			const size_t first_line = this->line_views_.size();

			for (const screenai::OcrLine& line : block.lines) {
				screenai_line_view line_view{};

				line_view.text = line.text.data();
				line_view.text_length = line.text.size();
				if (line.bounding_box) {
					line_view.bounding_box = ToView(*line.bounding_box);
					line_view.has_bounding_box = 1;
				}
				if (line.confidence) {
					line_view.confidence = *line.confidence;
					line_view.has_confidence = 1;
				}

				this->line_views_.push_back(line_view);
			}

			if (!block.lines.empty())
				block_view.lines = this->line_views_.data() + first_line;
			block_view.line_count = block.lines.size();

			this->block_views_.push_back(block_view);
		}

		this->view_.text = this->result_.text.data();
		this->view_.text_length = this->result_.text.size();
		this->view_.image_width = this->result_.image_width;
		this->view_.image_height = this->result_.image_height;
		this->view_.blocks = this->block_views_.data();
		this->view_.block_count = this->block_views_.size();
	}

	screenai::core::OcrResult result_;
	std::vector<screenai_line_view> line_views_;
	std::vector<screenai_block_view> block_views_;
	screenai_ocr_result_view view_{};
};

namespace {

void SetError(char* output, size_t capacity, const char* message) noexcept {
	if (!output || capacity == 0)
		return;

	const char* source = message ? message : "Unknown error.";
	const size_t source_length = std::strlen(source);
	const size_t length = capacity - 1 < source_length ? capacity - 1 : source_length;

	std::memcpy(output, source, length);
	output[length] = '\0';
}


void SetError(char* output, size_t capacity, const std::string& message) noexcept {
	SetError(output, capacity, message.c_str());
}


screenai_status HandleProcessingError(
	const std::exception& error,
	screenai_status status,
	char* error_message,
	size_t error_capacity
) {
	SetError(error_message, error_capacity, error.what());

	return status;
}

} // namespace

extern "C" {

screenai_status SCREENAI_CALL screenai_create(
	int32_t batch_worker_count,
	screenai_recognizer** out_recognizer,
	char* error_message,
	size_t error_capacity
) {
	if (!out_recognizer || batch_worker_count <= 0) {
		SetError(
			error_message,
			error_capacity,
			"A positive worker count and output pointer are required."
		);

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*out_recognizer = nullptr;

	try {
		*out_recognizer = new screenai_recognizer(
			static_cast<uint32_t>(batch_worker_count)
		);

		return SCREENAI_STATUS_OK;
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate a recognizer.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const screenai::api::WorkerError& error) {
		SetError(error_message, error_capacity, error.what());

		return error.GetStatus();
	} catch (const std::exception& error) {
		SetError(error_message, error_capacity, error.what());

		return SCREENAI_STATUS_RUNTIME_ERROR;
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while initializing ScreenAI.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


void SCREENAI_CALL screenai_destroy(screenai_recognizer* recognizer) {
	delete recognizer;
}


screenai_status SCREENAI_CALL screenai_get_max_image_dimension(
	screenai_recognizer* recognizer,
	uint32_t* out_dimension,
	char* error_message,
	size_t error_capacity
) {
	if (!recognizer || !out_dimension) {
		SetError(error_message, error_capacity, "Recognizer and output dimension are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		*out_dimension = recognizer->implementation->GetMaxImageDimension();

		return SCREENAI_STATUS_OK;
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while reading the image limit.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


screenai_status SCREENAI_CALL screenai_recognize(
	screenai_recognizer* recognizer,
	const wchar_t* image_path,
	screenai_ocr_result** out_result,
	char* error_message,
	size_t error_capacity
) {
	if (out_result)
		*out_result = nullptr;

	if (!recognizer || !image_path || !*image_path || !out_result) {
		SetError(
			error_message,
			error_capacity,
			"Recognizer, image path, and output pointer are required."
		);

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	*out_result = nullptr;

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		screenai::core::OcrResult result = recognizer->implementation->RecognizeFile(image_path);

		*out_result = new screenai_ocr_result(std::move(result));

		return SCREENAI_STATUS_OK;
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate the OCR result.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const screenai::image::DecodeError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_IMAGE_ERROR,
			error_message,
			error_capacity
		);
	} catch (const screenai::backend::OcrError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_OCR_ERROR,
			error_message,
			error_capacity
		);
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while processing the image.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


screenai_status SCREENAI_CALL screenai_recognize_batch(
	screenai_recognizer* recognizer,
	const wchar_t* const* image_paths,
	size_t image_count,
	screenai_ocr_result** out_results,
	size_t result_capacity,
	char* error_message,
	size_t error_capacity
) {
	if (!recognizer ||
		(image_count > 0 && (!image_paths || !out_results || result_capacity < image_count))) {
		SetError(
			error_message,
			error_capacity,
			"Recognizer, input paths, and result slots are required."
		);

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	for (size_t image_index = 0; image_index < image_count; ++image_index)
		out_results[image_index] = nullptr;

	try {
		std::vector<std::wstring> path_values;
		path_values.reserve(image_count);

		for (size_t image_index = 0; image_index < image_count; ++image_index)
			path_values.emplace_back(image_paths[image_index] ? image_paths[image_index] : L"");

		std::lock_guard<std::mutex> lock(recognizer->mutex);
		const std::vector<std::optional<screenai::core::OcrResult>> results =
			recognizer->RecognizeBatch(path_values);
		std::vector<std::unique_ptr<screenai_ocr_result>> result_owners(image_count);

		for (size_t image_index = 0; image_index < results.size(); ++image_index) {
			if (results[image_index]) {
				result_owners[image_index] = std::make_unique<screenai_ocr_result>(
					std::move(*results[image_index])
				);
			}
		}

		for (size_t image_index = 0; image_index < image_count; ++image_index)
			out_results[image_index] = result_owners[image_index].release();

		return SCREENAI_STATUS_OK;
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate the OCR batch results.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while processing the image batch.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


screenai_status SCREENAI_CALL screenai_recognize_pixels(
	screenai_recognizer* recognizer,
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	screenai_pixel_format pixel_format,
	screenai_ocr_result** out_result,
	char* error_message,
	size_t error_capacity
) {
	if (out_result)
		*out_result = nullptr;

	if (!recognizer || !pixels || !out_result) {
		SetError(error_message, error_capacity, "Recognizer, pixels, and output pointer are required.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}
	if (width == 0 || height == 0 || stride < static_cast<uint64_t>(width) * 4 ||
		static_cast<uint64_t>(stride) * height > pixel_buffer_size) {
		SetError(error_message, error_capacity, "Pixel dimensions, stride, or buffer size are invalid.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}
	if (pixel_format != SCREENAI_PIXEL_FORMAT_BGRA8 && pixel_format != SCREENAI_PIXEL_FORMAT_RGBA8) {
		SetError(error_message, error_capacity, "Pixel format must be BGRA8 or RGBA8.");

		return SCREENAI_STATUS_INVALID_ARGUMENT;
	}

	try {
		std::lock_guard<std::mutex> lock(recognizer->mutex);

		screenai::core::OcrResult result = recognizer->implementation->RecognizePixels(
			pixels,
			pixel_buffer_size,
			width,
			height,
			stride,
			pixel_format == SCREENAI_PIXEL_FORMAT_RGBA8
		);

		*out_result = new screenai_ocr_result(std::move(result));

		return SCREENAI_STATUS_OK;
	} catch (const std::bad_alloc&) {
		SetError(error_message, error_capacity, "Could not allocate the OCR result.");

		return SCREENAI_STATUS_OUT_OF_MEMORY;
	} catch (const screenai::image::DecodeError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_IMAGE_ERROR,
			error_message,
			error_capacity
		);
	} catch (const screenai::backend::OcrError& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_OCR_ERROR,
			error_message,
			error_capacity
		);
	} catch (const std::exception& error) {
		return HandleProcessingError(
			error,
			SCREENAI_STATUS_INTERNAL_ERROR,
			error_message,
			error_capacity
		);
	} catch (...) {
		SetError(error_message, error_capacity, "Unknown error while processing the pixel buffer.");

		return SCREENAI_STATUS_INTERNAL_ERROR;
	}
}


const screenai_ocr_result_view* SCREENAI_CALL screenai_ocr_result_get_view(
	const screenai_ocr_result* result
) {
	if (!result)
		return nullptr;

	return result->GetView();
}


void SCREENAI_CALL screenai_ocr_result_destroy(screenai_ocr_result* result) {
	delete result;
}


} // extern "C"

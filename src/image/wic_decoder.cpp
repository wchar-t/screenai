#include "image/wic_decoder.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace screenai::image {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment {
public:
	ComApartment() {
		const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		if (SUCCEEDED(result)) {
			this->initialized_ = true;

			return;
		}
		if (result != RPC_E_CHANGED_MODE)
			throw DecodeError(
				"COM initialization failed (HRESULT " +
				std::to_string(static_cast<uint32_t>(result)) + ")."
			);
	}

	~ComApartment() {
		if (this->initialized_)
			CoUninitialize();
	}

	ComApartment(const ComApartment&) = delete;
	ComApartment& operator=(const ComApartment&) = delete;

private:
	bool initialized_ = false;
};


[[noreturn]] void ThrowWicError(const char* action, HRESULT result) {
	throw DecodeError(
		std::string(action) + " (HRESULT " +
		std::to_string(static_cast<uint32_t>(result)) + ")."
	);
}


core::Image ConvertSource(
	IWICImagingFactory* factory,
	IWICBitmapSource* original,
	uint32_t source_width,
	uint32_t source_height,
	uint32_t max_dimension
) {
	if (max_dimension == 0)
		throw DecodeError("ScreenAI reported an invalid maximum image dimension.");

	ComPtr<IWICBitmapSource> source = original;

	if (source_width > max_dimension || source_height > max_dimension) {
		const double scale = std::min(
			static_cast<double>(max_dimension) / source_width,
			static_cast<double>(max_dimension) / source_height
		);
		const UINT width = std::max<UINT>(1, static_cast<UINT>(source_width * scale));
		const UINT height = std::max<UINT>(1, static_cast<UINT>(source_height * scale));
		ComPtr<IWICBitmapScaler> scaler;

		HRESULT result = factory->CreateBitmapScaler(scaler.GetAddressOf());

		if (SUCCEEDED(result))
			result = scaler->Initialize(
				original,
				width,
				height,
				WICBitmapInterpolationModeFant
			);

		if (FAILED(result))
			ThrowWicError("WIC could not scale the image", result);

		source = scaler;
	}

	ComPtr<IWICFormatConverter> converter;
	HRESULT result = factory->CreateFormatConverter(converter.GetAddressOf());

	if (SUCCEEDED(result)) {
		result = converter->Initialize(
			source.Get(),
			GUID_WICPixelFormat32bppBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom
		);
	}
	if (FAILED(result))
		ThrowWicError("WIC could not convert the image to 32-bit BGRA", result);

	core::Image image;
	UINT width = 0;
	UINT height = 0;

	result = converter->GetSize(&width, &height);

	if (FAILED(result) || width == 0 || height == 0)
		ThrowWicError("WIC returned invalid image dimensions", result);

	const uint64_t stride = static_cast<uint64_t>(width) * 4;
	const uint64_t byte_count = stride * height;

	if (stride > UINT_MAX || byte_count > UINT_MAX ||
		byte_count > std::numeric_limits<size_t>::max()) {
		throw DecodeError("The decoded image is too large for WIC.");
	}

	image.width = width;
	image.height = height;
	image.bgra.resize(static_cast<size_t>(byte_count));

	result = converter->CopyPixels(
		nullptr,
		static_cast<UINT>(stride),
		static_cast<UINT>(byte_count),
		image.bgra.data()
	);

	if (FAILED(result))
		ThrowWicError("WIC could not copy the converted pixels", result);

	for (size_t index = 3; index < image.bgra.size(); index += 4)
		image.bgra[index] = 255;

	return image;
}


ComPtr<IWICImagingFactory> CreateFactory() {
	ComPtr<IWICImagingFactory> factory;
	const HRESULT result = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(factory.GetAddressOf())
	);

	if (FAILED(result))
		ThrowWicError("Could not create the Windows Imaging Component factory", result);

	return factory;
}

} // namespace


core::Image DecodeFile(const std::filesystem::path& image_path, uint32_t max_dimension) {
	ComApartment apartment;
	auto factory = CreateFactory();
	ComPtr<IWICBitmapDecoder> decoder;

	HRESULT result = factory->CreateDecoderFromFilename(
		image_path.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		decoder.GetAddressOf()
	);

	if (FAILED(result))
		ThrowWicError("WIC could not open the image", result);

	ComPtr<IWICBitmapFrameDecode> frame;

	result = decoder->GetFrame(0, frame.GetAddressOf());

	if (FAILED(result))
		ThrowWicError("WIC could not read the first image frame", result);

	UINT width = 0;
	UINT height = 0;

	result = frame->GetSize(&width, &height);

	if (FAILED(result) || width == 0 || height == 0)
		ThrowWicError("The image has invalid dimensions", result);

	return ConvertSource(factory.Get(), frame.Get(), width, height, max_dimension);
}


core::Image NormalizePixels(
	const uint8_t* pixels,
	size_t pixel_buffer_size,
	uint32_t width,
	uint32_t height,
	uint32_t stride,
	bool rgba,
	uint32_t max_dimension
) {
	if (!pixels || width == 0 || height == 0 || stride < static_cast<uint64_t>(width) * 4)
		throw DecodeError("Pixel input has invalid dimensions, stride, or buffer.");

	const uint64_t required = static_cast<uint64_t>(stride) * height;

	if (required > pixel_buffer_size || stride > UINT_MAX || required > UINT_MAX)
		throw DecodeError("The pixel buffer is smaller than its dimensions require.");

	ComApartment apartment;
	auto factory = CreateFactory();
	ComPtr<IWICBitmap> bitmap;
	const WICPixelFormatGUID format = rgba
		? GUID_WICPixelFormat32bppRGBA
		: GUID_WICPixelFormat32bppBGRA;

	const HRESULT result = factory->CreateBitmapFromMemory(
		width,
		height,
		format,
		stride,
		static_cast<UINT>(required),
		const_cast<BYTE*>(reinterpret_cast<const BYTE*>(pixels)),
		bitmap.GetAddressOf()
	);

	if (FAILED(result))
		ThrowWicError("WIC could not create a bitmap from the supplied pixels", result);

	return ConvertSource(factory.Get(), bitmap.Get(), width, height, max_dimension);
}

} // namespace screenai::image

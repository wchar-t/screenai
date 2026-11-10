#include "screenai/screenai.hpp"

#include <iostream>

int wmain(int argc, wchar_t** argv) {
	if (argc != 2) {
		std::wcerr << L"Usage: ocr_image.exe <image-path>\n";

		return 1;
	}

	try {
		screenai::Recognizer recognizer;

		const screenai::OcrResult result = recognizer.RecognizeFileResult(argv[1]);

		std::cout.write(result.text.data(), static_cast<std::streamsize>(result.text.size()));
		std::cout << '\n';

		for (const screenai::OcrBlock& block : result.blocks) {
			for (const screenai::OcrLine& line : block.lines) {
				if (line.bounding_box) {
					std::cout << "Line box: "
						<< line.bounding_box->x << ", "
						<< line.bounding_box->y << ", "
						<< line.bounding_box->width << ", "
						<< line.bounding_box->height << '\n';
				}

				if (line.confidence)
					std::cout << "Line confidence: " << *line.confidence << '\n';
			}
		}

		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';

		return 2;
	}
}

#include "screenai/screenai.hpp"

#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
	if (argc != 2) {
		std::wcerr << L"Usage: ocr_image.exe <image-path>\n";

		return 1;
	}

	try {
		screenai::Recognizer recognizer;

		const std::string text = recognizer.RecognizeFile(argv[1]);

		std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
		std::cout << '\n';

		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';

		return 2;
	}
}

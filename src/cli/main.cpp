#include "screenai/screenai.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iostream>
#include <string>

namespace {

void PrintUsage() {
	std::wcerr << L"Usage: screenai.exe <image-path>\n"
		L"The fixed 153.5 runtime package beside screenai.dll is required.\n";
}

} // namespace


int wmain(int argc, wchar_t** argv) {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	if (argc == 2 &&
		(std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"-h")) {
		PrintUsage();

		return 0;
	}
	if (argc != 2) {
		PrintUsage();

		return 1;
	}

	const std::wstring image_path = argv[1];

	try {
		screenai::Recognizer recognizer;

		const std::string text = recognizer.RecognizeFile(image_path);

		std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
		if (text.empty() || text.back() != '\n')
			std::cout << '\n';

		return 0;
	} catch (const std::exception& error) {
		std::cerr << "ScreenAI error: " << error.what() << '\n';

		return 2;
	}
}

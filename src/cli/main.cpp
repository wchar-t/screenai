#include "screenai/screenai.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iostream>
#include <string>

namespace {

void PrintUsage() {
	std::wcerr << L"Usage: screenai.exe <image-path> [--runtime-dir|-runtime-dir <directory>]\n"
		L"The fixed 153.5 runtime package beside screenai.dll is used by default.\n"
		L"An external runtime directory may be set with SCREENAI_RUNTIME_DIR, --runtime-dir, or -runtime-dir.\n";
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
	if (argc != 2 && argc != 4) {
		PrintUsage();

		return 1;
	}

	const std::wstring image_path = argv[1];
	std::wstring runtime_directory;

	if (argc == 4) {
		const std::wstring runtime_option = argv[2];

		if (runtime_option != L"--runtime-dir" && runtime_option != L"-runtime-dir") {
			PrintUsage();

			return 1;
		}

		runtime_directory = argv[3];
	}

	try {
		screenai::Recognizer recognizer(runtime_directory);

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

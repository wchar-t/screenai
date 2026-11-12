#include "screenai/screenai.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

void PrintUsage() {
	std::wcerr << L"Usage: screenai.exe <image-path> [--json]\n"
		L"The fixed 153.7 runtime package beside screenai.dll is required.\n";
}


void WriteJsonString(std::ostream& output, std::string_view value) {
	static constexpr char HEX_DIGITS[] = "0123456789abcdef";

	output.put('"');

	for (const unsigned char character : value) {
		switch (character) {
		case '"':
			output << "\\\"";
			break;
		case '\\':
			output << "\\\\";
			break;
		case '\b':
			output << "\\b";
			break;
		case '\f':
			output << "\\f";
			break;
		case '\n':
			output << "\\n";
			break;
		case '\r':
			output << "\\r";
			break;
		case '\t':
			output << "\\t";
			break;
		default:
			if (character < 0x20) {
				output << "\\u00"
					<< HEX_DIGITS[character >> 4]
					<< HEX_DIGITS[character & 0x0f];
			} else {
				output.put(static_cast<char>(character));
			}
			break;
		}
	}

	output.put('"');
}


void WriteJsonFloat(std::ostream& output, float value) {
	std::ostringstream formatted;

	formatted.imbue(std::locale::classic());
	formatted << std::setprecision(std::numeric_limits<float>::max_digits10) << value;

	output << formatted.str();
}


void WriteJsonBoundingBox(
	std::ostream& output,
	const std::optional<screenai::BoundingBox>& bounding_box
) {
	if (!bounding_box) {
		output << "null";

		return;
	}

	output << "{\"x\":" << bounding_box->x
		<< ",\"y\":" << bounding_box->y
		<< ",\"width\":" << bounding_box->width
		<< ",\"height\":" << bounding_box->height
		<< ",\"angle\":";
	WriteJsonFloat(output, bounding_box->angle);
	output << '}';
}


void WriteJsonResult(std::ostream& output, const screenai::OcrResult& result) {
	output << "{\"text\":";
	WriteJsonString(output, result.text);
	output << ",\"image_width\":" << result.image_width
		<< ",\"image_height\":" << result.image_height
		<< ",\"blocks\":[";

	for (size_t block_index = 0; block_index < result.blocks.size(); ++block_index) {
		const screenai::OcrBlock& block = result.blocks[block_index];

		if (block_index > 0)
			output.put(',');

		output << "{\"lines\":[";

		for (size_t line_index = 0; line_index < block.lines.size(); ++line_index) {
			const screenai::OcrLine& line = block.lines[line_index];

			if (line_index > 0)
				output.put(',');

			output << "{\"text\":";
			WriteJsonString(output, line.text);
			output << ",\"bounding_box\":";
			WriteJsonBoundingBox(output, line.bounding_box);
			output << ",\"confidence\":";
			if (line.confidence)
				WriteJsonFloat(output, *line.confidence);
			else
				output << "null";
			output.put('}');
		}

		output << "]}";
	}

	output << "]}\n";
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

	const bool json_output = argc == 3 && std::wstring(argv[2]) == L"--json";

	if (argc != 2 && !json_output) {
		PrintUsage();

		return 1;
	}

	const std::wstring image_path = argv[1];

	try {
		screenai::Recognizer recognizer;

		const screenai::OcrResult result = recognizer.Recognize(image_path);

		if (json_output) {
			WriteJsonResult(std::cout, result);

			return 0;
		}

		std::cout.write(result.text.data(), static_cast<std::streamsize>(result.text.size()));

		if (result.text.empty() || result.text.back() != '\n')
			std::cout << '\n';

		return 0;
	} catch (const std::exception& error) {
		std::cerr << "ScreenAI error: " << error.what() << '\n';

		return 2;
	}
}

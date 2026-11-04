#include "backend/chrome_screen_ai/annotation_decoder.h"

#include <iostream>
#include <string>

int main() {
	const std::string valid("\x12\x07\x1a\x05hello", 9);
	std::string text;

	if (!screenai::backend::ParseVisualAnnotation(valid, text) || text != "hello") {
		std::cerr << "Could not decode the text from a VisualAnnotation.\n";

		return 1;
	}

	const std::string malformed("\x12\x05\x1a\x05hey", 7);

	if (screenai::backend::ParseVisualAnnotation(malformed, text)) {
		std::cerr << "Accepted a truncated VisualAnnotation.\n";

		return 1;
	}

	return 0;
}

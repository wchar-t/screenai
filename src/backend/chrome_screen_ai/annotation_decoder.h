#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_ANNOTATION_DECODER_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_ANNOTATION_DECODER_H_

#include "core/result.h"

#include <string>
#include <string_view>

namespace screenai::backend {

bool ParseVisualAnnotation(std::string_view bytes, core::OcrResult& result);
bool ParseVisualAnnotation(std::string_view bytes, std::string& text);

} // namespace screenai::backend

#endif

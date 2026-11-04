#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_MODEL_FILES_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_MODEL_FILES_H_

#include <filesystem>
#include <cstdint>
#include <string>

namespace screenai::backend::model_files {

bool RegisterRuntimeDirectory(const std::filesystem::path& directory);
void UnregisterRuntimeDirectory();
void ClearCallbackError();
std::string GetCallbackError();
uint32_t GetModelSize(const char* name);
void ReadModel(const char* name, uint32_t capacity, char* output);

} // namespace screenai::backend::model_files

#endif

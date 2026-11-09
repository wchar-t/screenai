#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_MODEL_FILES_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_MODEL_FILES_H_

#include <cstdint>
#include <memory>
#include <string>

namespace screenai::backend::runtime_package {
class Package;
}

namespace screenai::backend::model_files {

bool RegisterRuntimePackage(std::shared_ptr<const runtime_package::Package> package);
void UnregisterRuntimePackage();
void ClearCallbackError();
std::string GetCallbackError();
uint32_t GetModelSize(const char* name);
void ReadModel(const char* name, uint32_t capacity, char* output);

} // namespace screenai::backend::model_files

#endif

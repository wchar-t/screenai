#ifndef SCREENAI_CORE_RUNTIME_CONFIG_H_
#define SCREENAI_CORE_RUNTIME_CONFIG_H_

#include <filesystem>
#include <optional>

namespace screenai::core {

std::optional<std::filesystem::path> ResolveRuntimeDirectory(
	const wchar_t* configured_directory
);

} // namespace screenai::core

#endif

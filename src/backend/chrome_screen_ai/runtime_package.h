#ifndef SCREENAI_BACKEND_CHROME_SCREEN_AI_RUNTIME_PACKAGE_H_
#define SCREENAI_BACKEND_CHROME_SCREEN_AI_RUNTIME_PACKAGE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace screenai::backend::runtime_package {

struct AssetView {
	const char* data = nullptr;
	uint32_t size = 0;
};

class Package final {
public:
	static std::shared_ptr<const Package> LoadEmbedded();

	const std::string& version() const;
	const std::string& package_id() const;
	const AssetView* FindFile(std::string_view path) const;
	std::filesystem::path ExtractNativeLibraries() const;

private:
	struct Entry {
		AssetView asset;
		std::string digest;
		bool is_native_library = false;
	};

	Package(const uint8_t* data, size_t size);

	void Parse();

	const uint8_t* data_ = nullptr;
	size_t size_ = 0;
	std::string version_;
	std::string package_id_;
	std::unordered_map<std::string, Entry> files_;
	std::vector<std::string> native_library_paths_;
};

} // namespace screenai::backend::runtime_package

#endif

#include "backend/chrome_screen_ai/runtime_package.h"

#include "resources/runtime_resource.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace screenai::backend::runtime_package {
namespace {

constexpr char PACKAGE_MAGIC[] = "SAIPAK1";
constexpr uint32_t PACKAGE_FORMAT_VERSION = 1;
constexpr uint16_t FLAG_NATIVE_LIBRARY = 1;
constexpr size_t SHA256_LENGTH = 32;
constexpr size_t HASH_CHUNK_SIZE = 1024 * 1024;

class Sha256 final {
public:
	Sha256() {
		NTSTATUS status = BCryptOpenAlgorithmProvider(
			&this->algorithm_,
			BCRYPT_SHA256_ALGORITHM,
			nullptr,
			0
		);

		if (status < 0)
			throw std::runtime_error("Could not open the Windows SHA-256 provider.");

		DWORD object_length = 0;
		DWORD result_length = 0;
		status = BCryptGetProperty(
			this->algorithm_,
			BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<PUCHAR>(&object_length),
			sizeof(object_length),
			&result_length,
			0
		);

		if (status < 0) {
			this->Close();

			throw std::runtime_error("Could not read the Windows SHA-256 object size.");
		}

		this->object_.resize(object_length);
		status = BCryptCreateHash(
			this->algorithm_,
			&this->hash_,
			this->object_.data(),
			static_cast<ULONG>(this->object_.size()),
			nullptr,
			0,
			0
		);

		if (status < 0) {
			this->Close();

			throw std::runtime_error("Could not create a Windows SHA-256 hash.");
		}
	}

	~Sha256() {
		this->Close();
	}

	Sha256(const Sha256&) = delete;
	Sha256& operator=(const Sha256&) = delete;

	void Update(const uint8_t* data, size_t size) {
		while (size > 0) {
			const ULONG length = static_cast<ULONG>(
				std::min<size_t>(size, std::numeric_limits<ULONG>::max())
			);
			const NTSTATUS status = BCryptHashData(
				this->hash_,
				const_cast<PUCHAR>(data),
				length,
				0
			);

			if (status < 0)
				throw std::runtime_error("Could not update a Windows SHA-256 hash.");

			data += length;
			size -= length;
		}
	}

	std::string Finish() {
		std::array<uint8_t, SHA256_LENGTH> digest{};
		const NTSTATUS status = BCryptFinishHash(
			this->hash_,
			digest.data(),
			static_cast<ULONG>(digest.size()),
			0
		);

		if (status < 0)
			throw std::runtime_error("Could not finish a Windows SHA-256 hash.");

		return std::string(
			reinterpret_cast<const char*>(digest.data()),
			digest.size()
		);
	}

private:
	void Close() noexcept {
		if (this->hash_) {
			BCryptDestroyHash(this->hash_);
			this->hash_ = nullptr;
		}
		if (this->algorithm_) {
			BCryptCloseAlgorithmProvider(this->algorithm_, 0);
			this->algorithm_ = nullptr;
		}
	}

	BCRYPT_ALG_HANDLE algorithm_ = nullptr;
	BCRYPT_HASH_HANDLE hash_ = nullptr;
	std::vector<uint8_t> object_;
};


void RequireBytes(size_t cursor, size_t length, size_t total) {
	if (cursor > total || length > total - cursor)
		throw std::runtime_error("The embedded ScreenAI runtime package is truncated.");
}


template <typename T>
T ReadValue(const uint8_t* data, size_t& cursor, size_t total) {
	RequireBytes(cursor, sizeof(T), total);

	T value{};
	std::memcpy(&value, data + cursor, sizeof(T));
	cursor += sizeof(T);

	return value;
}


std::string ReadString(const uint8_t* data, size_t& cursor, size_t length, size_t total) {
	RequireBytes(cursor, length, total);
	std::string value(reinterpret_cast<const char*>(data + cursor), length);
	cursor += length;

	return value;
}


std::string NormalizePath(std::string_view path) {
	if (path.empty() || path.front() == '/' || path.front() == '\\')
		return {};

	std::string key;
	key.reserve(path.size());
	bool segment_is_empty = true;

	for (const unsigned char character : path) {
		if (character == ':' || character == '\0')
			return {};

		if (character == '/' || character == '\\') {
			if (segment_is_empty)
				return {};
			if (key.size() >= 1 && key.back() == '.')
				return {};
			key.push_back('/');
			segment_is_empty = true;

			continue;
		}

		if (segment_is_empty && character == '.')
			return {};

		key.push_back(static_cast<char>(std::tolower(character)));
		segment_is_empty = false;
	}

	if (segment_is_empty || key.empty() || key.back() == '.')
		return {};

	return key;
}


std::string Hex(const std::string& bytes) {
	static constexpr char DIGITS[] = "0123456789abcdef";
	std::string result;
	result.reserve(bytes.size() * 2);

	for (const unsigned char byte : bytes) {
		result.push_back(DIGITS[byte >> 4]);
		result.push_back(DIGITS[byte & 0x0f]);
	}

	return result;
}


std::wstring ReadEnvironmentVariable(const wchar_t* name) {
	const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);

	if (required == 0)
		return {};

	std::vector<wchar_t> value(required);
	const DWORD length = GetEnvironmentVariableW(
		name,
		value.data(),
		static_cast<DWORD>(value.size())
	);

	if (length == 0 || length >= value.size())
		throw std::runtime_error("Could not read a Windows environment variable.");

	return std::wstring(value.data(), length);
}


std::filesystem::path GetCacheDirectory(
	const std::string& version,
	const std::string& package_id
) {
	std::wstring base = ReadEnvironmentVariable(L"TMP");

	if (base.empty())
		base = ReadEnvironmentVariable(L"TEMP");
	if (base.empty())
		throw std::runtime_error("Neither TMP nor TEMP is available for the ScreenAI DLL cache.");

	return std::filesystem::path(base) /
		std::filesystem::u8path("chrome_screen_ai_" + version + "_" + package_id);
}


class ScopedMutex final {
public:
	explicit ScopedMutex(const std::wstring& name) {
		this->handle_ = CreateMutexW(nullptr, FALSE, name.c_str());

		if (!this->handle_)
			throw std::runtime_error("Could not create the ScreenAI runtime cache mutex.");

		const DWORD result = WaitForSingleObject(this->handle_, INFINITE);

		if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
			CloseHandle(this->handle_);
			this->handle_ = nullptr;

			throw std::runtime_error("Could not lock the ScreenAI runtime cache.");
		}
	}

	~ScopedMutex() {
		if (this->handle_) {
			ReleaseMutex(this->handle_);
			CloseHandle(this->handle_);
		}
	}

	ScopedMutex(const ScopedMutex&) = delete;
	ScopedMutex& operator=(const ScopedMutex&) = delete;

private:
	HANDLE handle_ = nullptr;
};


std::string HashFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);

	if (!file)
		return {};

	Sha256 hash;
	std::array<uint8_t, HASH_CHUNK_SIZE> buffer{};

	while (file) {
		file.read(
			reinterpret_cast<char*>(buffer.data()),
			static_cast<std::streamsize>(buffer.size())
		);
		const std::streamsize count = file.gcount();

		if (count > 0)
			hash.Update(buffer.data(), static_cast<size_t>(count));
	}

	if (!file.eof())
		return {};

	return hash.Finish();
}


void WriteAssetAtomically(
	const std::filesystem::path& path,
	const AssetView& asset,
	const std::string& expected_digest
) {
	std::filesystem::path temporary_path = path;
	temporary_path += std::wstring(L".tmp-") + std::to_wstring(GetCurrentProcessId());

	{
		std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);

		if (!file)
			throw std::runtime_error("Could not create a cached ScreenAI runtime DLL.");

		file.write(asset.data, asset.size);
		file.flush();

		if (!file)
			throw std::runtime_error("Could not write a cached ScreenAI runtime DLL.");
	}

	if (HashFile(temporary_path) != expected_digest) {
		std::error_code ignored;
		std::filesystem::remove(temporary_path, ignored);

		throw std::runtime_error("A cached ScreenAI runtime DLL failed its SHA-256 check.");
	}

	if (!MoveFileExW(
		temporary_path.c_str(),
		path.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	)) {
		const DWORD error = GetLastError();
		std::error_code ignored;
		std::filesystem::remove(temporary_path, ignored);

		throw std::runtime_error(
			"Could not move a ScreenAI runtime DLL into the cache (Win32 error " +
			std::to_string(error) + ")."
		);
	}
}


HMODULE GetPackageModule() {
	static const int module_anchor = 0;
	HMODULE module = nullptr;

	if (!GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&module_anchor),
		&module
	))
		throw std::runtime_error("Could not locate the ScreenAI SDK module.");

	return module;
}

} // namespace


std::shared_ptr<const Package> Package::LoadEmbedded() {
	static const std::shared_ptr<const Package> package = []() {
		const HMODULE module = GetPackageModule();
		const HRSRC resource = FindResourceW(
			module,
			MAKEINTRESOURCEW(IDR_SCREENAI_RUNTIME),
			MAKEINTRESOURCEW(10)
		);

		if (!resource)
			throw std::runtime_error("The ScreenAI SDK has no embedded runtime package.");

		const DWORD size = SizeofResource(module, resource);
		const HGLOBAL loaded_resource = LoadResource(module, resource);
		const void* data = loaded_resource ? LockResource(loaded_resource) : nullptr;

		if (!size || !data)
			throw std::runtime_error("Could not read the embedded ScreenAI runtime package.");

		return std::shared_ptr<const Package>(
			new Package(static_cast<const uint8_t*>(data), size)
		);
	}();

	return package;
}


Package::Package(const uint8_t* data, size_t size)
	: data_(data),
	  size_(size) {
	this->Parse();
}


void Package::Parse() {
	size_t cursor = 0;
	RequireBytes(cursor, 8, this->size_);

	if (std::memcmp(this->data_, PACKAGE_MAGIC, 8) != 0)
		throw std::runtime_error("The embedded ScreenAI runtime package has an invalid signature.");

	cursor += 8;
	const uint32_t format_version = ReadValue<uint32_t>(this->data_, cursor, this->size_);
	const uint32_t file_count = ReadValue<uint32_t>(this->data_, cursor, this->size_);
	const uint16_t version_length = ReadValue<uint16_t>(this->data_, cursor, this->size_);
	const uint16_t reserved = ReadValue<uint16_t>(this->data_, cursor, this->size_);
	const std::string expected_package_id = ReadString(
		this->data_,
		cursor,
		SHA256_LENGTH,
		this->size_
	);

	if (format_version != PACKAGE_FORMAT_VERSION || reserved != 0 || file_count == 0)
		throw std::runtime_error("The embedded ScreenAI runtime package header is unsupported.");

	this->version_ = ReadString(this->data_, cursor, version_length, this->size_);

	const size_t minimum_entry_size = 2 + 2 + 8 + SHA256_LENGTH + 1;
	if (file_count > (this->size_ - cursor) / minimum_entry_size)
		throw std::runtime_error("The embedded ScreenAI runtime package has an invalid file count.");

	if (this->version_.empty() ||
		!std::all_of(this->version_.begin(), this->version_.end(), [](unsigned char character) {
			return std::isalnum(character) || character == '.' || character == '_' || character == '-';
		}))
		throw std::runtime_error("The embedded ScreenAI runtime package has an invalid version.");

	Sha256 package_hash;
	package_hash.Update(
		reinterpret_cast<const uint8_t*>(this->version_.data()),
		this->version_.size()
	);
	const uint8_t separator = 0;
	package_hash.Update(&separator, sizeof(separator));

	std::unordered_map<std::string, std::string> native_names;
	this->files_.reserve(file_count);
	this->native_library_paths_.reserve(file_count);

	for (uint32_t index = 0; index < file_count; ++index) {
		const uint16_t path_length = ReadValue<uint16_t>(this->data_, cursor, this->size_);
		const uint16_t flags = ReadValue<uint16_t>(this->data_, cursor, this->size_);
		const uint64_t file_size = ReadValue<uint64_t>(this->data_, cursor, this->size_);
		const std::string expected_digest = ReadString(
			this->data_,
			cursor,
			SHA256_LENGTH,
			this->size_
		);
		const std::string path = ReadString(this->data_, cursor, path_length, this->size_);
		const std::string key = NormalizePath(path);

		if (key.empty())
			throw std::runtime_error("The embedded ScreenAI runtime contains an unsafe file path.");
		if (flags & ~FLAG_NATIVE_LIBRARY)
			throw std::runtime_error("The embedded ScreenAI runtime contains unsupported file flags.");
		if (file_size > std::numeric_limits<uint32_t>::max())
			throw std::runtime_error("An embedded ScreenAI runtime file exceeds the callback size limit.");
		RequireBytes(cursor, static_cast<size_t>(file_size), this->size_);

		const char* file_data = reinterpret_cast<const char*>(this->data_ + cursor);
		Sha256 file_hash;
		file_hash.Update(this->data_ + cursor, static_cast<size_t>(file_size));

		if (file_hash.Finish() != expected_digest)
			throw std::runtime_error("An embedded ScreenAI runtime file failed its SHA-256 check.");

		const std::array<uint8_t, sizeof(file_size)> encoded_size = {
			static_cast<uint8_t>(file_size),
			static_cast<uint8_t>(file_size >> 8),
			static_cast<uint8_t>(file_size >> 16),
			static_cast<uint8_t>(file_size >> 24),
			static_cast<uint8_t>(file_size >> 32),
			static_cast<uint8_t>(file_size >> 40),
			static_cast<uint8_t>(file_size >> 48),
			static_cast<uint8_t>(file_size >> 56),
		};
		package_hash.Update(reinterpret_cast<const uint8_t*>(path.data()), path.size());
		package_hash.Update(&separator, sizeof(separator));
		package_hash.Update(encoded_size.data(), encoded_size.size());
		package_hash.Update(
			reinterpret_cast<const uint8_t*>(expected_digest.data()),
			expected_digest.size()
		);

		Entry entry;
		entry.asset = { file_data, static_cast<uint32_t>(file_size) };
		entry.digest = expected_digest;
		entry.is_native_library = (flags & FLAG_NATIVE_LIBRARY) != 0;

		if (!this->files_.emplace(key, std::move(entry)).second)
			throw std::runtime_error("The embedded ScreenAI runtime contains duplicate paths.");

		if (flags & FLAG_NATIVE_LIBRARY) {
			const size_t separator_position = path.find_last_of("/\\");
			const std::string file_name = path.substr(
				separator_position == std::string::npos ? 0 : separator_position + 1
			);
			const std::string name_key = NormalizePath(file_name);

			if (!native_names.emplace(name_key, file_name).second)
				throw std::runtime_error("The embedded ScreenAI runtime has duplicate DLL filenames.");

			this->native_library_paths_.push_back(path);
		}

		cursor += static_cast<size_t>(file_size);
	}

	if (cursor != this->size_ || package_hash.Finish() != expected_package_id)
		throw std::runtime_error("The embedded ScreenAI runtime package failed its integrity check.");

	const auto primary_library = this->files_.find("chrome_screen_ai.dll");

	if (primary_library == this->files_.end() || !primary_library->second.is_native_library)
		throw std::runtime_error("The embedded runtime package has no native chrome_screen_ai.dll.");

	this->package_id_ = Hex(expected_package_id);
}


const std::string& Package::version() const {
	return this->version_;
}


const std::string& Package::package_id() const {
	return this->package_id_;
}


const AssetView* Package::FindFile(std::string_view path) const {
	const std::string key = NormalizePath(path);

	if (key.empty())
		return nullptr;

	const auto found = this->files_.find(key);

	if (found == this->files_.end())
		return nullptr;

	return &found->second.asset;
}


std::filesystem::path Package::ExtractNativeLibraries() const {
	if (this->native_library_paths_.empty())
		throw std::runtime_error("The embedded ScreenAI runtime has no native DLLs.");

	const std::filesystem::path directory = GetCacheDirectory(
		this->version_,
		this->package_id_
	);
	std::error_code error;
	std::filesystem::create_directories(directory, error);

	if (error)
		throw std::runtime_error("Could not create the ScreenAI runtime DLL cache: " + error.message());

	const std::wstring mutex_name =
		L"Local\\ScreenAI_" +
		std::filesystem::u8path(this->version_).wstring() +
		L"_" +
		std::filesystem::u8path(this->package_id_).wstring();
	ScopedMutex lock(mutex_name);

	for (const std::string& relative_path : this->native_library_paths_) {
		const std::string key = NormalizePath(relative_path);
		const Entry& entry = this->files_.at(key);
		const size_t separator_position = relative_path.find_last_of("/\\");
		const std::string file_name = relative_path.substr(
			separator_position == std::string::npos ? 0 : separator_position + 1
		);
		const std::filesystem::path target = directory / std::filesystem::u8path(file_name);

		if (HashFile(target) == entry.digest)
			continue;

		std::filesystem::remove(target, error);
		error.clear();
		WriteAssetAtomically(target, entry.asset, entry.digest);
	}

	const std::filesystem::path primary_library = directory / L"chrome_screen_ai.dll";

	if (!std::filesystem::is_regular_file(primary_library, error) || error)
		throw std::runtime_error("The embedded runtime package has no chrome_screen_ai.dll.");

	return directory;
}

} // namespace screenai::backend::runtime_package

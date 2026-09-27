#include "ArchiveManager.h"

#include "../../General/SerializationUtils.h"
#include "../../General/Utf8.h"
#include "../Command/CommandManager.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>

namespace {

bool IsSafeRelativePath(const std::filesystem::path& path) {
	if (path.empty() || path.is_absolute() || path == std::filesystem::path(".")) {
		return false;
	}
	for (const auto& component : path) {
		if (component == std::filesystem::path("..")) {
			return false;
		}
	}
	return true;
}

} // namespace

ArchiveManager::ArchiveManager(System& sys) : Manager(sys) {
}

ArchiveManager::~ArchiveManager() {
}

void ArchiveManager::Initialize(){
	Get<CommandManager>()->RegisterCommand("set-archive", "Enable/disable packing in backup",
	                                       SetArchive, this);
}

// 该设置只决定是否做正向处理（打包）：关掉时 Backup 里那一步整段跳过，什么也不做，
// 所以这里没有可失败的地方
bool ArchiveManager::SetArchive(bool enabled) {
	this->enabled = enabled;
	return true;
}

bool ArchiveManager::Enabled() const noexcept {
	return enabled;
}

bool ArchiveManager::Pack(const std::vector<FileEntry>& entries,
							std::uint16_t& fileCount,
							std::vector<uint8_t>& archive) const {
	archive.clear();
	 
	std::unordered_set<std::string> paths;
	std::size_t offset = 0;
	for (const FileEntry& entry : entries) {
		const std::filesystem::path& relativePath = entry.Path();
		if (!IsSafeRelativePath(relativePath)) {
			return false;
		}
		const std::string path = Utf8::FromPath(relativePath);
		if (!paths.insert(path).second) {
			return false;
		}

		const FileEntry::SerializedData serializedEntry = entry.Serialize();
		if (!SerializationUtils::WriteBytes(archive, offset, serializedEntry)) {
			return false;
		}
	}

	fileCount = entries.size();
	return true;
}

bool ArchiveManager::Unpack(const std::vector<uint8_t>& archive,
							const std::uint16_t fileCount,
                            std::vector<FileEntry>& entries) const {
    entries.clear();
	std::unordered_set<std::string> paths;
	std::size_t offset = 0;

	for (std::uint32_t index = 0; index < fileCount; ++index) {
		FileEntry::SerializedData serializedEntry;
		if (!SerializationUtils::ReadBytes(archive, offset, serializedEntry)) {
			return false;
		}

		FileEntry entry;
		if (!entry.Deserialize(serializedEntry) || !IsSafeRelativePath(entry.Path()) ||
			!paths.insert(Utf8::FromPath(entry.Path())).second) {
			return false;
		}
		entries.emplace_back(std::move(entry));
	}
	return true;
}

#include "HardLinkIO.h"

#include "FileContentIO.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../../../../General/Debug.h"

namespace fs = std::filesystem;

namespace {

constexpr const char* LogTag = "HardLink";

// 把硬链接条目按来源身份分组，组内保持 entries 顺序
std::map<std::pair<std::uint64_t, std::uint64_t>, std::vector<std::size_t>> GroupHardLinks(
	const std::vector<FileEntry>& entries) {
	std::map<std::pair<std::uint64_t, std::uint64_t>, std::vector<std::size_t>> groups;
	for (std::size_t i = 0; i < entries.size(); ++i) {
		const FileMetaData& metadata = entries[i].MetaData();
		const std::uint64_t fileId = metadata.FileId();
		// fileId == 0 说明没有文件系统身份（合成条目等），不参与分组
		if (metadata.Type() != FileType::HardLink || fileId == 0) {
			continue;
		}
		groups[std::make_pair(metadata.DeviceId(), fileId)].push_back(i);
	}
	return groups;
}

} // namespace

void HardLinkIO::Deduplicate(std::vector<FileEntry>& entries) const {
	for (const auto& group : GroupHardLinks(entries)) {
		const std::vector<std::size_t>& members = group.second;
		for (std::size_t k = 1; k < members.size(); ++k) {
			// 真正释放，不只是清长度
			std::vector<std::uint8_t>().swap(entries[members[k]].Content());
		}
	}
}

void HardLinkIO::Relink(const std::vector<FileEntry>& entries,
                        const std::vector<std::optional<fs::path>>& targets,
                        const FileContentIO& contentIO) const {
	constexpr std::size_t none = static_cast<std::size_t>(-1);

	for (const auto& group : GroupHardLinks(entries)) {
		const std::vector<std::size_t>& members = group.second;
		if (members.size() < 2) {
			continue;
		}

		// 去重后组内只有内容源非空；全空说明这组本来就是 0 字节文件
		std::size_t source = none;
		for (const std::size_t index : members) {
			if (!entries[index].Content().empty()) {
				source = index;
				break;
			}
		}

		// 代表必须已经写在盘上：内容源写成功就直接用它，否则退选组内第一个写成功的，
		// 并把内容补写到那里
		std::size_t representative = none;
		if (source != none && targets[source].has_value()) {
			representative = source;
		} else {
			for (const std::size_t index : members) {
				if (targets[index].has_value()) {
					representative = index;
					break;
				}
			}
			if (representative == none) {
				continue;   // 整组都没写出来，没什么可做
			}
			if (source != none && representative != source) {
				contentIO.Write(targets[representative].value(), entries[source].Content(),
				                entries[representative].MetaData().Type());
			}
		}

		for (const std::size_t index : members) {
			if (index == representative || !targets[index].has_value()) {
				continue;
			}
			if (Replace(targets[index].value(), targets[representative].value())) {
				continue;
			}
			// 建链失败：这个成员是去重后的空副本，必须把内容补回去。否则 Replace 已经
			// 删掉副本时，降级出来的不是"多占空间的独立副本"而是空文件甚至缺失
			contentIO.Write(targets[index].value(),
			                source == none ? entries[index].Content()
			                               : entries[source].Content(),
			                entries[index].MetaData().Type());
			Debug::Warning("Failed to restore hard link: " + targets[index]->string(), LogTag);
		}
	}
}

#ifdef _WIN32

bool HardLinkIO::Replace(const fs::path& linkPath, const fs::path& existingPath) const {
	const std::wstring linkNative = linkPath.wstring();
	const std::wstring existingNative = existingPath.wstring();

	if (DeleteFileW(linkNative.c_str()) == FALSE) {
		const DWORD error = GetLastError();
		if (error != ERROR_FILE_NOT_FOUND) {
			Debug::Error("Failed to drop the copy before linking: " + linkPath.string() +
			                 " (win32 error " + std::to_string(error) + ")",
			             LogTag);
			return false;
		}
	}

	if (CreateHardLinkW(linkNative.c_str(), existingNative.c_str(), nullptr) == FALSE) {
		Debug::Error("Failed to create hard link: " + linkPath.string() + " -> " +
		                 existingPath.string() + " (win32 error " +
		                 std::to_string(GetLastError()) + ")",
		             LogTag);
		return false;
	}
	return true;
}

#else

bool HardLinkIO::Replace(const fs::path& linkPath, const fs::path&) const {
	Debug::Warning("Hard link is not supported on this platform: " + linkPath.string(),
	               LogTag);
	return false;
}

#endif

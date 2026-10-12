#include "PathUtils.h"

#include <windows.h>

#include <string>
#include <vector>

namespace {

// GetModuleFileNameW 会截断，所以给不够就翻倍重试
std::filesystem::path QueryExecutablePath() {
	std::vector<wchar_t> buffer(MAX_PATH);
	for (;;) {
		const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(),
		                                          static_cast<DWORD>(buffer.size()));
		if (length == 0) {
			return {}; // 理论上不会发生
		}
		if (length < buffer.size()) {
			return std::filesystem::path(std::wstring(buffer.data(), length));
		}
		if (buffer.size() >= 32768) { // Windows 的路径长度上限
			return {};
		}
		buffer.resize(buffer.size() * 2);
	}
}

} // namespace

std::filesystem::path PathUtils::ExecutablePath() {
	// 同一个进程里它不会变，量一次就够
	static const std::filesystem::path cached = QueryExecutablePath();
	return cached;
}

std::filesystem::path PathUtils::ExecutableDirectory() {
	const std::filesystem::path exe = ExecutablePath();
	return exe.empty() ? std::filesystem::path{} : exe.parent_path();
}

std::filesystem::path PathUtils::BesideExecutable(const std::filesystem::path& relative) {
	const std::filesystem::path directory = ExecutableDirectory();
	return directory.empty() ? relative : directory / relative;
}

#pragma once


#include <filesystem>
#include <vector>

class FileContentIO;
class FileMetaDataIO;

class PathHandler
{
public:
    PathHandler();
    ~PathHandler();

    std::filesystem::path GetCurrentPath();
    bool SetCurrentPath(const std::filesystem::path& path);
    std::filesystem::path ToAbsolute(const std::filesystem::path& relative);
    std::filesystem::path ToRelative(const std::filesystem::path& absolute);
    bool CD(const std::string& target);
    
    bool CheckPath(const std::filesystem::path& path);
    bool CheckRegular(const std::filesystem::path& path);

    // ---- 跟"程序自己在哪里"有关的路径 ----
    //
    // 进程的当前目录是会被改掉的（用户敲 cd、界面里换目录，甚至别的程序），所以凡是
    // 程序自己要长期读写的文件（存档、配置……）都不该用相对路径或 current_path ——
    // 那样"从哪个目录启动"就变成了程序行为的一部分。这里统一以可执行文件所在目录为基准。

    // 可执行文件的完整路径；取不到（或不是 Windows）返回空 path
    std::filesystem::path ExecutablePath() const;
    // 可执行文件所在目录；取不到时返回空 path
    std::filesystem::path ExecutableDirectory() const;
    // 把相对路径挂到可执行文件所在目录下；
    // 万一连目录都取不到，就原样返回（退化成相对当前目录，至少还能跑）
    std::filesystem::path BesideExecutable(const std::filesystem::path& relative) const;
private:
};          
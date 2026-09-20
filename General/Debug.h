#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <mutex>

enum class ErrorLevel {
    Info,
    Warning,
    Error,
    Critical
};

struct ErrorEntry {
    std::string tag;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    ErrorLevel level;
    
    ErrorEntry(const std::string& t, const std::string& msg, ErrorLevel lvl = ErrorLevel::Info);
};

class Debug {
    public:
    Debug() = delete;
    ~Debug() = delete;
    
    static void Log( const std::string& message, const std::string& tag = DefaultTag, ErrorLevel level = ErrorLevel::Info);
    static void Info(const std::string& message, const std::string& tag = DefaultTag);
    static void Warning(const std::string& message, const std::string& tag = DefaultTag);
    static void Error(const std::string& message, const std::string& tag = DefaultTag);
    static void Critical(const std::string& message, const std::string& tag = DefaultTag);

    static std::vector<ErrorEntry> GetAllErrors();
    static std::vector<ErrorEntry> GetErrorsByTag(const std::string& tag);
    static std::vector<ErrorEntry> GetLastErrors(size_t count);
    
    static void Clear();
    static void ClearByTag(const std::string& tag);
    
    static std::string ToStringByTag(const std::string& tag);
    static std::string ToStringAll();
    static std::string ToJson(const std::string& tag = "");
    
    static size_t GetErrorCount();
    static size_t GetErrorCountByTag(const std::string& tag);
    static bool HasErrors();
    static bool HasErrorsAbove(ErrorLevel level);
    static std::string LevelToString(ErrorLevel level);
    
    private:
    static constexpr const char* DefaultTag = "Default";
    inline static std::vector<ErrorEntry> errors;
    inline static std::mutex mutex;  

    static std::vector<ErrorEntry>& getErrors();
    static std::mutex& getMutex();
    static std::string formatEntry(const ErrorEntry& entry, size_t index = 0, size_t total = 0);
    static std::string formatTime(const std::chrono::system_clock::time_point& time);
    static std::string escapeJson(const std::string& str);
};

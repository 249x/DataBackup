#include "CommandManager.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include "../FileIO/FileIOManager.h"

#include "../../General/Debug.h"

CommandManager::CommandManager(System& sys) : Manager(sys) {}

CommandManager::~CommandManager() = default;

void CommandManager::Initialize() {
    RegisterCommand("help", "Print all commands description",
                    [this]() -> bool {
                        PrintCommands();
                        return true;
                    });
    RegisterCommand("run", "Run commands in path", RunCommands, this);
}

bool CommandManager::RegisterEntry(std::unique_ptr<CommandEntry> entry) {
    if (!entry || !entry->IsRegistered()) {
        return false;
    }
    const std::string& name = entry->GetName();
    auto result = commands.emplace(name, std::move(entry));
    return result.second;
}

bool CommandManager::UnregisterCommand(const std::string& name) {
    return commands.erase(name) > 0;
}

bool CommandManager::HasCommand(const std::string& name) const {
    return commands.find(name) != commands.end();
}

bool CommandManager::Execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return false;
    }

    const std::string& commandName = args.front();
    auto it = commands.find(commandName);
    if (it == commands.end()) {
        Debug::Error("No command is found", "Command");
        return false;
    }

    CommandEntry& entry = *it->second;
    const std::vector<std::type_index>& expectedTypes = entry.GetTypes();

    std::vector<const char*> rawArgs;
    rawArgs.reserve(args.size() - 1);
    for (std::size_t i = 1; i < args.size(); ++i) {
        rawArgs.push_back(args[i].c_str());
    }

    std::vector<std::any> parsedValues;
    if (!parser.ParseBatch(rawArgs, expectedTypes, parsedValues)) {
        return false;
    }

    return entry.ExecuteAny(parsedValues);
}

std::vector<std::string> CommandManager::Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;
    bool started = false; // 引号也算 token 的开始，所以 "" 能切出一个空 token

    for (const char c : line) {
        if (c == '"') {
            inQuote = !inQuote;
            started = true;
            continue;
        }
        if (!inQuote && std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (started) {
                tokens.emplace_back(std::move(token));
                token.clear();
                started = false;
            }
            continue;
        }
        token.push_back(c);
        started = true;
    }

    if (inQuote) {
        Debug::Warning("Unmatched '\"' in: " + line, "Command");
    }
    if (started) {
        tokens.emplace_back(std::move(token));
    }
    return tokens;
}

bool CommandManager::Execute(const std::string& commandLine) {
    std::istringstream stream(commandLine);
    std::string line;
    bool lastResult = true;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // 命令文件多半是 CRLF
        }
        if (line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }

        const std::vector<std::string> tokens = Tokenize(line);
        if (!tokens.empty()) {
            lastResult = Execute(tokens);
            // if (!lastResult) break;
        }
    }
    return lastResult;
}

void CommandManager::RunConsole(const std::string& prompt) {
    std::string line;

    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, line)) {
        std::cout << "\n";
        return;
    }

    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    auto first = std::find_if(line.begin(), line.end(), notSpace);
    if (first == line.end()) {
        return;
    }
    auto last = std::find_if(line.rbegin(), line.rend(), notSpace).base();
    std::string trimmed(first, last);

    if (trimmed == "quit" || trimmed == "exit") {
        exit(0);
    }
    Execute(trimmed);
}
void CommandManager::PrintCommands() const {
    for (const auto& [name, entry] : commands) {
        std::cout << entry->ToString() << "\n";
    }
}

bool CommandManager::RunCommands(const std::filesystem::path& path){
    FileIOManager* IO = Get<FileIOManager>();
    std::string commands;
    if(!IO->ReadText(path, commands)){
        return false;
    }
    if(!Execute(commands)){
        return false;
    }
    return true;
}
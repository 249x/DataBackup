#include "CommandManager.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

CommandManager::CommandManager(System& sys) : Manager(sys) {}

CommandManager::~CommandManager() = default;

void CommandManager::Initialize() {
    RegisterCommand("help", "Print all commands description",
                    [this]() -> bool {
                        PrintCommands();
                        return true;   // 正常执行返回 true
                    });
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
        return false;
    }

    CommandEntry& entry = *it->second;
    const std::vector<std::type_index>& expectedTypes = entry.GetTypes();
    const std::size_t expectedCount = expectedTypes.size();
    const std::size_t suppliedCount = args.size() - 1;

    if (expectedCount != suppliedCount) {
        return false;
    }

    std::vector<const char*> rawArgs;
    rawArgs.reserve(suppliedCount);
    for (std::size_t i = 1; i < args.size(); ++i) {
        rawArgs.push_back(args[i].c_str());
    }

    std::vector<std::any> parsedValues;
    if (!parser.ParseBatch(rawArgs, expectedTypes, parsedValues)) {
        return false;
    }

    return entry.ExecuteAny(parsedValues);
}

bool CommandManager::Execute(const std::string& commandLine) {
    std::istringstream stream(commandLine);
    std::string line;
    bool lastResult = true;

    while (std::getline(stream, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;
        }

        std::istringstream lineStream(line);
        std::vector<std::string> tokens;
        std::string token;
        while (lineStream >> token) {
            tokens.push_back(token);
        }

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
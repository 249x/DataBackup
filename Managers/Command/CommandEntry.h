#pragma once

#include <any>
#include <array>
#include <functional>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

class CommandEntry {
public:
    CommandEntry(std::string name, std::string description);
    ~CommandEntry();

    template<typename... Args>
    bool Register(std::function<bool(Args...)> func) {
        if (registered) {
            return false;
        }
        types = {typeid(std::decay_t<Args>)...};
        arity = sizeof...(Args);
        callback = [func = std::move(func)](const std::vector<std::any>& args) -> bool {
            if (args.size() != sizeof...(Args)) {
                return false;
            }
            return InvokeHelper(func, args, std::index_sequence_for<Args...>{});
        };

        registered = true;
        return true;
    }

    template<typename... Args>
    bool Execute(Args&&... args) const {
        std::vector<std::any> anyArgs;
        anyArgs.reserve(sizeof...(Args));
        (anyArgs.emplace_back(std::forward<Args>(args)), ...);
        return ExecuteAny(anyArgs);
    }

    template<typename... Args>
    bool IsMatch() const {
        if (sizeof...(Args) != types.size()) {
            return false;
        }
        std::array<std::type_index, sizeof...(Args)> check = {
            typeid(std::decay_t<Args>)...
        };
        return std::equal(check.begin(), check.end(), types.begin());
    }

    bool ExecuteAny(const std::vector<std::any>& args) const;
    bool IsMatch(const std::vector<std::type_index>& otherTypes) const;

    const std::string& GetName() const;
    const std::string& GetDescription() const;
    const std::vector<std::type_index>& GetTypes() const;
    std::size_t GetArity() const;
    bool IsRegistered() const;

    std::string ToString() const;

private:
    template<typename... Args, std::size_t... Is>
    static bool InvokeHelper(const std::function<bool(Args...)>& func,
                             const std::vector<std::any>& args,
                             std::index_sequence<Is...>) {
        try {
            return func(std::any_cast<std::decay_t<Args>>(args[Is])...);
        } catch (const std::bad_any_cast&) {
            return false;
        }
    }

    static std::string ReadableTypeName(const std::type_index& idx);

private:
    std::function<bool(const std::vector<std::any>&)> callback;
    std::vector<std::type_index> types;
    std::string name;
    std::string description;
    std::size_t arity = 0;
    bool registered = false;
};
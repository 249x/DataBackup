#pragma once

#include "../Manager.h"
#include "CommandEntry.h"
#include "ArgumentParser.h"

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <filesystem>

namespace detail {

    // 主模板
    template<typename F>
    struct FunctionTraits;

    // 普通函数类型
    template<typename R, typename... Args>
    struct FunctionTraits<R(Args...)> {
        using ReturnType = R;
        using ArgsTuple  = std::tuple<Args...>;
        static constexpr std::size_t Arity = sizeof...(Args);
    };

    // 函数指针
    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...)> : FunctionTraits<R(Args...)> {};

    // 函数引用
    template<typename R, typename... Args>
    struct FunctionTraits<R(&)(Args...)> : FunctionTraits<R(Args...)> {};

    // 成员函数指针
    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...)> : FunctionTraits<R(Args...)> {};

    // const 成员函数指针
    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const> : FunctionTraits<R(Args...)> {};

    // noexcept 版本
    template<typename R, typename... Args>
    struct FunctionTraits<R(Args...) noexcept> : FunctionTraits<R(Args...)> {};

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...) noexcept> : FunctionTraits<R(Args...)> {};

    // 通用可调用对象（lambda、std::function、仿函数）
    template<typename F>
    struct FunctionTraits
        : FunctionTraits<decltype(&std::decay_t<F>::operator())> {};
}


class CommandManager : public Manager
{
public:
    CommandManager(System& sys);
    ~CommandManager() override;

    void Initialize() override;

    template<typename F>
    bool RegisterCommand(const std::string& name,
                         const std::string& description,
                         F&& func)
    {
        using Decayed = std::decay_t<F>;
        return RegisterCommandImpl(
            name, description, std::forward<F>(func),
            std::make_index_sequence<detail::FunctionTraits<Decayed>::Arity>{}
        );
    }

    template<typename C, typename R, typename... Args>
    bool RegisterCommand(const std::string& name,
                         const std::string& description,
                         R(C::*func)(Args...), C* obj)
    {
        return RegisterCommand(name, description,
            [obj, func](Args... args) -> R {
                return (obj->*func)(std::forward<Args>(args)...);
            });
    }

    bool RegisterEntry(std::unique_ptr<CommandEntry> entry);
    bool UnregisterCommand(const std::string& name);
    bool HasCommand(const std::string& name) const;

    bool Execute(const std::vector<std::string>& args);
    bool Execute(const std::string& commandLine);

    void RunConsole(const std::string& prompt = "> ");
    bool RunCommands(const std::filesystem::path& path);
    
    void PrintCommands() const;

private:
    std::unordered_map<std::string, std::unique_ptr<CommandEntry>> commands;
    ArgumentParser parser;

    template<typename F, std::size_t... Is>
    bool RegisterCommandImpl(const std::string& name,
                             const std::string& description,
                             F&& func,
                             std::index_sequence<Is...>)
    {
        using Traits = detail::FunctionTraits<std::decay_t<F>>;
        using ArgsTuple = typename Traits::ArgsTuple;

        std::function<bool(std::tuple_element_t<Is, ArgsTuple>...)> wrapper =
            std::forward<F>(func);

        auto entry = std::make_unique<CommandEntry>(name, description);
        if (!entry->Register<std::tuple_element_t<Is, ArgsTuple>...>(std::move(wrapper))) {
            return false;
        }
        return RegisterEntry(std::move(entry));
    }
};
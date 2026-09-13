#pragma once

#include <any>
#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

class ArgumentParser {
public:
    ArgumentParser();
    ~ArgumentParser();

    template<typename T>
    void Register(std::function<T(const char*)> converter) {
        static_assert(!std::is_reference_v<T>,
                      "ArgumentParser::Register<T> requires a non-reference type");
        converters[typeid(std::decay_t<T>)] =
            [converter = std::move(converter)](const char* input) -> std::any {
                return std::any(converter(input));
            };
    }

    template<typename T>
    void RegisterDefault() {
        Register<T>([](const char* input) -> T {
            return T(input);
        });
    }

    template<typename T>
    bool Parse(const char* input, T& outValue) const {
        std::any out;
        if (!Parse(typeid(std::decay_t<T>), input, out)) {
            return false;
        }
        try {
            outValue = std::any_cast<std::decay_t<T>>(std::move(out));
        } catch (const std::bad_any_cast&) {
            return false;
        }
        return true;
    }

    bool Parse(const std::type_index& type, const char* input, std::any& out) const;

    bool ParseBatch(const std::vector<const char*>& inputs,
                    const std::vector<std::type_index>& types,
                    std::vector<std::any>& outValues) const;

    bool IsRegistered(const std::type_index& type) const;

private:
    using Converter = std::function<std::any(const char*)>;
    std::unordered_map<std::type_index, Converter> converters;

    void registerCommonTypes();
};
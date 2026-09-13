#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>

template <typename T>
class TypeContainer {
public:
    TypeContainer() = default;
    ~TypeContainer() = default;

    template <typename U, typename... Args>
    U* Register(Args&&... args) {
        static_assert(std::is_base_of_v<T, U>, "U must be derived from T");
        
        auto key = std::type_index(typeid(U));
        auto it = objectsMap.find(key);
        if (it != objectsMap.end()) {
            return static_cast<U*>(it->second.get());
        }

        auto ptr = std::make_unique<U>(std::forward<Args>(args)...);
        U* ref = ptr.get();
        objectsMap.emplace(key, std::move(ptr));
        return ref;
    }

    template <typename U>
    U* Get() {
        auto key = std::type_index(typeid(U));
        auto it = objectsMap.find(key);
        if (it == objectsMap.end()) return nullptr;
        return static_cast<U*>(it->second.get());
    }
    
    T* Get(const std::type_index& index) {
        auto it = objectsMap.find(index);
        if (it == objectsMap.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    template <typename U>
    bool Contains() const {
        return objectsMap.find(std::type_index(typeid(U))) != objectsMap.end();
    }

    template <typename U>
    bool Remove() {
        return objectsMap.erase(std::type_index(typeid(U))) > 0;
    }

    void Clear() { objectsMap.clear(); }
    std::size_t Size() const { return objectsMap.size(); }

    template <typename Func>
    void ForEach(Func&& func) {
        for (auto& [key, ptr] : objectsMap) {
            func(ptr.get());
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<T>> objectsMap;
};
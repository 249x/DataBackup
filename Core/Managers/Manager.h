#pragma once
#include "../System.h"
#include <string>

class Manager
{
private:
    System* system = nullptr;

public:
    Manager() = default;
    Manager(System& sys);
    virtual ~Manager();
    virtual void Initialize();

    std::string GetTypeName() const;

    template <typename T>
    T* Get() {
        return system->Get<T>();
    }

    template <typename T>
    T* Register() {
        return system->Register<T>();
    }

    inline System& GetSystem() {
        return *system;
    }
};
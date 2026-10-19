#pragma once

#include <utility>

#include "General/TypeContainer.h"

class Manager;

class System
{
private:
    TypeContainer<Manager> managers;

public:
    System();
    ~System();

    void Start();

    int Run(int argc, char* argv[]);

    template <typename T>
    T* Get() {
        return managers.Get<T>();
    }

    template <typename T>
    T* Register() {
        return managers.Register<T>(*this);
    }

    template <typename Func>
    void ForEach(Func&& func) {
        managers.ForEach(std::forward<Func>(func));
    }
};
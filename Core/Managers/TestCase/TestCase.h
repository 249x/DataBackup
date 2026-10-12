#pragma once

#include <functional>
#include <string>

struct TestCase {
    std::string name;
    std::function<bool()> run;
    bool expected = true;

    TestCase() = default;

    TestCase(std::string n, std::function<bool()> fn, bool exp = true)
        : name(std::move(n)), run(std::move(fn)), expected(exp) {}
};
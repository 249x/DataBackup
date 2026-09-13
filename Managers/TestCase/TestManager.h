#include "../Manager.h"
#include "TestCase.h"

class TestManager : public Manager
{
public:
    TestManager(System& sys);
    ~TestManager() override;

    void Initialize() override;
    void Register(const std::string& name, std::function<bool()> fn, bool expected = true);
    void RunAll() const;
private:
    std::vector<TestCase> cases;

};
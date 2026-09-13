#include "Manager.h"

Manager::Manager(System& sys) : system(&sys)
{
}

Manager::~Manager()
{
}

void Manager::Initialize()
{

}

std::string Manager::GetTypeName() const {
    return typeid(*this).name();
}

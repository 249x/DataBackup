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

void Manager::OnDestroy()
{
    
}

std::string Manager::GetTypeName() const {
    return typeid(*this).name();
}

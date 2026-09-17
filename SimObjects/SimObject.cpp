#include "SimObject.h"

//Update this as soon as you create a physics world
std::shared_ptr<PhysicsWorld> SimObject::world = nullptr;

//Overwritten from the hosting/ settings in the LoopServer constructor, these are the defaults it registers
float NetRelevanceSettings::nearDistance = 300.0f;
float NetRelevanceSettings::midDistance = 700.0f;
float NetRelevanceSettings::farDistance = 1500.0f;
int NetRelevanceSettings::bytesPerTick = 8192;
float NetRelevanceSettings::highPingFactor = 0.5f;
float NetRelevanceSettings::veryHighPingFactor = 0.25f;

/*	
	This is a virtual class with its creation handled by a factory class so there's not really anything to go here
	Constructor is private/protected
*/
SimObject::SimObject()
{

}

SimObject::~SimObject()
{

}

bool SimObject::requiresNetUpdate()// const
{
    return requiresUpdate;
}

netIDType SimObject::getID() const
{
    return netID;
}

uint32_t SimObject::getCreationTime() const
{
    return creationTime;
}


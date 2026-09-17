#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/ClientLua.h"
#include "../../LuaFunctions/SoundLua.h"
#include "../../LuaFunctions/VehicleLua.h"

#include <cmath>

//How far from the camera a vehicle can be right clicked to get in, like the old game
static constexpr float vehicleReach = 30.0f;

//Honking while driving waits this long between honks, like the old game
static constexpr unsigned int honkCooldownMS = 350;

//Nobody drives it and nobody stands on any of its seats, so left clicking it flips it upright
static bool isVehicleEmpty(const Vehicle& vehicle)
{
	if (vehicle.driverID != NO_ID)
		return false;

	for (const PassengerSeat& seat : vehicle.passengerSeats)
		if (seat.riderID != NO_ID)
			return false;

	return true;
}

/*
	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void clickDetails(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2 + sizeof(float) * 6)
		return;

	int byteIteartor = 1;

	glm::vec3 pos, dir;

	memcpy(&pos, packet->data + byteIteartor, sizeof(glm::vec3));
	byteIteartor += sizeof(glm::vec3);

	memcpy(&dir, packet->data + byteIteartor, sizeof(glm::vec3));
	byteIteartor += sizeof(glm::vec3);

	unsigned char mask = packet->data[byteIteartor];
	byteIteartor++;

	unsigned char clickFlags = byteIteartor < packet->dataLength ? packet->data[byteIteartor] : 0;
	bool release = clickFlags & ClickFlag_Release;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	bool usable = client && !release && std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) && std::isfinite(dir.x) && std::isfinite(dir.y) && std::isfinite(dir.z) && glm::length(dir) > 0.0001f;

	if (usable && (clickFlags & ClickFlag_RightPress))
	{
		//Right click gets out of a vehicle, or into the one under the crosshair, like the old game
		if (!client->vehicle.expired())
		{
			exitVehicle(*client, true);
			return;
		}

		btRigidBody* ignore = client->controlledObjects.empty() ? nullptr : client->controlledObjects[0]->body;
		btVector3 hitPosition, hitNormal;
		btRigidBody* hit = pd->physicsWorld->doRaycast(g2b3(pos), g2b3(pos + glm::normalize(dir) * vehicleReach), ignore, hitPosition, hitNormal);
		if (std::shared_ptr<Vehicle> vehicle = vehicleFromBody(hit))
		{
			//Nobody driving it, they drive, otherwise they stand on the free seat nearest where they clicked
			if (vehicle->driverID == NO_ID)
			{
				if (enterVehicle(*client, vehicle, Vehicle::driverSeat, true))
					return;
			}
			else if (!vehicle->passengerSeats.empty())
			{
				int seat = vehicle->findFreeSeat(b2g3(hitPosition));
				if (seat == -1)
					source->sendCenterPrint("Every seat on this vehicle is taken.", 2000, 1.0f, 1.0f, 1.0f);
				else if (enterVehicle(*client, vehicle, seat, true))
					return;
			}
		}
	}
	else if (usable && (clickFlags & ClickFlag_LeftPress))
	{
		//Left click honks while driving, with whatever horn the vehicle was wrenched to have (the old game's Honk sound unless changed)
		//The sound rides on the vehicle, so the Doppler effect leaves it alone for the driver, who moves with it, and bends it for everyone else
		std::shared_ptr<Vehicle> vehicle = client->vehicle.lock();
		if (vehicle && vehicle->body && client->vehicleSeat == Vehicle::driverSeat && SDL_GetTicks() - client->lastHonkMS > honkCooldownMS)
		{
			client->lastHonkMS = SDL_GetTicks();
			if (!vehicle->hornName.empty())
				playSoundOnVehicle(vehicle->hornName, vehicle, 1.0f, 1.0f);
		}
		else if (!vehicle && !client->getHeldItem())
		{
			//An empty hand left clicking a vehicle nobody is in stands it back on its wheels, for getting one unstuck
			//A tool in their hand gets the click instead, so a wrench still opens the vehicle's dialog rather than flipping it out from under the crosshair
			btRigidBody* ignore = client->controlledObjects.empty() ? nullptr : client->controlledObjects[0]->body;
			btVector3 hitPosition, hitNormal;
			btRigidBody* hit = pd->physicsWorld->doRaycast(g2b3(pos), g2b3(pos + glm::normalize(dir) * vehicleReach), ignore, hitPosition, hitNormal);
			std::shared_ptr<Vehicle> clicked = vehicleFromBody(hit);
			if (clicked && isVehicleEmpty(*clicked))
				clicked->flipUpright();
		}
	}

	pushClientLua(pd->luaState, source->me);
	lua_pushnumber(pd->luaState, pos.x);
	lua_pushnumber(pd->luaState, pos.y);
	lua_pushnumber(pd->luaState, pos.z);
	lua_pushnumber(pd->luaState, dir.x);
	lua_pushnumber(pd->luaState, dir.y);
	lua_pushnumber(pd->luaState, dir.z);
	lua_pushnumber(pd->luaState, mask);
	pd->eventManager->callEvent(pd->luaState, release ? "ClientClickRelease" : "ClientClick", 8);

	//Either return values will be correct, or they will be zero, do nothing special if lua functions messed up the event
	if (lua_gettop(pd->luaState) != 0)
	{
		//Nothing specific to do at the moment for a click, it's all in the scripts
		lua_settop(pd->luaState, 0);
	}
}

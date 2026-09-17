#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/VehicleLua.h"

/*
	1 byte		-	packet type
	1 byte		-	1 to turn the flashlight on, 0 for off, or FlashlightRequest_VehicleLight to switch the headlight of the vehicle they drive
	12 bytes	-	red, green, blue floats, 0-1, ignored for a headlight

	Do not attempt to assign a handle to JoinedClient to other objects directly
	Grab a smart pointer from the server for this
	server->getClientByNetId(source->getNetId());
	Also do not attempt to delete the JoinedClient, just call kick
*/
void flashlightRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	if (packet->dataLength < 2 + sizeof(float) * 3)
		return;

	auto clientData = pd->getClient(source->me);
	if (!clientData)
		return;

	//The driver's flashlight key works the vehicle's headlight instead of their own light, see LoopClient::updateFlashlight
	if (packet->data[1] == FlashlightRequest_VehicleLight)
	{
		std::shared_ptr<Vehicle> vehicle = clientData->vehicle.lock();
		if (vehicle && clientData->vehicleSeat == Vehicle::driverSeat && vehicle->headlight.hasLight)
			setVehicleHeadlightOn(*vehicle, !vehicle->headlightOn, true);
		return;
	}

	bool on = packet->data[1];

	float color[3];
	memcpy(color, packet->data + 2, sizeof(float) * 3);
	for (float channel : color)
		if (!std::isfinite(channel))
			return;

	//Ignored while Lua has it disabled
	clientData->setFlashlight(pd, on, glm::vec3(color[0], color[1], color[2]));
}

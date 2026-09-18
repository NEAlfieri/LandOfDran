#include "../Server.h"
#include "../../GameLoop/ServerProgramData.h"
#include "../../LuaFunctions/VehicleLua.h"

/*
	1 byte		-	packet type

	The client pressed their next seat key (comma by default) while in a vehicle: they move to its next free seat, the driver's seat
	then the passenger seats in order and around again, without getting out in between. Nothing happens if every other seat is taken
	or the vehicle has no other seat, and a ClientEnterVehicle listener can keep them where they are, see switchSeat
*/
void seatCycleRequest(JoinedClient* source, Server const* const server, ENetPacket const* const packet, const void* pdv)
{
	//This is only needed because I needed to avoid a circular dependancy by not including SPD in Server.h
	const ServerProgramData* pd = (const ServerProgramData*)pdv;

	std::shared_ptr<ClientData> client = pd->getClient(source->me);
	if (!client)
		return;

	std::shared_ptr<Vehicle> vehicle = client->vehicle.lock();
	if (!vehicle)
		return;

	int seat = nextFreeSeat(*client, *vehicle);
	if (seat == client->vehicleSeat)
	{
		source->sendCenterPrint(vehicle->passengerSeats.empty() ? "This vehicle has no other seats." : "Every other seat on this vehicle is taken.", 2000, 1.0f, 1.0f, 1.0f);
		return;
	}

	switchSeat(*client, seat, true);
}

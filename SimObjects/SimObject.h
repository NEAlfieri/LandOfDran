#pragma once

#include "../Networking/ObjHolder.h"
#include "../Physics/PhysicsWorld.h"

/*
	Abstract base class for anything which has state that is maintained by the server
	and is referenced on the client.
*/
class SimObject
{
	//Should be the factory class that creates and deletes all SimObjects
	friend ObjHolder<SimObject>;

	protected:

	//Called by objHolder when destroy is first called, gives object an oppertunity to reset smart pointers it might have
	virtual void requestDestruction() = 0;

	explicit SimObject();

	//Set by ObjHolder in milliseconds since program start:
	uint32_t creationTime = 0;
	
	/*
		Set by ObjHolder on creation
		Unique to objects of this type, i.e.no 2 dynamics will ever share an id, but a brick and a dynamic might
	*/
	netIDType netID = 0;

	/*
		Has something changed on this object such that clients need to be sent an update over the internet
		since the last time updates were sent out on this object type
	*/
	bool requiresUpdate = false;

	/*
		Set by ObjHolder on creation
		Allows the object to give out a common smart pointer to itself instead of 'this' to other objects it makes that may want a reference back to it
	*/
	std::shared_ptr<SimObject> me;

	public:

	//Set during ObjHolder::sendRecent and reset at the end of that function
	bool flaggedForUpdate = false;

	//Never use 'this' always use this
	std::shared_ptr<SimObject> getMe() const
	{
		return me;
	}

	//Yes, global scope is bad and all but we really can only have one dynamicsWorld and it's needed constantly by everything
	//The methods I used to use to get around making this global were honestly far worse
	static std::shared_ptr<PhysicsWorld> world;

	//Has this object been updated in such a way that we need to resend its properties to clients?
	virtual bool requiresNetUpdate();// const;

	//Get netID
	netIDType getID() const;

	//Time in MS since program start
	uint32_t getCreationTime() const;

	//Called after this object has an id, type, me pointer, and vector index assigned by ObjHolder
	virtual void onCreation() = 0;

	//Server: the Lua metatable ObjHolder::pushLua gives this object instead of its holder's, nullptr for the holder's
	virtual const char* getLuaMetatable() const { return nullptr; }

	//How many bytes would this add to a packet creating objects if it was added to it
	virtual unsigned int getCreationPacketBytes() const = 0;

	//How many bytes would this add to a packet updating objects if it was added to it
	virtual unsigned int getUpdatePacketBytes() const = 0;

	/*
		Server: where in the world this object is, for deciding how often a given client needs to hear about it,
		see ObjHolder::sendRecent
		Returning false means updates for it always go to everyone at full rate, which is the right answer for
		anything with no meaningful position of its own, or that clients need regardless of where they're standing
	*/
	virtual bool getNetRelevancePosition(glm::vec3& position) const { return false; }

	/*
		Server: makes the next update for this object carry its whole state rather than anything measured against
		what a client is assumed to already have. Called for a client that just got sent creation packets, since
		nothing it holds yet can be delta compressed against, see ObjHolder::sendAll
	*/
	virtual void requireFullNetUpdate() {}

	/*
		Server: whether later updates for this object are measured against the one just written into a packet, which
		means a client that skips this one can't use those either, see Dynamic::writeUpdatePosition
		Distance throttling sends these to everyone who can see the object at all rather than skipping them, since
		skipping one costs a client every update until the next, not just this one
	*/
	virtual bool lastUpdateAnchorsLaterOnes() const { return false; }

	/*
		Server: an update says how long the client receiving it should play it back over, which is the rate the server
		writes them at - but a client far enough away is only sent every second or fourth one, and playing those back
		at the writing rate means a step of movement and then a wait. The update is staged before we know which band
		any client is in, so this rewrites that interval in a copy already placed in a packet
		Types whose updates carry no interpolated transform have nothing to rewrite
	*/
	virtual void scaleUpdateInterval(enet_uint8* update, unsigned int multiplier) const {}

	//Add getCreationPacketBytes() worth of data to the given packet with all the data needed for the client to create it
	virtual void addToCreationPacket(enet_uint8* dest) const = 0;

	//Add getUpdatePacketBytes() worth of data to the given packet with all the data needed for the client to update it
	virtual void addToUpdatePacket(enet_uint8* dest) = 0;

	virtual ~SimObject();
};


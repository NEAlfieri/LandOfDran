#pragma once

#include "SimObject.h"
#include "Dynamic.h"
#include "Vehicle.h"
#include "../Physics/RopeConstraint.h"

//What one end of a Rope is tied to, sent with its state
enum RopeAnchorKind : unsigned char
{
	RopeAnchorFixed = 0,	//A spot in the world
	RopeAnchorDynamic = 1,	//A spot on a dynamic, which goes along with it
	RopeAnchorBrick = 2,	//A spot in the world, on a brick, the rope is removed along with the brick
	RopeAnchorVehicle = 3	//A spot on a vehicle, which goes along with it
};

struct RopeAnchor
{
	RopeAnchorKind kind = RopeAnchorFixed;

	//The dynamic's, brick's, or vehicle's net ID
	netIDType id = NO_ID;

	//Where in the world for a fixed spot or a brick, and how far from the middle of the dynamic's or vehicle's body along its own axes
	glm::vec3 offset = glm::vec3(0);

	/*
		A dynamic this end is drawn on instead, NO_ID for none, and how far from its middle along its own axes
		For an end that pulls on one thing but should look like it comes from another: a rope that holds a player
		by their body but comes out of the barrel of the tool in their hand, which is a dynamic with no body in
		the world to pull on
	*/
	netIDType drawnOnID = NO_ID;
	glm::vec3 drawnOffset = glm::vec3(0);

	//Found again from the IDs whenever they aren't what's here, see Rope::updatePhysics
	std::weak_ptr<Dynamic> dynamic;
	std::weak_ptr<Vehicle> vehicle;
	std::weak_ptr<Dynamic> drawnOn;

	static constexpr unsigned int packetBytes = 1 + sizeof(netIDType) * 2 + sizeof(float) * 6;
};

/*
	A rope tied between two things, made by server Lua with createRope. The old game's were Bullet soft bodies that
	only the server simulated, sending where every node was to every client each tick. This one is two separate parts:

	What it does is a RopeConstraint, which keeps its ends within its length of each other along its path. Both the
	server and every client put one in their own physics world from the same few numbers, so a rope on a player is
	felt by that player's own simulation, which stays in charge of them as it is for everything else they do, and
	a rope between two other things is the server's to simulate, as they are

	What it looks like is a chain of nodes hanging between wherever its ends are drawn, which each client works out
	for itself, see Graphics/RopeRenderer.h. Nothing about the curve is sent, only what the rope is tied to, how long
	it is, and how it's drawn, and only when one of those changes
*/
class Rope : public SimObject
{
	friend ObjHolder<Rope>;

	//Update packets go out unreliably, so a change is sent this many times to make sure it arrives
	static constexpr int resendCount = 4;
	int updatesLeft = 0;

	RopeAnchor anchors[2];

	//As far apart along its path as the ends can get, in studs
	float length = 10.0f;

	//How many sections it's drawn with, the more there are the rounder it hangs
	unsigned char links = 15;

	//Studs across
	float width = 0.15f;

	glm::u8vec4 color = glm::u8vec4(110, 84, 52, 255);

	//Fixed spots in the world it runs over between its ends, in order from the first end
	std::vector<glm::vec3> bends;

	//The world it was made in: single player has the server's and the client's in one process, swapping SimObject::world between them
	std::shared_ptr<PhysicsWorld> ropeWorld;

	//nullptr whenever there's nothing to hold together: an end whose body isn't around, or both ends fixed
	RopeConstraint* constraint = nullptr;

	//The bodies and lengths have to be woken and handed to the constraint again
	bool physicsChanged = true;

	void dropConstraint();

	//nullptr if this end's body isn't in the world right now, the shared fixed body for a spot in the world
	btRigidBody* getAnchorBody(int end) const;

	void changed();

	protected:

	explicit Rope();

	virtual void onCreation() override {}

	virtual void requestDestruction() override;

	public:

	static constexpr int minLinks = 1;
	static constexpr int maxLinks = 50;
	static constexpr int maxBends = 8;
	static constexpr float maxLength = 2000.0f;

	//Client only: where each node of the drawn rope is and was a step ago, links + 1 of them, see RopeRenderer::simulate
	std::vector<glm::vec3> nodes;
	std::vector<glm::vec3> lastNodes;

	//Client only: seconds the nodes have yet to be moved through, they move in fixed steps
	float simulationDebt = 0;

	//Client: false while something it's tied to or drawn on isn't here, which is either yet to arrive or gone, with the rope's removal on its way
	bool isDrawable() const;

	const RopeAnchor& getAnchor(int end) const { return anchors[end]; }
	float getLength() const { return length; }
	int getLinks() const { return links; }
	float getWidth() const { return width; }
	const glm::u8vec4& getColor() const { return color; }
	const std::vector<glm::vec3>& getBends() const { return bends; }

	//Each of these has clients sent the change

	void tieToPoint(int end, const glm::vec3& position);
	void tieToDynamic(int end, const std::shared_ptr<Dynamic>& target, const glm::vec3& offset);
	void tieToBrick(int end, netIDType brickID, const glm::vec3& position);
	void tieToVehicle(int end, const std::shared_ptr<Vehicle>& target, const glm::vec3& offset);

	//nullptr draws the end where it's tied again
	void drawOn(int end, const std::shared_ptr<Dynamic>& target, const glm::vec3& offset);

	void setLength(float _length);
	void setLinks(int _links);
	void setWidth(float _width);
	void setColor(const glm::u8vec4& _color);
	void setBends(const std::vector<glm::vec3>& _bends);

	//Where an end is tied, from its body's transform
	glm::vec3 getEnd(int end) const;

	//Client: where an end is drawn, which for a dynamic or vehicle is where that's drawn rather than where its body is
	//tied gives where the thing it's tied to is drawn even if the end itself is drawn on something else, see RopeAnchor::drawnOnID
	glm::vec3 getDrawnEnd(int end, bool tied = false) const;

	//How far it is from one end to the other along its bends right now
	float getPathLength() const;

	//Server: whether something it was tied to no longer exists, which is the end of the rope too, see LoopServer::updateRopes
	bool isAnchorGone(const class BrickHolder* bricks) const;

	/*
		Before each physics step, on the server and on clients: finds what the ends are tied to by ID again if
		need be, and keeps the constraint in the world matching them, making it once both bodies are there and
		letting go of it when one leaves, like a player getting into a vehicle
	*/
	void updatePhysics(const ObjHolder<Dynamic>* dynamics, const ObjHolder<Vehicle>* vehicles);

	//How many bytes of state there are at src, given how many can be read, 0 if it's cut short
	static unsigned int readStateBytes(const enet_uint8* src, unsigned int available);

	unsigned int getStateBytes() const;

	//Client: applies getStateBytes() of state written by the server
	void readFromPacket(const enet_uint8* src);

	void writeState(enet_uint8* dest) const;

	virtual bool requiresNetUpdate() override;

	virtual unsigned int getCreationPacketBytes() const override;

	virtual unsigned int getUpdatePacketBytes() const override;

	virtual void addToCreationPacket(enet_uint8* dest) const override;

	virtual void addToUpdatePacket(enet_uint8* dest) override;

	~Rope();
};

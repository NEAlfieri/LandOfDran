#pragma once

#include "Dynamic.h"

struct ClientData;

//How many items a client can carry at once, see ClientData::inventory
constexpr int inventorySize = 5;

//Animations an item plays besides its model's own, which go by their server IDs: none, and the swing and kick every item has
constexpr int itemNoAnimation = -1;
constexpr int itemSwingAnimation = -2;
constexpr int itemKickAnimation = -3;

//What a click action's animation step carries for the kick, see Networking/ClickAction.h. A model's own go by their IDs below it
constexpr int clickStepKickAnimation = 255;

/*
	Tools like the hammer: dynamics players can carry in their inventory
	On the ground an item is like any other dynamic. While carried its body is out of the physics world, and games draw it
	in the hand of the player carrying it while their item bar is out with its slot picked, or not at all otherwise
*/
class Item : public Dynamic
{
	friend ObjHolder<Dynamic>;

	protected:

	explicit Item(std::shared_ptr<DynamicType> _type, const btVector3& initialPos, const btQuaternion& initialRot);

	private:

	//Client: how far through a swing it is, 0 to 2 pi with 0 at rest, and whether it's swinging and keeps going around
	float swingPhase = 0;
	bool swinging = false;
	bool swingLooping = false;

	//Client: milliseconds into a kick, the jolt of a gun going off, and whether it's kicking and keeps on kicking
	float kickMS = 0;
	bool kicking = false;
	bool kickLooping = false;

	//Client: starts or stops one of its animations on the model, the swing, or the kick
	void startClientAnimation(int id, bool loop);
	void stopClientAnimation(int id);

	public:

	//Holder net ID, flags, slot, looping animation, one shot animation and its count, then position and rotation
	static constexpr unsigned int stateBytes = sizeof(netIDType) + 5 + PositionBytes + QuaternionBytes;

	virtual const char* getLuaMetatable() const override { return "metatable_item"; }
	virtual DynamicKind getKind() const override { return DynamicKind_Item; }
	virtual unsigned int getKindCreationBytes() const override { return stateBytes; }
	virtual void addKindCreationData(enet_uint8* dest) const override;

	/*
		Client: plays one of its animations right away, without the server having said to. Used by
		ClickActionPlayer for the animation a predicted click starts, see Networking/ClickAction.h.
		The server's own ItemState will play it again for everyone else a moment later, which lands
		on the same animation and so just restarts the one already running
	*/
	void startPredictedAnimation(int id) { startClientAnimation(id, false); }

	//Server: the client carrying it, empty while it's on the ground
	std::weak_ptr<ClientData> owner;

	/*
		Server: a dynamic nobody plays holding it, for a bot handed one with dynamic:setHeldItem. It's kept here
		rather than in owner because there's no client to carry it: no inventory, no slot and no item bar to put
		it away into, so it's simply in that dynamic's hand until something takes it back.
		ServerProgramData::botHeldItems is what carries it along, the way botControllers walk the bots themselves
	*/
	std::weak_ptr<Dynamic> botHolder;

	//Which of its carrier's slots it's in, -1 on the ground
	int slot = -1;

	//Server: already waiting in ServerProgramData::changedItems
	bool stateChanged = false;

	//Server: who held it and whether it was in their hand in the last ItemState sent, so LoopServer::updateItems knows to send another
	netIDType sentHolderID = NO_ID;
	bool sentEquipped = false;

	//Client: from the server, see writeState
	bool held = false;
	bool equipped = false;
	netIDType holderID = NO_ID;

	//Client: holderID's dynamic, once it's been looked up
	std::weak_ptr<Dynamic> holder;

	/*
		Whether it's a display item: a copy of its type floating over a brick wrenched to offer that item, which spins slowly
		(clients turn it themselves, see LoopClient::placeHeldItems), never falls or moves, collides with nothing, and is
		outlined on a client's screen while their crosshair is on it within reach. Raycasts and clicks still hit it, so
		Inventory.lua can hand whoever clicks it an item of the same type. It can't be picked up itself, see ClientData::addItem
		Made by the server with makeDisplay, which clients learn from ItemFlag_Display in its state
	*/
	bool display = false;

	//Server: the brick it floats over, see BrickAttachments::itemSpawnName
	netIDType displayBrickID = NO_ID;

	//How far over a brick a display item's collision box floats, world units, and how long one turn takes it on clients, like the item bar's icons
	static constexpr float displayHover = 0.5f;
	static constexpr double displaySpinMS = 6000.0;

	//Server: turns it into a display item over that brick, see display
	void makeDisplay(netIDType brickID);

	//Both sides: its body stops colliding, falling, or being simulated at all, while rays still hit it, for a display item
	void applyDisplayBody();

	//The animation it keeps playing, see itemNoAnimation
	int loopAnimation = itemNoAnimation;

	//The last animation it played once, and a count that changes each time one plays, so games know to play it again
	int oneShotItemAnimation = itemNoAnimation;
	unsigned char oneShotItemCount = 0;

	//In someone's inventory
	bool isHeld() const;

	//Server: the player its carrier moves around, which holds it, nullptr on the ground or for a carrier without a player
	std::shared_ptr<Dynamic> getHolder() const;

	//Server: in its holder's hand, their item bar is out with its slot picked
	bool isEquipped() const;

	//Server: plays an animation (see itemNoAnimation) on a loop, replacing whatever looped before, or once. Doesn't send it, see ServerProgramData::markItemChanged
	void playAnimation(int id, bool loop);

	//Server: stops a looping animation if it's the one looping, or whatever loops for itemNoAnimation
	void stopAnimation(int id);

	//Server: stateBytes of its state as of now
	void writeState(enet_uint8* dest) const;

	//Server: an ItemState packet with its state for everyone, remembering what it sent
	ENetPacket* makeStatePacket();

	//Client: applies writeState's bytes, creating is true for the state in its creation packet, which doesn't replay the last one shot animation
	void readState(enet_uint8* src, bool creating, float idealBufferSize);

	//Client: moves the swing and the kick along, once per frame
	void updateSwing(float deltaT);

	//Client: how far the swing tips it forward right now in radians around its right, 0 at rest and negative toward the ground
	float getSwingAngle() const;

	//Client: how far the kick has it right now, from 0 at rest to 1 at its sharpest, see LoopClient::placeHeldItems
	float getKickAmount() const;
};

#pragma once

#include "../Networking/ObjHolder.h"
#include "../SimObjects/SimObject.h"
#include "../NetTypes/DynamicType.h"
#include "../Graphics/Interpolator.h"
#include "../Networking/Quantization.h"
#include "../Utility/GlobalStartup.h" //getTicksMS 

/*
	These are objects which can collide with objects and have a model
	But themselves cannot ever move
*/
class StaticObject : public SimObject
{
	friend ObjHolder<StaticObject>;

protected:

	/*
		Determines its physical appearance and collision mesh
	*/
	std::shared_ptr<DynamicType> type = nullptr;

	explicit StaticObject(std::shared_ptr<DynamicType> _type, const btVector3& pos, const btQuaternion& rot);

	//Called after this object has an id, type, me pointer, and vector index assigned by ObjHolder
	virtual void onCreation() override;

	//Client: holds buffer data for instanced mesh rendering
	//Server: Only uses the vectors to store per-node color/hidden data for now
	ModelInstance* modelInstance = nullptr;

	//Called by objHolder when destroy is first called, gives object an oppertunity to reset smart pointers it might have
	virtual void requestDestruction() override;

public:

	bool serverHidden = false;
	bool serverCollides = true;

	bool hiddenUpdated = false;
	bool collisionUpdated = false;
	bool restitutionUpdated = false;
	bool frictionUpdated = false;

	void setColliding(bool collides);
	void setHiddenServer(bool hidden);

	void play(int id, bool loop) { if (!modelInstance) return; modelInstance->playAnimation(id, loop); }

	void stop(int id) { if (!modelInstance) return;  modelInstance->stopAnimation(id); }

	bool getHidden() const { return modelInstance->getHidden(); };

	void setHidden(bool hidden) { modelInstance->setHidden(hidden); };

	const std::shared_ptr<DynamicType>& getType() const { return type; }

	//Physics object
	btRigidBody* body = nullptr;

	btVector3 getPosition() const;

	virtual bool requiresNetUpdate() override;//const override;

	//How many bytes would this add to a packet creating objects if it was added to it
	virtual unsigned int getCreationPacketBytes() const override;

	//How many bytes would this add to a packet updating objects if it was added to it
	virtual unsigned int getUpdatePacketBytes() const override;

	//Add getCreationPacketBytes() worth of data to the given packet with all the data needed for the client to create it
	virtual void addToCreationPacket(enet_uint8* dest) const override;

	//Add getUpdatePacketBytes() worth of data to the given packet with all the data needed for the client to update it
	virtual void addToUpdatePacket(enet_uint8* dest) override;

	//Client side: 
	void setMeshColor(int idx, const glm::vec4& color);

	//Server side: returns a fully created packet ready to broadcast to relay the mesh color update
	ENetPacket* setMeshColor(const std::string& meshName, const glm::vec4& color);

	//Applies (or, if color.a <= 0, clears) an outline/highlight effect on this object. Used both client-side when
	//applying a packet and server-side for bookkeeping so late-joining clients get it baked into their creation packet
	void setHighlight(const glm::vec4& color, float thickness);

	//Server side: returns a fully created packet ready to broadcast to relay the highlight update, does not apply it locally
	ENetPacket* makeHighlightPacket(const glm::vec4& color, float thickness) const;

	~StaticObject();
};

//Helper function to get a normal dynamic shared_ptr from its btRigidBody with a ton of error checking, returns nullptr on error
std::shared_ptr<StaticObject> staticFromBody(const btRigidBody* in);


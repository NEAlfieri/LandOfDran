#include "StaticObject.h"

void StaticObject::setColliding(bool collides)
{
	serverCollides = collides;
	collisionUpdated = true;

	if (collides)
		body->setCollisionFlags(body->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
	else
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	body->activate();
}
 
void StaticObject::setHiddenServer(bool hidden)
{
	serverHidden = hidden;
	hiddenUpdated = true;
}

StaticObject::StaticObject(std::shared_ptr<DynamicType> _type, const btVector3& pos, const btQuaternion& rot)
	: type(_type)
{
	btTransform t;
	t.setIdentity();
	t.setOrigin(pos);
	t.setRotation(rot);

	body = type->createBodyStatic(t); 
	body->forceActivationState(ISLAND_SLEEPING);
	world->addBody(body);

	modelInstance = new ModelInstance(type->getModel().get());
	modelInstance->setModelTransform(glm::translate(b2g3(pos)) * glm::toMat4(glm::quat(rot.w(), rot.x(), rot.y(), rot.z())));
}

void StaticObject::onCreation()
{
	//Pointer to a smart pointer to this
	body->setUserPointer((void*)new std::shared_ptr<SimObject>(getMe()));
}

btVector3 StaticObject::getPosition() const
{
	return body->getWorldTransform().getOrigin();
}

bool StaticObject::requiresNetUpdate() //const
{
	bool ret = frictionUpdated || restitutionUpdated || hiddenUpdated || collisionUpdated;
	flaggedForUpdate = ret;
	return ret;
}

unsigned int StaticObject::getUpdatePacketBytes() const
{
	return sizeof(float) * 2 + 1; //restitution and friction, flags for hidden and colliding
}

void StaticObject::addToUpdatePacket(enet_uint8* dest)
{
	float bufF = body->getFriction();
	memcpy(dest, &bufF, sizeof(float));
	bufF = body->getRestitution();
	memcpy(dest + sizeof(float), &bufF, sizeof(float));

	dest[sizeof(float) * 2] = (serverHidden ? 1 : 0) | (serverCollides ? 2 : 0);

	frictionUpdated = false;
	restitutionUpdated = false;
	hiddenUpdated = false;
	collisionUpdated = false;
}

void StaticObject::setMeshColor(int meshIdx, const glm::vec4& color)
{
	modelInstance->setColor(meshIdx, color);
}

ENetPacket* StaticObject::setMeshColor(const std::string& meshName, const glm::vec4& color)
{
	int meshIdx = getType()->getModel()->getMeshIdx(meshName);
	if (meshIdx == -1)
	{
		error("Mesh " + meshName + " not found in model");
		return nullptr;
	}

	modelInstance->setColor(meshIdx, color);

	ENetPacket* ret = enet_packet_create(NULL, sizeof(netIDType) + 3 + sizeof(glm::vec4), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)MeshAppearance;
	ret->data[1] = (unsigned char)StaticTypeId;
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	ret->data[sizeof(netIDType) + 2] = meshIdx;
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 0, &color.r, sizeof(float));
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 1, &color.g, sizeof(float));
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 2, &color.b, sizeof(float));
	memcpy(ret->data + sizeof(netIDType) + 3 + sizeof(float) * 3, &color.a, sizeof(float));

	return ret;
}

void StaticObject::setHighlight(const glm::vec4& color, float thickness)
{
	modelInstance->setHighlight(color, thickness);
}

ENetPacket* StaticObject::makeHighlightPacket(const glm::vec4& color, float thickness) const
{
	ENetPacket* ret = enet_packet_create(NULL, sizeof(netIDType) + 2 + sizeof(glm::vec4) + sizeof(float), getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)HighlightAppearance;
	ret->data[1] = (unsigned char)StaticTypeId;
	memcpy(ret->data + 2, &netID, sizeof(netIDType));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 0, &color.r, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 1, &color.g, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 2, &color.b, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 3, &color.a, sizeof(float));
	memcpy(ret->data + 2 + sizeof(netIDType) + sizeof(float) * 4, &thickness, sizeof(float));

	return ret;
}

/*
	Note, does not include packet header or anything, only the marginal bytes added by* this* object in a bigger packet
	Same goes for all packet related functions here
*/
unsigned int StaticObject::getCreationPacketBytes() const
{
	int meshColorsSize = 0;
	glm::vec4 color;
	for (int a = 0; a < modelInstance->getNumMeshes(); a++)
		if (modelInstance->getMeshColor(a, color))
			meshColorsSize++;

	meshColorsSize *= sizeof(glm::vec4) + 1; //1 byte for mesh index
	meshColorsSize++; //1 extra byte for how many mesh colors we are sending

	float highlightThickness;
	//1 flag byte for whether a highlight is present, plus its data if so
	int highlightSize = 1 + (modelInstance->getHighlight(color, highlightThickness) ? sizeof(glm::vec4) + sizeof(float) : 0);

	//Type id, object id, position, rotation, not compressed, friction, restitution
	return sizeof(netIDType) * 2  + sizeof(float) * 9 + meshColorsSize + highlightSize + 1;
}

void StaticObject::addToCreationPacket(enet_uint8* dest) const
{
	netIDType buf = type->getID();
	memcpy(dest, &buf, sizeof(netIDType));
	buf = getID();
	memcpy(dest + sizeof(netIDType), &buf, sizeof(netIDType));
	glm::vec3 pos = b2g3(body->getWorldTransform().getOrigin());
	memcpy(dest + sizeof(netIDType) * 2, &pos, sizeof(glm::vec3));
	btQuaternion bQuat = body->getWorldTransform().getRotation();
	glm::quat gQuat = glm::quat(bQuat.getW(), bQuat.getX(), bQuat.getY(), bQuat.getZ());
	memcpy(dest + sizeof(netIDType) * 2 + sizeof(glm::vec3), &gQuat, sizeof(glm::quat));
	float bufF = body->getFriction();
	memcpy(dest + sizeof(netIDType) * 2 + sizeof(glm::vec3) + sizeof(glm::quat), &bufF, sizeof(float));
	bufF = body->getRestitution();
	memcpy(dest + sizeof(netIDType) * 2 + sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(float), &bufF, sizeof(float));

	dest[sizeof(netIDType) * 2 + sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(float) + sizeof(float)] = (serverHidden ? 1 : 0) | (serverCollides ? 2 : 0);

	int meshColorsStart = sizeof(netIDType) * 2 + sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(float) + sizeof(float) + 1;

	//Skip the byte with the mesh colors count for now until we actually know how many meshes need colors sent
	int byteIterator = meshColorsStart + 1;

	int meshColors = 0;
	glm::vec4 color;
	for (int a = 0; a < modelInstance->getNumMeshes(); a++)
	{
		if (modelInstance->getMeshColor(a, color))
		{
			dest[byteIterator] = a;
			byteIterator++;

			memcpy(dest + byteIterator, &color.r, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.g, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.b, sizeof(float));
			byteIterator += sizeof(float);
			memcpy(dest + byteIterator, &color.a, sizeof(float));
			byteIterator += sizeof(float);

			meshColors++;
		}
	}

	//Now we know how many meshes need updating
	dest[meshColorsStart] = meshColors;

	glm::vec4 highlightColor;
	float highlightThickness;
	bool hasHighlight = modelInstance->getHighlight(highlightColor, highlightThickness);

	dest[byteIterator] = hasHighlight ? 1 : 0;
	byteIterator++;

	if (hasHighlight)
	{
		memcpy(dest + byteIterator, &highlightColor.r, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.g, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.b, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightColor.a, sizeof(float));
		byteIterator += sizeof(float);
		memcpy(dest + byteIterator, &highlightThickness, sizeof(float));
		byteIterator += sizeof(float);
	}
}

void StaticObject::requestDestruction()
{
	((std::shared_ptr<SimObject>*)body->getUserPointer())->reset();
}

StaticObject::~StaticObject()
{
	if (modelInstance)
		delete modelInstance;

	if (body)
	{
		if (body->getUserPointer())
		{
			std::shared_ptr<SimObject>* userDataPtr = (std::shared_ptr<SimObject>*)body->getUserPointer();
			delete userDataPtr;
		}
		world->removeBody(body);
		delete body;
	}
}

std::shared_ptr<StaticObject> staticFromBody(const btRigidBody* in)
{
	if (in->getUserIndex() != staticBody)
	{
		error("btRigidBody expected to have static type but didn't");
		return nullptr;
	}

	if (!in->getUserPointer())
		return nullptr;

	std::shared_ptr<SimObject>* userDataPointer = (std::shared_ptr<SimObject>*)in->getUserPointer();

	if (!(*userDataPointer))
		return nullptr;

	return std::dynamic_pointer_cast<StaticObject>(*userDataPointer);
}

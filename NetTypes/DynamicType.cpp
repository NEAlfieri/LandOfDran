#include "DynamicType.h"

/*
	1 byte			- packet type
	1 byte			- SimObject type
	4 bytes			- object id
	1 byte			- relative file path to model length
	1-255 bytes		- relative file path to model
*/
bool DynamicType::loadFromPacket(ENetPacket const* const packet, const ClientProgramData& pd)
{
	//packet type and simobject type were already processed to get to this point

	//Packet isn't even big enough to have a 1 character file path
	if (packet->dataLength < (sizeof(netIDType) + 4))
	{
		pd.gui->popupErrorMessage = "Packet too short to load DynamicType";
		return true;
	}

	//Get id of object
	int byteIterator = 2;
	memcpy(&id, packet->data + byteIterator, sizeof(netIDType));
	byteIterator += sizeof(netIDType);

	unsigned int filePathLen = packet->data[byteIterator];
	byteIterator++;

	//Packet not long enough for file path string
	if (packet->dataLength < byteIterator + filePathLen)
	{
		pd.gui->popupErrorMessage = "Packet too short to load DynamicType";
		return true;
	}

	std::string filePath = std::string((char*)packet->data + byteIterator, filePathLen);
	byteIterator += filePathLen;

	glm::vec3 baseScale;
	//getPosition(packet->data + byteIterator, baseScale);
	//byteIterator += PositionBytes;
	memcpy(&baseScale.x, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(&baseScale.y, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(&baseScale.z, packet->data + byteIterator, sizeof(float));
	byteIterator += sizeof(float);

	model = std::make_shared<Model>(filePath,pd.textures,baseScale);

	if (!model->isValid())
	{
		pd.gui->popupErrorMessage = "Failed to load model: " + filePath + " check error log.";
		return true;
	}

	glm::vec3 halfExtents = model->getColHalfExtents();
	collisionBox = new btBoxShape(g2b3(halfExtents));

	collisionShape = new btCompoundShape();
	btTransform t;
	t.setIdentity();
	t.setOrigin(g2b3(model->getColOffset()));
	collisionShape->addChildShape(t, collisionBox);

	btScalar masses[1];
	masses[0] = 1.0;
	t.setIdentity();
	collisionShape->calculatePrincipalAxisTransform(masses, t, defaultInertia);

	int numAnims = packet->data[byteIterator];
	byteIterator++;

	for (int a = 0; a < numAnims; a++)
	{
		int animID;
		memcpy(&animID, packet->data + byteIterator, sizeof(int));
		byteIterator += sizeof(int);

		float startTime, endTime, defaultSpeed, fadeInMS, fadeOutMS;
		memcpy(&startTime, packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(&endTime, packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(&defaultSpeed, packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(&fadeInMS, packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(&fadeOutMS, packet->data + byteIterator, sizeof(float));
		byteIterator += sizeof(float);

		if (byteIterator >= packet->dataLength)
			break;

		unsigned int nameLength = packet->data[byteIterator];
		byteIterator++;

		if (byteIterator + nameLength > packet->dataLength)
			break;

		Animation anim;
		anim.startTime = startTime;
		anim.endTime = endTime;
		anim.defaultSpeed = defaultSpeed;
		anim.fadeInMS = fadeInMS;
		anim.fadeOutMS = fadeOutMS;
		anim.name = std::string((char*)packet->data + byteIterator, nameLength);
		byteIterator += nameLength;

		//Also works out which nodes it moves
		model->addAnimation(anim, animID);
	}

	//Item types, with their name, icon, script name (which the wrench dialog picks an item to offer by), and how they sit in a hand
	if (byteIterator < (int)packet->dataLength && packet->data[byteIterator] == 1)
	{
		byteIterator++;
		isItemType = true;

		for (std::string* text : { &itemName, &itemIconPath, &scriptName })
		{
			if (byteIterator >= (int)packet->dataLength)
				break;

			unsigned int length = packet->data[byteIterator];
			byteIterator++;
			if (byteIterator + length > packet->dataLength)
				break;

			*text = std::string((char*)packet->data + byteIterator, length);
			byteIterator += length;
		}

		if (byteIterator + sizeof(float) * 7 <= packet->dataLength)
		{
			memcpy(&handOffset.x, packet->data + byteIterator, sizeof(float) * 3);
			byteIterator += sizeof(float) * 3;

			float rotation[4];
			memcpy(rotation, packet->data + byteIterator, sizeof(rotation));
			byteIterator += sizeof(rotation);
			handRotation = glm::normalize(glm::quat(rotation[0], rotation[1], rotation[2], rotation[3]));
		}
	}

	loaded = true;
	return false;
}

void DynamicType::render(std::shared_ptr<ShaderManager> graphics, bool useMaterials) const
{
	model->render(graphics, useMaterials);
}

void DynamicType::serverSideLoad(const std::string &filePath,netIDType typeID,glm::vec3 baseScale)
{
	id = typeID;

	model = std::make_shared<Model>(filePath, true, baseScale);

	glm::vec3 halfExtents = model->getColHalfExtents();
	collisionBox = new btBoxShape(g2b3(halfExtents));

	collisionShape = new btCompoundShape();
	btTransform t;
	t.setIdentity();
	t.setOrigin(g2b3(model->getColOffset()));
	collisionShape->addChildShape(t, collisionBox);

	btScalar masses[1];
	masses[0] = mass;
	t.setIdentity();
	collisionShape->calculatePrincipalAxisTransform(masses, t, defaultInertia);

	loaded = true;
}

btRigidBody* DynamicType::createBody() const
{ 
	auto ret = new btRigidBody(mass, defaultMotionState, collisionShape, defaultInertia);
	ret->setUserIndex(dynamicBody);
	return ret;
}

btRigidBody* DynamicType::createBodyStatic(const btTransform& t) const
{
	//TODO: Do I need to delete the motion state after?
	btMotionState* state = new btDefaultMotionState(t);
	auto ret = new btRigidBody(0.0, state, collisionShape, btVector3(0, 0, 0));
	ret->setUserIndex(staticBody);
	ret->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
	return ret;
}

/*
	Create the packet on the server to be passed to loadFromPacket on the client
	1 byte   - Packet type
	1 byte   - SimObject type
	4 bytes  - object id
	1 byte   - string length of relative file path
	? bytes  - relative file path
	12 bytes - base scale of model
*/
ENetPacket* DynamicType::createTypePacket() const
{
	//How many byte each animation takes up, plus its name and name length byte
	unsigned int animationSize = 0;
	for (const Animation& animation : model->animations)
		animationSize += 5 * sizeof(float) + sizeof(int) + 1 + std::min(animation.name.length(), (size_t)255);
	animationSize++; //Extra byte for number of animations

	//A byte for whether it's an item type, then its name, icon, and script name each with a length byte, grip, and rotation
	unsigned int itemSize = 1;
	if (isItemType)
		itemSize += 3 + std::min(itemName.length(), (size_t)255) + std::min(itemIconPath.length(), (size_t)255) + std::min(scriptName.length(), (size_t)255) + sizeof(float) * 7;

	//unsigned int packetSize = model->loadedPath.length() + sizeof(netIDType) + 3 + PositionBytes + animationSize;
	unsigned int packetSize = model->loadedPath.length() + sizeof(netIDType) + 3 + sizeof(float)*3 + animationSize + itemSize;
	ENetPacket* ret = enet_packet_create(NULL, packetSize, getFlagsFromChannel(JoinNegotiation));

	unsigned int byteIterator = 0;

	ret->data[byteIterator] = AddSimObjectType;
	byteIterator++;
	ret->data[byteIterator] = DynamicTypeId;
	byteIterator++;

	memcpy(ret->data + byteIterator, &id, sizeof(netIDType));
	byteIterator += sizeof(netIDType);

	ret->data[byteIterator] = (unsigned char)model->loadedPath.length();
	byteIterator++;

	memcpy(ret->data + byteIterator, model->loadedPath.c_str(), model->loadedPath.length());
	byteIterator += model->loadedPath.length();

	//addPosition(ret->data + byteIterator, getScale());
	//byteIterator += PositionBytes;
	memcpy(ret->data + byteIterator, &getScale().x, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(ret->data + byteIterator, &getScale().y, sizeof(float));
	byteIterator += sizeof(float);
	memcpy(ret->data + byteIterator, &getScale().z, sizeof(float));
	byteIterator += sizeof(float);

	ret->data[byteIterator] = model->animations.size();
	byteIterator++;

	for (unsigned int a = 0; a < model->animations.size(); a++)
	{
		memcpy(ret->data + byteIterator, &a, sizeof(int));
		byteIterator += sizeof(int);

		memcpy(ret->data + byteIterator, &model->animations[a].startTime, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(ret->data + byteIterator, &model->animations[a].endTime, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(ret->data + byteIterator, &model->animations[a].defaultSpeed, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(ret->data + byteIterator, &model->animations[a].fadeInMS, sizeof(float));
		byteIterator += sizeof(float);

		memcpy(ret->data + byteIterator, &model->animations[a].fadeOutMS, sizeof(float));
		byteIterator += sizeof(float);

		//So clients can find animations the game plays by name, like grab
		size_t nameLength = std::min(model->animations[a].name.length(), (size_t)255);
		ret->data[byteIterator] = (unsigned char)nameLength;
		byteIterator++;
		memcpy(ret->data + byteIterator, model->animations[a].name.data(), nameLength);
		byteIterator += nameLength;
	}

	ret->data[byteIterator] = isItemType ? 1 : 0;
	byteIterator++;

	if (isItemType)
	{
		for (const std::string* text : { &itemName, &itemIconPath, &scriptName })
		{
			size_t length = std::min(text->length(), (size_t)255);
			ret->data[byteIterator] = (unsigned char)length;
			byteIterator++;
			memcpy(ret->data + byteIterator, text->data(), length);
			byteIterator += length;
		}

		memcpy(ret->data + byteIterator, &handOffset.x, sizeof(float) * 3);
		byteIterator += sizeof(float) * 3;

		float rotation[4] = { handRotation.w, handRotation.x, handRotation.y, handRotation.z };
		memcpy(ret->data + byteIterator, rotation, sizeof(rotation));
		byteIterator += sizeof(rotation);
	}

	return ret; 
}

//Doesn't allocate anything, wait for loadFromPacket or server-side construction from lua
DynamicType::DynamicType() : NetType(DynamicTypeId)
{
	defaultMotionState = new btDefaultMotionState(btTransform::getIdentity());
}

//Deallocates model if it exists
DynamicType::~DynamicType()
{
	delete defaultMotionState;

	if (collisionShape)
		delete collisionShape;

	if (collisionBox)
		delete collisionBox;

	if (model)
	{
		//Not needed this is a destructor lol
		model.reset();
		model = nullptr;
	}
}


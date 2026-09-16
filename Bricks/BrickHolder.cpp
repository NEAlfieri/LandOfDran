#include "BrickHolder.h"

#include "../NetTypes/NetType.h"
#include "../Graphics/InstancedBrickRenderer.h"

#include <BulletCollision/CollisionDispatch/btManifoldResult.h>

//Packet type byte plus a u16 count
static constexpr unsigned int packetHeaderBytes = 3;
static constexpr unsigned int maxPacketBytes = ENET_HOST_DEFAULT_MTU - 20;

static void getBounds(const Brick* brick, int min[3], int max[3])
{
	min[0] = brick->x;
	min[1] = brick->y;
	min[2] = brick->z;
	max[0] = brick->x + brick->footprintWidth() - 1;
	max[1] = brick->y + brick->height - 1;
	max[2] = brick->z + brick->footprintLength() - 1;
}

void BrickHolder::writeRecord(const Brick* brick, enet_uint8* data)
{
	int16_t x = brick->x;
	uint16_t y = brick->y;
	int16_t z = brick->z;

	memcpy(data, &brick->netId, sizeof(netIDType));
	memcpy(data + 4, &x, sizeof(int16_t));
	memcpy(data + 6, &y, sizeof(uint16_t));
	memcpy(data + 8, &z, sizeof(int16_t));
	data[10] = brick->width;
	data[11] = brick->height;
	data[12] = brick->length;
	data[13] = brick->angleID;
	for (int channel = 0; channel < 4; channel++)
		data[14 + channel] = brick->color[channel];
	//Collision in the low bit, material in the 4 above it
	data[18] = (brick->collides ? 1 : 0) | ((brick->material & 15) << 1);
	memcpy(data + 19, &brick->typeID, sizeof(uint16_t));
	memcpy(data + 21, &brick->printID, sizeof(uint16_t));
}

Brick BrickHolder::readRecord(const enet_uint8* data)
{
	int16_t x;
	uint16_t y;
	int16_t z;

	Brick brick;
	memcpy(&brick.netId, data, sizeof(netIDType));
	memcpy(&x, data + 4, sizeof(int16_t));
	memcpy(&y, data + 6, sizeof(uint16_t));
	memcpy(&z, data + 8, sizeof(int16_t));
	brick.x = x;
	brick.y = y;
	brick.z = z;
	brick.width = data[10];
	brick.height = data[11];
	brick.length = data[12];
	brick.angleID = data[13] % 4;
	for (int channel = 0; channel < 4; channel++)
		brick.color[channel] = data[14 + channel];
	brick.collides = data[18] & 1;
	brick.material = (data[18] >> 1) & 15;
	if (brick.material >= BrickMaterialCount)
		brick.material = BrickMaterial_None;
	memcpy(&brick.typeID, data + 19, sizeof(uint16_t));
	memcpy(&brick.printID, data + 21, sizeof(uint16_t));
	return brick;
}

bool BrickHolder::overlaps(int x, int y, int z, int footprintWidth, int height, int footprintLength) const
{
	int min[3] = { x, y, z };
	int max[3] = { x + footprintWidth - 1, y + height - 1, z + footprintLength - 1 };

	bool hit = false;
	tree.Search(min, max, [&hit](Brick* const&) { hit = true; return false; });
	return hit;
}

Brick* BrickHolder::getAt(int x, int y, int z) const
{
	int point[3] = { x, y, z };

	Brick* found = nullptr;
	tree.Search(point, point, [&found](Brick* const& brick) { found = brick; return false; });
	return found;
}

void BrickHolder::forEachInBox(const glm::ivec3& low, const glm::ivec3& high, const std::function<void(Brick*)>& visit) const
{
	int min[3] = { low.x, low.y, low.z };
	int max[3] = { high.x, high.y, high.z };
	tree.Search(min, max, [&visit](Brick* const& brick) { visit(brick); return true; });
}

Brick* BrickHolder::find(netIDType netId) const
{
	auto it = byId.find(netId);
	return it == byId.end() ? nullptr : it->second;
}

Brick* BrickHolder::add(const Brick& requested)
{
	Brick desc = requested;
	if (desc.isSpecial())
	{
		const SpecialBrickType* type = types ? types->getSpecial(desc.typeID - 1) : nullptr;
		if (!type)
			return nullptr;

		desc.width = type->width;
		desc.height = type->height;
		desc.length = type->length;
	}

	if (desc.width == 0 || desc.height == 0 || desc.length == 0 || desc.angleID > 3 || desc.material >= BrickMaterialCount)
		return nullptr;

	int footprintWidth = desc.footprintWidth();
	int footprintLength = desc.footprintLength();

	//Has to fit the i16 x/z and u16 y of brick network records
	if (desc.y < 0 || desc.y + desc.height > 65535)
		return nullptr;
	if (desc.x < -32768 || desc.x + footprintWidth > 32767 || desc.z < -32768 || desc.z + footprintLength > 32767)
		return nullptr;

	if (overlaps(desc.x, desc.y, desc.z, footprintWidth, desc.height, footprintLength))
		return nullptr;

	Brick* brick = new Brick(desc);
	brick->netId = lastNetId++;

	//Its own copy, with nothing made from it yet
	if (brick->attachments)
	{
		brick->attachments = std::make_shared<BrickAttachments>(*brick->attachments);
		brick->attachments->musicLoopID = NO_ID;
		brick->attachments->lightID = NO_ID;
		brick->attachments->emitterID = NO_ID;
	}

	insert(brick);

	if (server)
		pendingSends.insert(brick->netId);

	if (brick->attachments && spawnAttachments)
		spawnAttachments(brick);

	return brick;
}

Brick* BrickHolder::addFromServer(const Brick& desc)
{
	if (Brick* existing = find(desc.netId))
	{
		existing->color = desc.color;
		existing->material = desc.material;
		existing->printID = desc.printID;
		setColliding(existing, desc.collides);
		if (renderer)
			renderer->updateBrick(existing);
		return existing;
	}

	if (desc.width == 0 || desc.height == 0 || desc.length == 0)
		return nullptr;

	Brick* brick = new Brick(desc);

	//A special type this client doesn't have is still in the right place, as a plain box
	if (brick->isSpecial() && (!types || !types->getSpecial(brick->typeID - 1)))
		brick->typeID = 0;

	insert(brick);
	return brick;
}

void BrickHolder::insert(Brick* brick)
{
	brick->holderIndex = bricks.size();
	bricks.push_back(brick);
	byId[brick->netId] = brick;

	int min[3], max[3];
	getBounds(brick, min, max);
	tree.Insert(min, max, brick);

	createBody(brick);

	if (renderer)
		renderer->addBrick(brick);
}

void BrickHolder::remove(Brick* brick, bool showEffect)
{
	if (brick->attachments && removeAttachments)
		removeAttachments(brick);

	if (server)
	{
		pendingSends.erase(brick->netId);
		(showEffect ? pendingEffectRemovals : pendingRemovals).push_back(brick->netId);
	}

	int min[3], max[3];
	getBounds(brick, min, max);
	tree.Remove(min, max, brick);

	byId.erase(brick->netId);
	destroyBody(brick);

	if (renderer)
		renderer->removeBrick(brick);

	Brick* last = bricks.back();
	bricks[brick->holderIndex] = last;
	last->holderIndex = brick->holderIndex;
	bricks.pop_back();

	delete brick;
}

void BrickHolder::clear()
{
	if (removeAttachments)
	{
		for (Brick* brick : bricks)
		{
			if (brick->attachments)
				removeAttachments(brick);
		}
	}

	if (server)
	{
		pendingSends.clear();
		for (Brick* brick : bricks)
			pendingRemovals.push_back(brick->netId);
	}

	if (renderer)
		renderer->clear();

	for (Brick* brick : bricks)
	{
		destroyBody(brick);
		delete brick;
	}

	bricks.clear();
	byId.clear();
	tree.RemoveAll();
}

void BrickHolder::createBody(Brick* brick)
{
	int footprintWidth = brick->footprintWidth();
	int footprintLength = brick->footprintLength();

	btCollisionShape* shape = nullptr;
	const SpecialBrickType* special = brick->isSpecial() && types ? types->getSpecial(brick->typeID - 1) : nullptr;

	if (special && special->shape)
		shape = special->shape;
	else
	{
		btBoxShape*& box = shapes[footprintWidth | (brick->height << 8) | (footprintLength << 16)];
		if (!box)
			box = new btBoxShape(btVector3(footprintWidth * STUD_SIZE, brick->height * PLATE_SIZE, footprintLength * STUD_SIZE) * 0.5f);
		shape = box;
	}

	btRigidBody::btRigidBodyConstructionInfo info(0, nullptr, shape);
	info.m_startWorldTransform.setIdentity();
	info.m_startWorldTransform.setOrigin(g2b3(brick->getWorldCenter()));

	//Special shapes are built unturned, boxes already have their footprint swapped
	if (special)
		info.m_startWorldTransform.setRotation(btQuaternion(btVector3(0, 1, 0), brick->getAngle()));
	info.m_friction = 1.0f;

	brick->body = new btRigidBody(info);
	brick->body->setUserIndex(brickBody);
	brick->body->setUserPointer(brick);
	setColliding(brick, brick->collides);

	world->addBody(brick->body);
}

void BrickHolder::destroyBody(Brick* brick)
{
	if (!brick->body)
		return;

	world->removeBody(brick->body);
	delete brick->body;
	brick->body = nullptr;
}

void BrickHolder::setColliding(Brick* brick, bool collides)
{
	brick->collides = collides;

	if (server)
		pendingSends.insert(brick->netId);

	if (!brick->body)
		return;

	int flags = brick->body->getCollisionFlags();
	if (collides)
		flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
	else
		flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
	brick->body->setCollisionFlags(flags);
}

void BrickHolder::setColor(Brick* brick, const glm::u8vec4& color)
{
	brick->color = color;

	if (server)
		pendingSends.insert(brick->netId);

	if (renderer)
		renderer->updateBrick(brick);
}

void BrickHolder::setPrint(Brick* brick, uint16_t printID)
{
	brick->printID = printID;

	if (server)
		pendingSends.insert(brick->netId);

	if (renderer)
		renderer->updateBrick(brick);
}

void BrickHolder::setMaterial(Brick* brick, unsigned char material)
{
	brick->material = material < BrickMaterialCount ? material : BrickMaterial_None;

	if (server)
		pendingSends.insert(brick->netId);

	if (renderer)
		renderer->updateBrick(brick);

	//Something resting on it should notice it's bouncy or slippery now
	if (brick->body)
	{
		for (btRigidBody* touching : world->getTouching(brick->body))
			touching->activate();
	}
}

static const Brick* materialBrick(const btCollisionObject* body)
{
	return body->getUserIndex() == brickBody ? (const Brick*)body->getUserPointer() : nullptr;
}

//Bullet multiplies restitutions by default, and almost nothing else has any, so a bouncy brick decides on its own
static btScalar combinedRestitution(const btCollisionObject* body0, const btCollisionObject* body1)
{
	for (const btCollisionObject* body : { body0, body1 })
	{
		const Brick* brick = materialBrick(body);
		if (brick && brick->material == BrickMaterial_Bouncy)
			return 1.0f;
	}
	return body0->getRestitution() * body1->getRestitution();
}

//Same as Bullet's default, capped at 10, except a slippery brick has a friction of 0.01 whatever touches it
static btScalar combinedFriction(const btCollisionObject* body0, const btCollisionObject* body1)
{
	for (const btCollisionObject* body : { body0, body1 })
	{
		const Brick* brick = materialBrick(body);
		if (brick && brick->material == BrickMaterial_Slippery)
			return 0.01f;
	}
	return std::min(body0->getFriction() * body1->getFriction(), (btScalar)10.0f);
}

std::vector<ENetPacket*> BrickHolder::makeAddPackets(const std::vector<const Brick*>& toSend) const
{
	static constexpr unsigned int recordsPerPacket = (maxPacketBytes - packetHeaderBytes) / recordBytes;

	std::vector<ENetPacket*> packets;

	for (size_t start = 0; start < toSend.size(); start += recordsPerPacket)
	{
		uint16_t count = std::min<size_t>(recordsPerPacket, toSend.size() - start);

		ENetPacket* packet = enet_packet_create(NULL, packetHeaderBytes + count * recordBytes, getFlagsFromChannel(BrickLoading));
		packet->data[0] = AddBricks;
		memcpy(packet->data + 1, &count, sizeof(uint16_t));

		for (unsigned int a = 0; a < count; a++)
			writeRecord(toSend[start + a], packet->data + packetHeaderBytes + a * recordBytes);

		packets.push_back(packet);
	}

	return packets;
}

void BrickHolder::sendRecent()
{
	if (server->getNumClients() == 0)
	{
		pendingSends.clear();
		pendingRemovals.clear();
		pendingEffectRemovals.clear();
		return;
	}

	//Removals first, in case a removed brick's space was reused this tick
	sendRemovals(pendingRemovals, false);
	sendRemovals(pendingEffectRemovals, true);
	pendingRemovals.clear();
	pendingEffectRemovals.clear();

	std::vector<const Brick*> toSend;
	toSend.reserve(pendingSends.size());
	for (netIDType id : pendingSends)
	{
		if (const Brick* brick = find(id))
			toSend.push_back(brick);
	}
	pendingSends.clear();

	for (ENetPacket* packet : makeAddPackets(toSend))
		server->broadcast(packet, BrickLoading);
}

void BrickHolder::sendRemovals(const std::vector<netIDType>& ids, bool showEffect) const
{
	//Packet type, u16 count, then whether to show the brick popping loose
	static constexpr unsigned int removalHeaderBytes = 4;
	static constexpr unsigned int idsPerPacket = (maxPacketBytes - removalHeaderBytes) / sizeof(netIDType);

	for (size_t start = 0; start < ids.size(); start += idsPerPacket)
	{
		uint16_t count = std::min<size_t>(idsPerPacket, ids.size() - start);

		ENetPacket* packet = enet_packet_create(NULL, removalHeaderBytes + count * sizeof(netIDType), getFlagsFromChannel(BrickLoading));
		packet->data[0] = RemoveBricks;
		memcpy(packet->data + 1, &count, sizeof(uint16_t));
		packet->data[3] = showEffect ? 1 : 0;
		memcpy(packet->data + removalHeaderBytes, ids.data() + start, count * sizeof(netIDType));

		server->broadcast(packet, BrickLoading);
	}
}

void BrickHolder::sendAll(JoinedClient const* client) const
{
	std::vector<const Brick*> toSend(bricks.begin(), bricks.end());

	for (ENetPacket* packet : makeAddPackets(toSend))
		client->send(packet, BrickLoading);
}

void BrickHolder::sendSpecialTypes(JoinedClient const* client) const
{
	//Packet type, u16 count, then a u16 type ID, a name length byte, and the name for each
	std::vector<unsigned char> bytes;
	uint16_t count = 0;

	auto flush = [&]()
	{
		if (count == 0)
			return;

		bytes[0] = SpecialBrickTypes;
		memcpy(bytes.data() + 1, &count, sizeof(uint16_t));
		client->send(enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(JoinNegotiation)), JoinNegotiation);
		count = 0;
	};

	size_t typeCount = types ? std::min<size_t>(types->getSpecialCount(), 65534) : 0;
	for (size_t a = 0; a < typeCount; a++)
	{
		std::string name = types->getSpecial((int)a)->uiName.substr(0, 255);
		if (count > 0 && bytes.size() + 3 + name.length() > maxPacketBytes)
			flush();

		if (count == 0)
			bytes.assign(packetHeaderBytes, 0);

		uint16_t typeID = (uint16_t)(a + 1);
		bytes.resize(bytes.size() + sizeof(uint16_t));
		memcpy(bytes.data() + bytes.size() - sizeof(uint16_t), &typeID, sizeof(uint16_t));
		bytes.push_back((unsigned char)name.length());
		bytes.insert(bytes.end(), name.begin(), name.end());
		count++;
	}

	flush();
}

void BrickHolder::makeLuaMetatable(lua_State* L, const std::string& name, luaL_Reg* functions)
{
	metatableName = name;

	luaL_newmetatable(L, metatableName.c_str());
	luaL_setfuncs(L, functions, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, metatableName.c_str());

	delete[] functions;
}

void BrickHolder::pushLua(lua_State* L, const Brick* brick) const
{
	lua_newtable(L);
	lua_getglobal(L, metatableName.c_str());
	lua_setmetatable(L, -2);

	lua_pushinteger(L, brick->netId);
	lua_setfield(L, -2, "id");

	lua_pushinteger(L, BrickTypeId);
	lua_setfield(L, -2, "type");
}

Brick* BrickHolder::popLua(lua_State* L) const
{
	scope("BrickHolder::popLua");

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		error("Expected a brick");
		return nullptr;
	}

	lua_getfield(L, -1, "type");
	bool isBrick = lua_isinteger(L, -1) && lua_tointeger(L, -1) == BrickTypeId;
	lua_pop(L, 1);

	lua_getfield(L, -1, "id");
	bool hasId = lua_isinteger(L, -1);
	netIDType id = hasId ? (netIDType)lua_tointeger(L, -1) : 0;
	lua_pop(L, 2);

	if (!isBrick || !hasId)
	{
		error("Expected a brick");
		return nullptr;
	}

	Brick* brick = find(id);
	if (!brick)
		error("Brick with ID " + std::to_string(id) + " was removed");
	return brick;
}

BrickHolder::BrickHolder(std::shared_ptr<PhysicsWorld> _world, const BrickTypes* _types, Server* _server) : world(_world), types(_types), server(_server)
{
	//Global to Bullet, but they only look at the bodies they're given, so the client's and server's worlds can share them
	gCalculateCombinedRestitutionCallback = &combinedRestitution;
	gCalculateCombinedFrictionCallback = &combinedFriction;
}

BrickHolder::~BrickHolder()
{
	//Don't queue removal packets for a holder that's going away, or reach for the Lua state it's being torn down with
	server = nullptr;
	spawnAttachments = nullptr;
	removeAttachments = nullptr;
	clear();

	for (auto& entry : shapes)
		delete entry.second;
}

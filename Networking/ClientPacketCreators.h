#pragma once

#include "../LandOfDran.h"
#include "../Bricks/BrickHolder.h"
#include "../GameLoop/PlayerAppearance.h"

/*
	Global inline functions that help create miscellaneous packet types from passed parameters
	These are for packets sent to the server from the client
*/

/*
	1 byte		-	packet type
	12 bytes	-	camera position
	12 bytes	-	camera direction
	1 byte		-	SDL mouse button mask, just the button let go for a release
	1 byte		-	ClickFlag flags
	pressFlags is ClickFlag_LeftPress or ClickFlag_RightPress for a press of that button
*/
inline ENetPacket* makeMouseClickPacket(glm::vec3 pos, glm::vec3 dir, unsigned char mask, bool release = false, unsigned char pressFlags = 0)
{
	ENetPacket* ret = enet_packet_create(NULL, 3 + 6 * sizeof(float), getFlagsFromChannel(OtherReliable));
	ret->data[2 + sizeof(float) * 6] = (release ? ClickFlag_Release : 0) | pressFlags;

	ret->data[0] = (unsigned char)ClickDetails;
	memcpy(ret->data + 1 + sizeof(float) * 0, &pos.x, sizeof(float));
	memcpy(ret->data + 1 + sizeof(float) * 1, &pos.y, sizeof(float));
	memcpy(ret->data + 1 + sizeof(float) * 2, &pos.z, sizeof(float));
	memcpy(ret->data + 1 + sizeof(float) * 3, &dir.x, sizeof(float));
	memcpy(ret->data + 1 + sizeof(float) * 4, &dir.y, sizeof(float));
	memcpy(ret->data + 1 + sizeof(float) * 5, &dir.z, sizeof(float));
	ret->data[1 + sizeof(float) * 6] = mask;

	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	1 if the item bar is out
	1 byte		-	picked slot
*/
inline ENetPacket* makeInventorySelectPacket(bool open, int slot)
{
	ENetPacket* ret = enet_packet_create(NULL, 3, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)InventorySelect;
	ret->data[1] = open ? 1 : 0;
	ret->data[2] = (unsigned char)slot;
	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	picked slot
*/
inline ENetPacket* makeDropItemPacket(int slot)
{
	ENetPacket* ret = enet_packet_create(NULL, 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)DropItemRequest;
	ret->data[1] = (unsigned char)slot;
	return ret;
}

/*
	1 byte		-	packet type
	4 bytes		-	paint color: red, green, blue, alpha, 0-255
	1 byte		-	paint material, a BrickMaterial
*/
inline ENetPacket* makePaintChoicePacket(glm::u8vec4 color, unsigned char material)
{
	ENetPacket* ret = enet_packet_create(NULL, 6, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)PaintChoice;
	memcpy(ret->data + 1, &color[0], 4);
	ret->data[5] = material;
	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	1 while the paint palette wants a paint can in our hand

	Sent as the palette comes out and again once the item bar or the brick bar takes the can away
*/
inline ENetPacket* makePaintCanPacket(bool out)
{
	ENetPacket* ret = enet_packet_create(NULL, 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)PaintCanRequest;
	ret->data[1] = out ? 1 : 0;
	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	client game version
	1 byte		-	name length, max 255
	1-255 bytes	-	name
*/
inline ENetPacket* makeConnectionRequest(std::string name)
{
	ENetPacket* ret = enet_packet_create(NULL, name.length() + 3, getFlagsFromChannel(JoinNegotiation));

	ret->data[0] = (unsigned char)ConnectionRequest;
	ret->data[1] = (unsigned char)GAME_VERSION;
	ret->data[2] = (unsigned char)name.length();
	memcpy(ret->data + 3, name.c_str(), name.length());

	return ret;
}

/*
	1 byte		-		packet type
	4 bytes		-		controlled dynamic id
	1 byte		-		movement control flags
	4 bytes		-		camera x direction
	4 bytes		-		camera y direction
	4 bytes		-		camera z direction
	4 bytes		-		camera x position
	4 bytes		-		camera y position
	4 bytes		-		camera z position
*/
inline ENetPacket* makeMovementInputs(netIDType controlledDynamicID, bool jump, bool jumpHeld, bool forward,bool backward,bool left,bool right, bool jet, bool crawl, glm::vec3 cameraDirection, glm::vec3 cameraPosition)
{
	//Unreliable and resent every interval regardless of whether the state changed (see PlayerController::makeMovementInputsPacket) -
	//losing any single one just means the server acts on a stale input state for one more interval before the next resend corrects it,
	//rather than risking head-of-line blocking other client->server reliable traffic under sustained loss
	ENetPacket* ret = enet_packet_create(NULL, 30, getFlagsFromChannel(Unreliable));

	unsigned char movementFlags = 0;
	movementFlags |= (jump ? MovementFlag_Jump : 0);
	movementFlags |= (forward ? MovementFlag_Forward : 0);
	movementFlags |= (backward ? MovementFlag_Backward : 0);
	movementFlags |= (left ? MovementFlag_Left : 0);
	movementFlags |= (right ? MovementFlag_Right : 0);
	movementFlags |= (jumpHeld ? MovementFlag_JumpHeld : 0);
	movementFlags |= (jet ? MovementFlag_Jet : 0);
	movementFlags |= (crawl ? MovementFlag_Crawl : 0);

	ret->data[0] = (unsigned char)MovementInputs;
	memcpy(ret->data + 1, &controlledDynamicID, sizeof(netIDType));
	ret->data[1 + sizeof(netIDType)] = movementFlags;
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 0, &cameraDirection.x, sizeof(float));
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 1, &cameraDirection.y, sizeof(float));
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 2, &cameraDirection.z, sizeof(float));
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 3, &cameraPosition.x, sizeof(float));
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 4, &cameraPosition.y, sizeof(float));
	memcpy(ret->data + 1 + sizeof(netIDType) + 1 + sizeof(float) * 5, &cameraPosition.z, sizeof(float));

	return ret;
}

//1 byte packet type, then a brick record (see BrickHolder::writeRecord) whose id the server ignores
inline ENetPacket* makePlantBrickPacket(const Brick& brick)
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + BrickHolder::recordBytes, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)PlantBrickRequest;
	BrickHolder::writeRecord(&brick, ret->data + 1);
	return ret;
}

inline ENetPacket* makeUndoBrickPacket()
{
	char data = UndoBrickRequest;
	return enet_packet_create(&data, 1, getFlagsFromChannel(OtherReliable));
}

/*
	1 byte   - packet type
	1 byte   - flags, see VoiceFlag_End
	2 bytes  - sequence number, one more for each 20 ms frame
	The rest - one Opus frame, up to maxVoiceFrameBytes, nothing with VoiceFlag_End
*/
inline ENetPacket* makeVoiceFramePacket(unsigned char flags, uint16_t sequence, const unsigned char* frame, unsigned int length)
{
	length = std::min(length, maxVoiceFrameBytes);

	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(uint16_t) + length, getFlagsFromChannel(VoiceData));
	ret->data[0] = (unsigned char)VoiceFrame;
	ret->data[1] = flags;
	memcpy(ret->data + 2, &sequence, sizeof(uint16_t));
	if (length > 0)
		memcpy(ret->data + 2 + sizeof(uint16_t), frame, length);

	return ret;
}

//One byte lets the server know we finished phase one loading
inline ENetPacket* makeLoadingFinished()
{
	char theSmallestPacketEver[1];
	theSmallestPacketEver[0] = LoadingFinished;
	ENetPacket* ret = enet_packet_create(theSmallestPacketEver, 1, getFlagsFromChannel(JoinNegotiation));
	return ret;
}

//Send a chat message to the server
inline ENetPacket* makeChatMessage(const std::string &message)
{
	ENetPacket* ret = enet_packet_create(NULL, message.length() + 2, getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)ChatMessage;
	ret->data[1] = (unsigned char)message.length();
	memcpy(ret->data + 2, message.c_str(), message.length());

	return ret;
}

//Try to log into the server's eval console
inline ENetPacket* attemptEvalLogin(const std::string &password)
{
	ENetPacket* ret = enet_packet_create(NULL, password.length() + 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)EvalLogin;
	ret->data[1] = (unsigned char)password.length();
	memcpy(ret->data + 2, password.c_str(), password.length());

	return ret;
}

inline ENetPacket *evalCommand(const std::string& password, const std::string &command)
{
	ENetPacket* ret = enet_packet_create(NULL, command.length() + password.length() + 4, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)EvalCommand;

	unsigned short commandLength = command.length();
	memcpy(ret->data + 1, &commandLength, sizeof(unsigned short));

	memcpy(ret->data + 3, command.c_str(), command.length());
	ret->data[command.length() + 3] = (unsigned char)password.length();
	memcpy(ret->data + command.length() + 4, password.c_str(), password.length());
	//TODO: Hash password

	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	1 to turn the flashlight on, 0 for off
	12 bytes	-	red, green, blue floats, 0-1
*/
inline ENetPacket* makeFlashlightPacket(bool on, glm::vec3 color)
{
	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(float) * 3, getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)FlashlightRequest;
	ret->data[1] = on ? 1 : 0;
	memcpy(ret->data + 2, &color[0], sizeof(float) * 3);

	return ret;
}

//The same packet asking to switch the headlight of the vehicle we drive on or off, see FlashlightRequest_VehicleLight
inline ENetPacket* makeVehicleLightPacket()
{
	ENetPacket* ret = enet_packet_create(NULL, 2 + sizeof(float) * 3, getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)FlashlightRequest;
	ret->data[1] = FlashlightRequest_VehicleLight;
	memset(ret->data + 2, 0, sizeof(float) * 3);

	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	1 while our camera is off flying, 0 once it's back on our player

	Sent when the free camera keys drop the camera off our player or our player at the camera, see LoopClient::setFreeCamera
*/
inline ENetPacket* makeFreeCameraPacket(bool on)
{
	ENetPacket* ret = enet_packet_create(NULL, 2, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)FreeCameraRequest;
	ret->data[1] = on ? 1 : 0;
	return ret;
}

/*
	1 byte		-	packet type

	Pressed the next seat key while riding a vehicle
*/
inline ENetPacket* makeSeatCyclePacket()
{
	ENetPacket* ret = enet_packet_create(NULL, 1, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)SeatCycleRequest;
	return ret;
}

/*
	1 byte		-	packet type
*/
inline ENetPacket* makePlayerGrabPacket()
{
	ENetPacket* ret = enet_packet_create(NULL, 1, getFlagsFromChannel(Unreliable));
	ret->data[0] = (unsigned char)PlayerGrab;
	return ret;
}

/*
	1 byte		-	packet type
	12 bytes	-	camera position
	12 bytes	-	camera direction
*/
inline ENetPacket* makeWrenchRequestPacket(glm::vec3 position, glm::vec3 direction)
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(float) * 6, getFlagsFromChannel(OtherReliable));

	ret->data[0] = (unsigned char)WrenchRequest;
	memcpy(ret->data + 1, &position[0], sizeof(float) * 3);
	memcpy(ret->data + 1 + sizeof(float) * 3, &direction[0], sizeof(float) * 3);

	return ret;
}

/*
	1 byte		-	packet type
	4 bytes		-	brick net ID, from the WrenchDialog packet
	1 byte		-	1 if it collides
	1 byte		-	name length
	0-255 bytes	-	name
	Then		-	BrickAttachments::write
*/
inline ENetPacket* makeWrenchSubmitPacket(netIDType brickID, bool collides, const std::string& name, const BrickAttachments& attachments)
{
	std::string shortName = name.substr(0, 255);

	std::vector<unsigned char> bytes;
	bytes.push_back((unsigned char)WrenchSubmit);
	bytes.resize(1 + sizeof(netIDType));
	memcpy(bytes.data() + 1, &brickID, sizeof(netIDType));
	bytes.push_back(collides ? 1 : 0);
	bytes.push_back((unsigned char)shortName.length());
	bytes.insert(bytes.end(), shortName.begin(), shortName.end());
	attachments.write(bytes);

	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable));
}

/*
	1 byte		-	packet type
	4 bytes		-	brick net ID, from the OpenPrintMenu packet
	1 byte		-	print name length
	0-255 bytes	-	print name, "" for no print
*/
inline ENetPacket* makePrintSubmitPacket(netIDType brickID, const std::string& printName)
{
	std::string shortPrint = printName.substr(0, 255);

	std::vector<unsigned char> bytes;
	bytes.push_back((unsigned char)PrintSubmit);
	bytes.resize(1 + sizeof(netIDType));
	memcpy(bytes.data() + 1, &brickID, sizeof(netIDType));
	bytes.push_back((unsigned char)shortPrint.length());
	bytes.insert(bytes.end(), shortPrint.begin(), shortPrint.end());

	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable));
}

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
	The rest	-	BrickAttachments::write with its music, horn, and headlight as the light
*/
inline ENetPacket* makeVehicleWrenchSubmitPacket(netIDType vehicleID, const BrickAttachments& attachments)
{
	std::vector<unsigned char> bytes;
	bytes.push_back((unsigned char)VehicleWrenchSubmit);
	bytes.resize(1 + sizeof(netIDType));
	memcpy(bytes.data() + 1, &vehicleID, sizeof(netIDType));

	//Its music, horn, and headlight as the light; a vehicle has no emitter or wheel to speak of, and spawns nothing
	BrickAttachments settings = attachments;
	settings.emitterName = "";
	settings.hasWheel = false;
	settings.hasSteering = false;
	settings.lightIsHeadlight = false;
	settings.vehicleSpawnName = "";
	settings.itemSpawnName = "";
	settings.write(bytes);

	return enet_packet_create(bytes.data(), bytes.size(), getFlagsFromChannel(OtherReliable));
}

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
*/
inline ENetPacket* makeVehicleSaveRequestPacket(netIDType vehicleID)
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)VehicleSaveRequest;
	memcpy(ret->data + 1, &vehicleID, sizeof(netIDType));
	return ret;
}

/*
	A vehicle save file in pieces, each:
	1 byte		-	packet type
	4 bytes		-	upload ID, a new one for each file
	1 byte		-	1 to load it as a vehicle, 0 as bricks
	4 bytes		-	size of the whole file
	4 bytes		-	where in the file this packet's bytes go
	12 bytes	-	grid voxel the middle of its bottom goes at
	The rest	-	file bytes
*/
inline std::vector<ENetPacket*> makeVehicleUploadPackets(uint32_t uploadID, bool asVehicle, const glm::ivec3& spot, const std::string& file)
{
	static constexpr size_t chunkBytes = 1100;
	static constexpr size_t headerBytes = 1 + sizeof(uint32_t) + 1 + sizeof(uint32_t) * 2 + sizeof(int32_t) * 3;
	const int32_t spotValues[3] = { spot.x, spot.y, spot.z };

	std::vector<ENetPacket*> packets;
	uint32_t total = (uint32_t)file.size();
	for (size_t offset = 0; offset < file.size(); offset += chunkBytes)
	{
		size_t length = std::min(chunkBytes, file.size() - offset);
		uint32_t at = (uint32_t)offset;

		ENetPacket* ret = enet_packet_create(NULL, headerBytes + length, getFlagsFromChannel(OtherReliable));
		ret->data[0] = (unsigned char)VehicleUpload;
		memcpy(ret->data + 1, &uploadID, sizeof(uint32_t));
		ret->data[1 + sizeof(uint32_t)] = asVehicle ? 1 : 0;
		memcpy(ret->data + 2 + sizeof(uint32_t), &total, sizeof(uint32_t));
		memcpy(ret->data + 2 + sizeof(uint32_t) * 2, &at, sizeof(uint32_t));
		memcpy(ret->data + 2 + sizeof(uint32_t) * 3, spotValues, sizeof(spotValues));
		memcpy(ret->data + headerBytes, file.data() + offset, length);
		packets.push_back(ret);
	}
	return packets;
}

/*
	1 byte		-	packet type
	4 bytes		-	vehicle net ID
*/
inline ENetPacket* makeVehicleRemoveRequestPacket(netIDType vehicleID)
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(netIDType), getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)VehicleRemoveRequest;
	memcpy(ret->data + 1, &vehicleID, sizeof(netIDType));
	return ret;
}

/*
	1 byte		-	packet type
	12 bytes	-	selection box min corner, grid voxels, inclusive
	12 bytes	-	selection box max corner, exclusive
*/
inline ENetPacket* makeSliceRequestPacket(const glm::ivec3& min, const glm::ivec3& max)
{
	ENetPacket* ret = enet_packet_create(NULL, 1 + sizeof(int32_t) * 6, getFlagsFromChannel(OtherReliable));
	ret->data[0] = (unsigned char)SliceRequest;
	const int32_t corners[6] = { min.x, min.y, min.z, max.x, max.y, max.z };
	memcpy(ret->data + 1, corners, sizeof(corners));
	return ret;
}

/*
	1 byte		-	packet type
	1 byte		-	face name length, 0 for no face
	0-64 bytes	-	face name, a file in Assets/faces
	1 byte		-	shirt name length, 0 for no shirt
	0-64 bytes	-	shirt name, a file in Assets/shirts
	1 byte		-	how many painted parts follow
	Per part:
	1 byte		-	mesh name length
	1-64 bytes	-	mesh name
	3 bytes		-	red, green, blue, 0-255
*/
/*
	1 byte		-	packet type
	1 byte		-	face name length, then the name
	1 byte		-	shirt name length, then the name
	1 byte		-	hat name length, then the name
	1 byte		-	whether the hat is painted
	3 bytes		-	the hat's color
	1 byte		-	the hat's size as a percentage of its descriptor's, see PlayerAppearance::hatScale
	1 byte		-	how many parts are painted, then for each its name length, name, and 3 bytes of color
*/
inline ENetPacket* makeAppearanceChoicePacket(const PlayerAppearance& appearance)
{
	std::string face = appearance.face.substr(0, PlayerAppearance::maxNameLength);
	std::string shirt = appearance.shirt.substr(0, PlayerAppearance::maxNameLength);
	std::string hat = appearance.hat.substr(0, PlayerAppearance::maxNameLength);

	std::vector<std::pair<std::string, glm::vec3>> colors;
	for (const auto& [meshName, color] : appearance.colors)
	{
		if (colors.size() < PlayerAppearance::maxColors && !meshName.empty())
			colors.emplace_back(meshName.substr(0, PlayerAppearance::maxNameLength), color);
	}

	size_t length = 10 + face.length() + shirt.length() + hat.length();
	for (const auto& [meshName, color] : colors)
		length += 4 + meshName.length();

	ENetPacket* ret = enet_packet_create(NULL, length, getFlagsFromChannel(JoinNegotiation));

	ret->data[0] = (unsigned char)AppearanceChoice;

	size_t byteIterator = 1;
	for (const std::string& name : { face, shirt, hat })
	{
		ret->data[byteIterator++] = (unsigned char)name.length();
		memcpy(ret->data + byteIterator, name.data(), name.length());
		byteIterator += name.length();
	}

	ret->data[byteIterator++] = appearance.hatColor.a > 0 ? 1 : 0;
	glm::vec3 hatBytes = glm::clamp(glm::vec3(appearance.hatColor), 0.0f, 1.0f) * 255.0f + 0.5f;
	for (int channel = 0; channel < 3; channel++)
		ret->data[byteIterator++] = (unsigned char)hatBytes[channel];

	ret->data[byteIterator++] = (unsigned char)(glm::clamp(appearance.hatScale, PlayerAppearance::minHatScale, PlayerAppearance::maxHatScale) * 100.0f + 0.5f);

	ret->data[byteIterator++] = (unsigned char)colors.size();
	for (const auto& [meshName, color] : colors)
	{
		ret->data[byteIterator++] = (unsigned char)meshName.length();
		memcpy(ret->data + byteIterator, meshName.data(), meshName.length());
		byteIterator += meshName.length();

		glm::vec3 bytes = glm::clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f;
		for (int channel = 0; channel < 3; channel++)
			ret->data[byteIterator++] = (unsigned char)bytes[channel];
	}

	return ret;
}

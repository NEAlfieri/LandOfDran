#include "ClientData.h"

#include "ServerProgramData.h"
#include "../LuaFunctions/SoundLua.h"
#include "../LuaFunctions/VehicleLua.h"

void ClientData::leaveVehicle()
{
	if (!vehicle.expired())
		exitVehicle(*this, false);
}

//A bright, wide spotlight
static constexpr float flashlightBrightness = 150.0f;
static constexpr float flashlightConeAngle = 80.0f;
//A spotlight's corona only shows from inside its beam, so its owner doesn't see it, only people it's pointed at
static constexpr float flashlightCoronaWidth = 0.8f;

void ClientData::setFlashlight(const ServerProgramData* pd, bool on, const glm::vec3& color)
{
	if (!pd->lights)
		return;

	std::shared_ptr<Light> light = flashlight.lock();
	std::shared_ptr<Dynamic> holder = controllers.empty() ? nullptr : controllers[0].target.lock();

	if (on && flashlightEnabled && holder)
	{
		if (light)
		{
			if (light->getColor() != glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f)))
				light->setColor(color);
			return;
		}

		light = pd->lights->create(b2g3(holder->getPosition()), color, flashlightBrightness, 0.0f, flashlightCoronaWidth);
		light->setConeAngle(flashlightConeAngle);
		light->setDirection(controllers[0].lastCameraDirection);
		light->setHolder(holder);
		flashlight = light;

		playSoundOn("LightOn", holder, 1.0f, 1.0f);
		return;
	}

	if (!light)
		return;

	glm::vec3 lastPosition = light->getPosition();
	pd->lights->destroy(light);
	flashlight.reset();

	if (holder)
		playSoundOn("LightOff", holder, 1.0f, 1.0f);
	else
		playSoundAt("LightOff", lastPosition, 1.0f, 1.0f);
}

//A bright yellow ball of light, wide enough to spot from across the map
static constexpr float freeCameraBrightness = 150.0f;
static constexpr float freeCameraCoronaWidth = 6.0f;
static const glm::vec3 freeCameraColor = glm::vec3(1.0f, 0.85f, 0.25f);

glm::vec3 ClientData::getCameraPosition() const
{
	return controllers.empty() ? glm::vec3(0) : controllers[0].lastCameraPosition;
}

void ClientData::setFreeCamera(const ServerProgramData* pd, bool on)
{
	if (!pd->lights)
		return;

	freeCamera = on && freeCameraEnabled;

	std::shared_ptr<Light> light = freeCameraLight.lock();

	if (freeCamera)
	{
		if (!light)
		{
			light = pd->lights->create(getCameraPosition(), freeCameraColor, freeCameraBrightness, 0.0f, freeCameraCoronaWidth);
			freeCameraLight = light;
		}
		return;
	}

	if (light)
		pd->lights->destroy(light);
	freeCameraLight.reset();
}

void ClientData::sendAbilities() const
{
	if (!client)
		return;

	char data[2] = { (char)PlayerAbilities, (char)((jetsEnabled ? PlayerAbility_Jets : 0) | (flashlightEnabled ? PlayerAbility_Flashlight : 0) |
		(freeCameraEnabled ? PlayerAbility_FreeCamera : 0)) };
	client->send(data, 2, OtherReliable);
}

void ClientData::removeEffects(const ServerProgramData* pd)
{
	if (std::shared_ptr<Light> light = flashlight.lock())
		pd->lights->destroy(light);
	flashlight.reset();

	if (std::shared_ptr<Light> light = freeCameraLight.lock())
		pd->lights->destroy(light);
	freeCameraLight.reset();
	freeCamera = false;

	for (PlayerController& controller : controllers)
	{
		for (std::weak_ptr<Emitter>& jet : controller.jetEmitters)
		{
			if (std::shared_ptr<Emitter> flame = jet.lock())
				pd->emitters->destroy(flame);
			jet.reset();
		}
	}
}

int ClientData::addItem(const ServerProgramData* pd, const std::shared_ptr<Item>& item, int slot)
{
	if (item->isHeld())
		return -1;

	if (slot == -1)
	{
		for (int a = 0; a < inventorySize && slot == -1; a++)
		{
			if (inventory[a].expired())
				slot = a;
		}
	}

	if (slot < 0 || slot >= inventorySize || !inventory[slot].expired())
		return -1;

	item->removeFromWorld();
	item->owner = me;
	item->slot = slot;
	inventory[slot] = item;

	pd->markItemChanged(item);
	sendInventory();
	return slot;
}

btTransform ClientData::droppedItemTransform(const Item& item) const
{
	//Where LoopServer::updateItems last moved it along with its holder, or out in front of them so it doesn't land inside them
	btTransform transform = item.body->getWorldTransform();

	std::shared_ptr<Dynamic> holder = item.getHolder();
	if (!holder)
		return transform;

	std::shared_ptr<Model> model = holder->getType()->getModel();
	glm::vec3 halfExtents = model->getColHalfExtents();

	glm::vec3 ahead = controllers[0].lastCameraDirection;
	ahead.y = 0;
	ahead = glm::length(ahead) > 0.001f ? glm::normalize(ahead) : glm::vec3(0, 0, -1);

	float reach = std::max(halfExtents.x, halfExtents.z) + glm::length(item.getType()->getModel()->getColHalfExtents()) + 0.25f;
	transform.setOrigin(g2b3(b2g3(holder->getPosition()) + model->getColOffset() + ahead * reach));
	return transform;
}

std::shared_ptr<Item> ClientData::removeItem(const ServerProgramData* pd, int slot)
{
	if (slot < 0 || slot >= inventorySize)
		return nullptr;

	std::shared_ptr<Item> item = inventory[slot].lock();
	inventory[slot].reset();
	if (!item)
		return nullptr;

	btTransform transform = droppedItemTransform(*item);

	item->owner.reset();
	item->slot = -1;
	item->returnToWorld(transform);

	pd->markItemChanged(item);
	sendInventory();
	return item;
}

std::shared_ptr<Item> ClientData::setHandItem(const ServerProgramData* pd, const std::shared_ptr<Item>& item)
{
	std::shared_ptr<Item> previous = handItem.lock();
	handItem.reset();

	if (previous && previous != item)
	{
		btTransform transform = droppedItemTransform(*previous);
		previous->owner.reset();
		previous->slot = -1;
		previous->returnToWorld(transform);
		pd->markItemChanged(previous);
	}

	if (item)
	{
		item->removeFromWorld();
		item->owner = me;
		item->slot = -1;
		handItem = item;
		pd->markItemChanged(item);
	}

	return previous;
}

std::shared_ptr<Item> ClientData::getHeldItem() const
{
	//An item Lua put in their hand is held instead of whatever their item bar has picked, see Item::isEquipped
	if (std::shared_ptr<Item> hand = handItem.lock())
		return hand;

	if (!inventoryOpen || selectedSlot < 0 || selectedSlot >= inventorySize)
		return nullptr;

	return inventory[selectedSlot].lock();
}

void ClientData::forgetItem(const Item& item)
{
	//An item in their hand was never in a slot, see setHandItem
	if (handItem.lock().get() == &item)
	{
		handItem.reset();
		return;
	}

	if (item.slot < 0 || item.slot >= inventorySize || inventory[item.slot].lock().get() != &item)
		return;

	inventory[item.slot].reset();
	sendInventory();
}

void ClientData::dropAllItems(const ServerProgramData* pd)
{
	setHandItem(pd, nullptr);
	for (int a = 0; a < inventorySize; a++)
		removeItem(pd, a);
}

/*
	1 byte					-	packet type
	4 bytes per slot		-	net ID of the item in each slot, NO_ID for an empty one
*/
void ClientData::sendClickAction(const ClickAction& action) const
{
	if (!client)
		return;

	ENetPacket* packet = enet_packet_create(NULL, 1 + action.packedSize(), getFlagsFromChannel(OtherReliable));
	packet->data[0] = (unsigned char)SetClickAction;
	action.writeTo(packet->data + 1);

	client->send(packet, OtherReliable);
}

void ClientData::sendInventory() const
{
	if (!client)
		return;

	ENetPacket* packet = enet_packet_create(NULL, 1 + sizeof(netIDType) * inventorySize, getFlagsFromChannel(OtherReliable));
	packet->data[0] = (unsigned char)InventoryContents;
	for (int a = 0; a < inventorySize; a++)
	{
		std::shared_ptr<Item> item = inventory[a].lock();
		netIDType id = item ? item->getID() : NO_ID;
		memcpy(packet->data + 1 + sizeof(netIDType) * a, &id, sizeof(netIDType));
	}

	client->send(packet, OtherReliable);
}

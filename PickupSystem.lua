--[[
	Pickup system

	Left-click a non-player dynamic to pick it up. It snaps to a fixed spot in view
	space in front of your cursor (not just your avatar's facing) and hovers there,
	bobbing slightly. Left-click again to drop it, or right-click to throw it in
	whatever direction you're currently looking. It's also dropped automatically
	if you disconnect, or if the item stops being snapped for any other reason.

	Requires the engine's cursor-snapping support (dynamic:snapToCursor/unsnap/
	isSnapped/getSnapClient, client:getCursorItem) - see LoopServer/Dynamic/ClientLua.

	Add this to serverstart.lua with:
		dofile("PickupSystem.lua")
	(or just paste the contents in directly)
]]

--From NetTypes/NetType.h's SimObjectType enum
local DYNAMIC_TYPE_ID = 1

--How far (in world units) the cursor can reach to pick something up
local PICKUP_RANGE = 15

--View-space offset the item snaps to: x = right, y = up, z = distance in front of the camera
local CARRY_RIGHT = 0
local CARRY_HEIGHT = -1
local CARRY_FORWARD = 5

--Vertical bob size/speed of the hover effect, and how often (ms) it's refreshed
local BOB_AMPLITUDE = 0.15
local BOB_SPEED = 0.004
local BOB_TICK_MS = 100

--Speed (world units/sec) a thrown item is launched at, along the direction you're looking
local THROW_SPEED = 40

--clientId -> {itemId, bobPhase}, just enough bookkeeping to know what a client is holding
heldItemByClient = {}

function pickupItem(client, item)
	item:snapToCursor(client, CARRY_RIGHT, CARRY_HEIGHT, CARRY_FORWARD)
	heldItemByClient[client:getID()] = { itemId = item.id, bobPhase = 0 }
end

function dropItem(clientId)
	local held = heldItemByClient[clientId]
	if held == nil then
		return
	end

	local item = getDynamicId(held.itemId)
	if item ~= nil then
		item:unsnap()
	end

	heldItemByClient[clientId] = nil
end

function throwItem(clientId, dirX, dirY, dirZ)
	local held = heldItemByClient[clientId]
	if held == nil then
		return
	end

	local item = getDynamicId(held.itemId)
	if item ~= nil then
		item:unsnap()
		item:setVelocity(dirX * THROW_SPEED, dirY * THROW_SPEED, dirZ * THROW_SPEED)
	end

	heldItemByClient[clientId] = nil
end

function pickupClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	local clientId = client:getID()

	--Right click: throw whatever's currently held, in whatever direction you're looking
	if (mask & 4) ~= 0 then
		if heldItemByClient[clientId] ~= nil then
			throwItem(clientId, dirX, dirY, dirZ)
		end
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Only react to the left mouse button (SDL_BUTTON_LMASK) beyond this point
	if (mask & 1) == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	if client:getNumControlled() == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Already holding something? This click drops it, wherever it was aimed.
	if heldItemByClient[clientId] ~= nil then
		dropItem(clientId)
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local result = client:getCursorItem(PICKUP_RANGE)

	if result == nil or result.type ~= DYNAMIC_TYPE_ID then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Don't let players pick up other players
	if result:getNumControllers() > 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Don't let it be picked up twice
	if result:isSnapped() then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	pickupItem(client, result)

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "pickupClick")

function dropHeldOnLeave(client)
	dropItem(client:getID())
	return client
end
registerEventListener("ClientLeave", "dropHeldOnLeave")

--[[
	Re-snaps every held item slightly higher/lower each tick to produce a hover bob.
	Also doubles as cleanup: if an item stopped being snapped for any reason (unsnapped
	elsewhere, or the engine auto-unsnapped it because the owning client disconnected),
	this notices via isSnapped()/getSnapClient() and drops our own bookkeeping for it.
]]
function updateHeldItemBob()
	for clientId, held in pairs(heldItemByClient) do
		local item = getDynamicId(held.itemId)
		local snapClient = item ~= nil and item:getSnapClient() or nil

		if item == nil or snapClient == nil then
			heldItemByClient[clientId] = nil
		else
			held.bobPhase = held.bobPhase + BOB_TICK_MS
			local bob = math.sin(held.bobPhase * BOB_SPEED) * BOB_AMPLITUDE

			item:snapToCursor(snapClient, CARRY_RIGHT, CARRY_HEIGHT + bob, CARRY_FORWARD)
		end
	end

	schedule(BOB_TICK_MS, "updateHeldItemBob")
end
schedule(BOB_TICK_MS, "updateHeldItemBob")

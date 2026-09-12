--[[
	Pickup system

	Left-click a non-player dynamic to pick it up. It will hover just in front of
	wherever you're facing, bobbing slightly. Left-click again to drop it.
	It also gets dropped automatically if you disconnect, or if the dynamic that's
	carrying it disappears for any other reason (there's currently no health/death
	system in the engine to hook a "die" event off of, so this is the closest
	equivalent - see the note above updateHeldItems below).

	Add this to serverstart.lua with:
		dofile("PickupSystem.lua")
	(or just paste the contents in directly)
]]

--From NetTypes/NetType.h's SimObjectType enum
local DYNAMIC_TYPE_ID = 1

--How far (in world units) a click can reach to pick something up
local PICKUP_RANGE = 15
--How far in front of the carrier the item hovers
local CARRY_DISTANCE = 4
--How high above the carrier's feet the item hovers
local CARRY_HEIGHT = 4
--How often (ms) the held item's position is refreshed
local HOVER_TICK_MS = 50
--Vertical bob size/speed of the hover effect
local BOB_AMPLITUDE = 0.15
local BOB_SPEED = 0.004

--carrierDynamicId -> {itemId, gravityX/Y/Z (to restore on drop), bobPhase}
heldItems = {}
--itemDynamicId -> carrierDynamicId, so we can tell an item is already being carried
heldByItemId = {}

--Rotates vector (vx,vy,vz) by quaternion (w,x,y,z), same convention as dynamic:getRotation()
function quatRotateVector(w, x, y, z, vx, vy, vz)
	local tx = 2 * (y * vz - z * vy)
	local ty = 2 * (z * vx - x * vz)
	local tz = 2 * (x * vy - y * vx)

	local cx = y * tz - z * ty
	local cy = z * tx - x * tz
	local cz = x * ty - y * tx

	return vx + w * tx + cx, vy + w * ty + cy, vz + w * tz + cz
end

function pickupItem(carrierId, item)
	local gx, gy, gz = item:getGravity()

	item:setVelocity(0, 0, 0)
	item:setAngularVelocity(0, 0, 0)
	item:setGravity(0, 0, 0)

	heldItems[carrierId] = {
		itemId = item.id,
		gravityX = gx,
		gravityY = gy,
		gravityZ = gz,
		bobPhase = 0
	}
	heldByItemId[item.id] = carrierId
end

function dropItem(carrierId)
	local held = heldItems[carrierId]
	if held == nil then
		return
	end

	local item = getDynamicId(held.itemId)
	if item ~= nil then
		item:setGravity(held.gravityX, held.gravityY, held.gravityZ)
		item:setVelocity(0, 0, 0)
		item:activate()
	end

	heldByItemId[held.itemId] = nil
	heldItems[carrierId] = nil
end

function pickupClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	--Only react to the left mouse button (SDL_BUTTON_LMASK)
	if (mask & 1) == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	if client:getNumControlled() == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local carrier = client:getControlledIdx(0)

	--Already holding something? This click drops it, wherever it was aimed.
	if heldItems[carrier.id] ~= nil then
		dropItem(carrier.id)
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local result = raycast(
		posX, posY, posZ,
		posX + dirX * PICKUP_RANGE, posY + dirY * PICKUP_RANGE, posZ + dirZ * PICKUP_RANGE,
		carrier
	)

	if result == nil or result.type ~= DYNAMIC_TYPE_ID then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Don't let players pick up other players
	if result:getNumControllers() > 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Don't let it be picked up twice
	if heldByItemId[result.id] ~= nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	pickupItem(carrier.id, result)

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "pickupClick")

function dropHeldOnLeave(client)
	for i = 0, client:getNumControlled() - 1, 1 do
		local carrier = client:getControlledIdx(i)
		if heldItems[carrier.id] ~= nil then
			dropItem(carrier.id)
		end
	end

	return client
end
registerEventListener("ClientLeave", "dropHeldOnLeave")

--[[
	Runs continuously to keep held items hovering in front of their carrier.
	This also doubles as the "drop it if the carrier dies/disconnects" handling:
	there's no health/death event in the engine right now, so if the carrier
	dynamic ever disappears (deleted, disconnect not caught in time, etc) this
	notices it next tick and puts the item back into normal physics.
]]
function updateHeldItems()
	for carrierId, held in pairs(heldItems) do
		local carrier = getDynamicId(carrierId)
		local item = getDynamicId(held.itemId)

		if carrier == nil or item == nil then
			if item ~= nil then
				item:setGravity(held.gravityX, held.gravityY, held.gravityZ)
				item:activate()
			end
			heldByItemId[held.itemId] = nil
			heldItems[carrierId] = nil
		else
			local px, py, pz = carrier:getPosition()
			local w, x, y, z = carrier:getRotation()
			local fx, fy, fz = quatRotateVector(w, x, y, z, 0, 0, -1)

			held.bobPhase = held.bobPhase + HOVER_TICK_MS
			local bob = math.sin(held.bobPhase * BOB_SPEED) * BOB_AMPLITUDE

			item:setPosition(
				px + fx * CARRY_DISTANCE,
				py + CARRY_HEIGHT + bob,
				pz + fz * CARRY_DISTANCE
			)
			item:setVelocity(0, 0, 0)
			item:setAngularVelocity(0, 0, 0)
		end
	end

	schedule(HOVER_TICK_MS, "updateHeldItems")
end
schedule(HOVER_TICK_MS, "updateHeldItems")

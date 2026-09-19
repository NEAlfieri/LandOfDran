--[[
	Hats that can be shot off

	The hat a player picked in their appearance editor is only a part drawn on their head, nothing in the
	physics world knows it's there. So this watches where shots go instead: the add-on weapons report every
	stretch a shot travels through ShotPathListeners, see Support_Weapons.lua, and a stretch that crosses the
	space over someone's head where their hat is knocks it off. So does a shot that lands on the top of their
	head, which also hurts as much as it always did. Nobody shoots off their own hat, and the body a dead
	player leaves behind can lose its hat like anyone else.

	The hat lands as a dynamic nothing of which is drawn, wearing the same part the player wore, see
	Assets/brickhead/droppedhat/droppedHat.txt, so any hat in Assets/brickhead/parts works with nothing written
	for it here. Anyone with nothing on their head can left click one within reach to put it on, whoever it
	came off of. A hat nobody picks up is cleared away after a while, and only so many lie around at once.
	Respawning puts the hat from the appearance editor back on as it always has.

	Loaded as an add-on of its own. It needs the player model to measure where a head is, and hooks into
	the weapons' shot paths whether it loads before or after them.

	Lua can do the same to anyone with knockHatOff(player, dirX, dirY, dirZ), the way the hat should fly.
]]

--The slot a player's hat is worn in, PlayerAppearance::hatSlot
local HAT_SLOT = "hat"

--raycast and getCursorItem give what was hit with this type field for a Dynamic
local DYNAMIC_TYPE_ID = 1

--How far a hat can be reached from to put it on
local HAT_REACH = 12

--How long a hat lies around, and how many can at once before the oldest goes
local HAT_LIFETIME_MS = 180000
local MAX_DROPPED_HATS = 24

--How fast a hat leaves along the shot, how fast upward, and how much chance adds to each
local HAT_FLY_SPEED = 7
local HAT_LIFT_SPEED = 11
local HAT_SCATTER_SPEED = 2.5
local HAT_SPIN = 7

--Where a head is measured on, see the do block below
requireAddOn("System_Players")

--The player model's scale, which a hat's size is measured in, see droppedHat.txt
droppedHatType = newDynamicType("droppedHat", "Assets/brickhead/droppedhat/droppedHat.txt", 0.02, 0.02, 0.02)

--[[
	How tall each hat's crown stands and half how wide it is, in studs at full size, measured from their files.
	Brims, a cap's visor, and a jester cap's horns are left out: it's the crown that has to be hit.
	A hat that isn't listed gets DEFAULT_HAT_SHAPE
]]
local HAT_SHAPES = {
	["cap.txt"] = { height = 0.7, halfWidth = 0.75 },
	["top_hat.txt"] = { height = 0.9, halfWidth = 0.78 },
	["fedora.txt"] = { height = 0.7, halfWidth = 0.8 },
	["jester_cap.txt"] = { height = 1.3, halfWidth = 1.0 },
}
local DEFAULT_HAT_SHAPE = { height = 0.9, halfWidth = 0.75 }

--How far down over the head a hat comes, every one of them is sunk about this far, see their attachoffset lines
local HAT_SINK = 0.3

--The top of the player model's head over its feet, the stud included, which is where hats are put
local HEAD_TOP = 5.6
do
	local _, _, _, _, headTop = getTypeMeshBounds(brickhead, "Head")
	if headTop ~= nil then
		HEAD_TOP = headTop
	end
end

--The same turn Support_Weapons.lua does, for a server running without the add-on weapons: a vector turned by a quaternion, w first
local rotateByQuaternion = rotateByQuaternion or function(qw, qx, qy, qz, x, y, z)
	local tx = 2 * (qy * z - qz * y)
	local ty = 2 * (qz * x - qx * z)
	local tz = 2 * (qx * y - qy * x)
	return x + qw * tx + (qy * tz - qz * ty),
	       y + qw * ty + (qz * tx - qx * tz),
	       z + qw * tz + (qx * ty - qy * tx)
end

--Hats lying around by the ID of the dynamic wearing them, and those IDs oldest first
local droppedHats = {}
local droppedOrder = {}

function removeDroppedHat(hatID)
	if droppedHats[hatID] == nil then
		return
	end
	droppedHats[hatID] = nil

	for index, id in ipairs(droppedOrder) do
		if id == hatID then
			table.remove(droppedOrder, index)
			break
		end
	end

	if dynamicExists(hatID) then
		getDynamicId(hatID):destroy()
	end
end

local function scatter(amount)
	return (math.random() * 2 - 1) * amount
end

--Takes a player's hat off and sends it flying along dirX, dirY, dirZ. False if they weren't wearing one
function knockHatOff(player, dirX, dirY, dirZ)
	local name, r, g, b, a, scale = player:getPart(HAT_SLOT)
	if name == nil then
		return false
	end
	player:setPart(HAT_SLOT, "")

	--From the top of their head, wherever their body is pointing, facing the way they were
	local qw, qx, qy, qz = player:getRotation()
	local x, y, z = player:getPosition()
	local upX, upY, upZ = rotateByQuaternion(qw, qx, qy, qz, 0, HEAD_TOP, 0)

	local hat = createDynamic(droppedHatType, x + upX, y + upY, z + upZ)
	hat:setRotation(qw, qx, qy, qz)
	hat:setPart(HAT_SLOT, name, r, g, b, a, scale)

	--Along the shot but never into the ground, and up, on top of however they were moving
	local level = math.sqrt(dirX * dirX + dirZ * dirZ)
	if level < 0.001 then
		dirX, dirZ, level = scatter(1), scatter(1), 1
	end
	local velX, velY, velZ = player:getVelocity()
	hat:setVelocity(velX + dirX / level * HAT_FLY_SPEED + scatter(HAT_SCATTER_SPEED),
		math.max(velY, 0) + HAT_LIFT_SPEED + scatter(HAT_SCATTER_SPEED),
		velZ + dirZ / level * HAT_FLY_SPEED + scatter(HAT_SCATTER_SPEED))
	hat:setAngularVelocity(scatter(HAT_SPIN), scatter(HAT_SPIN), scatter(HAT_SPIN))
	hat:playSound("ClickMove")

	droppedHats[hat.id] = { name = name, r = r, g = g, b = b, a = a, scale = scale }
	table.insert(droppedOrder, hat.id)
	schedule(HAT_LIFETIME_MS, "removeDroppedHat", hat.id)
	while #droppedOrder > MAX_DROPPED_HATS do
		removeDroppedHat(droppedOrder[1])
	end

	return true
end

--Whether the way from one point to another crosses a box, both points and the box in the same space
local function crossesBox(fromX, fromY, fromZ, toX, toY, toZ, lowX, lowY, lowZ, highX, highY, highZ)
	local enter, leave = 0, 1
	local from = { fromX, fromY, fromZ }
	local along = { toX - fromX, toY - fromY, toZ - fromZ }
	local low = { lowX, lowY, lowZ }
	local high = { highX, highY, highZ }

	for axis = 1, 3 do
		if math.abs(along[axis]) < 0.000001 then
			if from[axis] < low[axis] or from[axis] > high[axis] then
				return false
			end
		else
			local first = (low[axis] - from[axis]) / along[axis]
			local second = (high[axis] - from[axis]) / along[axis]
			if first > second then
				first, second = second, first
			end
			enter = math.max(enter, first)
			leave = math.min(leave, second)
			if enter > leave then
				return false
			end
		end
	end

	return true
end

--A stretch of a shot's way, from Support_Weapons: whoever's hat it crosses, or whose head it lands on the top of, loses it
function hatShotPath(fromX, fromY, fromZ, toX, toY, toZ, shooter, hit)
	local dirX, dirY, dirZ = toX - fromX, toY - fromY, toZ - fromZ

	--Anything wearing a hat, not only players: the body someone left behind can lose its hat too
	--A hat knocked off in here is a new dynamic at the end of the list, which the count taken first leaves out
	for index = 0, getNumDynamics() - 1 do
		local player = getDynamicIdx(index)

		if droppedHats[player.id] == nil and (shooter == nil or player.id ~= shooter.id) then
			local name, _, _, _, _, scale = player:getPart(HAT_SLOT)
			if name ~= nil then
				local shape = HAT_SHAPES[name] or DEFAULT_HAT_SHAPE

				--Into the player's own space, their feet at the origin and their head straight up, however their body is turned
				local x, y, z = player:getPosition()
				local qw, qx, qy, qz = player:getRotation()
				local localFromX, localFromY, localFromZ = rotateByQuaternion(qw, -qx, -qy, -qz, fromX - x, fromY - y, fromZ - z)
				local localToX, localToY, localToZ = rotateByQuaternion(qw, -qx, -qy, -qz, toX - x, toY - y, toZ - z)

				local half = shape.halfWidth * scale
				local bottom = HEAD_TOP - HAT_SINK
				local top = HEAD_TOP + (shape.height - HAT_SINK) * scale

				local landedOnHat = hit ~= nil and hit.type == DYNAMIC_TYPE_ID and hit.id == player.id and localToY >= bottom - 0.1

				if landedOnHat or crossesBox(localFromX, localFromY, localFromZ, localToX, localToY, localToZ, -half, bottom, -half, half, top, half) then
					knockHatOff(player, dirX, dirY, dirZ)
				end
			end
		end
	end
end

--The table Support_Weapons.lua keeps its listeners in, made here if the weapons haven't loaded yet
ShotPathListeners = ShotPathListeners or {}
table.insert(ShotPathListeners, hatShotPath)

--A left click on a hat lying within reach puts it on a head with nothing on it
function hatClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if (mask & 1) ~= 0 and client:getNumControlled() > 0 then
		local hit = client:getCursorItem(HAT_REACH)
		local dropped = hit ~= nil and hit.type == DYNAMIC_TYPE_ID and droppedHats[hit.id] or nil

		if dropped ~= nil then
			local player = client:getControlledIdx(0)
			if player:getPart(HAT_SLOT) == nil then
				player:setPart(HAT_SLOT, dropped.name, dropped.r, dropped.g, dropped.b, dropped.a, dropped.scale)
				player:playSound("ClickPlant")
				removeDroppedHat(hit.id)
			else
				client:centerPrint("You're already wearing a hat", 2000)
			end
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "hatClick")

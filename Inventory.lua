--[[
	Inventory

	Everyone starts with a hammer, a wrench, a print gun, and a launcher, and can carry up to 5 items. Q slides their items out on the
	right of the screen and puts the picked one in their hand, the mouse wheel picks another while they're out, and Ctrl+W
	throws the one in their hand. Left clicking an item on the ground picks it up.

	The paint can isn't one of their items: opening the paint palette puts one in their hand, outside their slots, and it
	stays there until they take their items out or start building again, which also puts the palette away.

	Holding left mouse with the hammer or wrench in hand swings it, hitting right away and then about once a second until it's
	let go. The hammer knocks loose the brick it hits, and the wrench opens the brick's wrench dialog and stops. Hitting anything
	else plays HammerHit or WrenchMiss.

	Holding left mouse with the paint can in hand sprays their paint palette's color where they're looking, painting every
	brick the crosshair passes over with that color and material. Spraying someone instead paints the body part the crosshair
	is on, which puffs smoke, plays BodyRemove, and wears back off to however they painted themselves after a little while.

	Left clicking with the launcher in hand fires a shell toward the crosshair, at most once per LAUNCHER_RELOAD_MS. The shell
	falls in an arc and bursts on the first thing it touches, the ground included, pushing everything around it away with
	radiusImpulse.

	Left clicking with the print gun in hand shoots a laser at the crosshair and plays PrintFire. A shot that lands on a brick
	with printed faces opens that brick's print menu, where picking a print puts it straight on. Anything else it hits, or
	nothing at all, just makes the noise.

	A brick wrenched to offer an item has a copy of it spinning over it. Left clicking that within reach, whatever's in hand,
	makes a new item of the same type and puts it in the first empty slot, leaving the copy where it is for the next player.

	Run from serverstart.lua with dofile("Inventory.lua"), after the item types are added.
]]

--From NetTypes/NetType.h's SimObjectType enum
local DYNAMIC_TYPE_ID = 1
local BRICK_TYPE_ID = 4
local VEHICLE_TYPE_ID = 7

--How far out from the camera a click looks for an item or brick, a third person camera sits well behind its player
local CLICK_RANGE = 60
--How far what was clicked can be from the player and still be reached
local REACH = 10

--How often a hammer or wrench hits again while left mouse stays down, in milliseconds
local TOOL_REPEAT_MS = 350

--How far from the player the paint can reaches, a little further than the hammer and wrench, and how often it paints the brick under the crosshair while spraying, in milliseconds
local PAINT_REACH = 13
local PAINT_TICK_MS = 30

--How long a body part sprayed onto someone stays that color before it goes back to how they painted themselves, and how
--often that's counted down, both in milliseconds. Spraying it again starts its time over, to the nearest step
local PAINT_WEAR_OFF_MS = 15000
local PAINT_WEAR_STEP_MS = 2500

--How fast a thrown item leaves the player in studs per second, how far in front of them it starts, and how far above their position
local THROW_SPEED = 30
local THROW_START = 2.5
local THROW_HEIGHT = 1.5

--How fast a launcher shell leaves in studs per second, and how long until the launcher can fire again in milliseconds, about its fire animation's length
local SHELL_SPEED = 90
local LAUNCHER_RELOAD_MS = 650
--How far the crosshair aims a shell
local LAUNCHER_AIM_RANGE = 250
--Where a shell starts: the end of the launcher's barrel as it's drawn in first person, which is SHELL_RIGHT to the right, SHELL_DOWN down,
--and SHELL_AHEAD ahead of the player's eyes (EYE_HEIGHT above their position) along the way their camera looks
local EYE_HEIGHT = 4.8
local SHELL_RIGHT = 1.5
local SHELL_DOWN = 0.75
local SHELL_AHEAD = 4.6
--The end of the launcher's barrel, from the middle of its Gun mesh along the launcher's own up and forward (-z), in studs
local MUZZLE_UP = 0.39
local MUZZLE_FORWARD = -1.15
--How hard a shell pushes things away where it lands (see radiusImpulse), and how long its fog keeps coming, in milliseconds
local SHELL_IMPULSE = 140
local SHELL_FOG_MS = 1000
--The tag launcher shells get from addProjectile, so ProjectileHit listeners can tell them from other projectiles
local SHELL_TAG = "launcherShell"

--How far the print gun shoots, and how long its laser keeps coming out of the barrel in milliseconds
local PRINT_RANGE = 60
local PRINT_LASER_MS = 150

--Item types from serverstart.lua everyone gets as they join, filling their slots in this order
--The paint can isn't one of them, the paint palette hands it out, see paintCanChanged
local STARTING_ITEMS = {"hammer", "wrench", "printGun", "dranLauncher"}

--Net IDs of items giveStartingItems handed out and nobody has thrown yet
--These are removed instead of dropped when their player leaves, so people coming and going don't leave piles of tools behind
startingItems = {}

--By client net ID while their paint can sprays: the paint can, its spray emitter, its sound loop, and the schedule of the next paintTick
spraying = {}

--The paint can in each client's hand while their paint palette has one out, by client net ID, see paintCanChanged
paintCans = {}

--Body parts sprayed onto someone's player, by the net ID of the client it belongs to:
--which player was painted, and for each mesh its color, how long it has left, and the schedule counting that down
sprayedParts = {}

--The client's player, the first dynamic they control, or nil
local function playerOf(client)
	if client:getNumControlled() == 0 then
		return nil
	end
	return client:getControlledIdx(0)
end

--Whether a spot is within reach studs of the client's player
local function withinReach(client, x, y, z, reach)
	local player = playerOf(client)
	if player == nil then
		return false
	end

	local px, py, pz = player:getPosition()
	return (x - px) ^ 2 + (y - py) ^ 2 + (z - pz) ^ 2 <= reach * reach
end

--Bricks keep colors as bytes, so this is whether two 0-1 colors paint the same
local function sameColor(r1, g1, b1, a1, r2, g2, b2, a2)
	local function byte(value)
		return math.floor(math.max(0, math.min(1, value)) * 255 + 0.5)
	end
	return byte(r1) == byte(r2) and byte(g1) == byte(g2) and byte(b1) == byte(b2) and byte(a1) == byte(a2)
end

function giveStartingItems(client)
	for _, typeName in ipairs(STARTING_ITEMS) do
		local typeID = getDynamicType(typeName)
		if typeID ~= nil then
			local item = createItem(typeID, 0, 0, 0)
			if client:addItem(item) == nil then
				item:destroy()
			else
				startingItems[item.id] = true
			end
		end
	end

	return client
end
registerEventListener("ClientJoin", "giveStartingItems")

local function stopSpraying(client)
	local spray = spraying[client:getID()]
	if spray == nil then
		return
	end

	spraying[client:getID()] = nil
	if spray.tick ~= nil then
		cancel(spray.tick)
	end
	if spray.loop ~= nil then
		stopSoundLoop(spray.loop)
	end
	if spray.emitter ~= nil then
		spray.emitter:destroy()
	end
end

--Counts one sprayed body part down, and once its time is up puts it back to however its client painted themselves
function wearOffSprayedPart(client, mesh)
	local painted = sprayedParts[client:getID()]
	local part = painted ~= nil and painted.parts[mesh] or nil
	if part == nil then
		return
	end

	--Spraying it again put its time back up to PAINT_WEAR_OFF_MS, see sprayPlayer
	part.left = part.left - PAINT_WEAR_STEP_MS
	if part.left > 0 then
		part.tick = schedule(PAINT_WEAR_STEP_MS, "wearOffSprayedPart", client, mesh)
		return
	end

	part.tick = nil
	painted.parts[mesh] = nil

	--A new player of theirs already looks how they painted themselves, so there's nothing left to put back
	local player = playerOf(client)
	if player == nil or player.id ~= painted.playerID then
		sprayedParts[client:getID()] = nil
		return
	end

	--A puff of smoke off the part as the paint goes
	local smoke = addEmitter("hammerExplosionEmitter")
	if smoke ~= nil then
		smoke:attachToDynamic(player, mesh)
	end
	player:playSound("BodyRemove")

	--Their appearance is the only way back to the part's own color, and it puts every part back, so the sprayed ones go on again
	client:applyAppearance(player)

	local anyLeft = false
	for name, other in pairs(painted.parts) do
		player:setMeshColor(name, other.r, other.g, other.b, 1)
		anyLeft = true
	end

	if not anyLeft then
		sprayedParts[client:getID()] = nil
	end
end

--Paints the body part of someone's player the crosshair is on, which wearOffSprayedPart puts back later
local function sprayPlayer(player, x, y, z, r, g, b)
	--Only a player, whose client's appearance says what its parts should go back to
	if player:getNumControllers() == 0 then
		return
	end

	local owner = player:getControllerIdx(0)
	local mesh = owner ~= nil and player:getMeshAt(x, y, z) or nil
	if mesh == nil then
		return
	end

	local painted = sprayedParts[owner:getID()]
	--Their old player is gone, so nothing painted on it is still around
	if painted == nil or painted.playerID ~= player.id then
		painted = { playerID = player.id, parts = {} }
		sprayedParts[owner:getID()] = painted
	end

	local part = painted.parts[mesh]
	if part == nil then
		part = {}
		painted.parts[mesh] = part
	end

	part.left = PAINT_WEAR_OFF_MS

	--Spraying runs every PAINT_TICK_MS, so only a color that actually changed is worth telling everyone about
	if part.r == nil or not sameColor(r, g, b, 1, part.r, part.g, part.b, 1) then
		part.r, part.g, part.b = r, g, b
		player:setMeshColor(mesh, r, g, b, 1)
	end

	if part.tick == nil then
		part.tick = schedule(PAINT_WEAR_STEP_MS, "wearOffSprayedPart", owner, mesh)
	end
end

--Everything sprayed onto a client's player goes away without being put back, for a client who's leaving
local function forgetSprayedParts(client)
	local painted = sprayedParts[client:getID()]
	if painted == nil then
		return
	end

	sprayedParts[client:getID()] = nil
	for _, part in pairs(painted.parts) do
		if part.tick ~= nil then
			cancel(part.tick)
		end
	end
end

--Paints the brick under the crosshair and keeps the spray the palette's color, for as long as they hold the paint can out
function paintTick(client)
	local spray = spraying[client:getID()]
	if spray == nil then
		return
	end

	--Put away, switched for another item, or thrown
	local held = client:getHeldItem()
	if held == nil or held.id ~= spray.item.id then
		stopSpraying(client)
		return
	end

	local r, g, b, a = client:getPaintColor()
	local material = client:getPaintMaterial()
	if spray.emitter ~= nil then
		spray.emitter:setColor(r, g, b, a)
	end

	local hit, x, y, z = client:getCursorItem(CLICK_RANGE)
	if hit ~= nil and withinReach(client, x, y, z, PAINT_REACH) then
		if hit.type == BRICK_TYPE_ID then
			if not sameColor(r, g, b, a, hit:getColor()) then
				hit:setColor(r, g, b, a)
			end
			if hit:getMaterial() ~= material then
				hit:setMaterial(material)
			end
		elseif hit.type == DYNAMIC_TYPE_ID then
			sprayPlayer(hit, x, y, z, r, g, b)
		end
	end

	spray.tick = schedule(PAINT_TICK_MS, "paintTick", client)
end

local function startSpraying(client, can)
	--A stream from the can to wherever they're looking, in their paint color
	local emitter = addEmitter("paintEmitter")
	local player = playerOf(client)
	if emitter ~= nil then
		emitter:attachToDynamic(can)
		if player ~= nil then
			emitter:aimWith(player, PAINT_REACH)
		end
	end

	spraying[client:getID()] = { item = can, emitter = emitter, loop = can:startSoundLoop("SprayLoop") }
	paintTick(client)
end

--Their paint palette came out or something took the can back, so the can goes into or out of their hand, outside their item slots
function paintCanChanged(client, out)
	local can = paintCans[client:getID()]

	if not out then
		paintCans[client:getID()] = nil
		stopSpraying(client)
		if can ~= nil then
			--Destroying it takes it back out of their hand, it was never in a slot
			can:destroy()
		end
		return client, out
	end

	--Their palette can come back out while they still have the can from last time
	if can ~= nil then
		return client, out
	end

	local typeID = getDynamicType("paintCan")
	local player = playerOf(client)
	if typeID == nil or player == nil then
		return client, out
	end

	--Made where they're standing, so nobody sees it anywhere else before it's in their hand
	local x, y, z = player:getPosition()
	can = createItem(typeID, x, y, z)
	if can == nil then
		return client, out
	end

	client:setHandItem(can)
	paintCans[client:getID()] = can

	return client, out
end
registerEventListener("ClientPaintCan", "paintCanChanged")

--By client net ID while their hammer or wrench keeps hitting: the tool, and the schedule of its next toolTick
toolSwings = {}

local function stopToolSwings(client)
	local swing = toolSwings[client:getID()]
	if swing == nil then
		return
	end

	toolSwings[client:getID()] = nil
	if swing.tick ~= nil then
		cancel(swing.tick)
	end
end

--One hit of the hammer or wrench wherever the crosshair is, returns true if the wrench opened a brick's wrench dialog
local function hitWithTool(client, tool)
	local hit, x, y, z = client:getCursorItem(CLICK_RANGE)
	if x == nil or not withinReach(client, x, y, z, REACH) then
		return false
	end

	--Sparks and a burst wherever it hits, brick or not
	addEmitter(tool .. "SparkEmitter", x, y, z)
	addEmitter(tool .. "ExplosionEmitter", x, y, z)

	local isBrick = hit ~= nil and hit.type == BRICK_TYPE_ID
	if tool == "hammer" then
		if isBrick then
			hit:remove(true)
		else
			playSound("HammerHit", x, y, z)
		end
	elseif hit ~= nil and hit.type == VEHICLE_TYPE_ID then
		--A vehicle's dialog has its music
		playSound("WrenchHit", x, y, z)
		client:openWrenchDialog(hit)
		return true
	elseif isBrick then
		playSound("WrenchHit", x, y, z)
		client:openWrenchDialog(hit)
		return true
	else
		playSound("WrenchMiss", x, y, z)
	end

	return false
end

--Hits with the tool in their hand and does it again in a second, for as long as they hold it out
function toolTick(client)
	local swing = toolSwings[client:getID()]
	if swing == nil then
		return
	end

	--Put away, switched for another item, or thrown
	local held = client:getHeldItem()
	if held == nil or held.id ~= swing.item.id then
		stopToolSwings(client)
		return
	end

	--A wrenched brick's dialog is open, so it's done
	if hitWithTool(client, held:getTypeName()) then
		stopToolSwings(client)
		held:stopAnimation("swing")
		return
	end

	swing.tick = schedule(TOOL_REPEAT_MS, "toolTick", client)
end

function removePrintLaser(laser)
	laser:destroy()
end

--A laser out of the barrel toward the crosshair, and the print menu of a printed brick it lands on
local function firePrintGun(client, gun)
	gun:playSound("PrintFire")

	local laser = addEmitter("LaserEmitterA")
	local player = playerOf(client)
	if laser ~= nil then
		laser:attachToDynamic(gun)
		if player ~= nil then
			laser:aimWith(player, PRINT_RANGE)
		end
		schedule(PRINT_LASER_MS, "removePrintLaser", laser)
	end

	--Anything that isn't a brick with printed faces just gets shot at
	local hit = client:getCursorItem(PRINT_RANGE)
	if hit ~= nil and hit.type == BRICK_TYPE_ID and hit:canPrint() then
		client:openPrintMenu(hit)
	end
end

--Client net IDs whose launcher fired too recently to fire again
launcherReloading = {}

function launcherReloaded(clientID)
	launcherReloading[clientID] = nil
end

local function fireLauncher(client, launcher)
	local player = playerOf(client)
	local shellType = getDynamicType("launcherShell")
	if player == nil or shellType == nil or launcherReloading[client:getID()] then
		return
	end

	launcherReloading[client:getID()] = true
	schedule(LAUNCHER_RELOAD_MS, "launcherReloaded", client:getID())

	launcher:playAnimation("fire")
	launcher:playSound("Launch")

	--A puff out of the end of the barrel
	local smoke = addEmitter("gunSmokeEmitter")
	if smoke ~= nil then
		smoke:attachToDynamic(launcher, "Gun", 0, MUZZLE_UP, MUZZLE_FORWARD)
	end

	--Toward whatever the crosshair is on, or as far as it reaches
	local camX, camY, camZ = client:getCameraPosition()
	local dirX, dirY, dirZ = client:getCameraDirection()
	local _, aimX, aimY, aimZ = client:getCursorItem(LAUNCHER_AIM_RANGE)
	if aimX == nil then
		aimX, aimY, aimZ = camX + dirX * LAUNCHER_AIM_RANGE, camY + dirY * LAUNCHER_AIM_RANGE, camZ + dirZ * LAUNCHER_AIM_RANGE
	end

	--The camera's right and up, level right when looking straight up or down
	local rightX, rightZ = -dirZ, dirX
	local rightLength = math.sqrt(rightX * rightX + rightZ * rightZ)
	if rightLength < 0.001 then
		rightX, rightZ, rightLength = 1, 0, 1
	end
	rightX, rightZ = rightX / rightLength, rightZ / rightLength
	local upX, upY, upZ = -rightZ * dirY, rightZ * dirX - rightX * dirZ, rightX * dirY

	--Out of the end of the barrel
	local px, py, pz = player:getPosition()
	local x = px + rightX * SHELL_RIGHT - upX * SHELL_DOWN + dirX * SHELL_AHEAD
	local y = py + EYE_HEIGHT - upY * SHELL_DOWN + dirY * SHELL_AHEAD
	local z = pz + rightZ * SHELL_RIGHT - upZ * SHELL_DOWN + dirZ * SHELL_AHEAD

	local toX, toY, toZ = aimX - x, aimY - y, aimZ - z
	local distance = math.sqrt(toX * toX + toY * toY + toZ * toZ)
	--What the crosshair is on is right in front of the shell or behind it, like between a third person camera and the player
	if distance < 1 or toX * dirX + toY * dirY + toZ * dirZ <= 0 then
		toX, toY, toZ, distance = dirX, dirY, dirZ, 1
	end

	local velX, velY, velZ = player:getVelocity()
	velX = velX + toX / distance * SHELL_SPEED
	velY = velY + toY / distance * SHELL_SPEED
	velZ = velZ + toZ / distance * SHELL_SPEED

	local shell = addProjectile(shellType, x, y, z, velX, velY, velZ, SHELL_TAG, player)
	if shell ~= nil then
		local trail = addEmitter("shellTrailEmitter")
		if trail ~= nil then
			trail:attachToDynamic(shell)
		end
	end
end

function removeShellFog(fog)
	fog:destroy()
end

--Where a launcher shell lands: a push that knocks people and things away and breaks bricks off destructable vehicles, then smoke and fog
function launcherShellHit(projectile, hit, x, y, z, tag)
	if tag ~= SHELL_TAG then
		return projectile, hit, x, y, z, tag
	end

	radiusImpulse(x, y, z, SHELL_IMPULSE)

	for i = 1, 3 do
		addEmitter("hammerExplosionEmitter", x + math.random() * 2 - 1, y + math.random(), z + math.random() * 2 - 1)
	end

	for i = 1, 2 do
		local fog = addEmitter("FogEmitterA", x + math.random() * 2 - 1, y + 0.5, z + math.random() * 2 - 1)
		if fog ~= nil then
			schedule(SHELL_FOG_MS, "removeShellFog", fog)
		end
	end

	return projectile, hit, x, y, z, tag
end
registerEventListener("ProjectileHit", "launcherShellHit")

--Gives the client a new item of the type a brick's display item shows, if they have a slot for it. The display item itself stays over its brick
function takeDisplayItem(client, display)
	local player = playerOf(client)
	local typeID = getDynamicType(display:getTypeName())
	if player == nil or typeID == nil then
		return
	end

	--Made where they stand and put straight into their slot, so nobody sees it anywhere else first
	local x, y, z = player:getPosition()
	local item = createItem(typeID, x, y, z)
	if item == nil then
		return
	end

	if client:addItem(item) == nil then
		item:destroy()
		client:centerPrint("You can't carry any more items.", 2000)
	end
end

function inventoryClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	--Left mouse only
	if (mask & 1) == 0 or playerOf(client) == nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Clicking the ground gives no object, but still where it was hit
	local hit, x, y, z = client:getCursorItem(CLICK_RANGE)
	local reached = x ~= nil and withinReach(client, x, y, z, REACH)

	--An item on the ground goes into the first empty slot, and the item a brick offers hands out a new one of its type
	if reached and hit ~= nil and hit.type == DYNAMIC_TYPE_ID and hit:isItem() and not hit:isHeld() then
		if hit:isDisplay() then
			takeDisplayItem(client, hit)
		elseif client:addItem(hit) == nil then
			client:centerPrint("You can't carry any more items.", 2000)
		end
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local held = client:getHeldItem()
	if held == nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local tool = held:getTypeName()

	--Sprays until left mouse is let go, see inventoryClickRelease
	if tool == "paintCan" then
		if spraying[client:getID()] == nil then
			startSpraying(client, held)
		end
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	if tool == "dranLauncher" then
		fireLauncher(client, held)
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	if tool == "printGun" then
		firePrintGun(client, held)
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	if tool ~= "hammer" and tool ~= "wrench" then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--Swings and hits until left mouse is let go, see inventoryClickRelease
	held:playAnimation("swing", true)
	stopToolSwings(client)
	toolSwings[client:getID()] = { item = held }
	toolTick(client)

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "inventoryClick")

function inventoryClickRelease(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if (mask & 1) ~= 0 then
		stopSpraying(client)
		stopToolSwings(client)

		--Every carried item, in case they picked another slot partway through a swing
		for slot = 0, 4 do
			local item = client:getItem(slot)
			if item ~= nil then
				item:stopAnimation("swing")
			end
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClickRelease", "inventoryClickRelease")

--Throws the item in their hand the way they're looking
function throwItem(client, slot)
	local _, open = client:getSelectedSlot()
	if not open or client:getItem(slot) == nil then
		return client, slot
	end

	stopSpraying(client)
	stopToolSwings(client)

	local player = playerOf(client)
	local item = client:removeItem(slot)
	item:stopAnimation()
	startingItems[item.id] = nil

	if player ~= nil then
		local dirX, dirY, dirZ = client:getCameraDirection()
		local x, y, z = player:getPosition()
		local velX, velY, velZ = player:getVelocity()

		item:setPosition(x + dirX * THROW_START, y + THROW_HEIGHT + dirY * THROW_START, z + dirZ * THROW_START)
		item:setVelocity(velX + dirX * THROW_SPEED, velY + dirY * THROW_SPEED, velZ + dirZ * THROW_SPEED)
		--Tumbling end over end
		item:setAngularVelocity(math.random() * 10 - 5, math.random() * 10 - 5, math.random() * 10 - 5)
	end

	return client, slot
end
registerEventListener("ClientDropItem", "throwItem")

--Registered before serverstart.lua's leave, so their player is still around to drop things next to
function dropItemsOnLeave(client)
	stopSpraying(client)
	stopToolSwings(client)
	forgetSprayedParts(client)

	--The palette's paint can goes with them rather than being left on the ground
	local can = paintCans[client:getID()]
	paintCans[client:getID()] = nil
	if can ~= nil then
		can:destroy()
	end

	for slot = 0, 4 do
		local item = client:getItem(slot)
		if item ~= nil then
			if startingItems[item.id] then
				startingItems[item.id] = nil
				item:destroy()
			else
				client:removeItem(slot)
			end
		end
	end

	return client
end
registerEventListener("ClientLeave", "dropItemsOnLeave")

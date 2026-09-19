--[[
	Falling Tiles, a gamemode an admin switches the whole server into with /fallingTiles, and back out of the same way

	The stage is a scatter of brick platforms hanging in the air, made new every round: three sizes, four colors, at slightly
	different heights and 2 to 5 studs apart. One player is the controller, shut in a glass booth that hangs over one edge of the
	stage with a row of button bricks along its window. Everyone else is a contestant on the platforms, and whoever falls (or dies)
	is out and watches the rest from a glass box on the far side. The last contestant standing wins and is the next round's controller.

	The controller's buttons, left to right as they see them. Looking at one shows what it does, left clicking pushes it:
		Drop Left / Drop Right      every platform on that half of the stage, as the controller sees it
		Keep Red / Yellow / Green / Blue    every platform that isn't that color
		Drop Large / Medium / Small every platform of that size
		Low Gravity / High Gravity  for the contestants, for a while
		Give Weapons / Clear Weapons    a random weapon for every contestant, or everything they carry taken away
		Wind Gust                   shoves every contestant the same random way
	Platforms blink for FT_WARN_MS before they go and come back FT_GONE_MS later. A controller who pushes nothing for
	FT_IDLE_MS loses the booth to someone else, a spectator if there is one. From FT_SUDDEN_DEATH_MS on platforms start going
	for good, so a round always ends.

	Alone on the server you're the controller of an empty stage, to try the buttons out, and the round starts over when someone joins.
	With two players the one contestant wins by lasting FT_SOLO_SURVIVE_MS.

	While it runs nobody has jets or their starting tools (a hammer would take the stage apart), bricks planted around the stage
	are taken straight back out, and everyone who joins or respawns lands in the spectator box until the next round.
	All of it is server Lua on top of bricks, so nothing here needs anything from the engine.

	Loaded from serverstart.lua after its admin commands, Inventory.lua and Damage.lua.
]]

--The corner of the stage with the lowest x and z, in studs, and the plate height platforms are scattered around
--Nothing else should be built within about 10 studs of the stage or above it
FT_X, FT_Z = 100, -48
FT_BASE_Y = 75

--Platforms fill this many studs from that corner along z, and along x less the booth and box hanging over each end
FT_SIZE = 96
FT_ROOM_OVERHANG = 8

--How many platforms of each size, and the studs each side of one can be. Four players can just about cram onto a medium one
FT_SIZES = {
	{ name = "large",  count = 6,  low = 7, high = 9 },
	{ name = "medium", count = 30, low = 4, high = 5 },
	{ name = "small",  count = 12, low = 2, high = 3 }
}
--Studs between a platform and the one it was placed off. Most are an easy FT_MIN_GAP to FT_MAX_GAP, but FT_HARD_GAP_CHANCE of
--them are islands as far off as a running jump can just about make it, see hardGap, with nothing else any nearer
FT_MIN_GAP, FT_MAX_GAP = 2, 5
FT_HARD_GAP_CHANCE = 0.25
--A player walks at 10 studs a second and jumps off at 30 up against a gravity of 70: 8.5 studs far on the level, measured
FT_WALK_SPEED, FT_JUMP_SPEED = 10, 30
--Plates above or below FT_BASE_Y a platform's bottom can be, and how many plates thick
FT_HEIGHT_SPREAD = 3
FT_MAX_THICKNESS = 3

FT_COLORS = {
	{ name = "Red",    r = 0.9,  g = 0.1,  b = 0.1 },
	{ name = "Yellow", r = 1,    g = 0.85, b = 0.1 },
	{ name = "Green",  r = 0.1,  g = 0.75, b = 0.2 },
	{ name = "Blue",   r = 0.15, g = 0.3,  b = 0.95 }
}

--Platforms blink this long before they go, stay gone this long, and no more can be dropped until this long after they're back
FT_WARN_MS = 2500
FT_GONE_MS = 5000
FT_DROP_REST_MS = 2000
--The world's gravity is 70 down
FT_NORMAL_GRAVITY = -70
FT_LOW_GRAVITY = -30
FT_HIGH_GRAVITY = -160
FT_GRAVITY_MS = 10000
FT_GRAVITY_REST_MS = 5000
FT_WEAPONS_REST_MS = 10000
FT_GUST_REST_MS = 8000
--Studs a second a gust shoves everyone along the ground, and up
FT_GUST_SPEED = 20
FT_GUST_LIFT = 14
--Between any two pushes
FT_PRESS_REST_MS = 1000

--Item types Give Weapons picks from
FT_WEAPONS = {"gun", "pistol", "submachinegun", "pumpShotgun", "sportRifle", "bow", "rocketLauncher"}

FT_COUNTDOWN_SECONDS = 3
FT_INTERMISSION_MS = 6000
FT_IDLE_MS = 60000
FT_IDLE_WARNING_MS = 45000
--A lone contestant wins by lasting this long
FT_SOLO_SURVIVE_MS = 90000
--When platforms start going for good, and how often one does
FT_SUDDEN_DEATH_MS = 120000
FT_SUDDEN_DEATH_EVERY_MS = 8000
--Added to the winner's score in the player list
FT_WIN_SCORE = 5

local TICK_MS = 200
local BRICK_TYPE_ID = 4
local PLATE = 0.4

--The booth and the spectator box: plates their floor is above FT_BASE_Y, plates from floor to roof, and their size in studs
local ROOM_RAISE = 25
local ROOM_HEIGHT = 20
local ROOM_DEPTH = 14
local ROOM_LENGTH = 45
local GLASS = { 0.6, 0.8, 1, 0.25 }
local WALL = { 0.2, 0.2, 0.22, 1 }

--How far a button can be pushed from, which has to cover a third person camera hanging 20 studs behind its player
local BUTTON_RANGE = 40

FallingTiles = {
	active = false,
	--"waiting" for anyone to be here, "countdown", "playing", or "intermission"
	state = "waiting",
	--Scheduled calls carry the one they were made under, and do nothing once it has moved on
	session = 0,
	round = 0,
	startToken = 0,
	roundMS = 0,
	idleMS = 0,
	platforms = {},
	--Every brick of the booth and the box, buttons included
	structure = {},
	buttons = {},
	buttonsByBrick = {},
	--Items Give Weapons made, by net ID, so they can be cleared off the ground too
	givenItems = {}
}
local FT = FallingTiles

--Whether a brick we made is still there. Nothing tells Lua one was removed, an admin's /clearAllBricks say
local function brickAlive(brick)
	return brick ~= nil and getBrickId(brick.id) == brick
end

local function playerOf(client)
	if client.ftLeft or client:getNumControlled() == 0 then
		return nil
	end
	return client:getControlledIdx(0)
end

--Everyone connected, which during ClientLeave listeners still includes whoever is leaving
local function everyone()
	local clients = {}
	for i = 0, getNumClients() - 1 do
		local client = getClientIdx(i)
		if not client.ftLeft then
			table.insert(clients, client)
		end
	end
	return clients
end

--Whether a client we kept hold of is still connected. Their table outlives them, but its methods don't: client:getName() on
--someone who's gone takes the server down with it, so nothing kept from an earlier tick is used without asking this first
local function isHere(client)
	if client == nil or client.ftLeft then
		return false
	end
	for i = 0, getNumClients() - 1 do
		if getClientIdx(i) == client then
			return true
		end
	end
	return false
end

local function withRole(role)
	local clients = {}
	for _, client in ipairs(everyone()) do
		if client.ftRole == role then
			table.insert(clients, client)
		end
	end
	return clients
end

local function shuffle(list)
	for i = #list, 2, -1 do
		local j = math.random(i)
		list[i], list[j] = list[j], list[i]
	end
	return list
end

--What the controller is shown while their crosshair is on a button
local function showButtonHelp(button)
	if button ~= nil and isHere(FT.controller) then
		FT.controller:centerPrint(button.label .. ": " .. button.help, 60000, 1, 1, 0.6)
	end
end

--Center prints stack, so whatever was showing goes first, and the controller gets the button they're looking at back under it
local function announce(text, durationMS, r, g, b)
	centerPrintAll("", 0)
	centerPrintAll(text, durationMS or 3000, r or 1, g or 1, b or 1)
	showButtonHelp(FT.hovered)
end

--Starts a round after a while. Only the last one asked for happens, so skipping ahead doesn't leave another start on its way
local function queueRound(delayMS)
	FT.startToken = FT.startToken + 1
	schedule(delayMS, "ftStartRound", FT.session, FT.startToken)
end

------------------------------------------------------------------------------------------------------------------------
--The booth and the spectator box
------------------------------------------------------------------------------------------------------------------------

local function roomFloorY()
	return FT_BASE_Y + ROOM_RAISE
end

local function roomZ()
	return FT_Z + math.floor((FT_SIZE - ROOM_LENGTH) / 2)
end

--The booth hangs over the low x edge of the stage looking toward +x, the box over the high x edge looking back at it
local function boothX()
	return FT_X - (ROOM_DEPTH - FT_ROOM_OVERHANG)
end

local function boxX()
	return FT_X + FT_SIZE - FT_ROOM_OVERHANG
end

--Returns false if a brick wouldn't fit, which means something else is built there
local function structureBrick(x, y, z, width, height, length, color)
	local brick = addBrick(x, y, z, width, height, length, color[1], color[2], color[3], color[4])
	if brick == nil then
		return false
	end
	table.insert(FT.structure, brick)
	return true
end

--A closed room with a glass floor and glass on every side but its back, the x side away from the stage
local function buildRoom(x, z, backAtLowX)
	local floorY = roomFloorY()
	local wallY = floorY + 1
	local backX = backAtLowX and x or x + ROOM_DEPTH - 1
	local frontX = backAtLowX and x + ROOM_DEPTH - 1 or x

	return structureBrick(x, floorY, z, ROOM_DEPTH, 1, ROOM_LENGTH, GLASS)
		and structureBrick(x, wallY + ROOM_HEIGHT, z, ROOM_DEPTH, 1, ROOM_LENGTH, WALL)
		and structureBrick(backX, wallY, z, 1, ROOM_HEIGHT, ROOM_LENGTH, WALL)
		and structureBrick(frontX, wallY, z, 1, ROOM_HEIGHT, ROOM_LENGTH, GLASS)
		and structureBrick(x + 1, wallY, z, ROOM_DEPTH - 2, ROOM_HEIGHT, 1, GLASS)
		and structureBrick(x + 1, wallY, z + ROOM_LENGTH - 1, ROOM_DEPTH - 2, ROOM_HEIGHT, 1, GLASS)
end

--Where someone put in a room stands: the middle of the booth, or anywhere in the box
local function boothSpot()
	return boothX() + ROOM_DEPTH / 2 - 1, (roomFloorY() + 1) * PLATE + 0.1, roomZ() + ROOM_LENGTH / 2
end

local function boxSpot()
	return boxX() + 3 + math.random() * (ROOM_DEPTH - 6), (roomFloorY() + 1) * PLATE + 0.1, roomZ() + 3 + math.random() * (ROOM_LENGTH - 6)
end

------------------------------------------------------------------------------------------------------------------------
--The stage
------------------------------------------------------------------------------------------------------------------------

local function clearStage()
	for _, platform in ipairs(FT.platforms) do
		if brickAlive(platform.brick) then
			platform.brick:remove()
		end
	end
	FT.platforms = {}
end

--Studs of open space between two platforms, going by whichever axis they're further apart on
local function gapBetween(a, b)
	local gapX = math.max(b.x - (a.x + a.width), a.x - (b.x + b.width))
	local gapZ = math.max(b.z - (a.z + a.length), a.z - (b.z + b.length))
	return math.max(gapX, gapZ)
end

--The widest whole gap a running jump clears between two platform tops rise world units apart, going up, with half a stud to spare
local function hardGap(rise)
	local gravity = -FT_NORMAL_GRAVITY
	local airTime = (FT_JUMP_SPEED + math.sqrt(math.max(0, FT_JUMP_SPEED ^ 2 - 2 * gravity * rise))) / gravity
	return math.max(FT_MIN_GAP, math.floor(FT_WALK_SPEED * airTime - 0.5))
end

--Platforms stay out from under the booth and the box
local function stageBounds()
	return FT_X + FT_ROOM_OVERHANG, FT_Z, FT_X + FT_SIZE - FT_ROOM_OVERHANG, FT_Z + FT_SIZE
end

local function fitsStage(platform)
	local lowX, lowZ, highX, highZ = stageBounds()
	if platform.x < lowX or platform.z < lowZ or platform.x + platform.width > highX or platform.z + platform.length > highZ then
		return false
	end
	--One placed a hard jump away keeps everything else that far off too, or the platforms filling in around it later
	--would leave an easy way over
	for _, other in ipairs(FT.platforms) do
		if gapBetween(platform, other) < math.max(platform.clearance, other.clearance) then
			return false
		end
	end
	return true
end

--Puts a platform's brick in the world, again after it was dropped. False if something is in the way
local function addPlatformBrick(platform)
	local color = platform.color
	platform.brick = addBrick(platform.x, platform.y, platform.z, platform.width, platform.height, platform.length, color.r, color.g, color.b, 1)
	return platform.brick ~= nil
end

--[[
	The first platform goes in the middle, and each one after it a jumpable gap off a random side of a random one already placed,
	slid along that side, wherever that leaves it clear of all the others. So the stage is one cluster that can always be crossed,
	and never the same twice
]]
local function buildStage()
	clearStage()

	--In FT_SIZES' order, so the big ones get the room in the middle and the small ones fill in around them
	local sizes = {}
	for _, size in ipairs(FT_SIZES) do
		for i = 1, size.count do
			table.insert(sizes, size)
		end
	end

	--The same number of each color, give or take one
	local colors = {}
	for i = 1, #sizes do
		colors[i] = FT_COLORS[(i - 1) % #FT_COLORS + 1]
	end
	shuffle(colors)

	local lowX, lowZ, highX, highZ = stageBounds()
	local middleZ = (lowZ + highZ) / 2

	for i, size in ipairs(sizes) do
		local platform = {
			size = size.name,
			color = colors[i],
			width = math.random(size.low, size.high),
			length = math.random(size.low, size.high),
			height = math.random(1, FT_MAX_THICKNESS),
			y = FT_BASE_Y + math.random(-FT_HEIGHT_SPREAD, FT_HEIGHT_SPREAD)
		}

		local placed = false
		local hard = math.random() < FT_HARD_GAP_CHANCE
		platform.clearance = FT_MIN_GAP
		if #FT.platforms == 0 then
			platform.x = math.floor((lowX + highX - platform.width) / 2)
			platform.z = math.floor((lowZ + highZ - platform.length) / 2)
			placed = true
		end

		for attempt = 1, 200 do
			if placed then
				break
			end

			local from = FT.platforms[math.random(#FT.platforms)]
			local gap = math.random(FT_MIN_GAP, FT_MAX_GAP)
			platform.clearance = FT_MIN_GAP
			if hard then
				--Whichever of the two is lower has to be able to jump up to the other
				gap = hardGap(math.abs((platform.y + platform.height) - (from.y + from.height)) * PLATE)
				platform.clearance = gap
			end
			local side = math.random(4)
			if side <= 2 then
				platform.x = side == 1 and from.x + from.width + gap or from.x - gap - platform.width
				platform.z = math.random(from.z - platform.length + 1, from.z + from.length - 1)
			else
				platform.z = side == 3 and from.z + from.length + gap or from.z - gap - platform.length
				platform.x = math.random(from.x - platform.width + 1, from.x + from.width - 1)
			end
			placed = fitsStage(platform)
		end

		if placed and addPlatformBrick(platform) then
			--As the controller sees it, looking toward +x from the booth
			platform.side = (platform.z + platform.length / 2 < middleZ) and "left" or "right"
			table.insert(FT.platforms, platform)
		end
	end
end

--Where a player's feet go to stand on a platform: around its middle, so two who have to share one don't land inside each other
local function platformSpot(platform)
	local spreadX, spreadZ = math.max(0, platform.width - 3) / 2, math.max(0, platform.length - 3) / 2
	return platform.x + platform.width / 2 + (math.random() * 2 - 1) * spreadX, (platform.y + platform.height) * PLATE + 0.1,
		platform.z + platform.length / 2 + (math.random() * 2 - 1) * spreadZ
end

--Below this a contestant has fallen
local function fallenY()
	return (FT_BASE_Y - FT_HEIGHT_SPREAD) * PLATE - 6
end

------------------------------------------------------------------------------------------------------------------------
--Players
------------------------------------------------------------------------------------------------------------------------

--[[
	Puts a client's player somewhere by giving them a new one there, serverstart.lua's spawnPlayer with where it spawns swapped
	for the spot, rather than with setPosition. A player's own game says where it is, and hears that Lua moved it from one update
	that can be lost on the way. Scheduled Lua also runs at the end of a server tick, and the next tick reads what clients sent
	before it sends anything out, so an update from a player who's moving (falling off a platform that was just removed, say)
	puts them back before the move was ever sent. Either way they'd carry on from where they were, and with everyone moved at
	once as a round starts it happened every few rounds. A new player is sent the way every new object is, and can't be missed.
	Returns the new player
]]
local function place(client, x, y, z)
	local player = playerOf(client)
	if player == nil then
		return nil
	end
	if client:getVehicle() then
		client:exitVehicle()
	end
	player:destroy()

	local pick = pickSpawnPosition
	pickSpawnPosition = function() return x, y, z end
	player = spawnPlayer(client)
	pickSpawnPosition = pick
	return player
end

--Turns a client's camera to look across the stage, toward +x or -x and a little down: a fixed camera aimed that way
--and handed straight back to their player keeps the aim
local function lookAcrossStage(client, dirX)
	local player = playerOf(client)
	if player ~= nil then
		local x, y, z = player:getPosition()
		client:staticCamera(x, y + 4.5, z, dirX * 0.91, -0.41, 0)
		client:bindCamera(player, true, 20)
	end
end

local function resetGravity(client)
	local player = playerOf(client)
	if player ~= nil then
		player:setGravity(0, FT_NORMAL_GRAVITY, 0)
	end
end

--Everything they carry is gone, not left lying on the stage: dropCarriedItems removes their starting tools and lets go of the rest
local function stripItems(client)
	local carried = {}
	for slot = 0, 4 do
		local item = client:getItem(slot)
		if item ~= nil then
			table.insert(carried, item)
		end
	end

	if dropCarriedItems ~= nil and playerOf(client) ~= nil then
		dropCarriedItems(client)
	end

	for _, item in ipairs(carried) do
		if dynamicExists(item.id) then
			item:destroy()
		end
		FT.givenItems[item.id] = nil
	end
end

--Weapons that were dropped or left by someone who died
local function clearGivenItems()
	for id, item in pairs(FT.givenItems) do
		if dynamicExists(id) then
			item:destroy()
		end
	end
	FT.givenItems = {}
end

--Into the spectator box, to watch until the next round
local function makeSpectator(client)
	client.ftRole = "spectator"
	stripItems(client)
	local player = place(client, boxSpot())
	if player ~= nil then
		player.canBeDamaged = false
		lookAcrossStage(client, -1)
	end
end

local function seatInBooth(client)
	local player = place(client, boothSpot())
	if player ~= nil then
		player.canBeDamaged = false
		lookAcrossStage(client, 1)
	end
end

local function makeController(client)
	--Waiting to respawn, as a spectator who was shot
	if client.dead then
		respawnPlayer(client)
	end

	FT.controller = client
	FT.idleMS = 0
	FT.idleWarned = false
	FT.hovered = nil
	client.ftRole = "controller"
	stripItems(client)
	seatInBooth(client)
	client:message("You're the controller. Look at a button to see what it does, left click to push it.")
end

--What giveStartingItems and pickSpawnPosition are swapped for while the gamemode runs, see ftStart:
--nobody gets tools, and everyone who joins or respawns lands in the spectator box
function ftGiveStartingItems(client)
	if FT.active then
		return client
	end
	return FT.giveStartingItems(client)
end

function ftPickSpawnPosition()
	if FT.active then
		return boxSpot()
	end
	return FT.pickSpawnPosition()
end

------------------------------------------------------------------------------------------------------------------------
--What the buttons do. Each returns true, or false and why not
------------------------------------------------------------------------------------------------------------------------

local function dropPlatforms(label, matches)
	if FT.dropping then
		return false, "Platforms are already falling."
	end

	local count = 0
	for _, platform in ipairs(FT.platforms) do
		if not platform.gone and brickAlive(platform.brick) and matches(platform) then
			platform.dropping = true
			platform.brick:setMaterial("Blink")
			count = count + 1
		end
	end
	if count == 0 then
		return false, "There are none of those left."
	end

	FT.dropping = true
	announce(label, FT_WARN_MS, 1, 0.8, 0.2)
	playSound("Beep")
	schedule(FT_WARN_MS, "ftDropNow", FT.round)
	return true
end

function ftDropNow(round)
	if round ~= FT.round then
		return
	end

	for _, platform in ipairs(FT.platforms) do
		if platform.dropping then
			if brickAlive(platform.brick) then
				--Popping loose and flying off, a stage is few enough bricks for that
				platform.brick:remove(true)
			end
			platform.brick = nil
		end
	end
	schedule(FT_GONE_MS, "ftRestorePlatforms", round)
end

function ftRestorePlatforms(round)
	if round ~= FT.round then
		return
	end

	for _, platform in ipairs(FT.platforms) do
		if platform.dropping then
			platform.dropping = false
			if not platform.gone then
				addPlatformBrick(platform)
			end
		end
	end
	playSound("ClickPlant")
	schedule(FT_DROP_REST_MS, "ftDropReady", round)
end

function ftDropReady(round)
	if round == FT.round then
		FT.dropping = false
	end
end

local function setGravity(label, gravity)
	if FT.gravityBusy then
		return false, "Gravity hasn't settled yet."
	end

	FT.gravityBusy = true
	for _, client in ipairs(withRole("contestant")) do
		local player = playerOf(client)
		if player ~= nil then
			player:setGravity(0, gravity, 0)
		end
	end
	announce(label, 3000, 0.5, 0.8, 1)
	schedule(FT_GRAVITY_MS, "ftGravityNormal", FT.round)
	return true
end

function ftGravityNormal(round)
	if round ~= FT.round then
		return
	end

	for _, client in ipairs(everyone()) do
		resetGravity(client)
	end
	announce("Gravity is back to normal.", 2000, 0.5, 0.8, 1)
	schedule(FT_GRAVITY_REST_MS, "ftGravityReady", round)
end

function ftGravityReady(round)
	if round == FT.round then
		FT.gravityBusy = false
	end
end

function ftWeaponsReady(round)
	if round == FT.round then
		FT.weaponsBusy = false
	end
end

local function giveWeapons()
	if FT.weaponsBusy then
		return false, "The weapons locker is still being restocked."
	end

	local types = {}
	for _, name in ipairs(FT_WEAPONS) do
		local typeID = getDynamicType(name)
		if typeID ~= nil then
			table.insert(types, typeID)
		end
	end
	if #types == 0 then
		return false, "This server has no weapons."
	end

	for _, client in ipairs(withRole("contestant")) do
		if playerOf(client) ~= nil then
			local item = createItem(types[math.random(#types)], 0, 0, 0)
			if client:addItem(item) == nil then
				item:destroy()
			else
				FT.givenItems[item.id] = item
				client:centerPrint("You got a " .. item:getItemName() .. "! Press Q to take it out.", 3000, 1, 0.5, 0.5)
			end
		end
	end

	FT.weaponsBusy = true
	schedule(FT_WEAPONS_REST_MS, "ftWeaponsReady", FT.round)
	messageAll(FT.controller:getName() .. " handed out weapons.")
	return true
end

local function clearWeapons()
	for _, client in ipairs(withRole("contestant")) do
		stripItems(client)
	end
	clearGivenItems()
	announce("Weapons cleared!", 2000, 1, 0.5, 0.5)
	return true
end

function ftGustReady(round)
	if round == FT.round then
		FT.gustBusy = false
	end
end

--Everyone is shoved the same way, so it's the edge of whatever they stand on that matters
local function windGust()
	if FT.gustBusy then
		return false, "The wind is still catching its breath."
	end

	local angle = math.random() * math.pi * 2
	local pushX, pushZ = math.cos(angle) * FT_GUST_SPEED, math.sin(angle) * FT_GUST_SPEED
	for _, client in ipairs(withRole("contestant")) do
		local player = playerOf(client)
		if player ~= nil then
			local velX, velY, velZ = player:getVelocity()
			player:setVelocity(velX + pushX, velY + FT_GUST_LIFT, velZ + pushZ)
		end
	end

	FT.gustBusy = true
	schedule(FT_GUST_REST_MS, "ftGustReady", FT.round)
	announce("A gust of wind!", 1500, 0.8, 0.9, 1)
	return true
end

--Left to right as the controller sees them, which is toward +z
local function buttonList()
	local list = {}
	local function add(label, help, r, g, b, action)
		table.insert(list, { label = label, help = help, color = { r, g, b, 1 }, action = action })
	end

	add("Drop Left", "every platform on your left goes for a while", 0.95, 0.95, 0.95, function()
		return dropPlatforms("The LEFT side is going!", function(platform) return platform.side == "left" end)
	end)
	for _, color in ipairs(FT_COLORS) do
		add("Keep " .. color.name, "every platform that isn't " .. string.lower(color.name) .. " goes for a while", color.r, color.g, color.b, function()
			return dropPlatforms("Everything but " .. string.upper(color.name) .. " is going!", function(platform) return platform.color ~= color end)
		end)
	end
	local shades = { large = 0.25, medium = 0.5, small = 0.75 }
	for _, size in ipairs(FT_SIZES) do
		local shade = shades[size.name] or 0.5
		add("Drop " .. string.upper(string.sub(size.name, 1, 1)) .. string.sub(size.name, 2), "every " .. size.name .. " platform goes for a while", 1, shade, 0, function()
			return dropPlatforms("Every " .. string.upper(size.name) .. " platform is going!", function(platform) return platform.size == size.name end)
		end)
	end
	add("Low Gravity", "contestants jump high and fall slowly for a while", 0.3, 0.9, 0.9, function()
		return setGravity("Low gravity!", FT_LOW_GRAVITY)
	end)
	add("High Gravity", "contestants can barely jump for a while", 0.45, 0.1, 0.6, function()
		return setGravity("High gravity!", FT_HIGH_GRAVITY)
	end)
	add("Give Weapons", "a random weapon for every contestant", 0.5, 0, 0, giveWeapons)
	add("Clear Weapons", "takes away everything the contestants carry", 1, 0.6, 0.7, clearWeapons)
	add("Wind Gust", "shoves every contestant the same random way", 0.6, 0.65, 0.7, windGust)
	add("Drop Right", "every platform on your right goes for a while", 0.05, 0.05, 0.05, function()
		return dropPlatforms("The RIGHT side is going!", function(platform) return platform.side == "right" end)
	end)

	return list
end

--A row of 2x2 bricks along the inside of the booth's window, low enough to see the stage over
local function buildButtons()
	FT.buttons = buttonList()
	FT.buttonsByBrick = {}

	local x = boothX() + ROOM_DEPTH - 3
	local y = roomFloorY() + 1
	local firstZ = roomZ() + 1 + math.floor((ROOM_LENGTH - 2 - (#FT.buttons * 3 - 1)) / 2)

	for i, button in ipairs(FT.buttons) do
		local color = button.color
		button.brick = addBrick(x, y, firstZ + (i - 1) * 3, 2, 3, 2, color[1], color[2], color[3], color[4])
		if button.brick == nil then
			return false
		end
		table.insert(FT.structure, button.brick)
		FT.buttonsByBrick[button.brick] = button
	end
	return true
end

--The button a client's crosshair is on, within reach of their player
local function buttonUnderCursor(client)
	local player = playerOf(client)
	if player == nil then
		return nil
	end

	local hit, x, y, z = client:getCursorItem(BUTTON_RANGE)
	if hit == nil or hit.type ~= BRICK_TYPE_ID then
		return nil
	end

	local button = FT.buttonsByBrick[hit]
	if button == nil then
		return nil
	end

	local px, py, pz = player:getPosition()
	if (x - px) ^ 2 + (z - pz) ^ 2 > 12 * 12 then
		return nil
	end
	return button
end

function ftButtonUnlit(button, session)
	if session == FT.session and brickAlive(button.brick) then
		button.brick:setMaterial("None")
	end
end

function ftClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if FT.active and client == FT.controller and (mask & 1) ~= 0 then
		local button = buttonUnderCursor(client)
		if button ~= nil then
			if FT.state ~= "playing" then
				client:centerPrint("The round hasn't started.", 1500, 1, 0.6, 0.6)
			elseif FT.roundMS - (FT.lastPressMS or -FT_PRESS_REST_MS) >= FT_PRESS_REST_MS then
				local worked, why = button.action()
				if worked then
					FT.lastPressMS = FT.roundMS
					FT.idleMS = 0
					FT.idleWarned = false
					client:playSound("Beep")
					button.brick:setMaterial("Glow")
					schedule(600, "ftButtonUnlit", button, FT.session)
				else
					client:centerPrint(why, 1500, 1, 0.6, 0.6)
				end
			end
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "ftClick")

------------------------------------------------------------------------------------------------------------------------
--Rounds
------------------------------------------------------------------------------------------------------------------------

--Everything a round left going stops, and everyone is back to normal
local function settleRound()
	FT.round = FT.round + 1
	FT.dropping = false
	FT.gravityBusy = false
	FT.weaponsBusy = false
	FT.gustBusy = false
	FT.lastPressMS = nil

	for _, client in ipairs(everyone()) do
		resetGravity(client)
		if client.ftRole == "contestant" then
			stripItems(client)
		end
	end
	clearGivenItems()
end

local function endRound(winner, text)
	settleRound()
	FT.state = "intermission"

	if winner ~= nil then
		FT.nextController = winner
		if setScore ~= nil then
			setScore(winner, (winner.score or 0) + FT_WIN_SCORE)
		end
		playSound("ProcessComplete")
	end

	messageAll(text)
	announce(text, FT_INTERMISSION_MS - 1000, 0.4, 1, 0.4)
	queueRound(FT_INTERMISSION_MS)
end

function ftCountdown(round, secondsLeft)
	if round ~= FT.round or FT.state ~= "countdown" then
		return
	end

	if secondsLeft > 0 then
		announce("Falling Tiles starts in " .. secondsLeft .. "...", 1200)
		playSound("Beep")
		schedule(1000, "ftCountdown", round, secondsLeft - 1)
		return
	end

	FT.state = "playing"
	FT.roundMS = 0
	FT.idleMS = 0
	FT.suddenDeathMS = FT_SUDDEN_DEATH_MS
	announce("Go!", 1500, 0.4, 1, 0.4)
	playSound("UploadStart")
end

--A new stage, last round's winner (or else anyone) in the booth, and everyone else on a platform of their own while there are enough
function ftStartRound(session, token)
	if session ~= FT.session or not FT.active or (token ~= nil and token ~= FT.startToken) then
		return
	end
	FT.startToken = FT.startToken + 1

	settleRound()
	local clients = everyone()
	if #clients == 0 then
		FT.state = "waiting"
		FT.controller = nil
		return
	end

	buildStage()

	--Anyone waiting to respawn plays too
	for _, client in ipairs(clients) do
		if client.dead then
			respawnPlayer(client)
		end
		client:setJetsEnabled(false)
	end

	local controller = FT.nextController
	if not isHere(controller) or controller.ftRole == nil then
		controller = clients[math.random(#clients)]
	end
	FT.nextController = nil
	makeController(controller)

	--The biggest platforms first
	local spots = {}
	for _, platform in ipairs(FT.platforms) do
		table.insert(spots, platform)
	end
	table.sort(spots, function(a, b) return a.width * a.length > b.width * b.length end)

	FT.startCount = 0
	FT.lastEliminated = nil
	for _, client in ipairs(shuffle(clients)) do
		if client ~= controller then
			FT.startCount = FT.startCount + 1
			client.ftRole = "contestant"
			stripItems(client)
			if #spots > 0 then
				place(client, platformSpot(spots[(FT.startCount - 1) % #spots + 1]))
			end
		end
	end

	messageAll(controller:getName() .. " is the controller.")
	if FT.startCount == 0 then
		controller:message("Nobody else is here, so the stage is yours to try the buttons on. A round starts when someone joins.")
	elseif FT.startCount == 1 then
		messageAll("With one contestant, lasting " .. math.floor(FT_SOLO_SURVIVE_MS / 1000) .. " seconds wins.")
	end

	FT.state = "countdown"
	ftCountdown(FT.round, FT_COUNTDOWN_SECONDS)
end

local function eliminate(client, how)
	FT.lastEliminated = client
	makeSpectator(client)
	messageAll(client:getName() .. " " .. how .. " " .. #withRole("contestant") .. " left.")
	playSound("PlayerLeave")
end

--The booth goes to a spectator if there is one, else to a contestant, and whoever had it watches the rest of the round
local function replaceController(why)
	local old = FT.controller
	local candidates = withRole("spectator")
	if #candidates == 0 then
		candidates = withRole("contestant")
	end
	if #candidates == 0 then
		return false
	end

	if isHere(old) then
		old:centerPrint("", 0)
		makeSpectator(old)
	end

	local new = candidates[math.random(#candidates)]
	makeController(new)

	--That was the only contestant
	if FT.startCount > 0 and #withRole("contestant") == 0 then
		FT.nextController = new
		endRound(nil, why .. " " .. new:getName() .. " takes over next round.")
		return true
	end

	messageAll(why .. " " .. new:getName() .. " is the controller now.")
	return true
end

local function checkRoundOver()
	local left = withRole("contestant")

	if FT.startCount >= 2 then
		if #left == 1 then
			endRound(left[1], left[1]:getName() .. " wins the round!")
		elseif #left == 0 then
			--The last two went at once, or the last one left the server
			local last = FT.lastEliminated
			if isHere(last) and last ~= FT.controller then
				endRound(last, last:getName() .. " was the last to go, and wins the round!")
			else
				endRound(nil, "Nobody wins the round.")
			end
		end
	elseif FT.startCount == 1 then
		if #left == 0 then
			endRound(FT.controller, FT.controller:getName() .. " cleared the stage, and stays the controller!")
		elseif FT.roundMS >= FT_SOLO_SURVIVE_MS then
			endRound(left[1], left[1]:getName() .. " lasted " .. math.floor(FT_SOLO_SURVIVE_MS / 1000) .. " seconds, and wins the round!")
		end
	end
end

--One more platform goes and doesn't come back
local function suddenDeath()
	local remaining = {}
	for _, platform in ipairs(FT.platforms) do
		if not platform.gone then
			table.insert(remaining, platform)
		end
	end
	if #remaining == 0 then
		return
	end

	local platform = remaining[math.random(#remaining)]
	platform.gone = true
	if brickAlive(platform.brick) then
		platform.brick:remove(true)
		platform.brick = nil
	end
end

--Shows the controller what the button under their crosshair does, for as long as it's there
local function showHoveredButton()
	local controller = FT.controller
	local button = buttonUnderCursor(controller)
	if button == FT.hovered then
		return
	end

	FT.hovered = button
	controller:centerPrint("", 0)
	showButtonHelp(button)
end

function ftTick(session)
	if session ~= FT.session or not FT.active then
		return
	end
	schedule(TICK_MS, "ftTick", session)

	if FT.state == "waiting" then
		if #everyone() > 0 then
			ftStartRound(session)
		end
		return
	end

	--The controller left
	if not isHere(FT.controller) then
		FT.controller = nil
		if not replaceController("The controller left.") then
			settleRound()
			FT.state = "waiting"
			return
		end
	end

	--Nobody gets out of their room, whatever happens to its bricks
	local roomY = roomFloorY() * PLATE - 3
	for _, client in ipairs(everyone()) do
		local player = playerOf(client)
		if player ~= nil and client.ftRole ~= "contestant" then
			local _, y = player:getPosition()
			if y < roomY then
				if client.ftRole == "controller" then
					seatInBooth(client)
				else
					makeSpectator(client)
				end
			end
		end
	end

	if FT.state ~= "countdown" and FT.state ~= "playing" then
		return
	end

	for _, client in ipairs(withRole("contestant")) do
		local player = playerOf(client)
		if client.dead or player == nil then
			eliminate(client, "died.")
		else
			local _, y = player:getPosition()
			if y < fallenY() then
				eliminate(client, "fell.")
			end
		end
	end

	showHoveredButton()

	if FT.state ~= "playing" then
		return
	end

	FT.roundMS = FT.roundMS + TICK_MS

	if FT.startCount > 0 then
		FT.idleMS = FT.idleMS + TICK_MS
		if FT.idleMS >= FT_IDLE_MS then
			replaceController(FT.controller:getName() .. " didn't push anything for " .. math.floor(FT_IDLE_MS / 1000) .. " seconds.")
		elseif FT.idleMS >= FT_IDLE_WARNING_MS and not FT.idleWarned then
			FT.idleWarned = true
			FT.controller:centerPrint("Push a button in the next " .. math.floor((FT_IDLE_MS - FT_IDLE_WARNING_MS) / 1000) .. " seconds or lose the booth!", 4000, 1, 0.4, 0.4)
		end

		--Taking the booth off the only contestant's controller ends the round
		if FT.state ~= "playing" then
			return
		end

		if FT.roundMS >= FT.suddenDeathMS then
			if FT.suddenDeathMS == FT_SUDDEN_DEATH_MS then
				announce("Sudden death! Platforms are going for good.", 3000, 1, 0.3, 0.3)
			end
			FT.suddenDeathMS = FT.suddenDeathMS + FT_SUDDEN_DEATH_EVERY_MS
			suddenDeath()
		end
	end

	checkRoundOver()
end

------------------------------------------------------------------------------------------------------------------------
--Switching the gamemode on and off
------------------------------------------------------------------------------------------------------------------------

local function removeStructure()
	for _, brick in ipairs(FT.structure) do
		if brickAlive(brick) then
			brick:remove()
		end
	end
	FT.structure = {}
	FT.buttons = {}
	FT.buttonsByBrick = {}
end

--Returns true, or false and why not
function ftStart()
	if FT.active then
		return false, "Falling Tiles is already running."
	end

	if not (buildRoom(boothX(), roomZ(), true) and buildRoom(boxX(), roomZ(), false) and buildButtons()) then
		removeStructure()
		return false, "Something is built where the stage goes, around " .. FT_X .. ", " .. FT_Z .. ". Clear it or change FT_X and FT_Z."
	end

	FT.active = true
	FT.session = FT.session + 1
	FT.state = "waiting"
	FT.controller = nil
	FT.nextController = nil

	--The same way Item_Ammo.lua hangs itself off Damage.lua: listeners and spawnPlayer look these up by name as they run
	FT.giveStartingItems = giveStartingItems
	giveStartingItems = ftGiveStartingItems
	FT.pickSpawnPosition = pickSpawnPosition
	pickSpawnPosition = ftPickSpawnPosition

	for _, client in ipairs(everyone()) do
		client.ftRole = "spectator"
	end

	ftTick(FT.session)
	return true
end

function ftStop()
	if not FT.active then
		return false, "Falling Tiles isn't running."
	end

	settleRound()
	FT.active = false
	FT.session = FT.session + 1
	FT.state = "waiting"
	FT.controller = nil
	FT.nextController = nil

	giveStartingItems = FT.giveStartingItems
	pickSpawnPosition = FT.pickSpawnPosition

	centerPrintAll("", 0)
	for _, client in ipairs(everyone()) do
		client.ftRole = nil
		client:setJetsEnabled(true)
		if playerOf(client) ~= nil then
			stripItems(client)
			place(client, pickSpawnPosition())
			giveStartingItems(client)
		end
	end

	clearStage()
	removeStructure()
	return true
end

--Whoever joins watches from the box until the next round, where serverstart.lua's join just spawned them
--Someone joining a server with nobody on a stage gets a round going
function ftJoin(client)
	if FT.active then
		client.ftRole = "spectator"
		client:setJetsEnabled(false)
		local player = playerOf(client)
		if player ~= nil then
			player.canBeDamaged = false
		end
		client:message("Falling Tiles is being played. You're in the next round.")

		if (FT.state == "countdown" or FT.state == "playing") and FT.startCount == 0 then
			settleRound()
			FT.state = "intermission"
			FT.nextController = FT.controller
			queueRound(1000)
		end
	end
	return client
end
registerEventListener("ClientJoin", "ftJoin")

--ftTick notices a missing controller, and that the round may be over
function ftLeave(client)
	client.ftLeft = true
	client.ftRole = nil
	return client
end
registerEventListener("ClientLeave", "ftLeave")

--No building a way out, or a bridge
function ftPlantBrick(client, brick)
	if FT.active and brick then
		local x, _, z = brick:getPosition()
		local margin = ROOM_DEPTH + 10
		if x >= FT_X - margin and x <= FT_X + FT_SIZE + margin and z >= FT_Z - margin and z <= FT_Z + FT_SIZE + margin then
			brick:remove()
			client:centerPrint("No building around the Falling Tiles stage.", 2000, 1, 0.4, 0.4)
			return client, nil
		end
	end
	return client, brick
end
registerEventListener("ClientPlantBrick", "ftPlantBrick")

if adminCommands ~= nil then
	--/fallingTiles switches it on or off, /fallingTiles on and off say which, and /fallingTiles next starts a new round now
	adminCommands["/fallingtiles"] = function(client, argText)
		local arg = string.lower(argText)

		if arg == "next" or arg == "skip" then
			if not FT.active then
				client:message("Falling Tiles isn't running.")
				return
			end
			settleRound()
			FT.state = "intermission"
			messageAll(client:getName() .. " skipped to the next round of Falling Tiles.")
			ftStartRound(FT.session)
			return
		end

		local turnOn = not FT.active
		if arg == "on" then
			turnOn = true
		elseif arg == "off" then
			turnOn = false
		elseif arg ~= "" then
			client:message("Usage: /fallingTiles [on|off|next]")
			return
		end

		local worked, why
		if turnOn then
			worked, why = ftStart()
		else
			worked, why = ftStop()
		end
		if not worked then
			client:message(why)
		elseif turnOn then
			messageAll(client:getName() .. " started Falling Tiles: be the last one standing, and you control the next round.")
		else
			messageAll(client:getName() .. " ended Falling Tiles.")
		end
	end
	registerChatSuggestion("fallingtiles", "/fallingTiles [on|off|next] - admins: switch the Falling Tiles gamemode on or off, or skip to its next round")
end

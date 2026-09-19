--[[
	menudemo.lua - the game that plays behind the main menu.

	The client hosts this on a server of its own (MENU_DEMO_PORT) the moment it reaches the menu and joins
	it quietly: no HUD, no mouse capture, no keys, and no player of its own, see LoopClient::startMenuDemo.
	So everything here is an ordinary server script. Nobody is playing, which means nothing in it can be
	driven by a client: the players are bots walked with dynamic:setBotInput, the jeep drives itself with
	vehicle:drive, and the camera is a client:staticCamera re-aimed every tick.

	It runs serverstart.lua first, so every model, sound, emitter, weapon and add-on the real game has is
	registered here too and the demo shows off the actual game rather than a copy of it. What it then takes
	back out is the part of serverstart.lua that belongs to people playing: spawning a player for whoever joins.

	Turn the whole thing off in Settings under Graphics ("Demo Behind The Menu").
]]

dofile("serverstart.lua")

--------------------------------------------------------------------------------------------------------
-- Nobody plays the demo
--------------------------------------------------------------------------------------------------------

--[[
	serverstart.lua and everything it loads spawn a player, hand out tools, start a score and put whoever
	joins into the Falling Tiles round. The one client here is a camera, so all of that comes back off.
	Taken by walking the list rather than by name so a listener added later doesn't quietly come back
]]
for i = getNumListeners("ClientJoin") - 1, 0, -1 do
	unregisterEventListener("ClientJoin", getListenerIdx("ClientJoin", i))
end

--------------------------------------------------------------------------------------------------------
-- The island
--------------------------------------------------------------------------------------------------------

--Bricks are placed by their min corner, x and z in studs and y in plates, and a plate is 0.4 studs tall
local PLATE = 0.4

--The floor is this many plates thick, so everything walks around at FLOOR_TOP
local FLOOR_PLATES = 8
local FLOOR_TOP = FLOOR_PLATES * PLATE

--Half the island, in studs: it runs from -ISLAND to ISLAND on both axes. Small enough that a camera far
--enough back to take the whole thing in can still make out what everyone on it is doing
local ISLAND = 40

--The sea comes most of the way up the island's side, leaving a low shore all the way round
local WATER_LEVEL = 2.0

--Every color the build uses, so the whole thing can be re-tinted from one place. Bright and warm against
--the blue of the sea and sky: a pale stone island reads as fog at any distance
local C = {
	floorLight = {0.86, 0.78, 0.58},
	floorDark  = {0.72, 0.62, 0.42},
	wall       = {0.58, 0.20, 0.16},
	wallTrim   = {0.95, 0.78, 0.25},
	tower      = {0.50, 0.17, 0.14},
	towerTrim  = {0.95, 0.78, 0.25},
	towerStone = {0.80, 0.80, 0.82},
	plaza      = {0.88, 0.88, 0.90},
	plazaTrim  = {0.20, 0.35, 0.65},
	beacon     = {1.00, 0.85, 0.35},
	bridge     = {0.45, 0.45, 0.50},
	red        = {0.85, 0.25, 0.22},
	blue       = {0.25, 0.45, 0.90},
}

--Cover blocks are the colors of a brick box, which is what puts the color into the middle of the island
local COVER_COLORS = {
	{0.85, 0.22, 0.20}, {0.20, 0.40, 0.85}, {0.95, 0.80, 0.20},
	{0.25, 0.65, 0.30}, {0.90, 0.50, 0.15}, {0.55, 0.30, 0.70},
}

--Bricks can never overlap and addBrick quietly gives back nothing when one would, so the count of those
--is worth having: it's the only sign that a piece of the island didn't get built where it was meant to
local bricksRefused = 0

local function brick(x, y, z, w, h, l, color, material, angle)
	local b = addBrick(x, y, z, w, h, l, color[1], color[2], color[3], 1, angle or 0)
	if not b then
		bricksRefused = bricksRefused + 1
		return nil
	end
	if material then
		b:setMaterial(material)
	end
	return b
end

--A checkered plaza floor, in 8 stud tiles
local function buildFloor()
	local tile = 8
	for x = -ISLAND, ISLAND - tile, tile do
		for z = -ISLAND, ISLAND - tile, tile do
			local even = ((x + z) / tile) % 2 == 0
			brick(x, 0, z, tile, FLOOR_PLATES, tile, even and C.floorLight or C.floorDark)
		end
	end
end

--How much of each corner the towers take up, which is where the wall stops
local TOWER_SIZE = 12

--The catwalks between the towers run at this height, in plates off the ground
local BRIDGE_Y = 34

--A low wall round the edge, standing on the floor rather than replacing it, running between the corner
--towers with a gateway in the middle of each side, and a gold coping along the top
local function buildWall()
	local gap = 10
	local thickness = 2
	local height = 7

	--The two runs of one side: from the corner tower in to the gateway, on both sides of it
	local from = -ISLAND + TOWER_SIZE
	local to = ISLAND - TOWER_SIZE
	local runs = {{from, -gap / 2}, {gap / 2, to}}

	for _, span in ipairs(runs) do
		local start, finish = span[1], span[2]
		local length = finish - start
		if length > 0 then
			--North and south
			brick(start, FLOOR_PLATES, -ISLAND, length, height, thickness, C.wall)
			brick(start, FLOOR_PLATES + height, -ISLAND, length, 1, thickness, C.wallTrim)
			brick(start, FLOOR_PLATES, ISLAND - thickness, length, height, thickness, C.wall)
			brick(start, FLOOR_PLATES + height, ISLAND - thickness, length, 1, thickness, C.wallTrim)

			--West and east
			brick(-ISLAND, FLOOR_PLATES, start, thickness, height, length, C.wall)
			brick(-ISLAND, FLOOR_PLATES + height, start, thickness, 1, length, C.wallTrim)
			brick(ISLAND - thickness, FLOOR_PLATES, start, thickness, height, length, C.wall)
			brick(ISLAND - thickness, FLOOR_PLATES + height, start, thickness, 1, length, C.wallTrim)
		end
	end
end

--Where each tower stands and how high its light sits, filled in by buildTowers for the camera to look at
demoTowers = {}

--A corner tower of three tiers, a glowing top and a light on it
local function buildTower(cornerX, cornerZ)
	local size = TOWER_SIZE
	local x = cornerX < 0 and -ISLAND or ISLAND - size
	local z = cornerZ < 0 and -ISLAND or ISLAND - size

	local y = FLOOR_PLATES
	--Alternating red and stone tiers, so a tower reads as banded rather than as one tall block
	local tiers = {{size, 14, C.tower}, {size - 2, 12, C.towerStone}, {size - 4, 12, C.tower}, {size - 6, 10, C.towerStone}}
	local at = {x = x, z = z, size = size}

	for i, tier in ipairs(tiers) do
		local tierSize, tierHeight, tierColor = tier[1], tier[2], tier[3]
		--Each tier is centered on the one below it
		local inset = (size - tierSize) / 2
		brick(x + inset, y, z + inset, tierSize, tierHeight, tierSize, tierColor)
		--A gold band round the top of each tier
		brick(x + inset, y + tierHeight, z + inset, tierSize, 1, tierSize, C.towerTrim)
		y = y + tierHeight + 1
		at.top = y
	end

	--A glowing block on top with a real light in it, which is what the island is lit by after dark
	local topSize = size - 8
	local topInset = (size - topSize) / 2
	brick(x + topInset, y, z + topInset, topSize, 4, topSize, C.beacon, "Glow")

	local lightX = x + size / 2
	local lightZ = z + size / 2
	local lightY = (y + 4) * PLATE + 1
	createLight(lightX, lightY, lightZ, 1.0, 0.85, 0.5, 1400, 0.05, 1.4)

	--A brazier burning on the parapet, which is the moving thing a still tower needs
	addEmitter("BurnEmitterA", lightX, (y + 2) * PLATE, lightZ)

	at.lightX, at.lightY, at.lightZ = lightX, lightY, lightZ
	table.insert(demoTowers, at)
end

--[[
	Catwalks between the towers along all four sides, well above the wall. Nobody walks them: they're
	there so that every shot of the island has something crossing it at height, which is the difference
	between a flat plate with things on it and somewhere built
]]
local function buildBridges()
	--Even, so that the decks below land on whole studs either side of the middle of the run
	local width = 6
	local inner = ISLAND - TOWER_SIZE
	local length = inner * 2
	--Far enough out that the pillars holding them up stand clear of the jeep's lap
	local edge = ISLAND - 6

	local function deck(x, z, w, l)
		brick(x, BRIDGE_Y, z, w, 2, l, C.bridge)
		--A gold rail down each long side, a plate proud of the deck
		if w > l then
			brick(x, BRIDGE_Y + 2, z, w, 3, 1, C.wallTrim)
			brick(x, BRIDGE_Y + 2, z + l - 1, w, 3, 1, C.wallTrim)
		else
			brick(x, BRIDGE_Y + 2, z, 1, 3, l, C.wallTrim)
			brick(x + w - 1, BRIDGE_Y + 2, z, 1, 3, l, C.wallTrim)
		end
	end

	deck(-inner, -edge - width / 2, length, width)
	deck(-inner, edge - width / 2, length, width)
	deck(-edge - width / 2, -inner, width, length)
	deck(edge - width / 2, -inner, width, length)

	--Pillars holding the long runs up, spaced down each side
	for along = -inner + 8, inner - 8, 16 do
		for _, side in ipairs({-edge, edge}) do
			brick(along, FLOOR_PLATES, side - 1, 2, BRIDGE_Y - FLOOR_PLATES, 2, C.towerStone)
			brick(side - 1, FLOOR_PLATES, along, 2, BRIDGE_Y - FLOOR_PLATES, 2, C.towerStone)
		end
	end
end

--Where the plaza's top is, which is the height most of the fighting happens at
local PLAZA_HEIGHT = 6
local PLAZA_TOP = (FLOOR_PLATES + PLAZA_HEIGHT) * PLATE

--A raised square in the middle with a ramp up each of two sides, the high ground everything fights over,
--and a spire out of the top of it to give the middle of the island a shape
local function buildPlaza()
	local size = 22
	local half = size / 2

	brick(-half, FLOOR_PLATES, -half, size, PLAZA_HEIGHT, size, C.plaza)
	brick(-half, FLOOR_PLATES + PLAZA_HEIGHT, -half, size, 1, size, C.plazaTrim)

	--[[
		Stepped ramps either side, since a stack of plates reads as a slope well enough at this size. One
		plate up per stud along, which is well under the four plates a walking player steps onto, and keeps
		the ramps short enough that the jeep's lap passes outside them
	]]
	for step = 1, PLAZA_HEIGHT do
		local depth = 1
		local z = half + (PLAZA_HEIGHT - step) * depth
		brick(-half, FLOOR_PLATES, z, size, step, depth, C.plaza)
		brick(-half, FLOOR_PLATES, -z - depth, size, step, depth, C.plaza)
	end

	--The spire: a stack of ever smaller blocks with a glowing lamp on the very top
	local y = FLOOR_PLATES + PLAZA_HEIGHT + 1
	local widths = {10, 8, 6, 4}
	for i, w in ipairs(widths) do
		local inset = -w / 2
		brick(inset, y, inset, w, 8, w, i % 2 == 0 and C.plazaTrim or C.plaza)
		y = y + 8
	end

	brick(-2, y, -2, 4, 4, 4, C.beacon, "Glow")
	createLight(0, (y + 4) * PLATE, 0, 0.55, 0.75, 1.0, 1800, 0, 1.6)
end

--[[
	Blocks to run between and shoot from behind. They sit either well inside the jeep's lap or right out
	against the wall, never on it: nobody drives the jeep and it can't steer round anything, so whatever is
	left in its way is what it spends the rest of the demo stuck against

	The ones inside stand in two columns either side of the plaza. They have to be at least 12 studs out
	along x to miss the plaza and its ramps, which leaves nowhere for them along the other two sides, and
	none of them may reach further than about 20 studs from the middle or the jeep clips them on its lap
]]
local function buildCover()
	local inside = {
		{-15, -8}, {-15, -2}, {-15, 4},
		{11, -8}, {11, -2}, {11, 4},
	}

	for i, spot in ipairs(inside) do
		local height = (i % 3 == 0) and 12 or 8
		brick(spot[1], FLOOR_PLATES, spot[2], 4, height, 4, COVER_COLORS[(i - 1) % #COVER_COLORS + 1])
	end
end

buildFloor()
--Towers before the wall and the bridges, both of which stop short of them
buildTower(-1, -1)
buildTower(-1, 1)
buildTower(1, -1)
buildTower(1, 1)
buildWall()
buildBridges()
buildPlaza()
buildCover()

--------------------------------------------------------------------------------------------------------
-- Sky and sea
--------------------------------------------------------------------------------------------------------

setWaterLevel(WATER_LEVEL)

--[[
	Opens at night, a while after dark: the towers' beacons and the spire are the only light, so the first
	thing the menu shows off is the point light shadows they throw. A whole day runs in about eight minutes
	from there, so it's never the same picture twice - dawn, noon, a long gold evening, and back to this
]]
setTimeOfDay(0.82)
setTimeScale(2)

setFogDistance(260, 620)
setFogHeight(90)

--------------------------------------------------------------------------------------------------------
-- Bots
--------------------------------------------------------------------------------------------------------

--Everyone in the demo, red team and blue team
demoBots = {}

--[[
	Where they run to. A bot walks to one, looks around, then picks another. They're the spots beside the
	cover blocks and up on the plaza rather than an even scattering, so the fighting keeps coming back to
	the middle of the island where a camera can see it, instead of spreading out along the walls
]]
local BOT_WAYPOINTS = {
	{-26, -14}, {-14, -26}, {16, -22}, {24, -10}, {22, 16}, {10, 24}, {-18, 22}, {-26, 10},
	{-20, -2}, {18, 2}, {-2, -20}, {2, 18},
	{-8, -8}, {8, -8}, {8, 8}, {-8, 8},
	{0, 16}, {0, -16}, {16, 0}, {-16, 0},
}

--How close to a waypoint counts as having got there, studs
local WAYPOINT_REACHED = 4

--Studs a bot will shoot from
local BOT_RANGE = 70

--[[
	What the bots fight with. Every one of these is a real registered weapon out of the add-ons, and a bot
	fires it the way Support_Weapons.lua fires it for a player: the same round at the same speed with the
	same drop, spread and pellets, tagged with the weapon's own name, so the add-on's own ProjectileHit
	listener gives every shot its real flash, crack, dust and hole in the brick.

	fireMS is how long a bot waits between shots, which isn't the weapon's own fireDelayMS: a bot has all
	day to aim and never reloads, so what a gun can do and what looks right coming from one aren't the same
	thing. burst is how many shots in a row an automatic gets before it waits restMS instead
]]
local BOT_ARSENAL = {
	{ weapon = "gun",           fireMS = 520,  jitterMS = 250 },
	{ weapon = "pistol",        fireMS = 400,  jitterMS = 200 },
	{ weapon = "submachinegun", fireMS = 110,  jitterMS = 40, burst = 5, restMS = 1000 },
	{ weapon = "bow",           fireMS = 1000, jitterMS = 400 },
	{ weapon = "pumpShotgun",   fireMS = 950,  jitterMS = 300 },
}

--How far a bot's own aim wanders, in radians, before the weapon's spread goes on top of it per pellet.
--Nobody in the demo is a crack shot, and near misses cracking off the bricks are half the point
local BOT_AIM_CONE = 0.035

--Radians either way for each 1 of a weapon's spread field, the same conversion Support_Weapons.lua makes
local SPREAD_TO_RADIANS = 5 * math.pi

--A hitscan weapon's tracer is only something to watch, so it's tagged with something Weapons has no entry
--for: nothing is meant to happen where it lands
local DEMO_TRACER_TAG = "demoTracer"
local DEMO_TRACER_SPEED = 320

--From NetTypes/NetType.h's SimObjectType enum
local DYNAMIC_TYPE_ID = 1

--[[
	The bots keep their own health, because Damage.lua's lives on clients' players and a bot is not a
	client. The weapons' real damage numbers are used as they are, so this one number is the whole of how
	often somebody dies, and eight players is a lot of gunfire: at a real player's 100 health two or three
	of them were lying on the floor at any moment. This is about nine rounds of the Gun, or three shotgun
	shells from close up, which leaves one body on the ground most of the time and none of it for seconds
	at a stretch
]]
local BOT_HEALTH = 280

--How long a body lies there before that bot is back on its feet where it started
local BOT_RESPAWN_MS = 5000

--[[
	How often one bot can show being hurt. Every round that lands would be the honest thing, which is what a
	player gets, but a submachinegun puts eight of them a second into somebody and there are eight players
	fighting at once: the whole screen behind the menu fills up with OUCH! and the air with grunting. One
	every so often reads as someone taking fire just as well, and the shots themselves still land where they
	land
]]
local BOT_HURT_SHOW_MS = 700

--How a body is thrown as it goes over, in studs per second, and how fast it tips, in radians per second
local CORPSE_PUSH_SPEED = 6
local CORPSE_LIFT_SPEED = 8
local CORPSE_TIP_SPEED = 6

--[[
	How far off an enemy a bot is happy to be. Further than BOT_HOLD_FAR it carries on to wherever it was
	going, between the two it stands its ground and strafes, and closer than BOT_HOLD_NEAR it backs off

	Without this they walk into whoever they're shooting at, both teams end up in one scrum in the middle
	of the island, and every shot of the demo is a pile of players standing in each other
]]
local BOT_HOLD_FAR = 46
local BOT_HOLD_NEAR = 16

--How long a bot strafes one way before turning round, so a fight moves instead of being two statues
local BOT_STRAFE_MS = 2200

--Roughly chest height on a brickhead, where its shots come from
local BOT_MUZZLE_HEIGHT = 3.2

local FACES = {"BrownSmiley.png", "smileyBlonde.png", "ChefSmiley.png", "Male07Smiley.png", "KleinerSmiley.png", "Orc.png"}
local HATS = {"cap.txt", "top_hat.txt", "fedora.txt", "jester_cap.txt"}

local function randomFrom(list)
	return list[math.random(#list)]
end

--[[
	The weapon a bot carries is a real item in its hand, the same one a player would be given: everyone draws a
	held item at the middle of its holder's Right_Hand, turned the way that body faces, and dynamic:setHeldItem
	is what puts one there for a dynamic nobody plays. So the bots are seen carrying what they shoot with, their
	guns kick and work as they fire, and a dead one's weapon falls out of its hands
]]
function giveBotWeapon(bot)
	local typeID = getDynamicType(bot.kit.weapon)
	if typeID == nil then
		return
	end

	local x, y, z = bot:getPosition()
	bot.item = createItem(typeID, x, y + 2, z)
	if bot.item ~= nil then
		bot:setHeldItem(bot.item)
	end
end

--A player-looking bot of one team, standing where it's put, fighting with the weapon it was dealt
local function makeBot(team, x, z, kit)
	local bot = createDynamic(brickhead, x, FLOOR_TOP + 1, z)
	if not bot then
		return nil
	end

	--Same as a real player: nothing tips them over
	bot:setAngularFactor(0, 0, 0)

	--Mesh names are the ones in Brickhead.fbx and setMeshColor matches them exactly, capitals and all
	local shirt = team == "red" and C.red or C.blue
	bot:setMeshColor("Torso", shirt[1], shirt[2], shirt[3], 1)
	bot:setMeshColor("RightShoulder", shirt[1], shirt[2], shirt[3], 1)
	bot:setMeshColor("LeftShoulder", shirt[1], shirt[2], shirt[3], 1)
	bot:setMeshColor("Right_Hand", 0.90, 0.75, 0.55, 1)
	bot:setMeshColor("Left_Hand", 0.90, 0.75, 0.55, 1)
	bot:setMeshColor("Head", 0.90, 0.75, 0.55, 1)
	bot:setMeshColor("Face1", 0.90, 0.75, 0.55, 1)
	bot:setMeshColor("Lower_Body", 0.15, 0.15, 0.18, 1)
	bot:setMeshColor("Right_Leg", 0.15, 0.15, 0.18, 1)
	bot:setMeshColor("Wedge", 0.15, 0.15, 0.18, 1)
	bot:setMeshColor("Left_Leg", 0.15, 0.15, 0.18, 1)
	bot:setMeshColor("Right_Foot", 0.1, 0.1, 0.1, 1)
	bot:setMeshColor("Left_Foot", 0.1, 0.1, 0.1, 1)
	bot:setMeshDecal("Face1", randomFrom(FACES))

	--Hats, which the demo's own gunfire can knock off again, see Hats.lua. Which one it wears is kept, so
	--that one shot off its head is back on when it respawns
	bot.hat = randomFrom(HATS)
	bot:setPart("hat", bot.hat, 0, 0, 0, 0, 1)

	bot.team = team
	--Which of the demo's weapons it fights with, and how its trigger goes, see BOT_ARSENAL
	bot.kit = kit
	--And the weapon itself, really in its hand: an item is drawn on whatever dynamic holds it, client or not
	giveBotWeapon(bot)
	bot.burstLeft = 0
	--What everything that hurts a bot looks for, since nothing else in the world is one
	bot.demoBot = true
	bot.health = BOT_HEALTH
	bot.dead = false
	bot.deaths = 0
	bot.nextFireMS = 0
	bot.waypoint = nil
	bot.jetUntilMS = 0
	bot.homeX, bot.homeZ = x, z
	--Which way it sidesteps while it's holding its ground, and when it next turns round
	bot.strafe = (math.random(2) == 1) and 1 or -1
	bot.strafeUntilMS = 0

	table.insert(demoBots, bot)
	return bot
end

--[[
	Weapons are dealt out of a shuffled arsenal rather than picked at random for each bot, so that all five
	of them turn up among eight rather than three bots happening to draw the same gun
]]
local dealt = {}

local function dealKit()
	if #dealt == 0 then
		for i, kit in ipairs(BOT_ARSENAL) do
			dealt[i] = kit
		end
		for i = #dealt, 2, -1 do
			local j = math.random(i)
			dealt[i], dealt[j] = dealt[j], dealt[i]
		end
	end

	return table.remove(dealt)
end

--Spread along opposite sides so they don't spawn inside each other and meet somewhere in the middle
local function spawnTeams()
	for i = 1, 4 do
		makeBot("red", -30 + i * 6, -30, dealKit())
		makeBot("blue", 30 - i * 6, 30, dealKit())
	end
end

spawnTeams()

--The nearest bot of the other team with a clear shot, or nil
local function enemyInSight(bot)
	local x, y, z = bot:getPosition()
	local fromY = y + BOT_MUZZLE_HEIGHT

	local best, bestDistance = nil, BOT_RANGE
	for _, other in ipairs(demoBots) do
		--Nobody shoots at a body: it's lying there until it respawns, see demoKillBot
		if other.team ~= bot.team and not other.dead then
			local ox, oy, oz = other:getPosition()
			local toY = oy + BOT_MUZZLE_HEIGHT
			local distance = math.sqrt((ox - x) ^ 2 + (oy - y) ^ 2 + (oz - z) ^ 2)
			if distance < bestDistance then
				--Only if nothing is in the way. The ray ignores the shooter, so what it hits first
				--is either the target or whatever is between them
				local hit = raycast(x, fromY, z, ox, toY, oz, bot)
				if hit and hit.type == 1 and hit.id == other.id then
					best, bestDistance = other, distance
				end
			end
		end
	end

	return best
end

--[[
	Getting shot

	Damage.lua keeps health on clients' players and a bot is not a client, so the demo keeps its own: the
	weapons' real damage numbers off BOT_HEALTH, and a body that falls over where it died.

	A body is the same dynamic the bot was, rather than a new one. Damage.lua has to swap a player for a
	corpse because its client is still controlling that player; nothing controls a bot, so letting go of
	its keys and letting it tip over is the whole of it, and everything else in the demo that holds onto a
	bot - the camera's star, the crowd the shots are framed on - carries on without knowing anyone died
]]

--Takes health off a bot, showing it being hurt, and kills it if that was the last of it. x, y, z is where
--it was hit, or nothing
function demoDamageBot(bot, amount, x, y, z)
	if bot.dead or amount == nil or amount <= 0 then
		return
	end

	bot.health = bot.health - amount

	--serverstart.lua's: the ouch particles and the grunt, but not for every round that lands, see
	--BOT_HURT_SHOW_MS. The red vignette in there is for a client, and is quietly skipped for a bot
	if demoClockMS >= (bot.nextHurtShowMS or 0) then
		bot.nextHurtShowMS = demoClockMS + BOT_HURT_SHOW_MS
		hurtPlayer(bot, x, y, z)
	end

	if bot.health <= 0 then
		demoKillBot(bot, x, y, z)
	end
end

--[[
	A bot's body goes over where it died and lies there until it respawns. Damage.lua's corpseSettle
	watches it fall and stops it turning once it's down, the same as it does for a player's body, and
	bot.removed is the flag it reads to know a body it was watching has gone
]]
function demoKillBot(bot, fromX, fromY, fromZ)
	if bot.dead then
		return
	end

	bot.dead = true
	bot.health = 0
	bot.deaths = bot.deaths + 1
	bot.fighting = false
	--It stops walking, and its walk cycle stops with it, leaving an ordinary object to fall over
	bot:clearBotInput()

	--Its weapon falls out of its hands where it stood, the way Inventory.lua drops what a player was carrying
	bot:setHeldItem()

	local x, y, z = bot:getPosition()

	--Which way it goes over, level: away from whatever hit it, or any way at all
	local awayX, awayZ = 0, 0
	if fromX ~= nil then
		awayX, awayZ = x - fromX, z - fromZ
	end
	local length = math.sqrt(awayX * awayX + awayZ * awayZ)
	if length < 0.05 then
		local angle = math.random() * math.pi * 2
		awayX, awayZ, length = math.cos(angle), math.sin(angle), 1
	end
	awayX, awayZ = awayX / length, awayZ / length

	--[[
		A body is a box, and one that fell cornerways would come to rest on an edge, so it goes over
		forwards, backwards or to one side, whichever of those is nearest to the way it was pushed. A bot
		is only ever turned about its up, so those four are the way it's facing and the two ways across it
	]]
	local lookX, lookZ = bot.lookX or 1, bot.lookZ or 0
	local best = -2
	for _, axis in ipairs({{lookX, lookZ}, {lookZ, -lookX}}) do
		for sign = -1, 1, 2 do
			local along = (axis[1] * awayX + axis[2] * awayZ) * sign
			if along > best then
				best, awayX, awayZ = along, axis[1] * sign, axis[2] * sign
			end
		end
	end

	local velX, velY, velZ = bot:getVelocity()
	--Nothing tips a bot over while it's on its feet, so this is where it's let go of
	bot:setAngularFactor(1, 1, 1)
	bot:setVelocity(velX + awayX * CORPSE_PUSH_SPEED, velY + CORPSE_LIFT_SPEED, velZ + awayZ * CORPSE_PUSH_SPEED)
	--Turning around the level axis square to the way it falls tips its head that way
	bot:setAngularVelocity(awayZ * CORPSE_TIP_SPEED, 0, -awayX * CORPSE_TIP_SPEED)
	bot:playSound("Death")

	--Damage.lua's, which stops it turning once it's lying down: a look every 50 ms, 60 of them at most
	bot.removed = false
	schedule(50, "corpseSettle", bot, 60, 0)

	schedule(BOT_RESPAWN_MS, "demoRespawnBot", bot, bot.deaths)
end

--Back on its feet where it started, its body going in the same puff of smoke a player's leaves in
function demoRespawnBot(bot, deaths)
	--Put back some other way since this was scheduled
	if not bot.dead or bot.deaths ~= deaths then
		return
	end

	local x, y, z = bot:getPosition()
	addEmitter("bodyRemoveEmitter", x, y + 1, z)
	playSound("BodyRemove", x, y, z)

	--Nothing left for corpseSettle to do to it, whether or not it had finished watching it fall
	bot.removed = true

	bot:setPosition(bot.homeX, FLOOR_TOP + 2, bot.homeZ)
	bot:setRotation(1, 0, 0, 0)
	bot:setVelocity(0, 0, 0)
	bot:setAngularVelocity(0, 0, 0)
	--Back on its feet, and upright for good again
	bot:setAngularFactor(0, 0, 0)
	--And wearing its hat, in case a shot took that off, see Hats.lua
	bot:setPart("hat", bot.hat, 0, 0, 0, 0, 1)
	bot:playSound("Spawn")

	--The weapon it dropped as it died is cleared away with its body, and it comes back holding a new one
	if bot.item ~= nil then
		bot.item:destroy()
		bot.item = nil
	end
	giveBotWeapon(bot)

	bot.health = BOT_HEALTH
	bot.dead = false
	bot.waypoint = nil
	bot.burstLeft = 0
	bot.nextFireMS = 0
end

--[[
	Rounds of the bots' still in the air that have a lifetime, by ID, so that one whose time runs out is
	only taken away if it hasn't already hit something and been removed by the engine
]]
local demoFlyingShots = {}

function demoShotExpired(shotID)
	local shot = demoFlyingShots[shotID]
	if shot ~= nil then
		demoFlyingShots[shotID] = nil
		shot:destroy()
	end
end

--[[
	Where a bot's round lands. Support_Weapons.lua's own listener has already done everything a shot does
	to the world - the hole, the dust, the crack - and everything it does to a player; this is the one part
	it can't do, since its hurtIfPlayer only knows how to hurt a client's player
]]
function demoProjectileHit(projectile, hit, x, y, z, tag, normalX, normalY, normalZ)
	demoFlyingShots[projectile.id] = nil

	local weapon = Weapons[tag]
	if weapon ~= nil and hit ~= nil and hit.type == DYNAMIC_TYPE_ID and hit.demoBot then
		demoDamageBot(hit, weapon.damage, x, y, z)
	end

	return projectile, hit, x, y, z, tag, normalX, normalY, normalZ
end
registerEventListener("ProjectileHit", "demoProjectileHit")

--[[
	And the same for a blast, which is how a launcher shell does its damage: Damage.lua's damageByImpulse
	hurts every player a radiusImpulse pushed, by the square of the push that reached them, so the bots take
	the same off the same push. impulseHarmless is set around a blast that does its own damage and only
	wants the shove
]]
function demoImpulseHit(dynamic, x, y, z, strength)
	if strength > 0 and not impulseHarmless and dynamic.demoBot then
		demoDamageBot(dynamic, strength * strength * IMPULSE_DAMAGE_SCALE)
	end

	return dynamic, x, y, z, strength
end
registerEventListener("RadiusImpulseHit", "demoImpulseHit")

--A direction knocked off course by up to angle radians, the way a shotgun throws its pellets apart
local function wobble(dirX, dirY, dirZ, angle)
	if angle <= 0 then
		return dirX, dirY, dirZ
	end

	--Two axes across the shot to push it around in. Any pair at right angles to it will do
	local sideX, sideY, sideZ = -dirZ, 0, dirX
	local sideLength = math.sqrt(sideX * sideX + sideZ * sideZ)
	if sideLength < 0.0001 then
		sideX, sideY, sideZ, sideLength = 1, 0, 0, 1
	end
	sideX, sideZ = sideX / sideLength, sideZ / sideLength

	local upX = dirY * sideZ - dirZ * sideY
	local upY = dirZ * sideX - dirX * sideZ
	local upZ = dirX * sideY - dirY * sideX

	local across = (math.random() * 2 - 1) * angle
	local over = (math.random() * 2 - 1) * angle

	local outX = dirX + sideX * across + upX * over
	local outY = dirY + sideY * across + upY * over
	local outZ = dirZ + sideZ * across + upZ * over

	local outLength = math.sqrt(outX * outX + outY * outY + outZ * outZ)
	if outLength < 0.0001 then
		return dirX, dirY, dirZ
	end

	return outX / outLength, outY / outLength, outZ / outLength
end

--[[
	One round out of the barrel, the way Support_Weapons.lua's fireOneShot puts one there for a player: a
	real projectile for most of them, tagged with the weapon's name so everything that happens where it
	lands is the add-on's own, and a hitscan shot with a tracer to watch for the pistol, which is how that
	one was written
]]
local function botFireOne(bot, weapon, fromX, fromY, fromZ, dirX, dirY, dirZ)
	if weapon.projectile ~= nil then
		local projectileType = getDynamicType(weapon.projectile)
		if projectileType == nil then
			return
		end

		local speed = weapon.projectileSpeed or 200
		local shot = addProjectile(projectileType, fromX, fromY, fromZ,
			dirX * speed, dirY * speed, dirZ * speed, weapon.name, bot)
		if shot == nil then
			return
		end

		--gravityMod in the originals, how much of normal gravity the round feels on its way out
		shot:setGravity(0, WORLD_GRAVITY * (weapon.gravityScale or 0), 0)

		--What streams out behind it, like the bow's arrow trail
		if weapon.trailEmitter ~= nil then
			local trail = addEmitter(weapon.trailEmitter, fromX, fromY, fromZ)
			if trail ~= nil then
				trail:attachToDynamic(shot)
			end
		end

		--A round that hits nothing is cleared away rather than flying on out over the sea forever
		if weapon.projectileLifetimeMS ~= nil then
			demoFlyingShots[shot.id] = shot
			schedule(weapon.projectileLifetimeMS, "demoShotExpired", shot.id)
		end

		return
	end

	--Hitscan: where the shot lands is worked out right away, and the tracer is only there to be seen
	local range = weapon.range or 200
	local hit, hitX, hitY, hitZ, normalX, normalY, normalZ = raycast(fromX, fromY, fromZ,
		fromX + dirX * range, fromY + dirY * range, fromZ + dirZ * range, bot)

	if hitX == nil then
		hitX, hitY, hitZ = fromX + dirX * range, fromY + dirY * range, fromZ + dirZ * range
	else
		weaponImpactEffect(weapon, hitX, hitY, hitZ, hit, normalX, normalY, normalZ)
		if hit ~= nil and hit.type == DYNAMIC_TYPE_ID and hit.demoBot then
			demoDamageBot(hit, weapon.damage, hitX, hitY, hitZ)
		end
	end

	local tracerType = weapon.tracer ~= nil and getDynamicType(weapon.tracer) or nil
	if tracerType ~= nil then
		local toX, toY, toZ = hitX - fromX, hitY - fromY, hitZ - fromZ
		local distance = math.sqrt(toX * toX + toY * toY + toZ * toZ)
		if distance < 0.001 then
			toX, toY, toZ, distance = dirX, dirY, dirZ, 1
		end

		addProjectile(tracerType, fromX, fromY, fromZ, toX / distance * DEMO_TRACER_SPEED,
			toY / distance * DEMO_TRACER_SPEED, toZ / distance * DEMO_TRACER_SPEED, DEMO_TRACER_TAG, bot)
	end
end

--[[
	Where the middle of a bot's right hand is in its own space, which is where everyone draws what it holds.
	The model's -Z is its front, so the z of it is how far ahead of the bot its hand is
]]
local HAND_RIGHT, HAND_UP, HAND_AHEAD = 0.9, 3.0, 1.6
local handLowX, handLowY, handLowZ, handHighX, handHighY, handHighZ = getTypeMeshBounds(brickhead, "Right_Hand")
if handLowX ~= nil then
	HAND_RIGHT = (handLowX + handHighX) / 2
	HAND_UP = (handLowY + handHighY) / 2
	HAND_AHEAD = -(handLowZ + handHighZ) / 2
end

--[[
	The end of the barrel of the gun a bot is holding: its hand, plus the weapon's own muzzlePoint measured
	from where the hand holds it (muzzleFromHand, in the item's own space with the barrel down -Z, the same
	numbers Support_Weapons.lua uses to find it in a player's hands).

	fx, fz is the way the bot faces, level, since that's how games draw what it's holding whatever it aims at
]]
local function botMuzzle(bot, weapon, fx, fz)
	local x, y, z = bot:getPosition()
	--The body's right, across its facing
	local rx, rz = -fz, fx

	local offset = weapon.muzzleFromHand or {0, 0, 0}
	local right = HAND_RIGHT + offset[1]
	local up = HAND_UP + offset[2]
	local ahead = HAND_AHEAD - offset[3]

	return x + rx * right + fx * ahead, y + up, z + rz * right + fz * ahead
end

--[[
	One pull of a bot's trigger with whatever it's carrying: the shot's sound and the flash off the barrel,
	then a round for each pellet, thrown apart by the weapon's own spread around wherever the bot was aiming
]]
local function botShoot(bot, target)
	local weapon = Weapons[bot.kit.weapon]
	if weapon == nil then
		return
	end

	local x, y, z = bot:getPosition()
	local tx, ty, tz = target:getPosition()

	local fromY = y + BOT_MUZZLE_HEIGHT
	local toY = ty + BOT_MUZZLE_HEIGHT - 0.5

	local dx, dy, dz = tx - x, toY - fromY, tz - z
	local length = math.sqrt(dx * dx + dy * dy + dz * dz)
	if length < 0.001 then
		return
	end

	--Where this one is really aiming, which the pellets of a shotgun shell then scatter around
	dx, dy, dz = wobble(dx / length, dy / length, dz / length, BOT_AIM_CONE)

	--Out of the barrel of the gun in its hand, which is what everyone can see it firing
	local levelX, levelZ = dx, dz
	local levelLength = math.sqrt(levelX * levelX + levelZ * levelZ)
	if levelLength > 0.001 then
		levelX, levelZ = levelX / levelLength, levelZ / levelLength
	else
		levelX, levelZ = 1, 0
	end

	local muzzleX, muzzleY, muzzleZ = botMuzzle(bot, weapon, levelX, levelZ)

	if weapon.sounds ~= nil and weapon.sounds.fire ~= nil then
		playSound(weapon.sounds.fire, muzzleX, muzzleY, muzzleZ)
	end

	--The gun working: its own fire sequence, or the kick every item has, see item:playAnimation
	if bot.item ~= nil and weapon.fireAnimation ~= nil then
		bot.item:playAnimation(weapon.fireAnimation)
	end

	if weapon.muzzleEmitter ~= nil then
		local flash = addEmitter(weapon.muzzleEmitter, muzzleX, muzzleY, muzzleZ)
		if flash ~= nil then
			schedule(weapon.muzzleEmitterMS or 60, "weaponRemoveEmitter", flash)
		end
	end

	--The demo opens at night, so what a gun lights up around itself as it goes off is worth having
	if weapon.muzzleLight ~= nil then
		local color = weapon.muzzleLight.color
		local light = createLight(muzzleX, muzzleY, muzzleZ, color[1], color[2], color[3],
			weapon.muzzleLight.brightness, 0, weapon.muzzleLight.coronaWidth or 0)

		if light ~= nil then
			schedule(weapon.muzzleLight.forMS or 50, "weaponRemoveLight", light)
		end
	end

	local spread = (weapon.spread or 0) * SPREAD_TO_RADIANS
	for pellet = 1, (weapon.pellets or 1) do
		local pelletX, pelletY, pelletZ = wobble(dx, dy, dz, spread)
		botFireOne(bot, weapon, muzzleX, muzzleY, muzzleZ, pelletX, pelletY, pelletZ)
	end
end

--[[
	Every so often somebody puts a launcher shell into the middle of things instead. Tagged "launcherShell",
	which is what Inventory.lua's own listener bursts: the blast, the smoke, and the push that sends whoever
	was standing near it flying. Nothing else in the demo throws bodies around like it does
]]
local ROCKET_EVERY_MS = 14000
local nextRocketMS = 8000

local function botRocket(bot, target)
	local x, y, z = bot:getPosition()
	local tx, ty, tz = target:getPosition()

	local fromY = y + BOT_MUZZLE_HEIGHT
	--Aimed at their feet, where the blast does the most and looks the best
	local dx, dy, dz = tx - x, ty - fromY, tz - z
	local length = math.sqrt(dx * dx + dy * dy + dz * dz)
	if length < 6 then
		return false
	end
	dx, dy, dz = dx / length, dy / length, dz / length

	--Slow enough to watch it cross the island, and lobbed a little so it arcs
	local speed = 55
	local shell = addProjectile(launcherShell, x + dx * 2, fromY + dy * 2, z + dz * 2,
		dx * speed, dy * speed + 6, dz * speed, "launcherShell", bot)
	if not shell then
		return false
	end

	addEmitter("gunSmokeEmitter", x + dx * 2, fromY + dy * 2, z + dz * 2)
	playSound("Launch", x, fromY, z)
	return true
end

--Once every brain tick: pick somewhere to be, shoot at whoever is in the way of getting there
local function thinkBot(bot, nowMS)
	--Nothing to think about while it's lying there, see demoKillBot
	if bot.dead then
		return
	end

	local x, y, z = bot:getPosition()

	--Fell in the sea, or was thrown off the island by a blast: put them back where they started
	if y < WATER_LEVEL - 4 or math.abs(x) > ISLAND + 12 or math.abs(z) > ISLAND + 12 then
		bot:setPosition(bot.homeX, FLOOR_TOP + 2, bot.homeZ)
		bot:setVelocity(0, 0, 0)
		bot.waypoint = nil
		return
	end

	--Somewhere new to be, either because they arrived or because they've never had one
	if not bot.waypoint then
		bot.waypoint = randomFrom(BOT_WAYPOINTS)
	end

	local wx, wz = bot.waypoint[1], bot.waypoint[2]
	if math.sqrt((wx - x) ^ 2 + (wz - z) ^ 2) < WAYPOINT_REACHED then
		bot.waypoint = randomFrom(BOT_WAYPOINTS)
		wx, wz = bot.waypoint[1], bot.waypoint[2]
	end

	--Which way they walk, and so which way they face
	local dx, dz = wx - x, wz - z
	local length = math.sqrt(dx * dx + dz * dz)
	if length > 0.001 then
		dx, dz = dx / length, dz / length
	else
		dx, dz = 0, 1
	end

	local target = enemyInSight(bot)

	--What they're doing with their feet: walking on to the waypoint unless a fight says otherwise
	local forward, backward, left, right = true, false, false, false

	--Shooting, they face whoever they're shooting at instead of where they're going
	if target then
		local tx, ty, tz = target:getPosition()
		local ax, az = tx - x, tz - z
		local aim = math.sqrt(ax * ax + az * az)
		if aim > 0.001 then
			dx, dz = ax / aim, az / aim
		end

		--[[
			Close enough to fight: stand off them and sidestep rather than walking into them. Strafing is
			relative to where they're looking, which is at the enemy, so they circle each other
		]]
		if aim <= BOT_HOLD_FAR then
			forward = false

			if nowMS >= bot.strafeUntilMS then
				bot.strafe = -bot.strafe
				bot.strafeUntilMS = nowMS + BOT_STRAFE_MS + math.random(0, 1200)
			end

			if aim < BOT_HOLD_NEAR then
				--Right on top of them, give ground while keeping them covered
				backward = true
			else
				left = bot.strafe < 0
				right = bot.strafe > 0
			end
		end

		if nowMS >= nextRocketMS then
			--Whoever happens to have a clear shot when a rocket is due fires it
			if botRocket(bot, target) then
				nextRocketMS = nowMS + ROCKET_EVERY_MS + math.random(0, 4000)
				bot.nextFireMS = nowMS + 1200
			end
		elseif nowMS >= bot.nextFireMS then
			botShoot(bot, target)

			--[[
				An automatic fires a run of shots at its own rate and then waits, which is most of what
				makes a submachinegun sound like one. Everything else has a burst of one and so waits
				after every shot
			]]
			local kit = bot.kit
			if bot.burstLeft > 0 then
				bot.burstLeft = bot.burstLeft - 1
			else
				bot.burstLeft = (kit.burst or 1) - 1
			end

			local wait = bot.burstLeft > 0 and kit.fireMS or (kit.restMS or kit.fireMS)
			bot.nextFireMS = nowMS + wait + math.random(0, kit.jitterMS)
		end
	end

	--[[
		Every so often somebody hops, which is the flashiest thing a player can do. Kept short: jets lift a
		player at up to 30 studs a second for as long as they're held, so a burst of much more than this
		sends them forty studs up and they spend the next few seconds falling back down out of shot
	]]
	if nowMS > bot.jetUntilMS + 9000 and math.random() < 0.01 then
		bot.jetUntilMS = nowMS + 450
	end
	local jetting = nowMS < bot.jetUntilMS

	--Kept for the camera, which has no other way to ask which way a bot is facing
	bot.lookX, bot.lookZ = dx, dz
	bot.fighting = target ~= nil

	--A bot's look direction is where its shots and its head go, and walking uses only the flat part of it
	bot:setBotInput(dx, jetting and 0.35 or 0, dz, forward, backward, left, right, false, jetting, false)
end

--------------------------------------------------------------------------------------------------------
-- The jeep
--------------------------------------------------------------------------------------------------------

--Its lap of the island, in studs, just inside the wall
--[[
	Its lap of the island: a circle rather than a square. A car has to slow right down for a square's
	corners and runs wide out of them into the wall, which is both ugly and how it ends up wedged; a steady
	curve it can hold at speed looks like driving. It also never stops turning, and a vehicle only throws
	dirt off its wheels while it steers or brakes, so the circle is what keeps the dirt coming

	The clear ring runs from 17 studs out, where the cover inside ends, to 33, where the pillars holding the
	catwalks up stand. A jeep is 3.2 by 5.4, so about 3.2 from its middle to a corner, and it wanders a
	couple of studs either side of the line it's steering for.

	It's asked for a wider circle than it actually drives: chasing a point ahead of itself cuts the corner,
	and steering that's either full lock or nothing undershoots on top of that, so it settles about three
	studs inside whatever it's given, which puts 28 right down the middle of the ring
]]
local JEEP_RADIUS = 28

--How far round the circle ahead of itself it aims, in radians. Too short and it saws at the wheel
local JEEP_LOOK_AHEAD = 0.55

--[[
	Steering is a key, not a wheel: holding left throws the wheels to full lock. So the error either side
	of which it steers has to be a wide band with a dead zone in the middle that it holds its last choice
	through, or it flips lock to lock every tick as the error crosses zero and scrubs off all its speed
]]
local JEEP_STEER_ON = 0.13
local JEEP_STEER_OFF = 0.05
local jeepSteer = 0

--And the throttle the same way, or it cuts in and out every tick around the cruising speed
local jeepThrottle = true

--Studs a second it stops accelerating at. Faster than this and it slides wide off the circle instead of
--following it, since the grip of four tyres is all that's holding it on
local JEEP_CRUISE = 19

demoJeep = nil

--How slowly it has to be going, and for how long, before it's taken to be stuck on something
local JEEP_STUCK_SPEED = 4
local JEEP_STUCK_MS = 2500
local jeepStuckForMS = 0

--Where on the circle a spot is, and the point that far round it
local function lapPoint(angle)
	return math.cos(angle) * JEEP_RADIUS, math.sin(angle) * JEEP_RADIUS
end

--[[
	Puts it down on the circle at rest, pointing the way round it drives. Facing matters: a car only steers
	while it's moving, so one dropped in facing the wall drives at the wall, and a vehicle's default rotation
	faces -Z wherever on the circle it happens to be
]]
local function putJeepOnLap(angle)
	if not demoJeep then
		return
	end

	local x, z = lapPoint(angle)

	--The way round it goes at that point, which is the tangent to the circle
	local dx, dz = -math.sin(angle), math.cos(angle)

	--The yaw that turns the model's -Z forward onto that direction
	local yaw = math.atan(-dx, -dz)

	demoJeep:setPosition(x, FLOOR_TOP + 2, z)
	demoJeep:setRotation(math.cos(yaw / 2), 0, math.sin(yaw / 2), 0)
	demoJeep:setVelocity(0, 0, 0)
	demoJeep:setAngularVelocity(0, 0, 0)
	jeepStuckForMS = 0
end

local function startJeep()
	--spawnJeep puts one down and gives it wheels, lights and a horn, all from the add-on
	local x, z = lapPoint(0)
	demoJeep = spawnJeep(x, FLOOR_TOP + 2, z)
	if demoJeep then
		--A new vehicle has no headlight until something gives it one, and this one drives through the night
		demoJeep:setHeadlight({})
		demoJeep:setHeadlightOn(true)
		putJeepOnLap(0)
	end
end

startJeep()

--[[
	Somebody behind the wheel. A vehicle's driver is a dynamic rather than a client as far as everything
	that draws one is concerned, so a bot sat there with vehicle:setDriver is drawn in the seat, holding
	the jeep's sit pose, for nothing more than saying so

	It's dressed as whoever is at the menu: demoJoin puts their own appearance editor's colors, face,
	shirt and hat on it, so the player driving past is them
]]
demoDriver = nil

local function seatDriver()
	if not demoJeep then
		return
	end

	local x, y, z = demoJeep:getPosition()
	demoDriver = createDynamic(brickhead, x, y + 4, z)
	if not demoDriver then
		return
	end

	--It never walks anywhere, but a player that can be tipped over looks wrong the moment it's let out
	demoDriver:setAngularFactor(0, 0, 0)
	demoJeep:setDriver(demoDriver)
end

seatDriver()

--[[
	Nobody is walking it, so it drives itself: vehicle:drive holds the same keys a driver would and the vehicle
	steers, leans on its suspension and throws dirt off its wheels on its own.

	It chases a point a fixed way round the circle ahead of wherever it is now, rather than a list of
	waypoints to tick off. Anything on a fixed path has to decide when it has arrived, and a car that
	overshoots then turns back for the one it missed; a point that is always ahead of it can't be missed,
	and steering back onto the circle after a knock happens by itself
]]
local function driveJeep(deltaMS)
	if not demoJeep then
		return
	end

	local x, y, z = demoJeep:getPosition()

	--Rolled off the island or ended up on its roof: put it back on the circle
	if y < WATER_LEVEL - 2 or math.abs(x) > ISLAND + 10 or math.abs(z) > ISLAND + 10 then
		putJeepOnLap(math.atan(z, x))
		return
	end

	--[[
		Which way it's pointing now. Its rotation is a quaternion about the up axis for a car on level
		ground, and the jeep is a model vehicle whose forward is -Z, so that's the axis turned by it
	]]
	local w, qx, qy, qz = demoJeep:getRotation()
	local facingX = -2 * (qx * qz + w * qy)
	local facingZ = -(1 - 2 * (qx * qx + qy * qy))
	local facing = math.sqrt(facingX * facingX + facingZ * facingZ)
	if facing < 0.0001 then
		return
	end
	facingX, facingZ = facingX / facing, facingZ / facing

	--The spot on the circle it's steering for, always that far round ahead of where it is
	local targetX, targetZ = lapPoint(math.atan(z, x) + JEEP_LOOK_AHEAD)
	local dx, dz = targetX - x, targetZ - z
	local distance = math.sqrt(dx * dx + dz * dz)
	if distance < 0.001 then
		return
	end
	dx, dz = dx / distance, dz / distance

	local vx, _, vz = demoJeep:getVelocity()
	local speed = math.sqrt(vx * vx + vz * vz)

	--[[
		Caught on something anyway: lift it back onto the circle facing the right way. The lap is meant to be
		clear, but one loose brick or a player under a wheel would otherwise leave it parked there for as
		long as the game is open
	]]
	if speed < JEEP_STUCK_SPEED then
		jeepStuckForMS = jeepStuckForMS + deltaMS
		if jeepStuckForMS >= JEEP_STUCK_MS then
			putJeepOnLap(math.atan(z, x) + JEEP_LOOK_AHEAD)
			return
		end
	else
		jeepStuckForMS = 0
	end

	--How far round it would have to turn to point at that spot, positive being to its right
	local turn = math.atan(facingX * dz - facingZ * dx, facingX * dx + facingZ * dz)

	--Steer once the error is clearly to one side, and hold that until it's clearly back in the middle
	if turn > JEEP_STEER_ON then
		jeepSteer = 1
	elseif turn < -JEEP_STEER_ON then
		jeepSteer = -1
	elseif math.abs(turn) < JEEP_STEER_OFF then
		jeepSteer = 0
	end

	--Off the throttle once it's up to speed, back on well below it, so it holds the circle rather than
	--sliding wide off it and doesn't chatter on and off around one number
	if speed > JEEP_CRUISE then
		jeepThrottle = false
	elseif speed < JEEP_CRUISE * 0.8 then
		jeepThrottle = true
	end

	--Steering is also what keeps its wheels throwing dirt, see Vehicle::drive
	demoJeep:drive(jeepThrottle, false, jeepSteer < 0, jeepSteer > 0, false)
end

--------------------------------------------------------------------------------------------------------
-- The camera
--------------------------------------------------------------------------------------------------------

--The one client watching, set as it joins
demoViewer = nil

--[[
	Where the camera has to point for whatever it's watching to sit down and to the right of the middle of
	the screen, which is where the server browser isn't: the browser opens at the top left and the demo is
	only the picture behind it, so a shot composed on the middle of the screen would be half covered

	Aiming up and to the left of the subject by these fractions of the view puts the subject down-right
]]
local FRAME_LEFT = 0.38
local FRAME_UP = 0.16

local function lookFrom(x, y, z, atX, atY, atZ)
	local dx, dy, dz = atX - x, atY - y, atZ - z
	local length = math.sqrt(dx * dx + dy * dy + dz * dz)
	if length < 0.0001 then
		return 0, 0, -1
	end
	dx, dy, dz = dx / length, dy / length, dz / length

	--The camera's own right, which is the way it points crossed with the world's up
	local rightX, rightY, rightZ = -dz, 0, dx
	local rightLength = math.sqrt(rightX * rightX + rightZ * rightZ)
	if rightLength < 0.0001 then
		--Pointing straight up or down, there's no sideways to speak of
		return dx, dy, dz
	end
	rightX, rightZ = rightX / rightLength, rightZ / rightLength

	--And its up, which is its right crossed with the way it points
	local upX = rightY * dz - rightZ * dy
	local upY = rightZ * dx - rightX * dz
	local upZ = rightX * dy - rightY * dx

	--Turned left and tipped up, which slides whatever it was looking at down and to the right of the screen
	local outX = dx - rightX * FRAME_LEFT + upX * FRAME_UP
	local outY = dy - rightY * FRAME_LEFT + upY * FRAME_UP
	local outZ = dz - rightZ * FRAME_LEFT + upZ * FRAME_UP

	local outLength = math.sqrt(outX * outX + outY * outY + outZ * outZ)
	return outX / outLength, outY / outLength, outZ / outLength
end

--Eases a 0 to 1 run of a shot so it starts and stops gently rather than snapping into motion
local function ease(t)
	return t * t * (3 - 2 * t)
end

local function mix(a, b, t)
	return a + (b - a) * t
end

--[[
	Shoves a camera spot away from anyone standing too near it. A shot down among the bots is the best one
	in the demo right up until somebody walks into the lens, and then it's a wall of torso: they run where
	they like and the camera is on a fixed path, so the two do meet
]]
local function pushClear(x, y, z, minimum, except)
	--Twice, since being shoved clear of one of them can put it inside the next
	for pass = 1, 2 do
		for _, bot in ipairs(demoBots) do
			--A body lying on the floor is nothing for the camera to dodge
			if bot ~= except and not bot.dead then
			local bx, _, bz = bot:getPosition()
			local dx, dz = x - bx, z - bz
			local distance = math.sqrt(dx * dx + dz * dz)
			if distance < minimum then
				--Standing exactly on it, push it any which way rather than dividing by nothing
				if distance < 0.001 then
					dx, dz, distance = 1, 0, 1
				end
				local push = (minimum - distance) / distance
				x = x + dx * push
				z = z + dz * push
			end
			end
		end
	end
	return x, y, z
end

--Whoever the over-the-shoulder shot is following, picked again at each cut to it
demoStar = nil

--[[
	The way the camera thinks that player is facing, which lags the way they really are. A bot works out
	where to look once a brain tick and can turn right round in one of them when it loses sight of whoever
	it was shooting at; the camera is placed behind that direction, so following it exactly swings the
	camera a full 26 studs round them in a single frame. This turns instead, over about a fifth of a second
]]
local STAR_TURN_MS = 220

--Eases one angle toward another the short way round, so turning past pi doesn't go the long way
local function easeAngle(current, target, k)
	local difference = (target - current + math.pi) % (2 * math.pi) - math.pi
	return current + difference * k
end

--One of the bots in a fight if any of them are, otherwise any of them still on their feet
local function pickStar()
	local fighting = {}
	local living = {}
	for _, bot in ipairs(demoBots) do
		if not bot.dead then
			table.insert(living, bot)
			if bot.fighting then
				table.insert(fighting, bot)
			end
		end
	end
	if #fighting > 0 then
		demoStar = fighting[math.random(#fighting)]
	elseif #living > 0 then
		demoStar = living[math.random(#living)]
	else
		--Everyone on the floor at once, which the shot falls back to the wide one for
		demoStar = nil
	end

	--The shot opens behind whoever it picked, rather than turning to get there
	if demoStar and demoStar.lookX then
		demoStar.camAngle = math.atan(demoStar.lookZ, demoStar.lookX)
	end
end

--Turns the followed player's camera-facing toward the way they're really looking, once a frame
local function turnStar(msPerTick)
	local bot = demoStar
	if not bot or not bot.lookX then
		return
	end

	local want = math.atan(bot.lookZ, bot.lookX)
	if not bot.camAngle then
		bot.camAngle = want
	else
		bot.camAngle = easeAngle(bot.camAngle, want, 1 - math.exp(-msPerTick / STAR_TURN_MS))
	end
end

--[[
	Wherever the action is right now: the middle of everyone still on their feet, or the island's middle.
	Eased rather than taken straight, because a bot that falls in the sea is put back where it started, and
	that teleport moves the average of eight of them several studs in one frame
]]
local CROWD_EASE_MS = 400
local crowdX, crowdY, crowdZ = 0, FLOOR_TOP + 3, 0
local crowdKnown = false

local function updateCrowdCenter(msPerTick)
	local x, y, z, count = 0, 0, 0, 0
	for _, bot in ipairs(demoBots) do
		if not bot.dead then
			local bx, by, bz = bot:getPosition()
			x, y, z, count = x + bx, y + by, z + bz, count + 1
		end
	end

	if count == 0 then
		return
	end

	x, y, z = x / count, y / count + 3, z / count

	if not crowdKnown then
		crowdKnown = true
		crowdX, crowdY, crowdZ = x, y, z
		return
	end

	local k = 1 - math.exp(-msPerTick / CROWD_EASE_MS)
	crowdX = crowdX + (x - crowdX) * k
	crowdY = crowdY + (y - crowdY) * k
	crowdZ = crowdZ + (z - crowdZ) * k
end

local function crowdCenter()
	return crowdX, crowdY, crowdZ
end

--[[
	The island is a dense place and a camera dropped anywhere in it ends up inside a wall, so every shot
	keeps to one of two heights that nothing is at: under the catwalks and over the cover blocks, or over
	the catwalks entirely. These are in world units off the ground

	Cover tops out at 8, the catwalk decks run from 13.6 to 14.4 with rails to 15.6, and the towers go on
	up past 25
]]
local BAND_LOW = FLOOR_TOP + 9
local BAND_HIGH = 19

--Nothing on the island reaches further from the middle than a corner tower, so this is always the open sea
local OUTSIDE = 62

--[[
	The shots. Each runs for ms and is asked, with t from 0 at its start to 1 at its end, for where the
	camera is and the spot it's watching, as six numbers. They're cut between rather than blended, the way
	a trailer cuts

	A shot gives a spot to watch rather than a direction to point in because both are then eased toward
	frame by frame, see demoTick: a shot placed off something that moves in steps (a bot's facing, which is
	only worked out every brain tick) would otherwise jump every time that step lands
]]
local DEMO_SHOTS = {
	--High and slow, round the outside: what the place is
	{
		ms = 14000,
		camera = function(t)
			--Never inside the corner towers, whose outer corners are about 56 studs from the middle
			local angle = math.pi * 0.45 * ease(t) + math.pi * 0.15
			local radius = mix(98, 84, ease(t))
			local height = mix(46, 32, ease(t))
			local x, z = math.cos(angle) * radius, math.sin(angle) * radius
			return x, height, z, 0, PLAZA_TOP + 4, 0
		end
	},

	--[[
		Over one player's shoulder while they fight, which is the only close shot that can't be walked into:
		the camera is always the same distance behind whoever it's following, wherever they go, rather than
		sitting on a path of its own that they wander across
	]]
	{
		ms = 11000,
		camera = function(t)
			local bot = demoStar
			if not bot or not bot.camAngle then
				return DEMO_SHOTS[1].camera(t)
			end

			local bx, by, bz = bot:getPosition()
			--The turned-toward facing, not the real one, see turnStar
			local lookX, lookZ = math.cos(bot.camAngle), math.sin(bot.camAngle)

			--Behind and above them, swinging round to one side over the shot
			local side = mix(-0.35, 0.35, ease(t))
			local back = mix(13, 10, ease(t))
			local x = bx - lookX * back + lookZ * back * side
			local z = bz - lookZ * back - lookX * back * side
			local y = by + mix(6.5, 5, ease(t))

			--Anyone else wandering into the lens shoves it aside, but never the one it's following
			x, y, z = pushClear(x, y, z, 7, bot)

			--Looking past them at whatever they're shooting at, rather than at the back of their head
			--Aimed at head height, so the player sits in the frame rather than half out of the bottom of it
			return x, y, z, bx + lookX * 9, by + 5.5, bz + lookZ * 9
		end
	},

	--Running along with the jeep from out over the sea, with the island behind it
	{
		ms = 10000,
		camera = function(t)
			if not demoJeep then
				return DEMO_SHOTS[1].camera(t)
			end
			local jx, jy, jz = demoJeep:getPosition()

			--Straight out past the jeep from the island's middle, so the camera is always over open water
			local toward = math.atan(jz, jx)
			local angle = toward + mix(-0.3, 0.3, ease(t))
			local radius = mix(OUTSIDE + 10, OUTSIDE + 2, ease(t))
			local x = math.cos(angle) * radius
			local z = math.sin(angle) * radius
			local y = mix(BAND_HIGH + 7, BAND_HIGH - 2, ease(t))
			return x, y, z, jx, jy + 1.5, jz
		end
	},

	--Craning down the outside of the island toward the spire, along an axis so it never crosses a tower
	{
		ms = 10000,
		camera = function(t)
			local pull = ease(t)
			local angle = mix(0.12, -0.12, pull)
			local radius = mix(88, 66, pull)
			local x = math.cos(angle) * radius
			local z = math.sin(angle) * radius
			local y = mix(46, BAND_HIGH - 3, pull)
			return x, y, z, 0, PLAZA_TOP + mix(10, 5, pull), 0
		end
	},

	--High over one corner, coming down toward whoever is out in the middle, sea and sky in the frame
	{
		ms = 12000,
		camera = function(t)
			local atX, atY, atZ = crowdCenter()
			local pull = ease(t)
			--Stays over the catwalks the whole way down, so it flies over the island rather than into it
			local height = mix(52, BAND_HIGH, pull)
			local back = mix(24, 40, pull)
			local angle = mix(3.7, 3.3, pull)
			local x = atX + math.cos(angle) * back
			local z = atZ + math.sin(angle) * back
			return x, height, z, atX, atY, atZ
		end
	},
}

local shotIndex = 1
local shotStartedMS = 0

--[[
	Where the camera actually is, as opposed to where the shot asked for it this frame. It eases toward
	what the shot wants over about this many milliseconds, which is what takes the steps out of anything a
	shot is placed off: a bot's facing only changes on a brain tick, and pushClear moves in jumps as people
	walk in and out of range, so a close shot following either of those judders without this

	Small enough that the cameras still feel like they're on rails rather than trailing behind
]]
local SMOOTH_MS = 110
local camX, camY, camZ = 0, 0, 0
local aimX, aimY, aimZ = 0, 0, 0
--Nothing to ease from on the first frame of a shot, and a cut should be a cut
local camSnap = true

--------------------------------------------------------------------------------------------------------
-- Running it
--------------------------------------------------------------------------------------------------------

--How often the bots are thought about. Their keys stay held between these, so they walk smoothly regardless
local BRAIN_MS = 120
local lastBrainMS = 0

--[[
	Lua has no clock of its own here, and this runs once a frame rather than on a timer, so the demo keeps
	its own in milliseconds: every frame adds msPerTick, and a schedule (which is in real milliseconds)
	measures how long CALIBRATE_MS of them actually took and corrects it

	Without that the whole demo would run at the speed of the frame rate - shots cutting in two seconds on
	a fast machine and in half a minute on a slow one
]]
demoClockMS = 0
local msPerTick = 16
local ticksSinceCalibration = 0
local CALIBRATE_MS = 500

function demoCalibrate()
	schedule(CALIBRATE_MS, "demoCalibrate")

	if ticksSinceCalibration > 0 then
		--[[
			Eased toward the new measurement so one stuttering half second doesn't jolt the camera, and
			clamped: a half second in which the game managed two frames (loading something, or the window
			being dragged) would otherwise measure 250 ms a tick and throw the camera across the island on
			the next one
		]]
		local measured = CALIBRATE_MS / ticksSinceCalibration
		msPerTick = math.max(5, math.min(100, msPerTick * 0.5 + measured * 0.5))
	end
	ticksSinceCalibration = 0
end

--[[
	Every tick, which in single player is every frame the game draws, so the camera moves as smoothly as
	the picture does rather than in the steps a timer would give it
]]
function demoTick()
	--Not 0: the scheduler refuses that, and 1 ms is already less than a frame
	schedule(1, "demoTick")

	demoClockMS = demoClockMS + msPerTick
	ticksSinceCalibration = ticksSinceCalibration + 1

	if not demoViewer then
		return
	end

	--The bots, a few times a second
	if demoClockMS - lastBrainMS >= BRAIN_MS then
		lastBrainMS = demoClockMS
		for _, bot in ipairs(demoBots) do
			thinkBot(bot, demoClockMS)
		end
	end

	--[[
		The over-the-shoulder shot follows one player, and a body lying on the floor for the rest of the
		shot is a shot of nothing, so somebody else is picked the moment the one being followed dies. It's
		a cut rather than a swing: easing across the island to the next one would take the whole shot
	]]
	if demoStar ~= nil and demoStar.dead then
		pickStar()
		camSnap = true
	end

	driveJeep(msPerTick)

	--The shot, cut to the next one when its time is up
	local shot = DEMO_SHOTS[shotIndex]
	local elapsed = demoClockMS - shotStartedMS
	if elapsed >= shot.ms then
		shotIndex = shotIndex % #DEMO_SHOTS + 1
		shotStartedMS = demoClockMS
		shot = DEMO_SHOTS[shotIndex]
		elapsed = 0
		--A new player for the over-the-shoulder shot to follow every time round
		pickStar()
		--A cut lands where it lands, with nothing swinging into place after it
		camSnap = true
		--Only with logger/verbose on, which is how a shot that ends up inside the build gets found
		debug("Menu demo cut to shot " .. shotIndex)
	end

	turnStar(msPerTick)
	updateCrowdCenter(msPerTick)

	local t = math.min(elapsed / shot.ms, 1)
	local x, y, z, atX, atY, atZ = shot.camera(t)

	if camSnap then
		camSnap = false
		camX, camY, camZ = x, y, z
		aimX, aimY, aimZ = atX, atY, atZ
	else
		--Eased by time rather than by frames, so it settles the same way whatever the frame rate is
		local k = 1 - math.exp(-msPerTick / SMOOTH_MS)
		camX = camX + (x - camX) * k
		camY = camY + (y - camY) * k
		camZ = camZ + (z - camZ) * k
		aimX = aimX + (atX - aimX) * k
		aimY = aimY + (atY - aimY) * k
		aimZ = aimZ + (atZ - aimZ) * k
	end

	local dirX, dirY, dirZ = lookFrom(camX, camY, camZ, aimX, aimY, aimZ)
	demoViewer:staticCamera(camX, camY, camZ, dirX, dirY, dirZ)
end

--The client watching gets no player, only the camera
function demoJoin(client)
	demoViewer = client

	--They can't do any of it anyway with the demo's input turned off, but nothing should be offered either
	client:setJetsEnabled(false)
	client:setFlashlightEnabled(false)
	client:setFreeCameraEnabled(false)

	--The one at the wheel is dressed as whoever is looking at the menu, hat and all. Their game sends
	--their appearance as it connects, so this is the first moment there's anything to put on
	if demoDriver then
		client:applyAppearance(demoDriver)
	end

	shotIndex = 1
	shotStartedMS = demoClockMS

	return client
end
registerEventListener("ClientJoin", "demoJoin")

function demoLeave(client)
	if demoViewer == client then
		demoViewer = nil
	end
	return client
end
registerEventListener("ClientLeave", "demoLeave")

schedule(1, "demoTick")
schedule(CALIBRATE_MS, "demoCalibrate")

info("Menu demo ready: " .. getNumBricks() .. " bricks (" .. bricksRefused .. " refused), " .. #demoBots .. " bots")

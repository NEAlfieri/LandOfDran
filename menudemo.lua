--[[
	menudemo.lua - the game that plays behind the main menu.

	The client hosts this on a server of its own (MENU_DEMO_PORT) the moment it reaches the menu and joins
	it quietly: no HUD, no mouse capture, no keys, and no player of its own, see LoopClient::startMenuDemo.
	So everything here is an ordinary server script. Nobody is playing, which means nothing in it can be
	driven by a client: the players are bots walked with dynamic:setBotInput, the jeep is steered by having
	its velocity pushed the way it should go, and the camera is a client:staticCamera re-aimed every tick.

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
	local edge = ISLAND - 8

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

	--Stepped ramps either side, since a stack of plates reads as a slope well enough at this size
	for step = 1, PLAZA_HEIGHT do
		local depth = 2
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

--Blocks to run between and shoot from behind, all of them clear of the plaza and its ramps
local function buildCover()
	--All well clear of the plaza's ramps, which reach out to 23 studs either side of the middle along z
	local spots = {
		{-30, -16}, {-16, -30}, {18, -26}, {28, -12},
		{26, 18}, {12, 28}, {-20, 26}, {-30, 12},
		{-24, -2}, {20, 2}, {-26, -26}, {24, 24},
	}
	for i, spot in ipairs(spots) do
		local w = (i % 3 == 0) and 6 or 4
		local height = (i % 4 == 0) and 12 or 8
		brick(spot[1], FLOOR_PLATES, spot[2], w, height, 4, COVER_COLORS[(i - 1) % #COVER_COLORS + 1])
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

--Starts just after sunrise and runs a whole day in about eight minutes, so the menu is never the same
--picture twice: morning, noon, a long gold evening, then the towers' lights against a dark sea
setTimeOfDay(0.30)
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

--Studs a bot will shoot from, and how far apart shots are
local BOT_RANGE = 70
local BOT_FIRE_MS = 420

--Roughly chest height on a brickhead, where its shots come from
local BOT_MUZZLE_HEIGHT = 3.2

local FACES = {"BrownSmiley.png", "smileyBlonde.png", "ChefSmiley.png", "Male07Smiley.png", "KleinerSmiley.png", "Orc.png"}
local HATS = {"cap.txt", "top_hat.txt", "fedora.txt", "jester_cap.txt"}

local function randomFrom(list)
	return list[math.random(#list)]
end

--A player-looking bot of one team, standing where it's put
local function makeBot(team, x, z)
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
	bot:setMeshColor("Left_Leg", 0.15, 0.15, 0.18, 1)
	bot:setMeshColor("Right_Foot", 0.1, 0.1, 0.1, 1)
	bot:setMeshColor("Left_Foot", 0.1, 0.1, 0.1, 1)
	bot:setMeshDecal("Face1", randomFrom(FACES))

	--Hats, which the demo's own gunfire can knock off again, see Hats.lua
	bot:setPart("hat", randomFrom(HATS), 0, 0, 0, 0, 1)

	bot.team = team
	bot.nextFireMS = 0
	bot.waypoint = nil
	bot.jetUntilMS = 0
	bot.homeX, bot.homeZ = x, z

	table.insert(demoBots, bot)
	return bot
end

--Spread along opposite sides so they don't spawn inside each other and meet somewhere in the middle
local function spawnTeams()
	for i = 1, 4 do
		makeBot("red", -30 + i * 6, -30)
		makeBot("blue", 30 - i * 6, 30)
	end
end

spawnTeams()

--The nearest bot of the other team with a clear shot, or nil
local function enemyInSight(bot)
	local x, y, z = bot:getPosition()
	local fromY = y + BOT_MUZZLE_HEIGHT

	local best, bestDistance = nil, BOT_RANGE
	for _, other in ipairs(demoBots) do
		if other.team ~= bot.team then
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

--A round from the demo's own guns. Tagged "gun" so the weapon add-on's own ProjectileHit listener gives
--it the real thing: the dust and flash where it lands, the crack of it hitting, and a hole in the brick
local function botFire(bot, target)
	local x, y, z = bot:getPosition()
	local tx, ty, tz = target:getPosition()

	local fromY = y + BOT_MUZZLE_HEIGHT
	local toY = ty + BOT_MUZZLE_HEIGHT - 0.5

	local dx, dy, dz = tx - x, toY - fromY, tz - z
	local length = math.sqrt(dx * dx + dy * dy + dz * dz)
	if length < 0.001 then
		return
	end

	--Nobody in the demo is a crack shot, and near misses cracking off the bricks are half the point
	local spread = 0.035
	dx = dx / length + (math.random() - 0.5) * spread
	dy = dy / length + (math.random() - 0.5) * spread
	dz = dz / length + (math.random() - 0.5) * spread

	local gun = Weapons["gun"]
	local speed = gun and gun.projectileSpeed or 180

	--Out in front of them rather than out of their middle, so the round doesn't start inside their own box
	local muzzleX = x + dx * 1.6
	local muzzleY = fromY + dy * 1.6
	local muzzleZ = z + dz * 1.6

	local round = addProjectile(gunBullet, muzzleX, muzzleY, muzzleZ, dx * speed, dy * speed, dz * speed, "gun", bot)
	if round then
		round:setGravity(0, 0, 0)
	end

	local flash = addEmitter("GunFlashEmitter", muzzleX, muzzleY, muzzleZ)
	if flash then
		schedule(50, "weaponRemoveEmitter", flash)
	end

	playSound("GunShot1", muzzleX, muzzleY, muzzleZ)
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

	--Shooting, they face whoever they're shooting at instead of where they're going
	if target then
		local tx, ty, tz = target:getPosition()
		local ax, az = tx - x, tz - z
		local aim = math.sqrt(ax * ax + az * az)
		if aim > 0.001 then
			dx, dz = ax / aim, az / aim
		end

		if nowMS >= nextRocketMS then
			--Whoever happens to have a clear shot when a rocket is due fires it
			if botRocket(bot, target) then
				nextRocketMS = nowMS + ROCKET_EVERY_MS + math.random(0, 4000)
				bot.nextFireMS = nowMS + 1200
			end
		elseif nowMS >= bot.nextFireMS then
			botFire(bot, target)
			bot.nextFireMS = nowMS + BOT_FIRE_MS + math.random(0, 250)
		end
	end

	--Every so often somebody jets up over the wall, which is the flashiest thing a player can do
	if nowMS > bot.jetUntilMS + 6000 and math.random() < 0.02 then
		bot.jetUntilMS = nowMS + 1400
	end
	local jetting = nowMS < bot.jetUntilMS

	--A bot's look direction is where its shots and its head go, and walking uses only the flat part of it
	bot:setBotInput(dx, jetting and 0.35 or 0, dz, true, false, false, false, false, jetting, false)
end

--------------------------------------------------------------------------------------------------------
-- The jeep
--------------------------------------------------------------------------------------------------------

--Its lap of the island, in studs, just inside the wall
local JEEP_LAP = {
	{-34, -34}, {34, -34}, {34, 34}, {-34, 34},
}

demoJeep = nil
local jeepLeg = 1

local function startJeep()
	--spawnJeep puts one down and gives it wheels, lights and a horn, all from the add-on
	demoJeep = spawnJeep(JEEP_LAP[1][1], FLOOR_TOP + 2, JEEP_LAP[1][2])
	if demoJeep then
		--A new vehicle has no headlight until something gives it one, and this one drives through the night
		demoJeep:setHeadlight({})
		demoJeep:setHeadlightOn(true)
	end
end

startJeep()

--Nobody is driving, so it's pushed round its lap: velocity toward the next corner, turned to face the way it goes
local function driveJeep(deltaMS)
	if not demoJeep then
		return
	end

	local x, y, z = demoJeep:getPosition()

	--Rolled off the island or ended up on its roof: put it back on its lap
	if y < WATER_LEVEL - 2 or math.abs(x) > ISLAND + 10 or math.abs(z) > ISLAND + 10 then
		jeepLeg = 1
		demoJeep:setPosition(JEEP_LAP[1][1], FLOOR_TOP + 3, JEEP_LAP[1][2])
		demoJeep:setRotation(1, 0, 0, 0)
		demoJeep:setVelocity(0, 0, 0)
		demoJeep:setAngularVelocity(0, 0, 0)
		return
	end

	local corner = JEEP_LAP[jeepLeg]
	local dx, dz = corner[1] - x, corner[2] - z
	local distance = math.sqrt(dx * dx + dz * dz)

	if distance < 8 then
		jeepLeg = jeepLeg % #JEEP_LAP + 1
		return
	end

	dx, dz = dx / distance, dz / distance

	--Its own falling is left alone, only the way it drives is pushed
	local _, vy, _ = demoJeep:getVelocity()
	local speed = 26
	demoJeep:setVelocity(dx * speed, vy, dz * speed)

	--Turned to face where it's going. The model's forward is -Z, so the angle is measured from that
	local yaw = math.atan(dx, -dz)
	demoJeep:setRotation(math.cos(yaw / 2), 0, math.sin(yaw / 2), 0)
	demoJeep:setAngularVelocity(0, 0, 0)
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
local function pushClear(x, y, z, minimum)
	for _, bot in ipairs(demoBots) do
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
	return x, y, z
end

--Wherever the action is right now: the middle of everyone still on their feet, or the island's middle
local function crowdCenter()
	local x, y, z, count = 0, 0, 0, 0
	for _, bot in ipairs(demoBots) do
		local bx, by, bz = bot:getPosition()
		x, y, z, count = x + bx, y + by, z + bz, count + 1
	end
	if count == 0 then
		return 0, FLOOR_TOP + 3, 0
	end
	return x / count, y / count + 3, z / count
end

--The two bots nearest each other from opposite teams, which is where the shooting is
local function firefightCenter()
	local bestX, bestY, bestZ, bestDistance = nil, nil, nil, 9999
	for _, a in ipairs(demoBots) do
		if a.team == "red" then
			for _, b in ipairs(demoBots) do
				if b.team == "blue" then
					local ax, ay, az = a:getPosition()
					local bx, by, bz = b:getPosition()
					local distance = math.sqrt((ax - bx) ^ 2 + (az - bz) ^ 2)
					if distance < bestDistance then
						bestDistance = distance
						bestX, bestY, bestZ = (ax + bx) / 2, (ay + by) / 2 + 3, (az + bz) / 2
					end
				end
			end
		end
	end

	if not bestX then
		return crowdCenter()
	end
	return bestX, bestY, bestZ
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
	The shots. Each runs for ms and is asked, with t from 0 at its start to 1 at its end, where the camera
	is and what it looks at. They're cut between rather than blended, the way a trailer cuts
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
			local dirX, dirY, dirZ = lookFrom(x, height, z, 0, PLAZA_TOP + 4, 0)
			return x, height, z, dirX, dirY, dirZ
		end
	},

	--[[
		Down among them, drifting round the fighting. The camera swings round the island's middle rather
		than round the fight itself, which keeps it in the open ring between the plaza and the wall wherever
		the fight has wandered off to
	]]
	{
		ms = 11000,
		camera = function(t)
			local atX, atY, atZ = firefightCenter()
			--Out past the fight from the middle of the island, at a fixed distance so it's never in a wall
			--Inside the ring of pillars holding the catwalks up, which stand 32 studs out
			local toward = math.atan(atZ, atX)
			local angle = toward + mix(-0.45, 0.45, ease(t))
			local radius = mix(27, 22, ease(t))
			local x = math.cos(angle) * radius
			local z = math.sin(angle) * radius
			--Far enough off everyone that a whole player fits in the frame rather than filling it
			local y
			x, y, z = pushClear(x, BAND_LOW, z, 15)
			local dirX, dirY, dirZ = lookFrom(x, y, z, atX, atY, atZ)
			return x, y, z, dirX, dirY, dirZ
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
			local dirX, dirY, dirZ = lookFrom(x, y, z, jx, jy + 1.5, jz)
			return x, y, z, dirX, dirY, dirZ
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
			local dirX, dirY, dirZ = lookFrom(x, y, z, 0, PLAZA_TOP + mix(10, 5, pull), 0)
			return x, y, z, dirX, dirY, dirZ
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
			local dirX, dirY, dirZ = lookFrom(x, height, z, atX, atY, atZ)
			return x, height, z, dirX, dirY, dirZ
		end
	},
}

local shotIndex = 1
local shotStartedMS = 0

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
		--Eased toward the new measurement so one stuttering half second doesn't jolt the camera
		local measured = CALIBRATE_MS / ticksSinceCalibration
		msPerTick = msPerTick * 0.5 + measured * 0.5
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

	driveJeep(msPerTick)

	--The shot, cut to the next one when its time is up
	local shot = DEMO_SHOTS[shotIndex]
	local elapsed = demoClockMS - shotStartedMS
	if elapsed >= shot.ms then
		shotIndex = shotIndex % #DEMO_SHOTS + 1
		shotStartedMS = demoClockMS
		shot = DEMO_SHOTS[shotIndex]
		elapsed = 0
		--Only with logger/verbose on, which is how a shot that ends up inside the build gets found
		debug("Menu demo cut to shot " .. shotIndex)
	end

	local t = math.min(elapsed / shot.ms, 1)
	local x, y, z, dirX, dirY, dirZ = shot.camera(t)
	demoViewer:staticCamera(x, y, z, dirX, dirY, dirZ)
end

--The client watching gets no player, only the camera
function demoJoin(client)
	demoViewer = client

	--They can't do any of it anyway with the demo's input turned off, but nothing should be offered either
	client:setJetsEnabled(false)
	client:setFlashlightEnabled(false)
	client:setFreeCameraEnabled(false)

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

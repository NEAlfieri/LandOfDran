--[[
	System_Players

	Everyone who joins gets a player: the brickhead model, where it drops in, and what happens as
	they join and leave. Spawn Point bricks are here too, since they're where a player is put.

	spawnPlayer(client) makes a client a new player and puts them in it. It's called as they join,
	by System_Damage each time they respawn, and by anything else that wants to put someone back in
	the world, so a game mode can wrap it (FallingTiles does) to spawn people somewhere of its own.

	Health isn't here: a player is given some by System_Damage if that add-on is loaded, and without
	it nobody can be hurt at all.
]]

--The player
brickhead = newDynamicType("brickhead","Assets/brickhead/brickhead.txt",0.02,0.02,0.02)
addAnimation(brickhead,"walk",0,30,0.04,200,400) --For now it just uses the first added animation as the walk cycle
addAnimation(brickhead,"grab",56,65,0.03,0,0) --Played on every left click, over the walk cycle
addAnimation(brickhead,"sit",70,71,0.03,250,250) --One held pose, looped on anyone riding in a model vehicle like the jeep: the fades sit them down and stand them up

--Someone joining, leaving, and spawning, and the body a player leaves behind disappearing (System_Damage's
--corpses, and a sprayed body part coming clean in System_Inventory)
newSoundType("PlayerConnect","Assets/sound/playerConnect.wav")
newSoundType("PlayerLeave","Assets/sound/playerLeave.wav")
newSoundType("Spawn","Assets/sound/spawn.wav")
newSoundType("BodyRemove","Assets/sound/bodyRemove.wav")

--Where players drop in while there's no Spawn Point brick to spawn in
SPAWN_X, SPAWN_Y, SPAWN_Z = 0, 50, 0

--The special brick type players spawn in, which only admins can plant
SPAWN_BRICK_TYPE = "Spawn Point"

--Net IDs of the Spawn Point bricks. Nothing tells Lua a brick is gone, so ones that are get dropped as pickSpawnPosition runs into them
spawnBricks = {}

--Looks through every brick for Spawn Points, since loading a save doesn't say what it added, and takes out any a player without admin owns
function findSpawnBricks()
	local admins = {}
	for i = 0, getNumClients() - 1 do
		local client = getClientIdx(i)
		admins[client.id] = client:isAdmin()
	end

	spawnBricks = {}
	for i = getNumBricks() - 1, 0, -1 do
		local brick = getBrickIdx(i)
		if brick:getTypeName() == SPAWN_BRICK_TYPE then
			--A vehicle save loaded as bricks is its loader's, whatever is in it. Saves and Lua own theirs as -1
			if admins[brick:getOwner()] == false then
				brick:remove(true)
			else
				table.insert(spawnBricks, brick.id)
			end
		end
	end
end

--A player's feet go on the plate at the bottom of a random Spawn Point, the only part of one that collides
function pickSpawnPosition()
	while #spawnBricks > 0 do
		local index = math.random(#spawnBricks)
		local brick = getBrickId(spawnBricks[index])

		--A removed brick's ID can go to a new one
		if brick and brick:getTypeName() == SPAWN_BRICK_TYPE then
			local x, y, z = brick:getPosition()
			local width, height, length = brick:getDimensions()
			if brick:getAngleID() % 2 == 1 then
				width, length = length, width
			end
			return x + width / 2, (y + 1) * 0.4 + 0.1, z + length / 2
		end

		spawnBricks[index] = spawnBricks[#spawnBricks]
		table.remove(spawnBricks)
	end

	return SPAWN_X, SPAWN_Y, SPAWN_Z
end

--Registered before plantSound, which makes no sound for the nil brick a taken back Spawn Point leaves
function plantSpawnBrick(client, brick)
	if brick:getTypeName() ~= SPAWN_BRICK_TYPE then
		return client, brick
	end

	if not client:isAdmin() then
		brick:remove()
		client:centerPrint("Only admins can plant spawn points", 2000, 1, 0.4, 0.4)
		return client, nil
	end

	table.insert(spawnBricks, brick.id)
	return client, brick
end
registerEventListener("ClientPlantBrick","plantSpawnBrick")

--Both fire before anything is loaded
function findSpawnBricksAfterLoad(client, ...)
	schedule(100, "findSpawnBricks")
	return client, ...
end
registerEventListener("ClientLoadBricks","findSpawnBricksAfterLoad")
registerEventListener("ClientLoadVehicle","findSpawnBricksAfterLoad")

--Makes a client a player and puts them in it, as they join and each time they respawn after dying, see System_Damage
function spawnPlayer(client)

	--Create a player for the client
	local dynamic = createDynamic(brickhead,pickSpawnPosition())
	
	--Dynamic cannot tip over
	dynamic:setAngularFactor(0,0,0);
	
	--Client is responcible for physics simulation of this object
	client:giveControl(dynamic)
	
	--Max camera distance: 20
	client:bindCamera(dynamic,true,20)
	
	--This function will be replaced with something better, for now the only way to un-control the object is to delete it
	client:setDefaultController(dynamic)

	--The colors, face, and shirt they picked in their appearance editor
	client:applyAppearance(dynamic)

	--Their name floats over their head for everyone else
	dynamic:setNameTag(client:getName(),1,1,1)

	--Full health, and able to be damaged, if System_Damage is loaded
	if giveHealth ~= nil then
		giveHealth(dynamic)
	end

	dynamic:playSound("Spawn")

	return dynamic
end

--Client confirms finishes loading SimObject types
function join(client)	

	spawnPlayer(client)

	playSound("PlayerConnect")

	return client
end
registerEventListener("ClientJoin","join")

--Called right after client disconnects but client object is not deleted
function leave(client)

	--Destroy the client's player(s)
	for i = 0, client:getNumControlled() - 1, 1 do
		client:getControlledIdx(i):destroy()
	end

	playSound("PlayerLeave")

	return client
end
registerEventListener("ClientLeave","leave")

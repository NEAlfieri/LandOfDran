--[[
	serverstart.lua

	What this particular server sets up, run once the add-ons are in. Everything a game is actually
	made of now lives in add-ons of its own, in the Add-ons folder, and Add-ons/list.txt says which of
	them this server loads, see the add-ons section of LuaAPI.md:

		System_Players     the player model, spawning, and Spawn Point bricks
		System_Inventory   the tools everyone carries, and picking things up
		System_Damage      health, dying, and respawning
		System_Admin       slash commands, from /clearbricks to /kick
		System_Sounds      the sounds the base game plays
		System_Emitters    the particle and emitter types
		...and the weapons, vehicles and tools ported from Blockland

	So what's left in here is whatever this run wants on top of them: the environment, a game mode,
	the demo bricks below, and the testing helpers at the bottom.
]]

--For now dynamic types are used for creating both static and dynamic physics/mesh objects

--Different floor tile types
smallPlate = newDynamicType("small","Assets/cube/cube.txt",0.01,0.01,0.01)
mediumPlate = newDynamicType("medium","Assets/cube/cube.txt",0.02,0.01,0.02)
largePlate = newDynamicType("large","Assets/cube/cube.txt",0.04,0.01,0.04)
centerPlate = newDynamicType("center","Assets/cube/cube.txt",0.1,0.01,0.1)

button = newDynamicType("button","Assets/button/button.txt",1,1,1)

setSkybox("Assets/skyboxes/bluecloud","Assets/skyboxes/space")

--Every new vehicle, sliced or loaded, can have its bricks blown off by radiusImpulse
function makeVehicleDestructable(vehicle, builder)
	vehicle:setDestructable(true)
	return vehicle, builder
end
registerEventListener("VehicleCreated", "makeVehicleDestructable")

--Different arrays of kinds of plates that can be made to dissapear with their own button
larges = {}
mediums = {}
smalls = {}
lefts = {}
rights = {}
whites = {}
blacks = {}
reds = {}
greens = {}
blues = {}

buttonColor = 1
function jumpButtonColor()
	buttonColor = buttonColor + 1
	if buttonColor > 3 then
		buttonColor = 1
	end
	
	if buttonColor == 1 then
		jumpButton:setMeshColor("Button",1,0,0,1)
	elseif buttonColor == 2 then
		jumpButton:setMeshColor("Button",0,1,0,1)
	else
		jumpButton:setMeshColor("Button",0,0,1,1)
	end
	
	buttonColorSch = schedule(500,"jumpButtonColor")
end

doingAHide = false

function hide(obj)
	obj:setHidden(true)
end

function show(obj)
	obj:setHidden(false)
end

function reappear(obj)
	obj:setColliding(true)
	obj:setHidden(false)
	doingAHide = false
end

function dissapear(obj)
	obj:setColliding(false)
	obj:setHidden(true)
end

function selectPanel(obj)
	if doingAHide == true then
		return
	end

	hide(obj)
	schedule(500,"show",obj)
	schedule(1000,"hide",obj)
	schedule(1500,"show",obj)
	schedule(2000,"dissapear",obj)
	schedule(8000,"reappear",obj)
end

function gravityOn(d)
	for i=0,getNumDynamics()-1,1 do
		getDynamicIdx(i):setGravity(0,-50,0)
	end
end

--From NetTypes/NetType.h's SimObjectType enum
local STATIC_TYPE_ID = 2

function click(client,posX,posY,posZ,dirX,dirY,dirZ,mask)

	--Every button matched below belongs to the falling tiles, which may never have been created
	if not fallingTilesCreated then
		return client,posX,posY,posZ,dirX,dirY,dirZ,mask
	end

	ignore = nil
	if client:getNumControlled() > 0 then
		ignore = client:getControlledIdx(0)
	end
	result = raycast(posX,posY,posZ,posX + dirX * 30,posY + dirY * 30,posZ + dirZ * 30,ignore)

	if result == nil then
		return client,posX,posY,posZ,dirX,dirY,dirZ,mask
	end

	--All of the buttons matched by id below are statics, and a static's id is only unique among other statics -
	--a dynamic can easily share the same id, so without this a click on some unrelated dynamic could
	--accidentally match one of these buttons
	if result.type ~= STATIC_TYPE_ID then
		return client,posX,posY,posZ,dirX,dirY,dirZ,mask
	end

	if result.id == jumpButton.id then
		ignore:setPosition(50,50,0)
	end
	
	if result.id == redButton.id then
		for k,v in pairs(reds) do
			selectPanel(v)
		end
		doingAHide = true
	end
	
	if result.id == greenButton.id then
		for k,v in pairs(greens) do
			selectPanel(v)
		end
		doingAHide = true
	end
	
	if result.id == blueButton.id then
		for k,v in pairs(blues) do
			selectPanel(v)
		end
		doingAHide = true
	end
	
	if result.id == gravityButton.id then
		for i=0,getNumDynamics()-1,1 do
			getDynamicIdx(i):setGravity(0,-5,0)
		end
		schedule(8000,"gravityOn")
	end

	return client,posX,posY,posZ,dirX,dirY,dirZ,mask
end
registerEventListener("ClientClick","click")

--A demo of statics: rows of colored plates that dissapear when their button is clicked
--Nothing creates these on its own, call createFallingTiles() to put them in the world
--z > 0 left
--z < 0 right
fallingTilesCreated = false
function createFallingTiles()
	if fallingTilesCreated then
		return
	end
	fallingTilesCreated = true
	
	gravityButton = createStatic(button,40,42,5)
	gravityButton:setMeshColor("Button",1,1,0,1)
	
	redButton = createStatic(button,45,42,5)
	redButton:setMeshColor("Button",1,0,0,1)
	
	blueButton = createStatic(button,50,42,5)
	blueButton:setMeshColor("Button",0,0,1,1)
	
	greenButton = createStatic(button,55,42,5)
	greenButton:setMeshColor("Button",0,1,0,1)
	
	jumpButton = createStatic(button,0,1,0)
	jumpButtonColor()
	
	createStatic(centerPlate,50,40,0)

	last = createStatic(centerPlate,0,30,0)
	last:setMeshColor("Cube",1,1,1,1)
	table.insert(larges,last)
	table.insert(whites,last)
	
	last = createStatic(largePlate,0,30,15)
	last:setMeshColor("Cube",1,0,0,1)
	table.insert(larges,last)
	table.insert(reds,last)
	table.insert(lefts,last)
	
	last = createStatic(largePlate,0,30,-15)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(larges,last)
	table.insert(greens,last)
	table.insert(rights,last)
	
	last = createStatic(largePlate,15,30,0)
	last:setMeshColor("Cube",0,0,0,1)
	table.insert(larges,last)
	table.insert(blacks,last)
	
	last = createStatic(largePlate,-15,30,0)
	last:setMeshColor("Cube",0,0,0,1)
	table.insert(larges,last)
	table.insert(blacks,last)
	
	last = createStatic(mediumPlate,10,30,15)
	last:setMeshColor("Cube",1,1,1,1)
	table.insert(whites,last)
	table.insert(mediums,last)
	table.insert(lefts,last)
	
	last = createStatic(mediumPlate,-10,30,15)
	last:setMeshColor("Cube",0,0,0,1)
	table.insert(blacks,last)
	table.insert(mediums,last)
	table.insert(lefts,last)
	
	last = createStatic(mediumPlate,10,30,-15)
	last:setMeshColor("Cube",1,0,0,1)
	table.insert(reds,last)
	table.insert(mediums,last)
	table.insert(rights,last)
	
	last = createStatic(mediumPlate,-10,30,-15)
	last:setMeshColor("Cube",0,0,1,1)
	table.insert(blues,last)
	table.insert(mediums,last)
	table.insert(rights,last)
	
	last = createStatic(mediumPlate,15,30,10)
	last:setMeshColor("Cube",0,0,1,1)
	table.insert(blues,last)
	table.insert(mediums,last)
	table.insert(lefts,last)
	
	last = createStatic(mediumPlate,15,30,-10)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(greens,last)
	table.insert(mediums,last)
	table.insert(rights,last)
	
	last = createStatic(mediumPlate,-15,30,10)
	last:setMeshColor("Cube",1,0,0,1)
	table.insert(reds,last)
	table.insert(mediums,last)
	table.insert(lefts,last)
	
	last = createStatic(mediumPlate,-15,30,-10)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(greens,last)
	table.insert(mediums,last)
	table.insert(rights,last)
	
	last = createStatic(smallPlate,-25,30,0)
	last:setMeshColor("Cube",1,1,1,1)
	table.insert(smalls,last)
	table.insert(whites,last)
	
	last = createStatic(smallPlate,-22,30,15)
	last:setMeshColor("Cube",0,0,0,1)
	table.insert(smalls,last)
	table.insert(blacks,last)
	table.insert(lefts,last)
	
	last = createStatic(smallPlate,-22,30,-15)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(smalls,last)
	table.insert(greens,last)
	table.insert(rights,last)
	
	last = createStatic(smallPlate,25,30,0)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(smalls,last)
	table.insert(greens,last)
	
	last = createStatic(smallPlate,22,30,15)
	last:setMeshColor("Cube",1,0,0,1)
	table.insert(smalls,last)
	table.insert(reds,last)
	table.insert(lefts,last)
	
	last = createStatic(smallPlate,22,30,-15)
	last:setMeshColor("Cube",0,0,1,1)
	table.insert(smalls,last)
	table.insert(blues,last)
	table.insert(rights,last)
	
	last = createStatic(smallPlate,0,30,25)
	last:setMeshColor("Cube",0,1,0,1)
	table.insert(smalls,last)
	table.insert(greens,last)
	table.insert(lefts,last)
	
	last = createStatic(smallPlate,0,30,-25)
	last:setMeshColor("Cube",0,0,0,1)
	table.insert(smalls,last)
	table.insert(blacks,last)
	table.insert(rights,last)
end

--The Falling Tiles game mode and its /fallingTiles admin command, after System_Admin and System_Players
dofile("FallingTiles.lua")

--For easy testing
function gc()
	return getClientIdx(0)
end

function gp(client)
	return client:getControlledIdx(0)
end

function me()
	return gp(gc())
end

--Debug functions for testing physics

function resetCubePositions()
	for i=0, getNumDynamics()-1, 1 do
		d = getDynamicIdx(i)
		if d:getNumControllers() == 0 and not d:isItem() then
			d:setPosition(0,50,0)
		end
	end
end

function spawnNewCubes(numCubes, spread, rest)
	numCubes = numCubes or 20
	spread = spread or 0

	--Iterate backwards since destroying shifts later indices down
	for i=getNumDynamics()-1, 0, -1 do
		d = getDynamicIdx(i)
		if d:getNumControllers() == 0 and not d:isItem() then
			d:destroy()
		end
	end

	for i=1, numCubes, 1 do
		local x = 0
		local z = 0
		if spread > 0 then
			x = (math.random() - 0.5) * spread
			z = (math.random() - 0.5) * spread
		end
		d = createDynamic(getDynamicType("small"),x,50,z)
		d:setRestitution(rest)
	end
end

function lightTest()
	nl = getNumLights()
	for i=1, nl, 1 do
		getLightIdx(0):destroy()
	end
	
	for i=0, 20, 1 do
		createLight(20, 25, i*5, math.random(), math.random(), math.random(), 300, 0.15, 1)
	end
end

--Every emitter type from System_Emitters in two rows: ones that keep going, and one-off bursts every 1.5 seconds. Run it again to remove them
emitterTestOn = false
function emitterTest()
	for i=getNumEmitters()-1, 0, -1 do
		getEmitterIdx(i):destroy()
	end

	emitterTestOn = not emitterTestOn
	if not emitterTestOn then
		return
	end

	local continuous = {"fountainEmitter", "CameraEmitter", "playerJetEmitter", "shellTrailEmitter"}
	for i, name in ipairs(continuous) do
		addEmitter(name, 20 + i * 8, 5, -20)
	end

	emitterBursts()
end

function emitterBursts()
	if not emitterTestOn then
		return
	end

	local bursts = {"playerBubbleEmitter", "wrenchSparkEmitter", "hammerSparkEmitter", "hammerExplosionEmitter", "wrenchExplosionEmitter", "gunSmokeEmitter", "ouchEmitter"}
	for i, name in ipairs(bursts) do
		addEmitter(name, 20 + i * 8, 5, -35)
	end

	schedule(1500, "emitterBursts")
end

function darkMode()
	setTimeScale(0)
	setTimeOfDay(0)
	setSunColor("night" , 0.17, 0.2, 0.33)
	setAmbientColor("night", 0.003, 0.006, 0.01)
	setSkybox("Assets/skyboxes/bluecloud","Assets/skyboxes/space")
end

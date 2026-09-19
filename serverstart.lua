--For now dynamic types are used for creating both static and dynamic physics/mesh objects

--The player
brickhead = newDynamicType("brickhead","Assets/brickhead/brickhead.txt",0.02,0.02,0.02)
addAnimation(brickhead,"walk",0,30,0.04,200,400) --For now it just uses the first added animation as the walk cycle
addAnimation(brickhead,"grab",56,65,0.03,0,0) --Played on every left click, over the walk cycle
addAnimation(brickhead,"sit",70,71,0.03,250,250) --One held pose, looped on anyone riding in a model vehicle like the jeep: the fades sit them down and stand them up

--Different floor tile types
smallPlate = newDynamicType("small","Assets/cube/cube.txt",0.01,0.01,0.01)
mediumPlate = newDynamicType("medium","Assets/cube/cube.txt",0.02,0.01,0.02)
largePlate = newDynamicType("large","Assets/cube/cube.txt",0.04,0.01,0.04)
centerPlate = newDynamicType("center","Assets/cube/cube.txt",0.1,0.01,0.1)

button = newDynamicType("button","Assets/button/button.txt",1,1,1)

--Items players carry in their inventory, see Inventory.lua
--setItemHand's grip is the point on the model held in the hand, then the model is turned by pitch, yaw, and roll in degrees
--A negative pitch leans the top of the model forward
hammerItem = newItemType("hammer","Assets/tools/hammer.txt",0.02,0.02,0.02,"Hammer","Assets/tools/icons/hammerIcon.png")
setItemHand(hammerItem,0,-0.7,0,-20,0,0)
wrenchItem = newItemType("wrench","Assets/tools/wrench.txt",0.02,0.02,0.02,"Wrench","Assets/tools/icons/wrenchIcon.png")
setItemHand(wrenchItem,0,-1,0,-20,0,0)
paintCanItem = newItemType("paintCan","Assets/tools/spraycan.txt",0.02,0.02,0.02,"Paint Can","Assets/tools/icons/paintCanIcon.png")
setItemHand(paintCanItem,0,0,0,-10,0,0)
--Puts prints on printed bricks, see Inventory.lua. Its model has no textures of its own, so printGun.txt
--gives the whole thing one light grey metal material
printGunItem = newItemType("printGun","Assets/tools/printGun.txt",2,2,2,"Print Gun","")
setItemHand(printGunItem,0,-0.378,0,0,0,0)
--Fires launcherShell projectiles, see Inventory.lua. Its fire animation is frames 1 to 26 of the model
dranLauncherItem = newItemType("dranLauncher","Assets/dranlauncher/gun.txt",0.02,0.02,0.02,"Launcher","Assets/dranlauncher/icon.png")
setItemHand(dranLauncherItem,0,0.2,0.1,0,0,0)
addAnimation(dranLauncherItem,"fire",0,25,0.04,0,0)
launcherShell = newDynamicType("launcherShell","Assets/dranlauncher/shell.txt",0.01,0.01,0.01)

--The Gun add-on: the plain Blockland gun, its bullet and casing, and dropGun
dofile("Add-ons/Weapon_Gun/Weapon_Gun.lua")

--The Tier+Tactical Tier 1 add-on: its models, its weapons, and dropWeaponPackage
dofile("Add-ons/Weapon_Package_Tier1/Weapon_Package_Tier1.lua")

--The jeep add-on: its models and spawnJeep, a model vehicle that drives alongside the brick ones
dofile("Add-ons/Vehicle_Jeep/Vehicle_Jeep.lua")

--The Biplane add-on: Kaje's four seat biplane, a model vehicle that flies, see spawnBiplane
dofile("Add-ons/Vehicle_Biplane/Vehicle_Biplane.lua")

--The Stunt Plane add-on: the sharper of the two planes, with contrails off its wingtips, see spawnStuntPlane
dofile("Add-ons/Vehicle_Stunt_Plane/Vehicle_Stunt_Plane.lua")

--The Grapple Rope add-on: a hook that swings its holder from wherever it lands, and dropGrappleRope
dofile("Add-ons/Tool_GrappleRope/Tool_GrappleRope.lua")

--The Bow add-on: arrows that arc, stick where they land, and dropBow
dofile("Add-ons/Weapon_Bow/Weapon_Bow.lua")

--The Rocket Launcher add-on: rockets whose blast hurts and throws whoever is near, and dropRocketLauncher
dofile("Add-ons/Weapon_Rocket_Launcher/Weapon_Rocket_Launcher.lua")

--Sounds, with the old game's names and file names. Clients play ClickMove, ClickRotate, Jump, and BrickBreak on their own
--when the server has sounds by those names. A file that isn't in Assets/sound/ logs an error and is skipped
newSoundType("ClickMove","Assets/sound/clickMove.wav")
newSoundType("ClickRotate","Assets/sound/clickRotate.wav")
newSoundType("ClickPlant","Assets/sound/clickPlant.wav")
newSoundType("Jump","Assets/sound/jump.wav")
newSoundType("BrickBreak","Assets/sound/breakBrick.wav")
newSoundType("PlayerConnect","Assets/sound/playerConnect.wav")
newSoundType("PlayerLeave","Assets/sound/playerLeave.wav")
newSoundType("Admin","Assets/sound/admin.wav")
newSoundType("BrickClear","Assets/sound/brickClear.wav")
--The server plays these itself where dynamics fall into or jump out of the water
newSoundType("Splash","Assets/sound/splash1.wav")
newSoundType("ExitWater","Assets/sound/exitWater.wav")
--And these from a player whose flashlight turns on or off
newSoundType("LightOn","Assets/sound/lightOn.wav")
newSoundType("LightOff","Assets/sound/lightOff.wav")
--Inventory.lua plays these where the hammer and wrench hit, and loops SprayLoop from a spraying paint can
newSoundType("HammerHit","Assets/sound/hammerHit.WAV")
newSoundType("WrenchHit","Assets/sound/wrenchHit.wav")
newSoundType("WrenchMiss","Assets/sound/wrenchMiss.wav")
newSoundType("SprayLoop","Assets/sound/sprayLoop.wav")
--Games play SprayActivate themselves as their paint palette comes out, see LoopClient::handleInput
newSoundType("SprayActivate","Assets/sound/sprayActivate.wav")
newSoundType("BodyRemove","Assets/sound/bodyRemove.wav")
--And Pain from a player who was shot or caught in a radiusImpulse, see hurtPlayer
newSoundType("Pain","Assets/sound/pain.wav")
--Damage.lua plays Death from a player who dies, Spawn from one who spawns, and BodyRemove where their old body disappears
newSoundType("Death","Assets/sound/death.wav")
newSoundType("Spawn","Assets/sound/spawn.wav")
--And Launch from a firing launcher
newSoundType("Launch","Assets/sound/launch.wav")
--And PrintFire from a firing print gun
newSoundType("PrintFire","Assets/sound/printFire.wav")
--The horn a new vehicle honks with when its driver left clicks, until its wrench dialog picks another sound
newSoundType("Honk","Assets/sound/434878__mickthemicguy__car-honking.wav")
--Getting into or out of a vehicle, picking up or dropping an item, and an upload being accepted and then finished
newSoundType("PlayerMount","Assets/sound/playerMount.wav")
newSoundType("Beep","Assets/sound/beep.wav")
newSoundType("UploadStart","Assets/sound/uploadStart.wav")
newSoundType("ProcessComplete","Assets/sound/processComplete.wav")
--Music, which players can put on bricks by holding Insert and clicking one to open the wrench dialog
newSoundType("After School Special","Assets/music/After_School_Special.wav",true)
newSoundType("After School Special","Assets/music/analog.wav",true)
newSoundType("DJGriffen - Euphoria","Assets/music/djgriffinEuphoria.wav",true)
newSoundType("Drums","Assets/music/drums.wav",true)
newSoundType("Police Siren","Assets/music/policeSiren.wav",true)
newSoundType("Spring Birds","Assets/music/springBirds.wav",true)
newSoundType("Vehicle Hover","Assets/music/vehicleHover.wav",true)
newSoundType("Zero Day","Assets/music/zerodaywip.wav",true)
newSoundType("Ambient Deep","Assets/music/Ambient_Deep.ogg",true)
newSoundType("Bass 1","Assets/music/Bass_1.ogg",true)
newSoundType("Bass 2","Assets/music/Bass_2.ogg",true)
newSoundType("Bass 3","Assets/music/Bass_3.ogg",true)
newSoundType("Creepy","Assets/music/Creepy.ogg",true)
newSoundType("Distort","Assets/music/Distort.ogg",true)
newSoundType("Factory","Assets/music/Factory.ogg",true)
newSoundType("Icy","Assets/music/Icy.ogg",true)
newSoundType("Jungle","Assets/music/Jungle.ogg",true)
newSoundType("Paprika - Byakko no","Assets/music/Paprika_-_Byakko_no.ogg",true)
newSoundType("Peaceful","Assets/music/Peaceful.ogg",true)
newSoundType("Piano Bass","Assets/music/Piano_Bass.ogg",true)
newSoundType("Rock","Assets/music/Rock.ogg",true)
newSoundType("Stress","Assets/music/Stress_.ogg",true)
newSoundType("Vartan - Death","Assets/music/Vartan_-_Death.ogg",true)
newSoundType("Fire","Assets/music/fire.wav",true)
--newSoundType("Rain","Assets/music/dragon-studio-calming-rain-loop-398653.mp3",true)
--newSoundType("Rain2","Assets/music/dragon-studio-gentle-rain-01-437305.mp3",true)

--Particle and emitter types, including the splash the server makes where dynamics fall into the water
dofile("EmitterDefaults.lua")
--What lights and emitters on bricks in Blockland saves become, see addBlocklandLight and addBlocklandEmitter
dofile("BlocklandImports.lua")
--Starting tools, picking up and throwing items, and swinging the hammer and wrench
dofile("Inventory.lua")
--Health, dying, and respawning, after Inventory.lua so a click that respawns someone isn't also a click with their new tools
dofile("Damage.lua")
--Hats that a shot knocks off and anyone bare headed can click to put on, after the add-on weapons it listens to
dofile("Hats.lua")

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

--Makes a client a player and puts them in it, as they join and each time they respawn after dying, see Damage.lua
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

	--Full health, and able to be damaged
	giveHealth(dynamic)

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

--[[
	What being hurt looks and sounds like, which Damage.lua's damagePlayer shows on a player it takes health from: the
	old game's ouch particles and the Pain sound where it happened, and for whoever the player belongs to a red vignette
	that closes in from the edges of their screen and wobbles their picture, fading away over a second
	x, y, z is where they were hit, or nothing to put the particles at about chest height
]]
HURT_VIGNETTE_MS = 1000
--How hard the picture wobbles, 1 being about as much as being underwater
HURT_VIGNETTE_WAVE = 0.5

function hurtPlayer(player, x, y, z)
	if player == nil then
		return
	end

	if x == nil then
		x, y, z = player:getPosition()
		y = y + 3
	end

	addEmitter("ouchEmitter", x, y, z)
	player:playSound("Pain")

	if player:getNumControllers() > 0 then
		player:getControllerIdx(0):setVignette(1, 0, 0, 0.5, HURT_VIGNETTE_WAVE, HURT_VIGNETTE_MS)
	end
end

--Everyone nearby hears a brick get planted, from its center
function plantSound(client, brick)
	--plantSpawnBrick took it back
	if not brick then
		return client, brick
	end

	local x, y, z = brick:getPosition()
	local width, height, length = brick:getDimensions()
	if brick:getAngleID() % 2 == 1 then
		width, length = length, width
	end
	playSound("ClickPlant", x + width / 2, (y + height / 2) * 0.4, z + length / 2)
	return client, brick
end
registerEventListener("ClientPlantBrick","plantSound")

--Just the player who got the eval password right hears it
function adminLoginSound(client)
	client:playSound("Admin")
	return client
end
registerEventListener("ClientAdminLogin","adminLoginSound")

--Removes every brick planted by the client with that net ID, returns how many
function clearBricksOwnedBy(ownerID)
	local count = 0
	--Backwards, so removing a brick never moves one we haven't looked at yet
	for i = getNumBricks() - 1, 0, -1 do
		local brick = getBrickIdx(i)
		if brick:getOwner() == ownerID then
			brick:remove()
			count = count + 1
		end
	end
	return count
end

--Removes every vehicle the client with that net ID sliced, loaded, or was given by spawnJeep, returns how many
function clearVehiclesOwnedBy(ownerID)
	local count = 0
	for i = getNumVehicles() - 1, 0, -1 do
		local vehicle = getVehicleIdx(i)
		if vehicle:getBuilderID() == ownerID then
			vehicle:remove()
			count = count + 1
		end
	end
	return count
end

--Chat commands, the message arrives as "Name: text"
function chatCommands(client, message)
	local text = string.sub(message, string.len(client:getName()) + 3)

	if text == "/clearbricks" then
		local count = clearBricksOwnedBy(client:getID())
		if count == 0 then
			client:message("You don't have any bricks to clear.")
		else
			messageAll(client:getName() .. " cleared their " .. count .. (count == 1 and " brick." or " bricks."))
			playSound("BrickClear")
		end
		--Don't show the command itself in chat
		return client, ""
	end

	if text == "/clearvehicles" then
		local count = clearVehiclesOwnedBy(client:getID())
		if count == 0 then
			client:message("You don't have any vehicles to clear.")
		else
			messageAll(client:getName() .. " cleared their " .. count .. (count == 1 and " vehicle." or " vehicles."))
			playSound("BrickClear")
		end
		return client, ""
	end

	if text == "/sit" then
		toggleSitting(client)
		return client, ""
	end

	--The command word and its arguments, the server already lowercased the word
	local words = {}
	for word in string.gmatch(text, "%S+") do
		table.insert(words, word)
	end
	local command = words[1]
	if adminCommands[command] then
		if not client:isAdmin() then
			client:message("You need to be an admin to use " .. command .. ".")
		else
			--Arguments as typed, in case a name or number has spaces or capitals in it
			local argText = string.match(text, "^%S+%s+(.-)%s*$") or ""
			adminCommands[command](client, argText, words)
		end
		return client, ""
	end

	return client, message
end
registerEventListener("ClientChat","chatCommands")
--Listed in the chat window while typing a slash command, see registerChatSuggestion in LuaAPI.md
registerChatSuggestion("clearbricks", "/clearbricks - remove every brick you planted")
registerChatSuggestion("clearvehicles", "/clearvehicles - remove every vehicle you made")
registerChatSuggestion("sit", "/sit - sit down, or stand back up")

--Finds a connected client by name, ignoring case. An exact match wins, then the only one whose name starts with it.
--Returns the client, or nil and a message saying why not
function findClientByName(name)
	if name == "" then
		return nil, "Give a player's name."
	end
	local lowered = string.lower(name)
	local partial = nil
	local partialCount = 0
	for i = 0, getNumClients() - 1 do
		local other = getClientIdx(i)
		local otherName = string.lower(other:getName())
		if otherName == lowered then
			return other
		end
		if string.sub(otherName, 1, string.len(lowered)) == lowered then
			partial = other
			partialCount = partialCount + 1
		end
	end
	if partialCount == 1 then
		return partial
	elseif partialCount > 1 then
		return nil, "More than one player's name starts with " .. name .. "."
	end
	return nil, "No player named " .. name .. " is here."
end

--Where a client is, for /find and /fetch: their vehicle if they're in one, else their player
function clientPosition(client)
	local vehicle = client:getVehicle()
	if vehicle then
		return vehicle:getPosition()
	end
	if client:getNumControlled() > 0 then
		return client:getControlledIdx(0):getPosition()
	end
	return nil
end

--Puts a client's player next to a position, first getting them out of any vehicle. False if they have no player
function teleportClient(client, x, y, z)
	if client:getNumControlled() == 0 then
		return false
	end
	if client:getVehicle() then
		client:exitVehicle()
	end
	--A little up so they don't land inside whoever they're put next to
	client:getControlledIdx(0):setPosition(x, y + 3, z)
	return true
end

--Commands only admins (single player's host, or anyone who logged into the eval console) can use, by their lowercased name.
--Each gets the client, the arguments as one string, and the message split into words (the command first)
adminCommands = {}

adminCommands["/clearallvehicles"] = function(client)
	local count = getNumVehicles()
	if count == 0 then
		client:message("There are no vehicles to clear.")
		return
	end
	for i = count - 1, 0, -1 do
		getVehicleIdx(i):remove()
	end
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " vehicle." or " vehicles."))
	playSound("BrickClear")
end

adminCommands["/clearallbricks"] = function(client)
	local count = getNumBricks()
	if count == 0 then
		client:message("There are no bricks to clear.")
		return
	end
	clearAllBricks()
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " brick." or " bricks."))
	playSound("BrickClear")
end

--Items lying around on the ground, not ones anyone carries or the ones bricks offer
adminCommands["/clearallitems"] = function(client)
	local count = 0
	for i = getNumItems() - 1, 0, -1 do
		local item = getItemIdx(i)
		if not item:isHeld() and not item:isDisplay() then
			item:destroy()
			count = count + 1
		end
	end
	if count == 0 then
		client:message("There are no items on the ground to clear.")
		return
	end
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " item" or " items") .. " on the ground.")
	playSound("BrickClear")
end

adminCommands["/kick"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You can't kick yourself.")
		return
	end
	messageAll(target:getName() .. " was kicked by " .. client:getName() .. ".")
	target:kick()
end

adminCommands["/rain"] = function(client)
	if getRain() > 0 then
		setRain(0)
		messageAll(client:getName() .. " stopped the rain.")
	else
		setRain(1)
		messageAll(client:getName() .. " made it rain.")
	end
end

adminCommands["/settimescale"] = function(client, argText)
	local scale = tonumber(argText)
	if not scale then
		client:message("Usage: /setTimeScale <in-game seconds per real second>, 1 is normal and 0 freezes time.")
		return
	end
	setTimeScale(scale)
	messageAll(client:getName() .. " set the time scale to " .. scale .. ".")
end

adminCommands["/settimeofday"] = function(client, argText)
	local fraction = tonumber(argText)
	if not fraction then
		client:message("Usage: /setTimeOfDay <0-1>, 0 is midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset.")
		return
	end
	setTimeOfDay(fraction)
	messageAll(client:getName() .. " set the time of day to " .. fraction .. ".")
end

adminCommands["/setwaterlevel"] = function(client, argText)
	if argText == "" or string.lower(argText) == "off" or string.lower(argText) == "none" then
		setWaterLevel()
		messageAll(client:getName() .. " removed the water.")
		return
	end
	local level = tonumber(argText)
	if not level then
		client:message("Usage: /setWaterLevel <height>, or /setWaterLevel off to remove the water.")
		return
	end
	setWaterLevel(level)
	messageAll(client:getName() .. " set the water level to " .. level .. ".")
end

--Brings a player to the admin
adminCommands["/fetch"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You're already here.")
		return
	end
	--Their camera if it's off flying, so a fetched player lands where the admin is looking from
	local x, y, z
	if client:getFreeCamera() then
		x, y, z = client:getCameraPosition()
	else
		x, y, z = clientPosition(client)
	end
	if not x then
		client:message("You don't have a player to fetch them to.")
		return
	end
	if not teleportClient(target, x, y, z) then
		client:message(target:getName() .. " doesn't have a player to fetch.")
		return
	end
	target:message(client:getName() .. " fetched you.")
	client:message("Fetched " .. target:getName() .. ".")
end

--Takes the admin to a player
adminCommands["/find"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You found yourself.")
		return
	end
	local x, y, z = clientPosition(target)
	if not x then
		client:message(target:getName() .. " doesn't have a player to find.")
		return
	end
	if not teleportClient(client, x, y, z) then
		client:message("You don't have a player to go there with.")
		return
	end
	client:message("Found " .. target:getName() .. ".")
end

registerChatSuggestion("clearallvehicles", "/clearAllVehicles - admins: remove every vehicle")
registerChatSuggestion("clearallbricks", "/clearAllBricks - admins: remove every brick")
registerChatSuggestion("clearallitems", "/clearAllItems - admins: remove every item lying on the ground")
registerChatSuggestion("kick", "/kick <player> - admins: disconnect a player")
registerChatSuggestion("rain", "/rain - admins: start or stop the rain")
registerChatSuggestion("settimescale", "/setTimeScale <scale> - admins: 1 is normal, 0 freezes time")
registerChatSuggestion("settimeofday", "/setTimeOfDay <0-1> - admins: 0 midnight, 0.5 noon")
registerChatSuggestion("setwaterlevel", "/setWaterLevel <height|off> - admins: put water at that height")
registerChatSuggestion("fetch", "/fetch <player> - admins: bring a player to you")
registerChatSuggestion("find", "/find <player> - admins: go to a player")

--The Falling Tiles gamemode and its /fallingTiles admin command, after adminCommands, spawnPlayer, Inventory.lua and Damage.lua
dofile("FallingTiles.lua")

--Who's sitting down by /sit, by client ID. Riders of a model vehicle sit on their own, see LuaAPI.md's Model vehicles
sittingClients = {}

function toggleSitting(client)
	local player = client:getNumControlled() > 0 and client:getControlledIdx(0) or nil
	if not player then
		client:message("You don't have a player to sit down.")
		return
	end

	if client:getVehicle() then
		client:message("You're already sitting in a vehicle.")
		return
	end

	if sittingClients[client:getID()] then
		sittingClients[client:getID()] = nil
		player:stopAnimation("sit")
	else
		sittingClients[client:getID()] = true
		player:playAnimation("sit", true)
	end
end

--Forgotten when they leave, their player goes with them
function forgetSitting(client)
	sittingClients[client:getID()] = nil
	return client
end
registerEventListener("ClientLeave","forgetSitting")

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

--Every emitter type from EmitterDefaults.lua in two rows: ones that keep going, and one-off bursts every 1.5 seconds. Run it again to remove them
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

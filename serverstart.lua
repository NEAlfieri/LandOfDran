--For now dynamic types are used for creating both static and dynamic physics/mesh objects

--The player
brickhead = newDynamicType("brickhead","Assets/brickhead/brickhead.txt",0.02,0.02,0.02)
addAnimation(brickhead,"walk",0,30,0.04,200,400) --For now it just uses the first added animation as the walk cycle
addAnimation(brickhead,"grab",56,65,0.03,0,0) --Played on every left click, over the walk cycle

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
--Fires launcherShell projectiles, see Inventory.lua. Its fire animation is frames 1 to 26 of the model
dranLauncherItem = newItemType("dranLauncher","Assets/dranlauncher/gun.txt",0.02,0.02,0.02,"Launcher","Assets/dranlauncher/icon.png")
setItemHand(dranLauncherItem,0,0.2,0.1,0,0,0)
addAnimation(dranLauncherItem,"fire",0,25,0.04,0,0)
launcherShell = newDynamicType("launcherShell","Assets/dranlauncher/shell.txt",0.01,0.01,0.01)

--The Tier+Tactical Tier 1 add-on: its models, its weapons, and dropWeaponPackage
dofile("Add-ons/Weapon_Package_Tier1/Weapon_Package_Tier1.lua")

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
--And Launch from a firing launcher
newSoundType("Launch","Assets/sound/launch.wav")
--Drivers left click to honk
newSoundType("Honk","Assets/sound/434878__mickthemicguy__car-honking.wav")
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

--Client confirms finishes loading SimObject types
function join(client)	

	--Create a player for the client
	dynamic = createDynamic(0,0,50,0)
	
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

--Everyone nearby hears a brick get planted, from its center
function plantSound(client, brick)
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

	return client, message
end
registerEventListener("ClientChat","chatCommands")

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

function spawnNewCubes(numCubes, spread)
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
		createDynamic(getDynamicType("small"),x,50,z)
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

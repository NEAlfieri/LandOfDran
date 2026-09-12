--For now dynamic types are used for creating both static and dynamic physics/mesh objects

--The player
brickhead = newDynamicType("brickhead","Assets/brickhead/brickhead.txt",0.02,0.02,0.02)
addAnimation(brickhead,"walk",0,30,0.04,200,400) --For now it just uses the first added animation as the walk cycle

--Different floor tile types
smallPlate = newDynamicType("small","Assets/cube/cube.txt",0.01,0.01,0.01)
mediumPlate = newDynamicType("medium","Assets/cube/cube.txt",0.02,0.01,0.02)
largePlate = newDynamicType("large","Assets/cube/cube.txt",0.04,0.01,0.04)
centerPlate = newDynamicType("center","Assets/cube/cube.txt",0.1,0.01,0.1)

button = newDynamicType("button","Assets/button/button.txt",1,1,1)

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

function click(client,posX,posY,posZ,dirX,dirY,dirZ,mask)

	ignore = nil
	if client:getNumControlled() > 0 then
		ignore = client:getControlledIdx(0)
	end
	result = raycast(posX,posY,posZ,posX + dirX * 30,posY + dirY * 30,posZ + dirZ * 30,ignore)
	
	if result == nil then
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

--z > 0 left
--z < 0 right
function setUpLevel()
	levelSetUp = true
	
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

if levelSetUp == nil then
	setUpLevel()
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
	
	return client
end
registerEventListener("ClientJoin","join")

--Called right after client disconnects but client object is not deleted
function leave(client)

	--Destroy the client's player(s)
	for i = 0, client:getNumControlled() - 1, 1 do
		client:getControlledIdx(i):destroy()
	end
	
	return client
end
registerEventListener("ClientLeave","leave")

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
		if d:getNumControllers() == 0 then
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
		if d:getNumControllers() == 0 then
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

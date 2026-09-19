--[[
	Vehicle_Jeep

	The Blockland jeep by Eric Hartman, ported from the add-on that ships in this folder. The models
	come straight out of its .dts files (see the DTS models section of LuaAPI.md) and the vehicle
	itself is built with spawnModelVehicle, so it drives on Bullet's raycast vehicle exactly like a
	car sliced out of bricks does. The two kinds sit side by side: one is bricks, one is a model.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Vehicle_Jeep/Vehicle_Jeep.lua")

	and then spawnJeep() puts one in the world, and a Vehicle Spawn brick wrenched to "Jeep" keeps one
	above itself, see registerVehicleSpawn at the bottom.

	Where everything on it goes is read out of the shape's own nodes rather than written down here,
	the way jeep_*.cs reads them: hub0 to hub3 are the wheels and Mount0 to Mount6 are the seven
	seats the jeep's description advertises, with Mount0 the driver's.

	What the .cs files have that isn't here: the jeep's damage, its explosion when it dies, the
	spring and tire datablocks' flat tire states, and its dust and splash emitters. Nothing takes
	damage in the engine yet, which is the same reason the weapon package leaves damage out.
]]

local folder = "Add-ons/Vehicle_Jeep/"

--Offer this add-on's models, textures and sounds to anyone joining who hasn't got them, see LuaAPI.md
addServerFolder(folder)

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

jeepBodyType = newDynamicType("jeepBody", folder .. "jeep.dts", scale, scale, scale)
jeepTireType = newDynamicType("jeepTire", folder .. "jeeptire.dts", scale, scale, scale)

--The wreckage the jeep leaves behind when it's destroyed, here to be looked at until there's damage
jeepWreckageType = newDynamicType("jeepWreckage", folder .. "jeepwreckage.dts", scale, scale, scale)

--[[
	The shape's collision detail level, which our DTS loader skips in favour of the one that's drawn.
	Taken as half sizes and a middle in world units, straight out of the col-1 detail's mesh, so the
	jeep collides as its body rather than as a box tall enough to swallow its roll bar.
]]
local bodyBox = { 3.213, 2.629, 5.360 }
local bodyBoxOffset = { -0.021, 2.957, -0.182 }

--[[
	How far above Mount0 the driver goes. A player model's own origin is already down at its feet, so
	nothing has to be added to stand one on the seat the way the passengers stand on theirs
]]
local driverLift = 0

--[[
	jeep_Tire.cs and jeep_Spring.cs as our wheel settings. Torque's numbers don't carry over directly
	(its jeep weighs 300 and pushes with an engineTorque of 12000), so these are picked to drive the
	way the jeep did next to what a brick car of this size does here: all four wheels drive, and between
	them they get it up to about the sixty studs a second its maxWheelSpeed of 30 asks for in a few
	seconds. Its maxSteeringAngle of 0.9785 is most of a right angle, far more than the front wheels
	can actually turn, so the front wheels get something that looks right instead
]]
local jeepMass = 150

--[[
	The tire is drawn scaled to whatever radius its wheel is given, so the radius here is the one
	jeeptire.dts is actually modelled at rather than the 1 (two studs) jeep_Tire.cs asks for, which
	would leave the jeep hovering over tires too small for it. jeep_Spring.cs's length of 0.4 is two
	studs of travel here.

	The spring itself is left at what a brick car's wheels use, which this engine has already tuned
	against its own gravity: Bullet multiplies suspension damping by the chassis weight and by how
	fast the wheel is moving, so numbers much above these turn a hard landing into a catapult and
	make the whole thing twitch over small bumps. Damping wants to be roughly a quarter to a half of
	twice the root of the stiffness, which is where 6 and 10 sit for a stiffness of 100
]]
local frontWheel = {
	radius = 1.548, width = 1.18,
	engineForce = 600, brakeForce = 1200, steerAngle = 0.35,
	suspensionLength = 0.8, suspensionStiffness = 100,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

local rearWheel = {
	radius = 1.548, width = 1.18,
	engineForce = 600, brakeForce = 1200, steerAngle = 0,
	suspensionLength = 0.8, suspensionStiffness = 100,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

--The node each wheel hangs from, and which of the two sets of settings above it uses
local wheelNodes = {
	{ "hub0", frontWheel },
	{ "hub1", frontWheel },
	{ "hub2", rearWheel },
	{ "hub3", rearWheel }
}

--Mount0 is the driver's seat, the other six are the passengers', in the order the .cs numbers them
local driverNode = "Mount0"
local passengerNodes = { "Mount1", "Mount2", "Mount3", "Mount4", "Mount5", "Mount6" }

--A node's spot on the model, as a {x, y, z} list, or nil with a complaint if the shape hasn't got one
local function nodeSpot(name)
	local x, y, z = getTypeNodePosition(jeepBodyType, name)

	if not x then
		error("jeep.dts has no " .. name .. " node")
		return nil
	end

	return { x, y, z }
end

--[[
	Puts a jeep in the world with its model's own origin at x, y, z. Its wheels hang below that, so it
	comes to rest on flat ground with its origin about 0.7 up and wants a little height to drop from.
	Returns the vehicle, or nil and why not

	Called with a client instead, it drops one a few studs in front of where they're looking, which is
	what the spawnJeep in serverstart.lua is for
]]
function spawnJeep(x, y, z)
	--spawnJeep(client): out in front of them, high enough that it lands on its wheels. A client is a
	--table with the methods on it, so that's what one looks like from here
	local builder = nil
	if type(x) == "table" then
		local client = x
		builder = client
		local cameraX, cameraY, cameraZ = client:getCameraPosition()
		local lookX, _, lookZ = client:getCameraDirection()

		if not cameraX then
			error("That client isn't controlling anything yet")
			return nil
		end

		x = cameraX + lookX * 12
		y = cameraY + 2
		z = cameraZ + lookZ * 12
	end

	x = x or 0
	y = y or 3
	z = z or 0

	local wheels = {}
	for _, entry in ipairs(wheelNodes) do
		local spot = nodeSpot(entry[1])
		if not spot then
			return nil
		end

		--Each wheel is one of the shared settings tables plus where this one sits
		local wheel = { position = spot }
		for key, value in pairs(entry[2]) do
			wheel[key] = value
		end
		wheels[#wheels + 1] = wheel
	end

	local seats = {}
	for _, name in ipairs(passengerNodes) do
		local spot = nodeSpot(name)
		if spot then
			seats[#seats + 1] = spot
		end
	end

	local driver = nodeSpot(driverNode)
	if not driver then
		return nil
	end

	driver[2] = driver[2] + driverLift

	return spawnModelVehicle({
		model = jeepBodyType,
		wheelModel = jeepTireType,
		position = { x, y, z },
		--Torque drives along +Y, which is -Z here, and the jeep's hubs and windscreen agree
		forward = { 0, 0, -1 },
		box = bodyBox,
		boxOffset = bodyBoxOffset,
		mass = jeepMass,
		angularDamping = 0.05,
		seat = driver,
		seats = seats,
		wheels = wheels,
		builder = builder
	})
end

--[[
	A Vehicle Spawn brick's wrench dialog lists this as "Jeep": the engine calls spawnJeep(x, y, z, brick) with a spot
	above the brick, turns the jeep to face the way the brick does, and calls it again whenever that jeep is destroyed
]]
registerVehicleSpawn("Jeep", "spawnJeep")

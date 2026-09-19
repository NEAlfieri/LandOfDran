--[[
	Vehicle_Biplane

	Kaje's four seat biplane, ported from the add-on that ships in this folder. The shape comes straight
	out of biplane.dts and the plane itself is a model vehicle (see spawnModelVehicle in LuaAPI.md), so it
	sits in the world beside the jeep and beside anything anyone slices out of bricks. What makes it a
	plane rather than a car is the flight table it's given: the engine flies it from its driver's keys and
	where they look, see Flying in LuaAPI.md.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Vehicle_Biplane/Vehicle_Biplane.lua")

	and then spawnBiplane() puts one in the world, spawnBiplane(client) drops one in front of somebody,
	and a Vehicle Spawn brick wrenched to "Biplane" keeps one above itself.

	Flying it: W and S are the throttle, A and D roll, and it turns its nose toward wherever the pilot
	looks, so you point the camera where you want to go. On the ground it taxis on its wheels like any
	vehicle, A and D steering the tail wheel, space braking, and it flies off the runway once the air over
	its wings is doing more than the wheels are. Right click gets in and out, and its three other seats
	are the one behind the pilot and one on each lower wing, where mount1 to mount3 are on the shape.

	Everything about where things go is read off the shape's own nodes the way Vehicle_Biplane.cs reads
	them: hub0 and hub1 are the main wheels, hub2 the tail wheel, and mount0 to mount3 the four seats.

	How Kaje's numbers were carried over, since none of them can come over as they are (a Torque unit is
	two studs, its gravity is 20 to our 70, and the plane weighed 200 of Torque's own units):

		- mass 200 becomes 100, next to the jeep's 300 becoming 150
		- forwardThrust 3000 over that mass is 15 units a second squared, which is 30 studs
		- reverseThrust 2000 the same way is 20 studs, and maxForwardVel 40 units a second is 80 studs
		- stallSpeed 10 units a second is 20 studs
		- horizontalSurfaceForce and verticalSurfaceForce over the mass are 0.65 a second either way,
		  which is a rate rather than a force so it carries over as it is, rounded up a little because
		  our plane has more gravity to fight
		- rollForce 4000 against pitchForce and yawForce of 6000 are torques through an inertia tensor
		  nobody wrote down, so they come over as the ratios they are: it rolls quickest, pitches next,
		  and yaws slowest, the way it flew
		- lift 100 means nothing without the rest of Torque's wing model, so instead liftSpeed says how
		  fast it has to be going for its wings to hold it up, which is a little under its top speed

	What the .cs files have that isn't here: its damage, the explosion and wreckage it leaves when it's
	destroyed, its flat tires, and its dust and splash emitters. Nothing takes damage in the engine yet,
	which is the same reason the jeep and the weapon packages leave damage out.
]]

local folder = "Add-ons/Vehicle_Biplane/"

--Offer this add-on's models and textures to anyone joining who hasn't got them, see LuaAPI.md
addServerFolder(folder)

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

biplaneBodyType = newDynamicType("biplaneBody", folder .. "biplane.dts", scale, scale, scale)

--[[
	Both its wheels are drawn with this one, since a vehicle has one wheel model and the engine scales it
	to each wheel's radius: the tail wheel is the same wheel three quarters the size, which is almost
	exactly what biplanebackwheel.dts is anyway
]]
biplaneWheelType = newDynamicType("biplaneWheel", folder .. "biplanefrontwheel.dts", scale, scale, scale)

--What the plane weighs, see the conversion in the comment at the top
local biplaneMass = 100

--[[
	The radii the wheel shapes are actually modelled at, measured with getTypeMeshBounds, rather than the
	1 (two studs) Biplane_TireSpring.cs asks for: Torque scales a tire to the radius its datablock names
	and we scale ours to the radius the wheel is given, so these are what keep the wheels the size they
	look in Blockland. They also decide how the parked plane sits: the tail wheel is smaller and its hub
	is nearly two studs higher than the main gear's, which is what tips a tail dragger's nose up at rest
]]
local mainWheel = {
	radius = 0.895, width = 0.8,
	engineForce = 200, brakeForce = 800, steerAngle = 0,
	suspensionLength = 0.5, suspensionStiffness = 150,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

--[[
	The tail wheel steers, the way Vehicle_Biplane.cs gives wheel 2 a steering of -1 and the main gear 0.
	Its angle is negative because it's behind everything else: a wheel at the back turning right swings
	the tail right, which points the nose left, so the sign is what makes A and D taxi the way they read
]]
local tailWheel = {
	radius = 0.69, width = 0.8,
	engineForce = 200, brakeForce = 800, steerAngle = -0.5,
	suspensionLength = 0.5, suspensionStiffness = 150,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

--The node each wheel hangs from, and which of the two sets of settings above it uses
local wheelNodes = {
	{ "hub0", mainWheel },
	{ "hub1", mainWheel },
	{ "hub2", tailWheel }
}

--mount0 is the pilot's seat, then the one behind them and one on each lower wing, as the .cs numbers them
local driverNode = "mount0"
local passengerNodes = { "mount1", "mount2", "mount3" }

--How it flies, see the conversion in the comment at the top
local biplaneFlight = {
	thrust = 30,
	reverseThrust = 20,
	maxSpeed = 80,
	maxReverseSpeed = 20,
	liftSpeed = 78,
	maxLift = 6,
	stallSpeed = 20,
	wingDamping = 1.2,
	finDamping = 0.9,
	dragSpeed = 130,
	pitchRate = 1.2,
	yawRate = 0.35,
	rollRate = 2.0,
	response = 0.3,
	levelRate = 0.8,
	--How far it banks into a turn, 40 degrees, and it keeps whatever roll you leave it in while inverted
	turnBank = 0.7
}

--[[
	Its propeller, which biplane.dts came with as the sequences propslow and propfast. Vehicle_Biplane.cs
	switches between them every two seconds by how fast the plane is going, at 5 units a second, which is
	10 studs here. Ours looks twice a second so the propeller picks up about when you'd expect it to
]]
local propFastSpeed = 10
local propTickMS = 500

--Every biplane in the world by net ID, so one schedule looks after all of them, and what its propeller is doing
local planes = {}
local tickRunning = false

--A node's spot on the model, as a {x, y, z} list, or nil with a complaint if the shape hasn't got one
local function nodeSpot(name)
	local x, y, z = getTypeNodePosition(biplaneBodyType, name)

	if not x then
		error("biplane.dts has no " .. name .. " node")
		return nil
	end

	return { x, y, z }
end

--[[
	Runs while there's a biplane about: keeps each one's propeller turning at the speed it's going, the
	way biplanecontrailCheck does in the original
]]
function biplanePropTick()
	local anyLeft = false

	for id, state in pairs(planes) do
		local plane = getVehicleId(id)

		if not plane then
			planes[id] = nil
		else
			anyLeft = true

			local vx, vy, vz = plane:getVelocity()
			local wanted = math.sqrt(vx * vx + vy * vy + vz * vz) > propFastSpeed and "propfast" or "propslow"

			if state.prop ~= wanted then
				if state.prop then
					plane:stopAnimation(state.prop)
				end
				plane:playAnimation(wanted, true)
				state.prop = wanted
			end
		end
	end

	if anyLeft then
		schedule(propTickMS, "biplanePropTick")
	else
		tickRunning = false
	end
end

--[[
	Puts a biplane in the world with its model's own origin at x, y, z. Its wheels hang about three studs
	below that, so it wants a little height to drop from. Returns the vehicle, or nil and why not

	Called with a client instead, it drops one out in front of where they're looking, the way spawnJeep
	does, so spawnBiplane(client) from a script or the console puts one where somebody is standing
]]
function spawnBiplane(x, y, z)
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

		x = cameraX + lookX * 20
		y = cameraY + 3
		z = cameraZ + lookZ * 20
	end

	x = x or 0
	y = y or 5
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

	local plane = spawnModelVehicle({
		model = biplaneBodyType,
		wheelModel = biplaneWheelType,
		position = { x, y, z },
		--Torque builds a vehicle driving along +Y, which the DTS root turn makes -Z, and the nose agrees
		forward = { 0, 0, -1 },
		mass = biplaneMass,
		angularDamping = 0.05,
		seat = driver,
		seats = seats,
		wheels = wheels,
		flight = biplaneFlight,
		builder = builder
	})

	if not plane then
		return nil
	end

	--Its propeller turns from the moment it's made, slowly until it's rolling, see biplanePropTick
	plane:playAnimation("propslow", true)
	planes[plane.id] = { prop = "propslow" }

	if not tickRunning then
		tickRunning = true
		schedule(propTickMS, "biplanePropTick")
	end

	return plane
end

--[[
	A Vehicle Spawn brick's wrench dialog lists this as "Biplane": the engine calls spawnBiplane(x, y, z, brick)
	with a spot above the brick, turns the plane to face the way the brick does, and calls it again whenever
	that plane is destroyed
]]
registerVehicleSpawn("Biplane", "spawnBiplane")

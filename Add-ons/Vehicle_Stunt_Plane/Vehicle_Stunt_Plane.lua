--[[
	Vehicle_Stunt_Plane

	The stunt plane by Kaje and Ephialtes, ported from the add-on that ships in this folder. It's a model
	vehicle with a flight table, like the biplane next door (see Vehicle_Biplane.lua, and Flying in
	LuaAPI.md), and it's the sharper of the two: its datablock has three times the biplane's engine and
	twice its roll, and its own description calls it super-maneuverable.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Vehicle_Stunt_Plane/Vehicle_Stunt_Plane.lua")

	and then spawnStuntPlane() puts one in the world, spawnStuntPlane(client) drops one in front of
	somebody, and a Vehicle Spawn brick wrenched to "Stunt Plane" keeps one above itself.

	Flying it: W and S are the throttle, A and D roll, and it turns its nose toward wherever the pilot
	looks. Nothing rolls it back level on its own, unlike the biplane, so it stays wherever you leave it,
	which is what you want out of a plane meant for loops and rolls.

	Where everything goes is read off the shape's own nodes: hub0 and hub1 are the main wheels, hub2 the
	tail wheel, mount0 is the pilot and mount1 and mount2 the two passengers the datablock's three mount
	points come to, and mount3 and mount4 are the wingtips, which is where stuntplane_Contrail.cs hangs
	its two contrail emitters (its ContrailImage1 and ContrailImage2 mount to points 3 and 4).

	Kaje's numbers are carried over the same way the biplane's are, see the comment at the top of
	Vehicle_Biplane.lua for what each one becomes and why: engineTorque 12000 against the biplane's 4000
	and rollForce 8000 against its 4000 are what make this one's throttle and roll the quicker pair.

	Not here: its damage, explosion and wreckage, its flat tires, and its splash emitters, for the same
	reason the jeep and the biplane leave them out - nothing takes damage in the engine yet.
]]

local folder = "Add-ons/Vehicle_Stunt_Plane/"

--Offer this add-on's models and textures to anyone joining who hasn't got them, see LuaAPI.md
addServerFolder(folder)

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

stuntPlaneBodyType = newDynamicType("stuntPlaneBody", folder .. "stuntplane.dts", scale, scale, scale)
stuntPlaneWheelType = newDynamicType("stuntPlaneWheel", folder .. "stuntplanetire.dts", scale, scale, scale)

local stuntPlaneMass = 90

--[[
	The radius stuntplanetire.dts is actually modelled at, measured with getTypeMeshBounds, rather than
	the 1 (two studs) stuntplane_TireSpring.cs asks for, for the same reason as the biplane's: the engine
	scales the wheel model to whatever radius its wheel is given
]]
local wheel = {
	radius = 0.71, width = 1.0,
	engineForce = 300, brakeForce = 800, steerAngle = 0,
	suspensionLength = 0.5, suspensionStiffness = 150,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

--The tail wheel, which steers, and turns the other way for being behind everything else, see the biplane
local tailWheel = {
	radius = 0.71, width = 1.0,
	engineForce = 300, brakeForce = 800, steerAngle = -0.5,
	suspensionLength = 0.5, suspensionStiffness = 150,
	dampingCompression = 6, dampingRelaxation = 10,
	frictionSlip = 1.5, rollInfluence = 0.4
}

local wheelNodes = {
	{ "hub0", wheel },
	{ "hub1", wheel },
	{ "hub2", tailWheel }
}

local driverNode = "mount0"
local passengerNodes = { "mount1", "mount2" }

--Where its contrails come off, the two points stuntplane_Contrail.cs mounts its emitters to
local contrailNodes = { "mount3", "mount4" }

local stuntPlaneFlight = {
	thrust = 40,
	reverseThrust = 20,
	maxSpeed = 95,
	maxReverseSpeed = 20,
	liftSpeed = 90,
	maxLift = 8,
	stallSpeed = 18,
	wingDamping = 1.4,
	finDamping = 1.0,
	dragSpeed = 150,
	pitchRate = 2.2,
	yawRate = 0.5,
	rollRate = 4.5,
	response = 0.18,
	--It banks hard into its turns, and holds whatever roll you leave it in once it's past upside down
	levelRate = 1.4,
	turnBank = 1.0
}

--[[
	contrailParticle and contrailEmitter out of stuntplane_Contrail.cs, with two changes. Its particles
	are wider than twice its sizes (which is what a Blockland unit comes to in studs): at a hundred studs
	a second a quarter stud puff every eight milliseconds is a dotted line rather than a trail, and these
	overlap into one. And they go out every 8ms rather than its 1, since Blockland ejected one particle a
	tick where our emitters eject everything they owe each frame, see the note at the top of
	EmitterDefaults.lua
]]
addParticleType("stuntContrailParticle", {
	texture = "Assets/particles/cloud.png",
	color0 = {1, 1, 1, 1},
	color1 = {1, 1, 1, 0.7},
	color2 = {0.6, 0.6, 1, 0.3},
	color3 = {0.2, 0.2, 1, 0},
	size0 = 0.4, size1 = 0.9, size2 = 1.3, size3 = 1.6,
	time0 = 0, time1 = 0.3, time2 = 0.7, time3 = 1,
	lifetimeMS = 500,
	spinSpeed = 45,
	useInvAlpha = true,
	needsSorting = true
})

addEmitterType("stuntContrailEmitter", {
	particles = "stuntContrailParticle",
	ejectionPeriodMS = 8,
	ejectionVelocity = 0,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 180,
	phiVariance = 360
})

--[[
	Its propeller sequences, and how fast it has to be going to leave contrails: the datablock's
	minContrailSpeed of 30 units a second is 60 studs, and propfast comes in at 5 units, which is 10
]]
local propFastSpeed = 10
local contrailSpeed = 60
local propTickMS = 500

--Every stunt plane in the world by net ID, with what its propeller is doing and its two contrails
local planes = {}
local tickRunning = false

--A node's spot on the model, as a {x, y, z} list, or nil with a complaint if the shape hasn't got one
local function nodeSpot(name)
	local x, y, z = getTypeNodePosition(stuntPlaneBodyType, name)

	if not x then
		error("stuntplane.dts has no " .. name .. " node")
		return nil
	end

	return { x, y, z }
end

--Hangs a contrail off each wingtip of one that's going fast enough, and takes them off again when it isn't
local function updateContrails(plane, state, fast)
	if fast == (state.contrails ~= nil) then
		return
	end

	if not fast then
		for _, emitter in ipairs(state.contrails) do
			emitter:destroy()
		end
		state.contrails = nil
		return
	end

	state.contrails = {}
	for _, name in ipairs(contrailNodes) do
		local spot = nodeSpot(name)
		if spot then
			local emitter = addEmitter("stuntContrailEmitter", spot[1], spot[2], spot[3])
			emitter:attachToVehicle(plane, spot[1], spot[2], spot[3])
			state.contrails[#state.contrails + 1] = emitter
		end
	end
end

--[[
	Runs while there's a stunt plane about: each one's propeller at the speed it's going and its
	contrails while it's quick enough for them, which is what contrailCheck does in the original
]]
function stuntPlanePropTick()
	local anyLeft = false

	for id, state in pairs(planes) do
		local plane = getVehicleId(id)

		if not plane then
			--Emitters on a vehicle go when it does, so there's nothing left to take off it
			planes[id] = nil
		else
			anyLeft = true

			local vx, vy, vz = plane:getVelocity()
			local speed = math.sqrt(vx * vx + vy * vy + vz * vz)
			local wanted = speed > propFastSpeed and "propfast" or "propslow"

			if state.prop ~= wanted then
				if state.prop then
					plane:stopAnimation(state.prop)
				end
				plane:playAnimation(wanted, true)
				state.prop = wanted
			end

			updateContrails(plane, state, speed > contrailSpeed)
		end
	end

	if anyLeft then
		schedule(propTickMS, "stuntPlanePropTick")
	else
		tickRunning = false
	end
end

--[[
	Puts a stunt plane in the world with its model's own origin at x, y, z, or drops one out in front of
	a client. Returns the vehicle, or nil and why not
]]
function spawnStuntPlane(x, y, z)
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

		local made = { position = spot }
		for key, value in pairs(entry[2]) do
			made[key] = value
		end
		wheels[#wheels + 1] = made
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
		model = stuntPlaneBodyType,
		wheelModel = stuntPlaneWheelType,
		position = { x, y, z },
		--Torque builds a vehicle driving along +Y, which the DTS root turn makes -Z, and the nose agrees
		forward = { 0, 0, -1 },
		mass = stuntPlaneMass,
		angularDamping = 0.05,
		seat = driver,
		seats = seats,
		wheels = wheels,
		flight = stuntPlaneFlight,
		builder = builder
	})

	if not plane then
		return nil
	end

	plane:playAnimation("propslow", true)
	planes[plane.id] = { prop = "propslow" }

	if not tickRunning then
		tickRunning = true
		schedule(propTickMS, "stuntPlanePropTick")
	end

	return plane
end

--A Vehicle Spawn brick's wrench dialog lists this as "Stunt Plane", see registerVehicleSpawn in LuaAPI.md
registerVehicleSpawn("Stunt Plane", "spawnStuntPlane")

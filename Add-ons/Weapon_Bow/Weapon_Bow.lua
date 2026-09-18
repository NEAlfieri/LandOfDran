--[[
	Weapon_Bow

	The Bow, ported from the Blockland add-on that ships in this folder. The models are loaded straight
	out of its .dts files (see the DTS models section of LuaAPI.md), and the bow itself is rebuilt from
	the datablocks in its Weapon_Bow.cs.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Weapon_Bow/Weapon_Bow.lua")

	It shoots through the weapon support the Tier+Tactical add-on carries in Support_Weapons.lua, which
	this pulls in itself if it hasn't been loaded yet.

	From bowImage's states, one shot is Fire (0.05) then Reload (0.5), and then Check fires again if the
	trigger is still down: an arrow every 550 ms for as long as it's held. It has no ammo at all, the
	datablock's ammo is " ". arrowProjectile is a real arrow at 65 units a second that feels a quarter of
	gravity, trails arrowTrailEmitter's faint white streak, and is cleared away after 4 seconds in a grey
	puff (arrowExplosion, "Arrow Vanish"). Where it lands it throws out arrowStickExplosion's chips with
	arrowHit, and takes 30 off the health of a player it lands on, arrowProjectile's directDamage.

	A Blockland arrow stays stuck in what it hit until its lifetime is up. A projectile here is removed
	the moment it touches anything, so one that lands on the ground, a brick, or a static leaves another
	arrow behind, turned the way it flew, for ARROW_STUCK_MS. One that lands on a player or a vehicle
	doesn't, since it would hang in the air once they moved on.

	What isn't here: letting go of the trigger ran a StopFire state for another 0.2 seconds, so single
	clicks were a little slower than holding it down; here both are 550 ms. The nocked arrow is part of
	the bow's shape and the fire sequence hid it until the reload, which was a visibility track the DTS
	loader doesn't read, so it stays on the string. The arrow took on its shooter's velocity
	(velInheritFactor), which none of the weapons here do. weaponSwitchSound on the Activate state, since
	there's no event for picking an item yet.
]]

local folder = "Add-ons/Weapon_Bow/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--How long an arrow stays stuck in what it hit, in milliseconds. arrowProjectile's lifetime was 4000 from
--the moment it was fired
local ARROW_STUCK_MS = 3000

--How far into what it hit a stuck arrow is pushed along the way it flew, in studs. A projectile stops
--with the front of its collision box touching, which leaves the whole arrowhead showing
local ARROW_SINK = 0.8

local STATIC_TYPE_ID = 2
local BRICK_TYPE_ID = 4

--Tier+Tactical's weapon support does the shooting for this add-on too
if registerWeapon == nil then
	dofile("Add-ons/Weapon_Package_Tier1/Support_Weapons.lua")
end

--Sounds, the AudioProfile datablocks. The Grapple Rope ships the same bowFire.wav under the same name,
--and a name can only be registered once, so whichever add-on loads first registers it
if not bowFireSoundAdded then
	newSoundType("BowFire", folder .. "bowFire.wav")
	bowFireSoundAdded = true
end
newSoundType("ArrowHit", folder .. "arrowHit.wav")

--[[
	Particles, the ParticleData and ParticleEmitterData datablocks. Blockland's textures live in its own
	base/data/particles folder, but dot, chunk, and cloud are all in Assets/particles here. Sizes and
	speeds are doubled from the datablocks, since one of its units is two studs, and its gravity
	coefficients are of 9.81 of its units a second squared.
]]

--arrowTrailParticle: a faint white dot every 2 ms, left where the arrow was and gone in a fifth of a second
addParticleType("ArrowTrailParticle", {
	texture = "Assets/particles/dot.png",
	lit = false,
	useInvAlpha = false,
	color0 = {1.0, 1.0, 1.0, 0.2},
	color1 = {1.0, 1.0, 1.0, 0.0},
	color2 = {1.0, 1.0, 1.0, 0.0},
	color3 = {1.0, 1.0, 1.0, 0.0},
	size0 = 0.4, size1 = 0.02, size2 = 0.02, size3 = 0.02,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 3,
	gravity = {0, 0, 0},
	inheritedVelFactor = 0,
	lifetimeMS = 200,
	lifetimeVarianceMS = 0,
	spinSpeed = 10
})

addEmitterType("ArrowTrailEmitter", {
	particles = "ArrowTrailParticle",
	ejectionPeriodMS = 2,
	periodVarianceMS = 0,
	ejectionVelocity = 0,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Arrow Trail"
})

--arrowStickExplosionParticle: pale chips knocked out of whatever the arrow stuck in, thrown nearly flat
addParticleType("ArrowStickParticle", {
	texture = "Assets/particles/chunk.png",
	lit = true,
	useInvAlpha = true,
	color0 = {0.9, 0.9, 0.6, 0.9},
	color1 = {0.9, 0.5, 0.6, 0.0},
	color2 = {0.9, 0.5, 0.6, 0.0},
	color3 = {0.9, 0.5, 0.6, 0.0},
	size0 = 0.5, size1 = 0.0, size2 = 0.0, size3 = 0.0,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 5,
	gravity = {0, -1.962, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 500,
	lifetimeVarianceMS = 300,
	spinSpeed = 10
})

--particleDensity 10 in the explosion: ten of them at once, so a short burst of a fast emitter
addEmitterType("ArrowStickEmitter", {
	particles = "ArrowStickParticle",
	ejectionPeriodMS = 1,
	periodVarianceMS = 0,
	ejectionVelocity = 10,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 80,
	thetaMax = 80,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Arrow Stick"
})

--arrowExplosionParticle: the grey puff an arrow goes out in when its time is up
addParticleType("ArrowVanishParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.5, 0.5, 0.5, 0.9},
	color1 = {0.5, 0.5, 0.5, 0.0},
	color2 = {0.5, 0.5, 0.5, 0.0},
	color3 = {0.5, 0.5, 0.5, 0.0},
	size0 = 0.9, size1 = 0.0, size2 = 0.0, size3 = 0.0,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 8,
	gravity = {0, 5.886, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 500,
	lifetimeVarianceMS = 300,
	spinSpeed = 10
})

addEmitterType("ArrowVanishEmitter", {
	particles = "ArrowVanishParticle",
	ejectionPeriodMS = 1,
	periodVarianceMS = 0,
	ejectionVelocity = 6,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 180,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Arrow Vanish"
})

--[[
	Models. The arrow is a Torque projectile shape, built pointing down its +Y, which is -Z once it's
	loaded; the engine turns a DTS projectile along that axis. The bow goes through bow.txt for its brown.
]]
bowArrow = newDynamicType("bowArrow", folder .. "arrow.dts", scale, scale, scale)

--The bow itself, held by its mountPoint node. bowImage turned it 10 degrees around Torque's up,
--rotation = eulerToMatrix("0 0 10"), which is the yaw here
bowItem = newItemType("bow", folder .. "bow.txt", scale, scale, scale, "Bow", folder .. "icon_bow.png")

local gripX, gripY, gripZ = getTypeNodePosition(bowItem, "mountPoint")
setItemHand(bowItem, gripX or 0, gripY or 0, gripZ or 0, 0, 10, 0)

local muzzleX, muzzleY, muzzleZ = getTypeNodePosition(bowItem, "muzzlePoint")

--An arrow that stuck in something has been there long enough
function bowRemoveStuckArrow(arrow)
	if arrow ~= nil then
		arrow:destroy()
	end
end

--Reload, the state after Fire's 0.05 seconds: the string is drawn back again, for everyone watching
function bowReloadAnimation(item)
	if item ~= nil and item:isHeld() then
		item:playAnimation("reload")
	end
end

registerWeapon("bow", {
	--ammo = " ": nothing to count, run out of, or reload
	infiniteAmmo = true,
	showAmmo = false,

	--Fire (0.05) and Reload (0.5), then Check goes straight back to Fire while the trigger is down
	automatic = true,
	fireDelayMS = 550,

	--arrowProjectile: muzzleVelocity 65 in Blockland's units, gravityMod 0.25, lifetime 4000
	projectile = "bowArrow",
	projectileSpeed = 130,
	gravityScale = 0.25,
	projectileLifetimeMS = 4000,
	spread = 0,
	pellets = 1,
	--arrowProjectile's directDamage, what an arrow takes off the health of a player it lands on
	damage = 30,

	--particleEmitter, the streak behind the arrow
	trailEmitter = "ArrowTrailEmitter",

	muzzleFromHand = weaponNodeFromHand(bowItem, "muzzlePoint"),
	muzzleOffset = muzzleX and {muzzleX, muzzleY, muzzleZ} or nil,

	--arrowStickExplosion, which is its bloodExplosion too, so a player it lands on gets the same chips
	impactEmitter = "ArrowStickEmitter",
	impactEmitterMS = 10,
	impactSound = "ArrowHit",

	fireAnimation = "fire",

	sounds = {
		fire = "BowFire"
	},

	--The shooter's own game plays the shot the moment they click, and draws the string back after Fire
	predicted = {
		{ at = 50, animation = "reload" }
	},

	--And everyone else sees it drawn back from here
	onFire = function(client, weapon, item)
		schedule(50, "bowReloadAnimation", item)
	end,

	--Stuck in whatever it landed on that stays where it is, the ground being no object at all
	onHit = function(weapon, projectile, hit, x, y, z)
		if hit ~= nil and hit.type ~= STATIC_TYPE_ID and hit.type ~= BRICK_TYPE_ID then
			return
		end

		local px, py, pz = projectile:getPosition()
		local qw, qx, qy, qz = projectile:getRotation()

		--A DTS projectile flies along its -Z
		local aheadX, aheadY, aheadZ = rotateByQuaternion(qw, qx, qy, qz, 0, 0, -1)
		px, py, pz = px + aheadX * ARROW_SINK, py + aheadY * ARROW_SINK, pz + aheadZ * ARROW_SINK

		local arrow = createDynamic(bowArrow, px, py, pz)
		if arrow == nil then
			return
		end

		--Weightless and with no mass, which to the physics makes it part of the scenery: nothing that
		--walks into it moves it
		arrow:setRotation(qw, qx, qy, qz)
		arrow:setGravity(0, 0, 0)
		arrow:setMassProps(0, 0, 0, 0)
		arrow:setVelocity(0, 0, 0)
		arrow:setAngularVelocity(0, 0, 0)

		schedule(ARROW_STUCK_MS, "bowRemoveStuckArrow", arrow)
	end,

	--explodeOnDeath: arrowExplosion's puff where its four seconds ran out, 50 ms of it
	onExpire = function(weapon, shot, x, y, z)
		local puff = addEmitter("ArrowVanishEmitter", x, y, z)
		if puff ~= nil then
			schedule(50, "weaponRemoveEmitter", puff)
		end
	end
})

--[[
	Drops a Bow onto the ground, from just above whatever is under that spot so it lands on bricks
	too, and returns it
]]
function dropBow(x, z)
	x = x or 24
	z = z or -10

	local _, _, groundY = raycast(x, 300, z, x, -300, z)
	local dropY = groundY and (groundY + 1) or 25

	return createItem(bowItem, x, dropY, z)
end

--[[
	Weapon_Rocket_Launcher

	The Rocket Launcher ("Rocket L."), ported from the Blockland add-on that ships in this folder. The
	models are loaded straight out of its .dts files (see the DTS models section of LuaAPI.md), and the
	launcher itself is rebuilt from the datablocks in its "Weapon_Rocket Launcher.cs".

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Weapon_Rocket_Launcher/Weapon_Rocket_Launcher.lua")

	It shoots through the weapon support the Tier+Tactical add-on carries in Support_Weapons.lua, which
	this pulls in itself if it hasn't been loaded yet.

	From rocketLauncherImage's states, one shot is Fire (0.1), Smoke (0.1), and CoolDown (0.5), and then
	it waits for the trigger to come back up: one rocket per click, 700 ms apart, which is its minShotTime
	too. It has no ammo at all, the datablock's ammo is " ". Fire throws rocketLauncherFlashEmitter's
	sparks out of the back of the tube, its tailNode, and Smoke puffs rocketLauncherSmokeEmitter out of
	the front. rocketLauncherProjectile is a real rocket at 65 units a second that feels no gravity,
	trails rocketTrailEmitter's fire and smoke, hums rocketLoop, lights what it passes orange, and is
	cleared away after 4 seconds.

	Where it lands is rocketExplosion: a ring of smoke and a flash, tntExplode, and a white light that
	dies away over 350 ms. A player it lands on loses 30, its directDamage, and then everyone near loses
	up to 100, its radiusDamage, fading to nothing 3 units (6 studs) from the blast, measured to the
	nearest part of them the way Torque's radius damage was. So a direct hit kills, and one at someone's
	feet very nearly does, the shooter included. Whoever fired it gets the score. The blast's push is a
	radiusImpulse, which throws players, items, and vehicles and breaks bricks off destructable vehicles;
	its strength is picked so a rocket at your own feet throws you about as high as Blockland's did,
	rather than from impulseForce, which was in Torque's units of mass.

	What isn't here: the explosion's shape, explosionsphere1.dts, a see-through ball that swelled and
	faded, since nothing here blends a model. The camera shake near a blast. The brickExplosion fields,
	which knocked small bricks out of a build for a while: bricks here only come off destructable
	vehicles, through radiusImpulse. The rocket took on its shooter's velocity (velInheritFactor), which
	none of the weapons here do. weaponSwitchSound on the Activate state, since there's no event for
	picking an item yet.
]]

local folder = "Add-ons/Weapon_Rocket_Launcher/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--rocketExplosion's radiusDamage, and its damageRadius of 3 units in studs
local BLAST_DAMAGE = 100
local BLAST_RADIUS = 6

--How hard the blast pushes, see radiusImpulse: it reaches 2.5 * sqrt(50), about 18 studs, and a rocket
--at someone's feet sends them up at about 40 studs a second, a dozen studs into the air
local BLAST_IMPULSE = 50

--radiusImpulse pushes a dynamic along the line to its position, which for a player is their feet, so a
--rocket on the ground beside them would only slide them along it. Torque pushed along the line to the
--middle of them, which is what lifts a rocket jump, so the push comes from this far under the blast:
--about the height of a player's middle
local BLAST_IMPULSE_DROP = 2.6

--The box a player is hurt as: this far out from their position to each side, and this far up from it
local PLAYER_HALF_WIDTH = 1.25
local PLAYER_HEIGHT = 5.3

--The rocket's own light, hasLight with lightColor "1 0.5 0": how bright, and how often it catches up with
--its rocket in milliseconds, since a light can't follow a dynamic by itself
local ROCKET_LIGHT_BRIGHTNESS = 30
local ROCKET_LIGHT_TICK_MS = 50

--The explosion's light, lightStartColor white going to black over its 350 ms: brightness at each step
local BLAST_LIGHT_STEPS = { 400, 220, 110, 50, 15 }
local BLAST_LIGHT_STEP_MS = 70

--Tier+Tactical's weapon support does the shooting for this add-on too
if registerWeapon == nil then
	dofile("Add-ons/Weapon_Package_Tier1/Support_Weapons.lua")
end

--Sounds, the AudioProfile datablocks
newSoundType("RocketFire", folder .. "rocketFire.wav")
newSoundType("RocketExplode", folder .. "tntExplode.wav")
newSoundType("RocketLoop", folder .. "rocketLoop.wav")

--[[
	Particles, the ParticleData and ParticleEmitterData datablocks. Blockland's textures live in its own
	base/data/particles folder, but star1 and cloud are both in Assets/particles here. Sizes and speeds
	are doubled from the datablocks, since one of its units is two studs, and its gravity coefficients
	are of 9.81 of its units a second squared.
]]

--rocketLauncherFlashParticle: orange sparks out of the back of the tube that fall and lie about for a second and a half
addParticleType("RocketLauncherFlashParticle", {
	texture = "Assets/particles/star1.png",
	lit = false,
	useInvAlpha = false,
	color0 = {0.9, 0.4, 0.0, 0.9},
	color1 = {0.9, 0.5, 0.0, 0.0},
	color2 = {0.9, 0.5, 0.0, 0.0},
	color3 = {0.9, 0.5, 0.0, 0.0},
	size0 = 0.5, size1 = 0.0, size2 = 0.0, size3 = 0.0,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 1,
	gravity = {0, -29.43, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 1500,
	lifetimeVarianceMS = 150,
	spinSpeed = 10
})

addEmitterType("RocketLauncherFlashEmitter", {
	particles = "RocketLauncherFlashParticle",
	ejectionPeriodMS = 3,
	periodVarianceMS = 0,
	ejectionVelocity = 20,
	velocityVariance = 2,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 180,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Rocket Launcher Spark"
})

--rocketLauncherSmokeParticle: grey smoke out of the front that swells as it comes and goes in a third of a second
addParticleType("RocketLauncherSmokeParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.5, 0.5, 0.5, 0.0},
	color1 = {0.5, 0.5, 0.5, 0.9},
	color2 = {0.5, 0.5, 0.5, 0.0},
	color3 = {0.5, 0.5, 0.5, 0.0},
	size0 = 0.5, size1 = 2.0, size2 = 3.5, size3 = 3.5,
	time0 = 0, time1 = 0.5, time2 = 1, time3 = 1,
	drag = 5,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 300,
	lifetimeVarianceMS = 250,
	spinSpeed = 10
})

addEmitterType("RocketLauncherSmokeEmitter", {
	particles = "RocketLauncherSmokeParticle",
	ejectionPeriodMS = 5,
	periodVarianceMS = 0,
	ejectionVelocity = 20,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 25,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Rocket Launcher Smoke"
})

--rocketTrailParticle: yellow fire that turns red, then to dark smoke that thins out over a second
addParticleType("RocketTrailParticle", {
	texture = "Assets/particles/cloud.png",
	lit = false,
	useInvAlpha = false,
	color0 = {1.0, 1.0, 0.0, 0.4},
	color1 = {1.0, 0.2, 0.0, 0.5},
	color2 = {0.2, 0.2, 0.2, 0.3},
	color3 = {0.0, 0.0, 0.0, 0.0},
	size0 = 0.5, size1 = 1.7, size2 = 0.7, size3 = 0.1,
	time0 = 0, time1 = 0.05, time2 = 0.3, time3 = 1,
	drag = 3,
	gravity = {0, 0, 0},
	inheritedVelFactor = 0.15,
	lifetimeMS = 1000,
	lifetimeVarianceMS = 805,
	spinSpeed = 10
})

addEmitterType("RocketTrailEmitter", {
	particles = "RocketTrailParticle",
	ejectionPeriodMS = 5,
	periodVarianceMS = 1,
	ejectionVelocity = 0.5,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Rocket Trail"
})

--rocketExplosionParticle: the smoke of the blast, great pale clouds thrown flat out from 3 units off its middle
addParticleType("RocketExplosionParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.9, 0.9, 0.6, 0.9},
	color1 = {0.9, 0.5, 0.6, 0.0},
	color2 = {0.9, 0.5, 0.6, 0.0},
	color3 = {0.9, 0.5, 0.6, 0.0},
	size0 = 20, size1 = 30, size2 = 30, size3 = 30,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 3,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 700,
	lifetimeVarianceMS = 400,
	spinSpeed = 10
})

--particleDensity 10 in the explosion: ten of them at once, so a short burst of this emitter
addEmitterType("RocketExplosionEmitter", {
	particles = "RocketExplosionParticle",
	ejectionPeriodMS = 3,
	periodVarianceMS = 0,
	ejectionVelocity = 20,
	velocityVariance = 2,
	ejectionOffset = 6,
	thetaMin = 89,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Rocket Explosion Smoke"
})

--rocketExplosionRingParticle: the flash, big orange stars every way for a twentieth of a second
addParticleType("RocketExplosionRingParticle", {
	texture = "Assets/particles/star1.png",
	lit = false,
	useInvAlpha = false,
	color0 = {1.0, 0.5, 0.2, 0.5},
	color1 = {0.9, 0.0, 0.0, 0.0},
	color2 = {0.9, 0.0, 0.0, 0.0},
	color3 = {0.9, 0.0, 0.0, 0.0},
	size0 = 16, size1 = 26, size2 = 26, size3 = 26,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 8,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 40,
	lifetimeVarianceMS = 10,
	spinSpeed = 10
})

addEmitterType("RocketExplosionRingEmitter", {
	particles = "RocketExplosionRingParticle",
	ejectionPeriodMS = 1,
	periodVarianceMS = 0,
	ejectionVelocity = 10,
	velocityVariance = 0,
	ejectionOffset = 6,
	thetaMin = 0,
	thetaMax = 180,
	phiVariance = 360,
	lifetimeMS = 0,
	uiName = "Rocket Explosion Flash"
})

--[[
	Models. The rocket is a Torque projectile shape, built pointing down its +Y, which is -Z once it's
	loaded; the engine turns a DTS projectile along that axis. Both go through a .txt next to them:
	the launcher's for its green and both for the see-through parts that can't be drawn, see them.
]]
rocketProjectile = newDynamicType("rocketProjectile", folder .. "rocketProjectile.txt", scale, scale, scale)

--The launcher itself, held by its mountPoint node. It's built the way setItemHand expects already, +Y
--out of the hand and -Z down the tube, so nothing is turned
rocketLauncherItem = newItemType("rocketLauncher", folder .. "rocketLauncher.txt", scale, scale, scale,
	"Rocket L.", folder .. "icon_rocketLauncher.png")

local gripX, gripY, gripZ = getTypeNodePosition(rocketLauncherItem, "mountPoint")
setItemHand(rocketLauncherItem, gripX or 0, gripY or 0, gripZ or 0, 0, 0, 0)

local muzzleX, muzzleY, muzzleZ = getTypeNodePosition(rocketLauncherItem, "muzzlePoint")

--The back of the tube, where the sparks come out
local tailFromHand = weaponNodeFromHand(rocketLauncherItem, "tailNode")

--[[
	Lights riding along with rockets, by the rocket's ID. A light can't follow a dynamic by itself, so
	each one catches up with its rocket every ROCKET_LIGHT_TICK_MS until the rocket lands or runs out
]]
local rocketLights = {}

local function removeRocketLight(shotID)
	local riding = rocketLights[shotID]
	if riding ~= nil then
		rocketLights[shotID] = nil
		riding.light:destroy()
	end
end

function rocketLightTick(shotID)
	local riding = rocketLights[shotID]
	if riding == nil then
		return
	end

	local x, y, z = riding.shot:getPosition()
	if x == nil then
		removeRocketLight(shotID)
		return
	end

	riding.light:setPosition(x, y, z)
	schedule(ROCKET_LIGHT_TICK_MS, "rocketLightTick", shotID)
end

--Something other than landing or running out took the rocket away, and its light shouldn't hang there for good
function rocketLightBackstop(shotID)
	removeRocketLight(shotID)
end

--The explosion's light dying away, one step of BLAST_LIGHT_STEPS at a time
function rocketBlastLightStep(light, step)
	if light == nil then
		return
	end

	if BLAST_LIGHT_STEPS[step] == nil then
		light:destroy()
		return
	end

	light:setBrightness(BLAST_LIGHT_STEPS[step])
	schedule(BLAST_LIGHT_STEP_MS, "rocketBlastLightStep", light, step + 1)
end

--How far a spot is from the nearest part of a player, taken as an upright box from their position up
local function distanceToPlayer(player, x, y, z)
	local px, py, pz = player:getPosition()
	local dx = math.max(0, math.abs(x - px) - PLAYER_HALF_WIDTH)
	local dy = math.max(0, py - y, y - (py + PLAYER_HEIGHT))
	local dz = math.max(0, math.abs(z - pz) - PLAYER_HALF_WIDTH)
	return math.sqrt(dx * dx + dy * dy + dz * dz)
end

--radiusDamage: everyone near the blast is hurt by how near they are, the shooter included
local function rocketBlastDamage(x, y, z, attacker)
	if damagePlayer == nil then
		return
	end

	--Gathered first, since someone dying changes who has a player
	local caught = {}
	for i = 0, getNumClients() - 1, 1 do
		local client = getClientIdx(i)
		if client:getNumControlled() > 0 then
			local player = client:getControlledIdx(0)
			local distance = distanceToPlayer(player, x, y, z)
			if distance < BLAST_RADIUS then
				caught[#caught + 1] = { player = player, amount = BLAST_DAMAGE * (1 - distance / BLAST_RADIUS) }
			end
		end
	end

	for _, hurt in ipairs(caught) do
		damagePlayer(hurt.player, hurt.amount, attacker, x, y, z)
	end
end

--The trigger coming back up after the shot, the ready sequence of the Ready state
function rocketLauncherReady(item)
	if item ~= nil and item:isHeld() then
		item:playAnimation("ready")
	end
end

--The trigger held in, the TrigDown sequence of the Smoke, CoolDown, and Reload states
function rocketLauncherTrigDown(item)
	if item ~= nil and item:isHeld() then
		item:playAnimation("TrigDown")
	end
end

registerWeapon("rocketLauncher", {
	--ammo = " ": nothing to count, run out of, or reload
	infiniteAmmo = true,
	showAmmo = false,

	--One rocket per click: Fire (0.1), Smoke (0.1), CoolDown (0.5), then a wait for the trigger to come back up
	automatic = false,
	fireDelayMS = 700,

	--rocketLauncherProjectile: muzzleVelocity 65 in Blockland's units, gravityMod 0, lifetime 4000
	projectile = "rocketProjectile",
	projectileSpeed = 130,
	gravityScale = 0,
	projectileLifetimeMS = 4000,
	spread = 0,
	pellets = 1,
	--rocketLauncherProjectile's directDamage, on top of which comes the blast, see rocketBlastDamage
	damage = 30,

	--particleEmitter and sound, the fire behind the rocket and its hum
	trailEmitter = "RocketTrailEmitter",
	flightSound = "RocketLoop",

	muzzleFromHand = weaponNodeFromHand(rocketLauncherItem, "muzzlePoint"),
	muzzleOffset = muzzleX and {muzzleX, muzzleY, muzzleZ} or nil,

	--stateEmitter on Smoke, once Fire's 0.1 seconds are up, sent down the tube the way its shooter looks
	muzzleSmoke = { emitter = "RocketLauncherSmokeEmitter", delayMS = 100, forMS = 50, aimed = true },

	--rocketExplosion: particleEmitter is the smoke, ten clouds of it (particleDensity), and emitter[0] the
	--flash, which lasts its own lifeTimeMS of 50
	impactEmitter = "RocketExplosionEmitter",
	impactEmitterMS = 30,
	impactFlashEmitter = "RocketExplosionRingEmitter",
	impactFlashMS = 50,
	impactSound = "RocketExplode",

	fireAnimation = "fire",

	sounds = {
		fire = "RocketFire"
	},

	--The shooter's own game plays the shot the moment they click
	predicted = {},

	onFire = function(client, weapon, item)
		--stateEmitter on Fire: sparks out of the tailNode for 50 ms
		local x, y, z = weaponPointFromHand(client, tailFromHand)
		if x ~= nil then
			local sparks = addEmitter("RocketLauncherFlashEmitter", x, y, z)
			if sparks ~= nil then
				schedule(50, "weaponRemoveEmitter", sparks)
			end
		end

		schedule(100, "rocketLauncherTrigDown", item)
		schedule(700, "rocketLauncherReady", item)
	end,

	--hasLight: an orange light that rides along with the rocket
	onShot = function(weapon, shot, client)
		local x, y, z = shot:getPosition()
		local light = createLight(x, y, z, 1.0, 0.5, 0.0, ROCKET_LIGHT_BRIGHTNESS, 0, 0)
		if light == nil then
			return
		end

		rocketLights[shot.id] = { shot = shot, light = light }
		schedule(ROCKET_LIGHT_TICK_MS, "rocketLightTick", shot.id)
		schedule(weapon.projectileLifetimeMS + 500, "rocketLightBackstop", shot.id)
	end,

	onHit = function(weapon, projectile, hit, x, y, z)
		removeRocketLight(projectile.id)

		rocketBlastDamage(x, y, z, projectile.shooterClient)

		--The push only pushes: what the blast takes off anyone's health was worked out above, so Damage.lua's
		--damageByImpulse is told to leave this one alone
		impulseHarmless = true
		radiusImpulse(x, y - BLAST_IMPULSE_DROP, z, BLAST_IMPULSE)
		impulseHarmless = nil

		--lightStartColor "1 1 1" to lightEndColor "0 0 0" over lifeTimeMS 350
		local light = createLight(x, y + 0.5, z, 1.0, 0.95, 0.85, BLAST_LIGHT_STEPS[1], 0, 0)
		if light ~= nil then
			schedule(BLAST_LIGHT_STEP_MS, "rocketBlastLightStep", light, 2)
		end
	end,

	--No explodeOnDeath: a rocket that flies for four seconds without landing just goes out
	onExpire = function(weapon, shot, x, y, z)
		removeRocketLight(shot.id)
	end
})

--[[
	Drops a Rocket Launcher onto the ground, from just above whatever is under that spot so it lands on
	bricks too, and returns it
]]
function dropRocketLauncher(x, z)
	x = x or 28
	z = z or -10

	local _, _, groundY = raycast(x, 300, z, x, -300, z)
	local dropY = groundY and (groundY + 1) or 25

	return createItem(rocketLauncherItem, x, dropY, z)
end

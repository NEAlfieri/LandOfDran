--[[
	Weapon_Gun

	The plain Gun, by Eric Hartman, ported from the Blockland add-on that ships in this folder. The
	models are loaded straight out of its .dts files (see the DTS models section of LuaAPI.md), and
	the gun itself is rebuilt from the datablocks in its server.cs.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Weapon_Gun/Weapon_Gun.lua")

	It shoots the way the Tier+Tactical weapons do, through the weapon support that add-on carries
	in Support_Weapons.lua, which this pulls in itself if it hasn't been loaded yet. That's the
	reverse of Blockland, where Tier+Tactical needed this add-on for its bullet: here both need each
	other, so each loads what the other has if it's first.

	From gunImage's states, one shot is Fire (0.14) then Smoke (0.01), and it waits for the trigger
	to come back up, so it's one shot per click at 150 ms apart. It has no ammo at all: the datablock's
	ammo is " ", so it never runs dry or reloads. gunProjectile is a real round at 90 units a second
	that feels no gravity and is cleared away after 4 seconds, and gunExplosion is what it leaves
	where it lands: dust, a flash, bulletHit, and a light for 150 ms. Every shot throws out a
	gunShellDebris casing, which is gunshell.dts bouncing about for two seconds.

	What isn't here: gunImage::onFire played the shiftAway recoil thread on the player, which the
	player model doesn't have, and weaponSwitchSound on its Activate state, since there's no event
	for picking an item yet. colorShiftColor tinted the stock, and there's nothing to tint a type's
	meshes with.

	A bullet that lands on someone takes 30 off their health, gunProjectile's directDamage, see
	Support_Weapons.lua's hurtIfPlayer.

	Weapon_Guns_Akimbo isn't ported: it mounts a second gun in the left hand, and a player here holds
	one item.
]]

local folder = "Add-ons/Weapon_Gun/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--Tier+Tactical's weapon support does the shooting for this add-on too
if registerWeapon == nil then
	dofile("Add-ons/Weapon_Package_Tier1/Support_Weapons.lua")
end

--Sounds, the AudioProfile datablocks
newSoundType("GunShot1", folder .. "gunShot1.wav")
newSoundType("BulletHit", folder .. "bulletHit.wav")

--[[
	Particles, the ParticleData and ParticleEmitterData datablocks. Blockland's textures live in its
	own base/data/particles folder, but star1 and cloud are both in Assets/particles here. Sizes and
	speeds are doubled from the datablocks, since one of its units is two studs, and its gravity
	coefficients are of 9.81 of its units a second squared.
]]

--The muzzle flash: gunFlashParticle, yellow going out to orange in a few hundredths of a second
addParticleType("GunFlashParticle", {
	texture = "Assets/particles/star1.png",
	lit = false,
	useInvAlpha = false,
	color0 = {0.9, 0.9, 0.0, 0.9},
	color1 = {0.9, 0.5, 0.0, 0.0},
	color2 = {0.9, 0.5, 0.0, 0.0},
	color3 = {0.9, 0.5, 0.0, 0.0},
	size0 = 1.0, size1 = 2.0, size2 = 2.0, size3 = 2.0,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 3,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 25,
	lifetimeVarianceMS = 15,
	spinSpeed = 10
})

addEmitterType("GunFlashEmitter", {
	particles = "GunFlashParticle",
	ejectionPeriodMS = 3,
	periodVarianceMS = 0,
	ejectionVelocity = 2,
	velocityVariance = 2,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0
})

--The smoke after it: gunSmokeParticle, a small grey wisp for half a second. Not the gunSmokeEmitter
--in EmitterDefaults.lua, which is the launcher's
addParticleType("GunMuzzleSmokeParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.5, 0.5, 0.5, 0.9},
	color1 = {0.5, 0.5, 0.5, 0.0},
	color2 = {0.5, 0.5, 0.5, 0.0},
	color3 = {0.5, 0.5, 0.5, 0.0},
	size0 = 0.3, size1 = 0.3, size2 = 0.3, size3 = 0.3,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 3,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 525,
	lifetimeVarianceMS = 55,
	spinSpeed = 10
})

addEmitterType("GunMuzzleSmokeEmitter", {
	particles = "GunMuzzleSmokeParticle",
	ejectionPeriodMS = 3,
	periodVarianceMS = 0,
	ejectionVelocity = 2,
	velocityVariance = 2,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0
})

--Where a round lands, gunExplosion: gunExplosionParticle is the dust knocked loose, thrown flat out
--and falling, and gunExplosionRingParticle the flash over it, a star that shrinks away in a twentieth
--of a second
addParticleType("GunHitDustParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.9, 0.9, 0.9, 0.3},
	color1 = {0.9, 0.5, 0.6, 0.0},
	color2 = {0.9, 0.5, 0.6, 0.0},
	color3 = {0.9, 0.5, 0.6, 0.0},
	size0 = 0.5, size1 = 1.5, size2 = 1.5, size3 = 1.5,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 8,
	gravity = {0, -19.62, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 700,
	lifetimeVarianceMS = 400,
	spinSpeed = 10
})

--particleDensity 5 in the explosion: a handful of them at once, so a short burst of a fast emitter
addEmitterType("GunHitDustEmitter", {
	particles = "GunHitDustParticle",
	ejectionPeriodMS = 2,
	periodVarianceMS = 0,
	ejectionVelocity = 4,
	velocityVariance = 2,
	ejectionOffset = 0,
	thetaMin = 89,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0
})

addParticleType("GunHitFlashParticle", {
	texture = "Assets/particles/star1.png",
	lit = false,
	useInvAlpha = false,
	color0 = {1.0, 1.0, 0.0, 0.9},
	color1 = {0.9, 0.0, 0.0, 0.0},
	color2 = {0.9, 0.0, 0.0, 0.0},
	color3 = {0.9, 0.0, 0.0, 0.0},
	size0 = 2.0, size1 = 0.0, size2 = 0.0, size3 = 0.0,
	time0 = 0, time1 = 1, time2 = 1, time3 = 1,
	drag = 8,
	gravity = {0, 9.81, 0},
	inheritedVelFactor = 0.2,
	lifetimeMS = 50,
	lifetimeVarianceMS = 35,
	spinSpeed = 500
})

addEmitterType("GunHitFlashEmitter", {
	particles = "GunHitFlashParticle",
	ejectionPeriodMS = 3,
	periodVarianceMS = 0,
	ejectionVelocity = 0,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 89,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0
})

--[[
	Models. The bullet is a Torque projectile shape, built pointing down its +Y, which is -Z once it's
	loaded; the engine turns a DTS projectile along that axis. Its trail is a see-through streak in
	Blockland that here could only be a solid tube nine studs long, so bullet.txt leaves that mesh
	out and just the slug flies. The Tier+Tactical weapons fire this same bullet, as their datablocks did.
]]
gunBullet = newDynamicType("gunBullet", folder .. "bullet.txt", scale, scale, scale)
gunShell = newDynamicType("gunShell", folder .. "gunshell.dts", scale, scale, scale)

--The gun itself, held by its mountPoint node. It's built the way setItemHand expects already, +Y out
--of the hand and -Z down the barrel, so nothing is turned
gunItem = newItemType("gun", folder .. "pistol.dts", scale, scale, scale, "Gun", folder .. "Icon_gun.png")

local gripX, gripY, gripZ = getTypeNodePosition(gunItem, "mountPoint")
setItemHand(gunItem, gripX or 0, gripY or 0, gripZ or 0, 0, 0, 0)

--The end of the barrel and where the casings come out, off the shape's own nodes
local muzzleFromHand = weaponNodeFromHand(gunItem, "muzzlePoint")
local muzzleX, muzzleY, muzzleZ = getTypeNodePosition(gunItem, "muzzlePoint")

registerWeapon("gun", {
	--ammo = " ": nothing to count, run out of, or reload
	infiniteAmmo = true,
	showAmmo = false,

	--One shot per click, Fire (0.14) and Smoke (0.01) then a wait for the trigger to come back up
	automatic = false,
	fireDelayMS = 150,

	--gunProjectile: muzzleVelocity 90 in Blockland's units, gravityMod 0, lifetime 4000
	projectile = "gunBullet",
	projectileSpeed = 180,
	gravityScale = 0,
	projectileLifetimeMS = 4000,
	spread = 0,
	pellets = 1,
	--gunProjectile's directDamage, what a shot takes off the health of a player it lands on
	damage = 30,

	muzzleFromHand = muzzleFromHand,
	--The same point in the model's own space, which is where its own game hangs the predicted flash
	muzzleOffset = muzzleX and {muzzleX, muzzleY, muzzleZ} or nil,

	--stateEmitter on Fire, then on Smoke once Fire's 0.14 seconds are up
	muzzleEmitter = "GunFlashEmitter",
	muzzleEmitterMS = 50,
	--The flash lights what's around the barrel for as long as it lasts, the yellow of gunFlashParticle
	muzzleLight = { color = {1.0, 0.9, 0.3}, brightness = 35, coronaWidth = 0.35, forMS = 50 },
	muzzleSmoke = { emitter = "GunMuzzleSmokeEmitter", delayMS = 140, forMS = 50 },

	--gunExplosion
	impactEmitter = "GunHitDustEmitter",
	impactEmitterMS = 10,
	impactFlashEmitter = "GunHitFlashEmitter",
	impactFlashMS = 50,
	impactSound = "BulletHit",
	impactLight = { color = {0.5, 0.8, 0.9}, brightness = 20, coronaWidth = 0, forMS = 150 },

	--gunShellDebris, out of the shape's ejectPoint along shellExitDir "1 -1.3 1" at shellVelocity 7
	casing = {
		model = "gunShell",
		offsetFromHand = weaponNodeFromHand(gunItem, "ejectPoint"),
		exitDir = {1.0, 1.0, 1.3},
		speed = 14,
		variance = 15,
		lifetimeMS = 2000,
		gravityScale = 2,
		restitution = 0.5,
		friction = 0.2,
		minSpin = -400,
		maxSpin = 200
	},

	fireAnimation = "fire",

	sounds = {
		fire = "GunShot1"
	},

	--The shooter's own game plays the shot the moment they click; everything it needs is in the fields
	--above, and nothing else happens later in the shot
	predicted = {}
})

--[[
	Drops a Gun onto the ground, from just above whatever is under that spot so it lands on bricks
	too, and returns it
]]
function dropGun(x, z)
	x = x or 20
	z = z or -10

	local _, _, groundY = raycast(x, 300, z, x, -300, z)
	local dropY = groundY and (groundY + 1) or 25

	return createItem(gunItem, x, dropY, z)
end

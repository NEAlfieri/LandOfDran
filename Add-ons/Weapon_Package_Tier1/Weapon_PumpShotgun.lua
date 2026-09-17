--[[
	Weapon_PumpShotgun, from "Weapon_Pump Shotgun.cs"

	From the datablock and PumpShotgunImage::onFire:
		TT_maxAmmo = 6, TT_ammoType = "shotgun"
		spread 0.0032, shellCount 9
		PumpShotgunProjectile: muzzleVelocity 100, gravityMod 0.4

	Nine pellets go out at once, each its own projectile, which is what shellCount did.

	The pump is the Eject state: 0.48 seconds with the pump sound and the fire animation, after the
	Fire (0.1) and Smoke (0.192) states, and then it waits for the trigger to come back up. That
	whole run is the fire delay, and why it can't be held down.
]]

registerWeapon("pumpShotgun", {
	ammoType = "shotgun",
	magazineSize = 6,
	showAmmo = true,

	--Pumped between shots, so one shell per click
	automatic = false,
	fireDelayMS = 780,
	reloadMS = 1400,

	--The Gun's bullet, add-ons/Weapon_Gun/bullet.dts in its datablock
	projectile = "gunBullet",
	projectileSpeed = 200,
	gravityScale = 0.4,
	spread = 0.0032,
	pellets = 9,

	--The end of the barrel measured from the hand: its muzzle point node less its mount point node
	muzzleFromHand = {0.000, 0.666, -3.486},
	muzzleEmitter = "TTMuzzleFlashEmitter",
	muzzleEmitterMS = 80,
	impactEmitter = "TTImpactEmitter",


	--PumpShotgunShellDebris, its own shell, out of the shape's ejectPoint along "1 0.1 1" at shellVelocity 5
	casing = {
		model = "ttShotgunShell",
		--This shape has no ejectPoint node, so the shell comes from the hand, as Torque fell back to
		offsetFromHand = weaponNodeFromHand(getDynamicType("pumpShotgun"), "ejectPoint") or {0, 0, 0},
		exitDir = {1.0, 1.0, -0.1},
		speed = 10,
		variance = 10,
		lifetimeMS = 2000,
		gravityScale = 2
	},

	fireAnimation = "fire",

	sounds = {
		fire = "TTShotgunFire",
		empty = "TTPistolClick",
		reloadStart = "TTShotgunReload",
		reloadEnd = "TTShotgunReload"
	}
})

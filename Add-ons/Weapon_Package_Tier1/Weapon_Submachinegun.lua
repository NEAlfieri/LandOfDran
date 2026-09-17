--[[
	Weapon_Submachinegun, from Weapon_Submachinegun.cs

	From the datablock and SubmachineGunImage::onFire:
		TT_maxAmmo = 35, TT_ammoType = "9MM"
		spread 0.0015, shellCount 1
		SubmachineGunProjectile1: muzzleVelocity 100, gravityMod 0.2

	Blockland's muzzleVelocity is in its own units, where one unit is two studs, so 100 there is
	200 studs a second here.

	It's the automatic one: Smoke transitions straight back to FireCheckA while the trigger is still
	down, so it keeps firing. The cycle is Fire, Delay and the load checks, each of which Torque
	rounds up to one 32 ms tick, which is where the fire delay comes from.
]]

registerWeapon("submachinegun", {
	ammoType = "9MM",
	magazineSize = 35,
	showAmmo = true,

	automatic = true,
	fireDelayMS = 96,
	reloadMS = 2000,

	--A real round, as in the original: the Gun's bullet, add-ons/Weapon_Gun/bullet.dts in its datablock
	projectile = "gunBullet",
	projectileSpeed = 200,
	gravityScale = 0.2,
	spread = 0.0015,
	pellets = 1,

	--The end of the barrel measured from the hand: its muzzle point node less its mount point node
	muzzleFromHand = {0.000, 0.840, -2.160},
	muzzleEmitter = "TTMuzzleFlashEmitter",
	muzzleEmitterMS = 50,
	impactEmitter = "TTImpactEmitter",


	--GunShellDebris out of the shape's ejectPoint along shellExitDir "1 0.1 1" at shellVelocity 5
	casing = {
		model = "gunShell",
		--This shape has no ejectPoint node, so the shell comes from the hand, as Torque fell back to
		offsetFromHand = weaponNodeFromHand(getDynamicType("submachinegun"), "ejectPoint") or {0, 0, 0},
		exitDir = {1.0, 1.0, -0.1},
		speed = 10,
		variance = 10,
		lifetimeMS = 2000,
		gravityScale = 2
	},

	fireAnimation = "fire",

	sounds = {
		fire = "TTSubmachinegunFire",
		empty = "TTPistolClick",
		reloadStart = "TTMagazineOut",
		reloadEnd = "TTMagazineIn"
	}
})

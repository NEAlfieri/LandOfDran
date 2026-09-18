--[[
	Weapon_SportRifle, from "Weapon_Sport Rifle.cs"

	From the datablock and SportRifleImage::onFire:
		TT_maxAmmo = 10, TT_ammoType = "556"
		spread 0.0001, shellCount 1
		SportRifleProjectile: muzzleVelocity 190, gravityMod 0.02

	Note the datablock's shapeFile is Sport_Rifle.3.dts. The package also shipped two older models of it,
	sport_rifle.dts and SPORT_RIFLE.2.dts, which no datablock named and which have been taken out.

	Barely any spread and almost no drop, so it shoots nearly flat and straight. Fire is followed by
	Smoke (0.2), then it waits for the trigger, so it's one shot per click. Reload is 1.2 + 0.8.
]]

registerWeapon("sportRifle", {
	ammoType = "556",
	magazineSize = 10,
	showAmmo = true,

	automatic = false,
	fireDelayMS = 250,
	reloadMS = 2000,

	--Its own rifle_round.dts, where the other weapons fire the Gun's bullet
	projectile = "ttRifleRound",
	--190 in Blockland's units, where one unit is two studs
	projectileSpeed = 380,
	gravityScale = 0.02,
	spread = 0.0001,
	pellets = 1,
	--SportRifleProjectile's directDamage, what a shot takes off the health of a player it lands on
	damage = 24,

	--The end of the barrel measured from the hand: its muzzle point node less its mount point node
	muzzleFromHand = {0.000, 0.810, -4.500},
	muzzleEmitter = "TTMuzzleFlashEmitter",
	muzzleEmitterMS = 70,
	impactEmitter = "TTImpactEmitter",
	--Not in the original: the hole it leaves in a brick, see weaponImpactEffect
	impactDecal = "bulletHole",
	impactDecalSize = 0.6,

	fireAnimation = "fire",

	sounds = {
		fire = "TTSportRifleFire",
		empty = "TTPistolClick",
		reloadStart = "TTMagazineOut",
		reloadEnd = "TTGeneralReload"
	}
})

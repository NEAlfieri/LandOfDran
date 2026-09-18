--[[
	Weapon_Pistol, from Weapon_Pistol.cs

	The only hitscan weapon of the four: the original set TT_raycastEnabled on it and let the
	raycasting support do the shooting, while the others fire real projectiles.

	From the datablock:
		TT_maxAmmo = 12, TT_ammoType = "9MM"
		TT_raycastSpreadAmt   0.0009 standing, 0.0018 moving   (set in PistolImage::onFire)
		TT_raycastWeaponRange 200 standing, 85 moving
		TT_raycastSpreadCount 1

	Fire delay is the Fire (0.05) and Wait (0.001) states plus the load checks, rounded to the 32 ms
	ticks Torque actually runs states on. It waits for the trigger to come back up between shots, so
	it isn't automatic. Reload is ReloadWait (0.3) + ReloadStart (0.288) + DeathCheck + Reloaded (0.4).
]]

registerWeapon("pistol", {
	ammoType = "9MM",
	magazineSize = 12,
	showAmmo = true,

	--Semi automatic: one shot per click, see stateTransitionOnTriggerUp out of Smoke
	automatic = false,
	fireDelayMS = 150,
	reloadMS = 1020,

	--Hitscan, so no projectile field. The Gun's bullet is fired along the shot purely to be seen
	spread = 0.0009,
	range = 200,
	pellets = 1,
	--TT_raycastDirectDamage, what a shot takes off the health of a player it lands on
	damage = 12,
	tracer = "gunBullet",

	--The end of the barrel measured from the hand: its muzzle point node less its mount point node
	muzzleFromHand = {0.000, 0.854, -2.372},

	--The same point in the model's own space, which is where its own game hangs the predicted flash
	muzzleOffset = {0.000, 0.616, -2.244},
	muzzleLight = { color = {1.0, 0.85, 0.45}, brightness = 35, coronaWidth = 0.35, forMS = 60 },
	muzzleEmitter = "TTMuzzleFlashEmitter",
	muzzleEmitterMS = 60,
	impactEmitter = "TTImpactEmitter",


	--GunShellDebris out of the shape's ejectPoint along shellExitDir "1 0.1 1" at shellVelocity 5
	casing = {
		model = "gunShell",
		--This shape has no ejectPoint node, so the shell comes from the hand, as Torque fell back to
		offsetFromHand = weaponNodeFromHand(getDynamicType("pistol"), "ejectPoint") or {0, 0, 0},
		exitDir = {1.0, 1.0, -0.1},
		speed = 10,
		variance = 10,
		lifetimeMS = 2000,
		gravityScale = 2
	},

	fireAnimation = "fire",

	sounds = {
		fire = "TTPistolFire",
		empty = "TTPistolClick",
		reloadStart = "TTMagazineOut",
		reloadEnd = "TTMagazineIn"
	},

	--[[
		What the shooter's own game plays the moment they click, before the server has heard about it.
		The sound, the fire animation, the muzzle flash and its light are all filled in from the fields
		above; this is for anything that happens later in the shot.

		The slide coming back is the Wait state of the original, which sits between Smoke and the load
		check at about a tenth of a second in, with pistolClickSound on it.
	]]
	predicted = {
		{ at = 115, sound = "TTPistolClick" }
	},

	--PistolImage::onFire opened the shot up when the player was moving
	onFire = function(client, weapon, item)
		local player = client:getControlledIdx(0)
		if player == nil then
			return
		end

		local velX, velY, velZ = player:getVelocity()
		if math.sqrt(velX * velX + velY * velY + velZ * velZ) > 0.1 then
			weapon.spread = 0.0018
			weapon.range = 85
		else
			weapon.spread = 0.0009
			weapon.range = 200
		end
	end
})

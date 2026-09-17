--[[
	Weapon_Package_Tier1

	Tier+Tactical Tier 1, by Bushido and Space Guy, ported from the Blockland add-on that ships in
	this folder. The models are loaded straight out of its .dts files (see the DTS models section of
	LuaAPI.md), and the guns are rebuilt in Lua from the datablocks in its .cs files.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Weapon_Package_Tier1/Weapon_Package_Tier1.lua")

	What works: firing, the animations, sounds and muzzle effects each weapon came with, real
	projectiles for the weapons that had them, magazines and reloading, and spare ammo per type.

	What doesn't, yet: nothing takes damage, since the engine has no health system. A shot that
	lands only leaves a puff. Support_Weapons.lua's fireOneShot is where damage would go once
	there's something to damage. Ammo pickups aren't wired up either, so the AMMO_*.dts models are
	loaded as scenery and clients start with the spare ammo set below.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--The sounds and particles first, since the weapons name them
dofile(folder .. "Effects.lua")

--The round that comes out of a barrel, used both as a real projectile and as the pistol's tracer.
--RIFLE_ROUND is a single round rather than one of the ammo boxes, so it reads as a bullet. It's a
--lot smaller than true to size, since a round at its own scale is over a stud and a half long and
--fills the view as it leaves the barrel
local bulletScale = 0.45
ttBulletRound = newDynamicType("ttBulletRound", folder .. "RIFLE_ROUND.dts", bulletScale, bulletScale, bulletScale)

--The weapons themselves. Each shapeFile is the one its datablock named, which for the sport rifle
--is the .3 model rather than sport_rifle.dts
--The icon of each is the one its datablock's iconName named, which is the picture of the weapon the
--package ships rather than one of the ci_ crosshair images
local weaponItems = {
	{ "pistol",        "PISTOL_.dts",        "Pistol",        {0.000, -0.238, 0.128}, "Pistol.png" },
	{ "submachinegun", "submachinegun.dts",  "Submachine Gun",{0.000, -0.840, 0.440}, "Submachinegun.png" },
	{ "pumpShotgun",   "pump_shotgun.dts",   "Pump Shotgun",  {0.000, -0.516, 0.636}, "Pumpshotgun.png" },
	{ "sportRifle",    "SPORT_RIFLE.3.dts",  "Sport Rifle",   {0.000, -0.540, 1.260}, "Sportrifle.png" }
}

for _, weapon in ipairs(weaponItems) do
	local name, model, uiName, grip, icon = weapon[1], weapon[2], weapon[3], weapon[4], weapon[5]
	local typeID = newItemType(name, folder .. model, scale, scale, scale, uiName, folder .. icon)

	--The mount point node of each model is where its owner's hand goes. These models are built the
	--way setItemHand expects already, +Y out of the hand and -Z down the barrel, so nothing is turned
	setItemHand(typeID, grip[1], grip[2], grip[3], 0, 0, 0)
end

--How the guns behave, one file each like the .cs files they came from
dofile(folder .. "Support_Weapons.lua")
dofile(folder .. "Weapon_Pistol.lua")
dofile(folder .. "Weapon_Submachinegun.lua")
dofile(folder .. "Weapon_PumpShotgun.lua")
dofile(folder .. "Weapon_SportRifle.lua")

--Everything else the add-on ships, as models to look at until there's more to do with them
dofile(folder .. "Models.lua")

--Ammo pickups aren't in yet, so everyone starts with spares to reload out of
local startingAmmo = { ["9MM"] = 70, ["shotgun"] = 24, ["556"] = 40 }

function weaponPackageGiveAmmo(client)
	for ammoType, amount in pairs(startingAmmo) do
		setReserveAmmo(client, ammoType, amount)
	end

	return client
end
registerEventListener("ClientJoin", "weaponPackageGiveAmmo")

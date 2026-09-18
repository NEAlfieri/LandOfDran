--[[
	Weapon_Package_Tier1

	Tier+Tactical Tier 1, by Bushido and Space Guy, ported from the Blockland add-on that ships in
	this folder. The models are loaded straight out of its .dts files (see the DTS models section of
	LuaAPI.md), and the guns are rebuilt in Lua from the datablocks in its .cs files.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Weapon_Package_Tier1/Weapon_Package_Tier1.lua")

	What works: firing, the animations, sounds and muzzle effects each weapon came with, real
	projectiles for the weapons that had them, the casings they throw out, magazines and reloading,
	spare ammo per type, and the ammo boxes that refill it, see Item_Ammo.lua.

	Their bullet is the plain Gun's, add-ons/Weapon_Gun/bullet.dts in their datablocks, so this needs
	Weapon_Gun and loads it if it isn't in yet, the way ForceRequiredAddOn did. The sport rifle is
	the one exception: its datablock fires its own rifle_round.dts.

	A shot that lands on someone takes the weapon's damage field, the directDamage of its datablock,
	off their health (Support_Weapons.lua's hurtIfPlayer calls Damage.lua's damagePlayer), without the
	headshot multiplier the sport rifle had.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--The sounds and particles first, since the weapons name them
dofile(folder .. "Effects.lua")

--The sport rifle's round, its SportRifleProjectile's rifle_round.dts. Everything else fires the
--plain Gun's bullet, see below
ttRifleRound = newDynamicType("ttRifleRound", folder .. "RIFLE_ROUND.dts", scale, scale, scale)

--The pump shotgun's spent shell, PumpShotgunShellDebris. The pistol and submachine gun throw out the
--plain Gun's gunShell
ttShotgunShell = newDynamicType("ttShotgunShell", folder .. "bushido's_shell_shotgun.dts", scale, scale, scale)

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

--The plain Gun, whose bullet and casing these fire. Its own script loads the support above if it's first
if gunBullet == nil then
	dofile("Add-ons/Weapon_Gun/Weapon_Gun.lua")
end
dofile(folder .. "Weapon_Pistol.lua")
dofile(folder .. "Weapon_Submachinegun.lua")
dofile(folder .. "Weapon_PumpShotgun.lua")
dofile(folder .. "Weapon_SportRifle.lua")

--The ammo boxes, which fill the spare ammo those reload out of, and what everyone starts with
dofile(folder .. "Item_Ammo.lua")

--Everything else the add-on ships, as models to look at until there's more to do with them
dofile(folder .. "Models.lua")

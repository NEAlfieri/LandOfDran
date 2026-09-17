--[[
	Models

	The rest of the add-on's .dts files. The four working guns are registered in
	Weapon_Package_Tier1.lua; these are the ammo boxes, the spare weapon models the package ships
	alongside the ones its datablocks actually use, and the grenade. Nothing shoots them yet, so
	they're here to be looked at and picked up, and so dropWeaponPackage can put one of each out.

	Loaded from Weapon_Package_Tier1.lua.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"
local scale = 2

--Everything the package ships that isn't one of the four working guns. The weapon models in here
--are older versions the datablocks don't name: handgun.2 and the two other sport rifles
local extraModels = {
	{ "ttHandgun2",       "handgun.2.dts",                 "Handgun" },
	{ "ttShellShotgun",   "bushido's_shell_shotgun.dts",   "Shell Shotgun" },
	{ "ttSportRifle1",    "sport_rifle.dts",               "Sport Rifle (old)" },
	{ "ttSportRifle2",    "SPORT_RIFLE.2.dts",             "Sport Rifle (older)" },
	{ "ttAmmo9mm",        "AMMO_9MM.dts",                  "9mm Ammo" },
	{ "ttAmmo556",        "AMMO_556.dts",                  "5.56 Ammo" },
	{ "ttAmmo708",        "AMMO_708.dts",                  "7.08 Ammo" },
	{ "ttAmmoMagnum",     "AMMO_MAGNUM.dts",               "Magnum Ammo" },
	{ "ttAmmoRifle",      "AMMO_RIFLE.dts",                "Rifle Ammo" },
	{ "ttAmmoShotgun",    "AMMO_SHOTGUN.dts",              "Shotgun Ammo" },
	{ "ttAmmoGrenade",    "AMMO_GRENADE.dts",              "Grenade Ammo" },
	{ "ttAmmoRocket",     "AMMO_ROCKET.dts",               "Rocket Ammo" },
	{ "ttAmmoGroup",      "AMMO_GROUP.dts",                "Ammo Crate" },
	{ "ttAmmoDrop",       "AMMO_PLAYER_DROP.dts",          "Dropped Ammo" },
	{ "ttAmmoBolt",       "ammo_bolt_.dts",                "Bolts" },
	{ "ttGrenade",        "GRENADE_PROJECTILE.1.dts",      "Grenade" }
}

for _, model in ipairs(extraModels) do
	newItemType(model[1], folder .. model[2], scale, scale, scale, model[3], "")
end

--One of every model the add-on came with, the guns first
local everything = {
	"pistol", "submachinegun", "pumpShotgun", "sportRifle"
}

for _, model in ipairs(extraModels) do
	everything[#everything + 1] = model[1]
end

--[[
	Drops one of every model the add-on came with onto the ground, laid out in rows so none of them
	overlap, and returns the items it made. x and z are the near corner of the patch of ground to
	use, and each one is dropped from just above whatever is under that spot, so they land on
	bricks too.
]]
function dropWeaponPackage(x, z, perRow, spacing)
	x = x or 20
	z = z or 0
	perRow = perRow or 5
	--The longest of these models is about five studs, so seven apart leaves a gap between them
	spacing = spacing or 7

	local dropped = {}

	for i, name in ipairs(everything) do
		local spotX = x + ((i - 1) % perRow) * spacing
		local spotZ = z + math.floor((i - 1) / perRow) * spacing

		--Straight down from well overhead to find whatever it should be resting on
		local _, _, groundY = raycast(spotX, 300, spotZ, spotX, -300, spotZ)

		--A spot with nothing under it at all still gets its item, just dropped from where it was asked for
		local dropY = groundY and (groundY + 1) or 25

		dropped[#dropped + 1] = createItem(getDynamicType(name), spotX, dropY, spotZ)
	end

	info("Dropped " .. #dropped .. " models from " .. folder)

	return dropped
end

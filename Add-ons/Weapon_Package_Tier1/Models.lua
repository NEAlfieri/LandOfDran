--[[
	Models

	The rest of the add-on's .dts files that nothing else here uses: the handgun model the package
	ships alongside the pistol its datablock names, and the grenade, whose weapon is in another tier.
	Nothing shoots them, so they're here to be looked at and picked up, and so dropWeaponPackage can
	put one of everything out. They're given no uiName, which keeps them out of the wrench dialog's
	list of items a brick can offer: there's no reason to hand these out.

	The four working guns are registered in Weapon_Package_Tier1.lua, and the ammo boxes in Item_Ammo.lua.

	Loaded from Weapon_Package_Tier1.lua.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"
local scale = 2

local extraModels = {
	{ "ttHandgun2",       "handgun.2.dts" },
	{ "ttShellShotgun",   "bushido's_shell_shotgun.dts" },
	{ "ttGrenade",        "GRENADE_PROJECTILE.1.dts" }
}

for _, model in ipairs(extraModels) do
	newItemType(model[1], folder .. model[2], scale, scale, scale)
end

--One of every model the add-on came with: the guns first, then these, then the ammo boxes
local everything = {
	"pistol", "submachinegun", "pumpShotgun", "sportRifle"
}

for _, model in ipairs(extraModels) do
	everything[#everything + 1] = model[1]
end

for _, name in ipairs(AmmoItemNames or {}) do
	everything[#everything + 1] = name
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

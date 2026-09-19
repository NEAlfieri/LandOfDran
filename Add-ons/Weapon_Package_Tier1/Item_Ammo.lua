--[[
	Item_Ammo, from Item_Ammo.cs and the ammo half of Support_TT_Ammo.cs

	The ammo boxes the package ships, as pickups: walk into one, or click it, and whatever it holds goes
	into your spare ammo, the pool Support_Weapons.lua reloads out of. Nothing goes into a slot of the
	item bar, the way Armor::onCollision took the ammo and then pretended the item had been picked up.

	Each box is an item type with the uiName its datablock had, so a brick's wrench dialog can offer it.
	One floating over a brick is taken the same way, and comes back after AMMO_RESPAWN_MS like an item
	on a Blockland spawn brick did after its itemRespawnTime. One lying on the ground is used up.

	A box that would give nothing, because its owner is already carrying all they can of everything in
	it, is left where it is, which is $Pref::Server::TT::FullAmmoPickUp being off.

	From Ammo_Unified.cs: someone who dies leaves all their spare ammo behind in an ammo_player_drop box
	for whoever gets to it within AMMO_DROP_MS, and starts over with the starting amounts, as Armor::onAdd
	gave every new player. Both hang off System_Damage's killPlayer and respawnPlayer, see ammoHookDamage.

	From the prefs in server.cs: what a client starts with, and the most they can carry, of each type.
	The types past the first three belong to the package's other tiers, whose weapons aren't here, but
	their boxes are, and the Ammo Pile fills them all.

	Loaded from Weapon_Package_Tier1.lua, after Support_Weapons.lua.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"
local scale = 2

--$Pref::Server::TT::Start and ::Max, by ammo type
AmmoStart = { ["9MM"] = 140, ["556"] = 90, ["shotgun"] = 24, ["270"] = 16, ["880"] = 18, ["bomb"] = 3, ["rocket"] = 2, ["708"] = 72, ["bolt"] = 12 }
AmmoMax   = { ["9MM"] = 280, ["556"] = 180, ["shotgun"] = 48, ["270"] = 32, ["880"] = 36, ["bomb"] = 12, ["rocket"] = 12, ["708"] = 144, ["bolt"] = 24 }

--How long until a box taken off a brick is back, a Blockland brick's default itemRespawnTime
local AMMO_RESPAWN_MS = 4000

--How often players are checked against the boxes, and how near one they have to be: across the ground from
--their position, and how far below and above it, which is their feet
local AMMO_TOUCH_MS = 100
local AMMO_TOUCH_ACROSS = 2.5
local AMMO_TOUCH_BELOW = 2
local AMMO_TOUCH_ABOVE = 7

--How far away one can be clicked from, System_Inventory's CLICK_RANGE and REACH for picking an item up
local AMMO_CLICK_RANGE = 60
local AMMO_CLICK_REACH = 10

--How long the box someone leaves behind when they die lasts
local AMMO_DROP_MS = 12000

local DYNAMIC_TYPE_ID = 1

newSoundType("TTAmmoGet", folder .. "ammoget.wav")

--[[
	Every box: its item type, model, the uiName its datablock had, and its TT_ammoPickup lines as type then
	amount, where -1 is as much as can be carried. The 9mm box's line said "9mm", which TorqueScript's case
	blind array names made the same pool as the weapons' "9MM"
]]
local ammoItems = {
	{ "ttAmmo9mm",     "AMMO_9MM.dts",     "Ammo, 9mm",      { {"9MM", 70} } },
	{ "ttAmmo556",     "AMMO_556.dts",     "Ammo, 5.56",     { {"556", 45} } },
	{ "ttAmmoShotgun", "AMMO_SHOTGUN.dts", "Ammo, Buckshot", { {"shotgun", 18} } },
	{ "ttAmmo708",     "AMMO_708.dts",     "Ammo, 7.08",     { {"708", 48} } },
	{ "ttAmmoMagnum",  "AMMO_MAGNUM.dts",  "Ammo, .880M",    { {"880", 12} } },
	{ "ttAmmoRifle",   "AMMO_RIFLE.dts",   "Ammo, .270",     { {"270", 8} } },
	{ "ttAmmoBolt",    "ammo_bolt_.dts",   "Ammo, Bolt",     { {"bolt", 6} } },
	{ "ttAmmoGrenade", "AMMO_GRENADE.dts", "Ammo, Grenade",  { {"bomb", 1} } },
	{ "ttAmmoRocket",  "AMMO_ROCKET.dts",  "Ammo, Rocket",   { {"rocket", 1} } },
	{ "ttAmmoGroup",   "AMMO_GROUP.dts",   "Ammo Pile",      { {"9MM", -1}, {"556", -1}, {"shotgun", -1}, {"270", -1},
	                                                           {"708", -1}, {"880", -1}, {"bomb", -1}, {"rocket", -1}, {"bolt", -1} } }
}

--What each box holds, by the script name of its item type
AmmoPickups = {}

--The script names in the order above, for dropWeaponPackage
AmmoItemNames = {}

for _, box in ipairs(ammoItems) do
	newItemType(box[1], folder .. box[2], scale, scale, scale, box[3])
	AmmoPickups[box[1]] = box[4]
	AmmoItemNames[#AmmoItemNames + 1] = box[1]
end

--ammoDroppedItem, what someone who dies leaves behind. It had no uiName, since what one holds is up to who dropped
--it, so no brick offers it. Each one's contents are kept by its ID in droppedAmmo, and the type itself holds nothing
newItemType("ttAmmoDrop", folder .. "AMMO_PLAYER_DROP.dts", scale, scale, scale)
AmmoPickups["ttAmmoDrop"] = {}
AmmoItemNames[#AmmoItemNames + 1] = "ttAmmoDrop"

local droppedAmmo = {}

--The client's player, the first dynamic they control, or nil, which is also what someone lying dead has
local function playerOf(client)
	if client.dead or client:getNumControlled() == 0 then
		return nil
	end
	return client:getControlledIdx(0)
end

--[[
	TT_addAmmo: gives a client spare ammo of a type, up to the most they can carry of it unless ignoreMax,
	and returns how much of it they really got. An amount of -1 is as much as they can carry
]]
function giveAmmo(client, ammoType, amount, ignoreMax)
	local max = AmmoMax[ammoType]
	if max == nil or amount == nil then
		return 0
	end

	if amount == -1 then
		amount = max
	elseif amount < 0 then
		return 0
	end

	local current = getReserveAmmo(client, ammoType)
	if not ignoreMax then
		amount = math.max(0, math.min(amount, max - current))
	end

	if amount > 0 then
		setReserveAmmo(client, ammoType, current + amount)
	end

	return amount
end

--A box taken off a brick comes back, if the brick is still there and still offering one
function ammoRespawn(brickID)
	--Gone, or wrenched to offer something else or nothing since, in which case it already shows what it should
	local brick = getBrickId(brickID)
	if brick == nil then
		return
	end

	local offered = brick:getItemSpawn()
	if offered ~= nil and brick:getDisplayItem() == nil then
		brick:setItemSpawn(offered)
	end
end

--Hands a client what a box holds. False if there was nothing in it they had room for, and the box is left alone
function takeAmmoItem(client, item)
	local pickups = droppedAmmo[item.id] or AmmoPickups[item:getTypeName()]
	if pickups == nil then
		return false
	end

	local got = {}
	for _, pickup in ipairs(pickups) do
		local amount = giveAmmo(client, pickup[1], pickup[2])
		if amount > 0 then
			got[#got + 1] = "+" .. amount .. " " .. pickup[1]
		end
	end

	if #got < 1 then
		return false
	end

	local player = playerOf(client)
	if player ~= nil then
		player:playSound("TTAmmoGet")
	end

	--The Ammo Pile fills every type, which is too much to list
	if #got > 3 then
		client:centerPrint("Ammo filled", 2000)
	else
		client:centerPrint(table.concat(got, "   "), 2000)
	end

	--And what that makes their gun's counter read, if they're holding one it goes in
	for _, pickup in ipairs(pickups) do
		weaponShowAmmo(client, pickup[1])
	end

	--The brick keeps offering it, so taking its display away leaves it to be put back, see ammoRespawn
	if item:isDisplay() then
		local brick = item:getDisplayBrick()
		if brick ~= nil then
			schedule(AMMO_RESPAWN_MS, "ammoRespawn", brick.id)
		end
	end

	droppedAmmo[item.id] = nil
	item:destroy()
	return true
end

--Armor::onCollision: whoever walks into a box gets it. There's no event for a player touching an item, so
--this looks at where the players and the boxes are
function ammoTouchTick()
	schedule(AMMO_TOUCH_MS, "ammoTouchTick")

	local players = {}
	for i = 0, getNumClients() - 1, 1 do
		local client = getClientIdx(i)
		local player = playerOf(client)
		if player ~= nil then
			local x, y, z = player:getPosition()
			players[#players + 1] = { client = client, x = x, y = y, z = z }
		end
	end

	if #players < 1 then
		return
	end

	--Collected first, since taking one destroys it, which changes what getItemIdx counts through
	local boxes = {}
	for i = 0, getNumItems() - 1, 1 do
		local item = getItemIdx(i)
		if item ~= nil and AmmoPickups[item:getTypeName()] ~= nil and not item:isHeld() then
			boxes[#boxes + 1] = item
		end
	end

	for _, item in ipairs(boxes) do
		local x, y, z = item:getPosition()

		for _, player in ipairs(players) do
			local acrossX, acrossZ, up = x - player.x, z - player.z, y - player.y

			if acrossX * acrossX + acrossZ * acrossZ <= AMMO_TOUCH_ACROSS * AMMO_TOUCH_ACROSS
				and up >= -AMMO_TOUCH_BELOW and up <= AMMO_TOUCH_ABOVE
				and takeAmmoItem(player.client, item) then
				break
			end
		end
	end
end
schedule(AMMO_TOUCH_MS, "ammoTouchTick")

--[[
	A click on a box takes it too, from as far as any other item can be picked up from. This add-on loads
	before System_Inventory, so this hears the click first, and hands it on without its left button: to
	System_Inventory a box is an item like any other, and it would put the box itself in a slot of the item bar
]]
function ammoClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if (mask & 1) == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local hit, x, y, z = client:getCursorItem(AMMO_CLICK_RANGE)
	if hit == nil or hit.type ~= DYNAMIC_TYPE_ID or not hit:isItem() or hit:isHeld() or AmmoPickups[hit:getTypeName()] == nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local player = playerOf(client)
	if player ~= nil then
		local px, py, pz = player:getPosition()
		local awayX, awayY, awayZ = x - px, y - py, z - pz

		if awayX * awayX + awayY * awayY + awayZ * awayZ <= AMMO_CLICK_REACH * AMMO_CLICK_REACH
			and not takeAmmoItem(client, hit) then
			client:centerPrint("You can't carry any more of that ammo.", 2000)
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask & ~1
end
registerEventListener("ClientClick", "ammoClick")

--Everyone starts with what the package's Starting Ammo prefs gave them
function ammoGiveStarting(client)
	for ammoType, amount in pairs(AmmoStart) do
		setReserveAmmo(client, ammoType, amount)
	end

	return client
end
registerEventListener("ClientJoin", "ammoGiveStarting")

--A dropped box nobody got to in time
function ammoDropExpired(itemID)
	local contents = droppedAmmo[itemID]
	droppedAmmo[itemID] = nil

	if contents ~= nil and dynamicExists(itemID) then
		contents.item:destroy()
	end
end

--Everything a client was carrying goes into a box thrown from where they died
local function dropAmmoOnDeath(client, x, y, z, velX, velY, velZ)
	local contents = {}
	for ammoType, _ in pairs(AmmoMax) do
		local amount = getReserveAmmo(client, ammoType)
		if amount > 0 then
			contents[#contents + 1] = { ammoType, amount }
			setReserveAmmo(client, ammoType, 0)
		end
	end

	if #contents < 1 then
		return
	end

	local item = createItem(getDynamicType("ttAmmoDrop"), x, y + 2, z)
	if item == nil then
		return
	end

	--getRandom(-8, 8) across and 4 up on top of however they were moving, in Blockland's units of two studs
	item:setVelocity(velX + math.random(-16, 16), velY + 8, velZ + math.random(-16, 16))
	item:activate()

	contents.item = item
	droppedAmmo[item.id] = contents
	schedule(AMMO_DROP_MS, "ammoDropExpired", item.id)
end

--[[
	System_Damage has no events for dying or coming back, and loads after this does, so once everything is in its
	killPlayer and respawnPlayer are wrapped the way a Blockland package wrapped Armor::onDisabled and
	Armor::onAdd. Without System_Damage nobody dies, and none of this is needed
]]
function ammoHookDamage()
	if killPlayer == nil or respawnPlayer == nil then
		return
	end

	local damageKillPlayer, damageRespawnPlayer = killPlayer, respawnPlayer

	function killPlayer(player, attacker, fromX, fromY, fromZ)
		local client, x, y, z, velX, velY, velZ = nil, 0, 0, 0, 0, 0, 0
		if player ~= nil and player.type == DYNAMIC_TYPE_ID and player:getNumControllers() > 0 then
			client = player:getControllerIdx(0)
			x, y, z = player:getPosition()
			velX, velY, velZ = player:getVelocity()
		end

		damageKillPlayer(player, attacker, fromX, fromY, fromZ)

		if client ~= nil and client.dead then
			dropAmmoOnDeath(client, x, y, z, velX, velY, velZ)
		end
	end

	function respawnPlayer(client)
		local wasDead = client.dead
		damageRespawnPlayer(client)

		if wasDead and not client.dead then
			ammoGiveStarting(client)
		end
	end
end
schedule(1, "ammoHookDamage")

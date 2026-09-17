--[[
	Support_Weapons

	What every gun in the package has in common: firing, the round leaving the barrel, reloading, and
	the ammo it has left. The Blockland originals do all of this with a state machine per weapon, one
	numbered state per moment of the shot. There's no state machine here: a shot plays its sounds and
	effects, then schedules whatever comes next, and the trigger arrives as an event.

	A weapon is registered with a table of what makes it different from the others, so porting one
	from its .cs is a matter of reading the timings out of the states and writing them down:

		stateTimeoutValue[4] = 0.05;  (Fire)      \
		stateTimeoutValue[6] = 0.001; (Wait)       >  fireDelayMS, how long until it can fire again
		plus whatever the trigger has to do          /

		TT_maxAmmo = 12;                          ->  magazineSize = 12
		TT_ammoType = "9MM";                      ->  ammoType = "9MM"
		TT_raycastSpreadAmt = 0.0009;             ->  spread = 0.0009
		TT_raycastWeaponRange = 200;              ->  range = 200
		TT_raycastSpreadCount = 1;                ->  pellets = 1

	A weapon shoots one of two ways, the same way it did in Blockland. Most of them have a projectile
	field and fire a real round that travels and drops, out of the ProjectileData they came with. The
	pistol has none and is hitscan instead, like the raycasting support it came with, with a tracer
	fired from the muzzle toward wherever the shot landed so there's still something to watch.

	Nothing takes damage yet, so landing a shot only leaves an effect.

	A weapon with a predicted field also hands its owner's game the track for its next click, so the
	shot is seen and heard the instant the button goes down instead of a round trip later. The server
	still decides everything: it only pre-sends what the click looks like. See client:setClickAction
	in LuaAPI.md, and sendClickAction below.

	Loaded from Weapon_Package_Tier1.lua.
]]

--Every registered weapon, by the script name of its item type
Weapons = {}

--Rounds left in the magazine of a particular item, by that item's ID. Each gun keeps its own count,
--so two of the same weapon don't share a magazine
local magazines = {}

--Spare ammo a client has of each type, by client ID then ammo type name
local reserves = {}

--What each client's trigger is doing, by client ID
local firing = {}

--How far a tracer reaches if the shot hit nothing at all
local MAX_TRACE = 300

--How fast a tracer travels, in studs per second. Fast enough to read as a bullet, slow enough to see
local TRACER_SPEED = 320

--Tracers are fired as projectiles, and this tells them apart from anything else in ProjectileHit
local TRACER_TAG = "TT_tracer"

--What the physics world pulls things down at, for scaling a round's drop by its gravityScale
WORLD_GRAVITY = -70

--How long the ammo counter stays on screen, and how often it can be shown again. Each print is its
--own line on screen rather than replacing the last, so an automatic weapon would stack up a column
--of them without this
local AMMO_PRINT_MS = 2500
local AMMO_PRINT_EVERY_MS = 700

--Clients whose counter was shown too recently to show again, by client ID
local ammoPrintBlocked = {}

function registerWeapon(name, weapon)
	weapon.name = name
	Weapons[name] = weapon
end

--Spare ammo of a type, and how much of it there is
function getReserveAmmo(client, ammoType)
	local held = reserves[client:getID()]
	if held == nil or ammoType == nil then
		return 0
	end
	return held[ammoType] or 0
end

function setReserveAmmo(client, ammoType, amount)
	if ammoType == nil then
		return
	end

	local clientID = client:getID()
	if reserves[clientID] == nil then
		reserves[clientID] = {}
	end

	reserves[clientID][ammoType] = math.max(0, math.floor(amount))
end

function addReserveAmmo(client, ammoType, amount)
	setReserveAmmo(client, ammoType, getReserveAmmo(client, ammoType) + amount)
end

--Rounds in a particular gun, starting off with a full magazine like one picked up in Blockland
function getMagazine(item, weapon)
	local rounds = magazines[item.id]
	if rounds == nil then
		rounds = weapon.magazineSize
		magazines[item.id] = rounds
	end
	return rounds
end

local function setMagazine(item, rounds)
	magazines[item.id] = math.max(0, math.floor(rounds))
end

--The weapon a client is holding, and the item it is, or nil if they aren't holding a gun
local function heldWeapon(client)
	local item = client:getHeldItem()
	if item == nil then
		return nil, nil
	end

	local weapon = Weapons[item:getTypeName()]
	if weapon == nil then
		return nil, nil
	end

	return weapon, item
end

function weaponAmmoPrintReady(clientID)
	ammoPrintBlocked[clientID] = nil
end

local function showAmmo(client, weapon, item, always)
	if not weapon.showAmmo then
		return
	end

	local clientID = client:getID()

	if not always and ammoPrintBlocked[clientID] then
		return
	end

	ammoPrintBlocked[clientID] = true
	schedule(AMMO_PRINT_EVERY_MS, "weaponAmmoPrintReady", clientID)

	client:centerPrint(getMagazine(item, weapon) .. " / " .. getReserveAmmo(client, weapon.ammoType), AMMO_PRINT_MS)
end

--[[
	Where the end of the barrel is.

	A held item has no transform of its own on the server: the client draws it in its owner's hand,
	while the server side item stays wherever it was last dropped, usually the origin. So the muzzle
	is worked out from the player's eyes and the way they're looking, which is what Inventory.lua
	does for the launcher's barrel too.

	Each weapon gives muzzleFromHand, the end of its barrel measured from the point the hand holds
	it by, which is its muzzle point node less its mount point node. Held unturned, the model's +Y
	is up out of the hand and its -Z is the way its owner faces, so those turn into the camera's up
	and forward here.

	An item:getNodePosition("muzzlepoint") in the engine would replace this guesswork with the real
	thing, and would fix the muzzle drifting when the camera is in third person.
]]

--Where a player's eyes are above their position, and where their hand sits from there
local EYE_HEIGHT = 4.8
local HAND_RIGHT = 1.1
local HAND_UP = -0.9
local HAND_AHEAD = 0.7

function weaponMuzzlePosition(client, weapon)
	local player = client:getControlledIdx(0)
	if player == nil then
		return nil
	end

	local px, py, pz = player:getPosition()
	local dirX, dirY, dirZ = client:getCameraDirection()

	--The camera's right, kept level so the gun doesn't roll when looking up or down
	local rightX, rightZ = -dirZ, dirX
	local rightLength = math.sqrt(rightX * rightX + rightZ * rightZ)
	if rightLength < 0.001 then
		rightX, rightZ, rightLength = 1, 0, 1
	end
	rightX, rightZ = rightX / rightLength, rightZ / rightLength

	--And its up, across the other two
	local upX, upY, upZ = -rightZ * dirY, rightZ * dirX - rightX * dirZ, rightX * dirY

	local offset = weapon.muzzleFromHand or {0, 0, 0}
	local outRight = HAND_RIGHT + offset[1]
	local outUp = HAND_UP + offset[2]
	--The barrel runs down -Z, so a negative offset there is further forward
	local outAhead = HAND_AHEAD - offset[3]

	return px + rightX * outRight + upX * outUp + dirX * outAhead,
	       py + EYE_HEIGHT + upY * outUp + dirY * outAhead,
	       pz + rightZ * outRight + upZ * outUp + dirZ * outAhead
end

--A direction knocked off course by up to spread, the way a shotgun throws its pellets apart
local function spreadDirection(dirX, dirY, dirZ, spread)
	if spread == nil or spread <= 0 then
		return dirX, dirY, dirZ
	end

	--Two axes across the shot to push it around in. Any pair at right angles to it will do
	local sideX, sideY, sideZ = -dirZ, 0, dirX
	local sideLength = math.sqrt(sideX * sideX + sideZ * sideZ)
	if sideLength < 0.0001 then
		sideX, sideY, sideZ, sideLength = 1, 0, 0, 1
	end
	sideX, sideY, sideZ = sideX / sideLength, sideY / sideLength, sideZ / sideLength

	local upX = dirY * sideZ - dirZ * sideY
	local upY = dirZ * sideX - dirX * sideZ
	local upZ = dirX * sideY - dirY * sideX

	--Blockland's spread numbers are tiny, and this is about the angle they came out as there
	local angle = spread * 90
	local aroundX = (math.random() * 2 - 1) * angle
	local aroundY = (math.random() * 2 - 1) * angle

	local outX = dirX + sideX * aroundX + upX * aroundY
	local outY = dirY + sideY * aroundX + upY * aroundY
	local outZ = dirZ + sideZ * aroundX + upZ * aroundY

	local length = math.sqrt(outX * outX + outY * outY + outZ * outZ)
	if length < 0.0001 then
		return dirX, dirY, dirZ
	end

	return outX / length, outY / length, outZ / length
end

--[[
	Hands a client's game the track for their next click with this weapon, which it plays the moment
	they click rather than waiting to hear back. The server always sends whatever the *next* click
	does, so an empty gun gets the dry click track: nothing about magazines or reloading has to be
	known by their game at all.

	The effects here are the shooter's own, and arrive the instant they click. Everyone else gets the
	ones the server makes when the shot really happens, see fireOnce: the animation, a real emitter,
	and a real light. The shooter sees both of those, so the server's light is turned down while a
	weapon is predicted. Only the sound is kept off the shooter, since hearing a shot twice is much
	more noticeable than a slightly brighter flash.
]]
function sendClickAction(client, weapon, item)
	if weapon.predicted == nil or client == nil or item == nil then
		return
	end

	local rounds = getMagazine(item, weapon)
	local steps = {}

	if rounds > 0 then
		if weapon.sounds ~= nil and weapon.sounds.fire ~= nil then
			steps[#steps + 1] = { at = 0, sound = weapon.sounds.fire }
		end

		if weapon.fireAnimation ~= nil then
			steps[#steps + 1] = { at = 0, animation = weapon.fireAnimation }
		end

		if weapon.muzzleEmitter ~= nil and weapon.muzzleOffset ~= nil then
			steps[#steps + 1] = { at = 0, emitter = weapon.muzzleEmitter,
				forMS = weapon.muzzleEmitterMS or 60, offset = weapon.muzzleOffset }
		end

		--The flash lights up what's around the barrel for as long as it lasts
		if weapon.muzzleLight ~= nil and weapon.muzzleOffset ~= nil then
			steps[#steps + 1] = { at = 0, light = weapon.muzzleLight.color,
				brightness = weapon.muzzleLight.brightness, coronaWidth = weapon.muzzleLight.coronaWidth,
				forMS = weapon.muzzleLight.forMS or 50, offset = weapon.muzzleOffset }
		end

		--Anything the weapon wants later in the shot, like the pistol's slide coming back
		for _, step in ipairs(weapon.predicted) do
			steps[#steps + 1] = step
		end
	elseif weapon.sounds ~= nil and weapon.sounds.empty ~= nil then
		steps[#steps + 1] = { at = 0, sound = weapon.sounds.empty }
	end

	if #steps < 1 then
		client:setClickAction()
		return
	end

	--repeatMS is how fast the weapon can fire at all, which their game uses to throw away a click
	--that comes too soon, exactly as the server does.
	--repeatLimit is how many more shots the track is good for before the server has to send another,
	--which is simply what's left in the magazine. It covers an automatic weapon carrying on while
	--held and someone clicking faster than the round trip; without it their game runs ahead of the
	--server and shows shots the magazine didn't have. A dry click costs no ammo, so it isn't limited
	local repeatMS = weapon.fireDelayMS
	local repeatLimit = 255
	if rounds > 0 then
		repeatLimit = math.min(255, math.max(0, rounds - 1))
	end

	client:setClickAction(item, { repeatMS = repeatMS, repeatLimit = repeatLimit, steps = steps })
end

--Plays a sound for everyone but the shooter, whose own game played it the moment they clicked
function playSoundForOthers(shooter, name, x, y, z)
	local shooterID = shooter:getID()

	for i = 0, getNumClients() - 1, 1 do
		local other = getClientIdx(i)
		if other:getID() ~= shooterID then
			if x ~= nil then
				other:playSound(name, x, y, z)
			else
				other:playSound(name)
			end
		end
	end
end

--One shot out of the barrel. Most of these weapons fire a real projectile that travels and drops,
--the way their ProjectileData datablocks did; the pistol is hitscan, like the raycasting support it
--came with, and gets a tracer fired along the shot so there's still something to watch
local function fireOneShot(client, weapon, item, player)
	local dirX, dirY, dirZ = client:getCameraDirection()
	dirX, dirY, dirZ = spreadDirection(dirX, dirY, dirZ, weapon.spread)

	local muzzleX, muzzleY, muzzleZ = weaponMuzzlePosition(client, weapon)
	if muzzleX == nil then
		return nil
	end

	if weapon.projectile ~= nil then
		local projectileType = getDynamicType(weapon.projectile)
		if projectileType == nil then
			return nil
		end

		local speed = weapon.projectileSpeed or 200
		local shot = addProjectile(projectileType, muzzleX, muzzleY, muzzleZ,
			dirX * speed, dirY * speed, dirZ * speed, weapon.name, player)

		--gravityMod in the originals, how much of normal gravity the round feels on its way out
		if shot ~= nil and weapon.gravityScale ~= nil then
			shot:setGravity(0, WORLD_GRAVITY * weapon.gravityScale, 0)
		end

		return shot
	end

	--Hitscan: where the shot lands is worked out right away along the crosshair
	local camX, camY, camZ = client:getCameraPosition()
	local range = weapon.range or MAX_TRACE
	local hit, hitX, hitY, hitZ = raycast(camX, camY, camZ,
		camX + dirX * range, camY + dirY * range, camZ + dirZ * range, player)

	--Nothing in the way, so the tracer runs out at the weapon's range
	if hitX == nil then
		hitX, hitY, hitZ = camX + dirX * range, camY + dirY * range, camZ + dirZ * range
	else
		weaponImpactEffect(weapon, hitX, hitY, hitZ)
	end

	--The tracer starts at the barrel rather than the camera, and heads for wherever the shot landed
	local toX, toY, toZ = hitX - muzzleX, hitY - muzzleY, hitZ - muzzleZ
	local distance = math.sqrt(toX * toX + toY * toY + toZ * toZ)
	if distance < 0.001 then
		toX, toY, toZ, distance = dirX, dirY, dirZ, 1
	end

	local tracerType = weapon.tracer ~= nil and getDynamicType(weapon.tracer) or nil
	if tracerType ~= nil then
		addProjectile(tracerType, muzzleX, muzzleY, muzzleZ,
			toX / distance * TRACER_SPEED, toY / distance * TRACER_SPEED, toZ / distance * TRACER_SPEED,
			TRACER_TAG, player)
	end

	return hit
end

function weaponRemoveEmitter(emitter)
	if emitter ~= nil then
		emitter:destroy()
	end
end

function weaponRemoveLight(light)
	if light ~= nil then
		light:destroy()
	end
end

--The puff a shot leaves where it lands
function weaponImpactEffect(weapon, x, y, z)
	if weapon.impactEmitter == nil then
		return
	end

	local puff = addEmitter(weapon.impactEmitter, x, y, z)
	if puff ~= nil then
		schedule(400, "weaponRemoveEmitter", puff)
	end
end

--Where a round lands. A tracer is only something to look at, so it leaves nothing behind; a real
--round from one of the projectile weapons puffs where it hits. The engine removes either one
function weaponProjectileHit(projectile, hit, x, y, z, tag)
	if tag ~= TRACER_TAG then
		local weapon = Weapons[tag]
		if weapon ~= nil then
			weaponImpactEffect(weapon, x, y, z)
		end
	end

	return projectile, hit, x, y, z, tag
end
registerEventListener("ProjectileHit", "weaponProjectileHit")

--Everything one pull of the trigger does
local function fireOnce(client, weapon, item)
	local player = client:getControlledIdx(0)
	if player == nil then
		return false
	end

	local rounds = getMagazine(item, weapon)

	--An empty gun clicks instead, and starts loading a fresh magazine if there's one to load
	if rounds <= 0 then
		if weapon.sounds ~= nil and weapon.sounds.empty ~= nil then
			item:playSound(weapon.sounds.empty)
		end

		startReload(client, weapon, item)
		return false
	end

	setMagazine(item, rounds - 1)

	local muzzleX, muzzleY, muzzleZ = weaponMuzzlePosition(client, weapon)

	local predicted = weapon.predicted ~= nil

	--The shooter's own game already played all of this the moment they clicked, so playing it again
	--here would double it up for them. Everyone else still needs it, and gets it one at a time
	if weapon.sounds ~= nil and weapon.sounds.fire ~= nil then
		if predicted then
			playSoundForOthers(client, weapon.sounds.fire, muzzleX, muzzleY, muzzleZ)
		else
			item:playSound(weapon.sounds.fire)
		end
	end

	--Everyone sees the gun work, the shooter's own game having already started the same animation
	if weapon.fireAnimation ~= nil then
		item:playAnimation(weapon.fireAnimation)
	end

	--The flash off the end of the barrel, left where the shot happened. This one is a real object,
	--so it's what everyone else sees; the shooter already had their own a moment earlier
	if muzzleX ~= nil and weapon.muzzleEmitter ~= nil then
		local flash = addEmitter(weapon.muzzleEmitter, muzzleX, muzzleY, muzzleZ)
		if flash ~= nil then
			schedule(weapon.muzzleEmitterMS or 60, "weaponRemoveEmitter", flash)
		end
	end

	--And the light it throws, likewise for everyone. Dimmer than the predicted one, since the shooter
	--is seeing both at once and only this one reaches anybody else
	if muzzleX ~= nil and weapon.muzzleLight ~= nil then
		local color = weapon.muzzleLight.color
		local brightness = weapon.muzzleLight.brightness * (predicted and 0.5 or 1)
		local light = createLight(muzzleX, muzzleY, muzzleZ, color[1], color[2], color[3],
			brightness, 0, weapon.muzzleLight.coronaWidth or 0)

		if light ~= nil then
			schedule(weapon.muzzleLight.forMS or 50, "weaponRemoveLight", light)
		end
	end

	for pellet = 1, (weapon.pellets or 1) do
		fireOneShot(client, weapon, item, player)
	end

	--The shove a gun gives whoever is firing it, which is all the recoil there is for now
	if weapon.knockback ~= nil and weapon.knockback > 0 then
		local dirX, dirY, dirZ = client:getCameraDirection()
		local velX, velY, velZ = player:getVelocity()
		player:setVelocity(velX - dirX * weapon.knockback, velY - dirY * weapon.knockback, velZ - dirZ * weapon.knockback)
	end

	showAmmo(client, weapon, item)

	if weapon.onFire ~= nil then
		weapon.onFire(client, weapon, item)
	end

	--A round has gone, so their game is told what the next click does now. Without this it keeps
	--predicting a full shot after the magazine is empty, which looks like firing straight through it
	sendClickAction(client, weapon, item)

	return true
end

--Reloading is a few scheduled steps: the magazine out, the new one in, and the gun ready again
function startReload(client, weapon, item)
	local clientID = client:getID()
	local state = firing[clientID]
	if state == nil or state.reloading then
		return
	end

	local rounds = getMagazine(item, weapon)
	if rounds >= weapon.magazineSize then
		return
	end

	--Nothing to load
	if getReserveAmmo(client, weapon.ammoType) <= 0 then
		return
	end

	state.reloading = true
	state.reloadGeneration = (state.reloadGeneration or 0) + 1

	--Every click is thrown away until the magazine is in, so their game predicts nothing meanwhile
	if weapon.predicted ~= nil then
		client:setClickAction()
	end

	if weapon.sounds ~= nil and weapon.sounds.reloadStart ~= nil then
		item:playSound(weapon.sounds.reloadStart)
	end

	if weapon.reloadAnimation ~= nil then
		item:playAnimation(weapon.reloadAnimation)
	end

	schedule(weapon.reloadMS or 1200, "weaponReloadFinished", clientID, state.reloadGeneration)
end

--The fresh magazine is in: whatever was left in the gun goes back to the spares, as it does in T+T
function weaponReloadFinished(clientID, generation)
	local state = firing[clientID]
	if state == nil or not state.reloading or state.reloadGeneration ~= generation then
		return
	end

	state.reloading = false

	local client = state.client
	local weapon, item = heldWeapon(client)

	--They put the gun away partway through, so the reload is dropped
	if weapon == nil or item == nil or item.id ~= state.itemID then
		return
	end

	local pool = getReserveAmmo(client, weapon.ammoType) + getMagazine(item, weapon)
	local loaded = math.min(weapon.magazineSize, pool)

	setMagazine(item, loaded)
	setReserveAmmo(client, weapon.ammoType, pool - loaded)

	if weapon.sounds ~= nil and weapon.sounds.reloadEnd ~= nil then
		item:playSound(weapon.sounds.reloadEnd)
	end

	sendClickAction(client, weapon, item)

	--A finished reload always shows, since it's the moment the count jumps
	showAmmo(client, weapon, item, true)
end

--An automatic weapon keeps firing while the trigger is held, one shot per fireDelayMS
function weaponFireTick(clientID, generation)
	local state = firing[clientID]
	if state == nil or state.generation ~= generation then
		return
	end

	state.scheduleID = nil

	local client = state.client
	local weapon, item = heldWeapon(client)

	--Let go of the trigger, changed weapon, or in the middle of a reload
	if weapon == nil or item == nil or item.id ~= state.itemID or not state.triggerDown or state.reloading then
		return
	end

	if not fireOnce(client, weapon, item) then
		return
	end

	if weapon.automatic then
		state.scheduleID = schedule(weapon.fireDelayMS, "weaponFireTick", clientID, generation)
	end
end

function weaponTriggerDown(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	--Left mouse only
	if (mask & 1) == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local weapon, item = heldWeapon(client)
	if weapon == nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local clientID = client:getID()
	local state = firing[clientID]

	--Picked up a different gun since last time, so none of the old shot is still running. Items are
	--compared by ID: getHeldItem gives back a new wrapper each call, so the same gun isn't the same value
	if state == nil or state.itemID ~= item.id then
		state = { client = client, item = item, itemID = item.id, generation = 0, reloading = false }
		firing[clientID] = state
	end

	if state.reloading then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	--A gun that's still recovering from the last shot does nothing until it's ready
	if state.scheduleID ~= nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	state.triggerDown = true
	state.generation = state.generation + 1

	if fireOnce(client, weapon, item) then
		--Semi automatic guns still wait out their fire delay, they just don't shoot again on their own
		state.scheduleID = schedule(weapon.fireDelayMS, "weaponFireTick", clientID, state.generation)
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "weaponTriggerDown")

function weaponTriggerUp(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if (mask & 1) ~= 0 then
		local state = firing[client:getID()]
		if state ~= nil then
			state.triggerDown = false
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClickRelease", "weaponTriggerUp")

--[[
	The track has to already be on its way before the first click, and there's no event yet for a
	client picking another item, so this watches for a change. It's the one thing here that polls;
	an ItemEquipped event in the engine would replace it.
]]
local watchedItem = {}

function weaponClickActionPoll()
	for i = 0, getNumClients() - 1, 1 do
		local client = getClientIdx(i)
		local clientID = client:getID()
		local weapon, item = heldWeapon(client)
		local nowHolding = item ~= nil and item.id or nil

		if watchedItem[clientID] ~= nowHolding then
			watchedItem[clientID] = nowHolding

			if weapon ~= nil and weapon.predicted ~= nil then
				sendClickAction(client, weapon, item)
			else
				--Holding something that isn't a predicted weapon, so nothing is predicted
				client:setClickAction()
			end
		end
	end

	schedule(250, "weaponClickActionPoll")
end
schedule(250, "weaponClickActionPoll")

function weaponClientLeft(client)
	firing[client:getID()] = nil
	reserves[client:getID()] = nil
	watchedItem[client:getID()] = nil
	return client
end
registerEventListener("ClientLeave", "weaponClientLeft")

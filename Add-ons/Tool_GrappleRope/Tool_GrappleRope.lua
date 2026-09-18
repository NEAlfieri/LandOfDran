--[[
	Tool_GrappleRope

	The Grapple Rope, by Qwertyuiopas, SolarFlare, and Demian, ported from the Blockland add-on that
	ships in this folder. The tool is the print gun's shape in orange, and the rope is rebuilt from
	the datablocks and the swing from the GrappleRope function in its Tool_GrappleRope.cs.

	The whole add-on loads from one line in serverstart.lua:

		dofile("Add-ons/Tool_GrappleRope/Tool_GrappleRope.lua")

	Holding left mouse fires a hook, bowFire and all, at whatever the crosshair is on. Where it
	lands is the anchor, as long as nothing stands between the player and it, and the rope is as
	long as the player is from there at that moment. For as long as the button stays down the rope
	holds them within that length of the anchor, so stepping off something swings them, and letting
	go drops them wherever they are. The rope itself is drawn the way the original drew it: links
	fired from the gun toward the anchor, each leaving a trail of dark dots behind it.

	The states of GrappleRopeImage are one shot on the trigger, then Hold every hundredth of a
	second until it comes up, then Release; here the shot is the click, the hold is a scheduled
	tick, and the release is the button coming back up. GrappleRopeProjectile and ChainProjectile
	had no shape, so the hook and the links are projectiles with a hidden model, see chain.txt.

	What isn't here: $Pref::Server::GrappleRopeAnywhere, which is only ever 1 in the original
	(its 0 setting wanted objects flagged GrappleRopeTarget, which nothing does), and
	weaponSwitchSound on its Activate state, since there's no event for picking an item yet.
	Blockland's player barely steers in the air; here holding a movement key while swinging pulls
	the player toward walking speed, which the engine's player controller does for anyone airborne.
]]

local folder = "Add-ons/Tool_GrappleRope/"

--A Blockland unit is two studs, and a stud is one world unit, so a DTS model is true to size at 2
local scale = 2

--Where a held item's muzzle is comes from the weapon support the Tier+Tactical add-on carries
if weaponPointFromHand == nil then
	dofile("Add-ons/Weapon_Package_Tier1/Support_Weapons.lua")
end

--stateSound on Fire: bowFireSound
newSoundType("BowFire", folder .. "bowFire.wav")

--[[
	The rope's links, ChainTrailParticle and ChainTrailEmitter: a dark dot every millisecond,
	standing still where the link left it. Blockland's dots lived 50 ms behind a link fired every
	10 ms; the links here come less often, so each trail lasts long enough to reach the next one
	and the rope stays in one piece. Sizes are doubled from the datablock, since one of its units
	is two studs.
]]
--How often a link leaves the gun while the rope holds, the 0.01 Hold state of the original
local CHAIN_PERIOD_MS = 75
--How fast a link flies, the ChainProjectile's 160 units a second
local CHAIN_SPEED = 320
--And how long its dots last: enough for a trail to overlap the link after it
local CHAIN_DOT_MS = 120

addParticleType("ChainTrailParticle", {
	texture = "Assets/particles/dot.png",
	lit = false,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.2, 0.2, 0.2, 1.0},
	color1 = {0.2, 0.2, 0.2, 1.0},
	color2 = {0.2, 0.2, 0.2, 1.0},
	color3 = {0.2, 0.2, 0.2, 1.0},
	size0 = 0.3, size1 = 0.3, size2 = 0.3, size3 = 0.3,
	time0 = 0, time1 = 0.1, time2 = 1, time3 = 1,
	drag = 3,
	gravity = {0, 0, 0},
	inheritedVelFactor = 0,
	lifetimeMS = CHAIN_DOT_MS,
	lifetimeVarianceMS = 0,
	spinSpeed = 10
})

addEmitterType("ChainTrailEmitter", {
	particles = "ChainTrailParticle",
	ejectionPeriodMS = 1,
	periodVarianceMS = 0,
	ejectionVelocity = 0,
	velocityVariance = 0,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 90,
	phiVariance = 360,
	lifetimeMS = 0
})

--[[
	Models. The hook and every link of the rope are the same projectile with nothing drawn on it,
	see chain.txt. The gun is the print gun's own shape, which Blockland tinted orange with
	GrappleRope's colorShiftColor, so grappleRope.txt gives it an orange metal instead of the print
	gun's grey. It's held by its mountPoint node like the guns, built the way setItemHand expects
	already, +Y out of the hand and -Z down the barrel
]]
grappleChain = newDynamicType("grappleChain", folder .. "chain.txt", 0.005, 0.005, 0.005)

grappleRopeItem = newItemType("grappleRope", folder .. "grappleRope.txt", scale, scale, scale, "Grapple Rope", "")

local gripX, gripY, gripZ = getTypeNodePosition(grappleRopeItem, "mountPoint")
setItemHand(grappleRopeItem, gripX or 0, gripY or -0.378, gripZ or 0, 0, 0, 0)

--The end of the barrel, off the shape's own node, measured from the point the hand holds it by
local muzzleFromHand = weaponNodeFromHand(grappleRopeItem, "muzzlePoint")

--[[
	The hook, GrappleRopeProjectile: 200 units a second, no gravity, gone after 4 seconds if it
	lands on nothing, and correctMuzzleVector so it leaves the barrel headed for whatever the
	crosshair is on
]]
local HOOK_SPEED = 400
local HOOK_LIFETIME_MS = 4000
--As far as the hook can reach in that time, which is how far the crosshair is looked for
local HOOK_RANGE = HOOK_SPEED * HOOK_LIFETIME_MS / 1000

--How often the rope pulls on its holder, the original's Hold state every 0.01 seconds, which its
--engine really ran every tick. This engine's tick is 25 ms
local ROPE_TICK_MS = 25

--[[
	GrappleRope, the swing itself. It looks a second ahead: where the player would be at their
	current velocity in one second, measured from the anchor. If that's further out than the rope
	is long, the velocity is turned to point from where they are toward that spot pulled back onto
	the rope's reach, keeping its speed, and a slow pull toward the anchor is added on top, one
	unit a second in the original. So the rope never yanks, it steers, and a player hanging still
	creeps up it a little
]]
local ROPE_LOOKAHEAD_S = 1
local ROPE_PULL = 2

--The line of sight check before the hook takes hold starts this far above the player's position,
--the "0 0 1" of the original
local ROPE_EYE_LIFT = 2
--And the anchor is clear when the line reaches within this much of it: the ray lands on the same
--surface the hook did, and a raycast stops a stud or so short of a brick's face
local ROPE_CLEAR_STUDS = 2.5

--Tags telling the hook and the links apart in ProjectileHit
local HOOK_TAG = "grappleHook"
local CHAIN_TAG = "grappleChain"

--The type name every Grapple Rope item has
local ITEM_NAME = "grappleRope"

--What each client's rope is doing, by client ID
local ropes = {}

--Hooks in flight, by projectile ID: whose they are, and which pull of the trigger fired them
local hooks = {}

--Links in flight, by projectile ID, so one the engine already removed isn't removed again
local chains = {}

--[[
	Holds a player within ropeLength of the anchor, the GrappleRope function of the original. Sets
	their velocity and returns true when the rope had to, false when it hung slack. Only their
	velocity is ever touched, so gravity still swings them
]]
function grappleConstrain(player, anchorX, anchorY, anchorZ, ropeLength)
	local px, py, pz = player:getPosition()
	local vx, vy, vz = player:getVelocity()

	--Where they'd be in a second, from the anchor
	local aheadX = px + vx * ROPE_LOOKAHEAD_S - anchorX
	local aheadY = py + vy * ROPE_LOOKAHEAD_S - anchorY
	local aheadZ = pz + vz * ROPE_LOOKAHEAD_S - anchorZ
	local aheadLength = math.sqrt(aheadX * aheadX + aheadY * aheadY + aheadZ * aheadZ)

	if aheadLength <= ropeLength then
		return false
	end

	--That spot pulled back onto the rope's reach, and the way there from where they are
	local reach = ropeLength / aheadLength
	local towardX = aheadX * reach + anchorX - px
	local towardY = aheadY * reach + anchorY - py
	local towardZ = aheadZ * reach + anchorZ - pz
	local towardLength = math.sqrt(towardX * towardX + towardY * towardY + towardZ * towardZ)

	--The slow pull up the rope
	local toAnchorX, toAnchorY, toAnchorZ = anchorX - px, anchorY - py, anchorZ - pz
	local toAnchorLength = math.sqrt(toAnchorX * toAnchorX + toAnchorY * toAnchorY + toAnchorZ * toAnchorZ)
	local pullX, pullY, pullZ = 0, 0, 0
	if toAnchorLength > 0.0001 then
		pullX = toAnchorX / toAnchorLength * ROPE_PULL
		pullY = toAnchorY / toAnchorLength * ROPE_PULL
		pullZ = toAnchorZ / toAnchorLength * ROPE_PULL
	end

	--Headed straight away from the anchor at the end of the rope, there's nowhere along it to
	--steer toward: the original divided by zero here. Only the pull is left
	local speed = math.sqrt(vx * vx + vy * vy + vz * vz)
	if towardLength < 0.0001 then
		player:setVelocity(pullX, pullY, pullZ)
		return true
	end

	local turn = speed / towardLength
	player:setVelocity(towardX * turn + pullX, towardY * turn + pullY, towardZ * turn + pullZ)
	return true
end

--The Grapple Rope in a client's hand, or nil if they're holding something else
local function heldGrapple(client)
	local item = client:getHeldItem()
	if item == nil or item:getTypeName() ~= ITEM_NAME then
		return nil
	end
	return item
end

--Lets go of the rope: the anchor is forgotten, and hooks and ticks from this pull of the trigger
--are ignored from here on, GrappleRopeImage::onRelease
local function releaseRope(state)
	state.holding = false
	state.anchor = nil
	state.ropeLength = nil
	state.generation = state.generation + 1
end

--A hook that landed on nothing in its 4 seconds
function grappleHookExpired(hookID)
	local hook = hooks[hookID]
	if hook ~= nil then
		hooks[hookID] = nil
		hook.projectile:destroy()
	end
end

--A link that flew past its anchor, because whatever it was on moved or went away
function grappleChainExpired(chainID)
	local chain = chains[chainID]
	if chain ~= nil then
		chains[chainID] = nil
		chain:destroy()
	end
end

--[[
	One link of the rope: fired from the muzzle straight at the anchor, trailing dots, and cleared
	away just after it should have got there if it somehow didn't land. GrappleRopeImage::onHold
]]
local function fireChain(client, state, player)
	local muzzleX, muzzleY, muzzleZ = weaponPointFromHand(client, muzzleFromHand)
	if muzzleX == nil then
		return
	end

	local anchor = state.anchor
	local dirX, dirY, dirZ = anchor[1] - muzzleX, anchor[2] - muzzleY, anchor[3] - muzzleZ
	local distance = math.sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ)
	if distance < 0.5 then
		return
	end

	local chain = addProjectile(grappleChain, muzzleX, muzzleY, muzzleZ,
		dirX / distance * CHAIN_SPEED, dirY / distance * CHAIN_SPEED, dirZ / distance * CHAIN_SPEED,
		CHAIN_TAG, player)
	if chain == nil then
		return
	end

	--gravityMod 0, it flies dead straight
	chain:setGravity(0, 0, 0)

	local trail = addEmitter("ChainTrailEmitter", muzzleX, muzzleY, muzzleZ)
	if trail ~= nil then
		trail:attachToDynamic(chain)
	end

	chains[chain.id] = chain
	schedule(math.ceil(distance / CHAIN_SPEED * 1000) + 250, "grappleChainExpired", chain.id)
end

--[[
	The Hold state: while the button stays down, the rope holds its player and a link is sent up
	it every so often. Stops on its own when the button comes up, the rope is put away, or its
	holder is gone
]]
function grappleRopeTick(clientID, generation)
	local state = ropes[clientID]
	if state == nil or not state.holding or state.generation ~= generation then
		return
	end

	local client = state.client
	local item = heldGrapple(client)
	local player = client:getControlledIdx(0)

	--Put it away, switched for another item, threw it, or lost their player
	if item == nil or item.id ~= state.itemID or player == nil then
		releaseRope(state)
		return
	end

	if state.anchor ~= nil then
		grappleConstrain(player, state.anchor[1], state.anchor[2], state.anchor[3], state.ropeLength)

		state.sinceChainMS = state.sinceChainMS + ROPE_TICK_MS
		if state.sinceChainMS >= CHAIN_PERIOD_MS then
			state.sinceChainMS = 0
			fireChain(client, state, player)
		end
	end

	schedule(ROPE_TICK_MS, "grappleRopeTick", clientID, generation)
end

--[[
	The hook landed, GrappleRopeProjectile::onCollision. It takes hold if a straight line from
	just above the player reaches it, so a hook that went round a corner or over a ledge the
	player can't see past does nothing, and the rope is as long as they are from it right now
]]
local function hookLanded(hook, x, y, z)
	local state = ropes[hook.clientID]
	if state == nil or not state.holding or state.generation ~= hook.generation then
		return
	end

	local client = state.client
	local player = client:getControlledIdx(0)
	if player == nil then
		return
	end

	local px, py, pz = player:getPosition()
	local fromY = py + ROPE_EYE_LIFT
	local toX, toY, toZ = x - px, y - fromY, z - pz
	local distance = math.sqrt(toX * toX + toY * toY + toZ * toZ)

	local blocker, hitX, hitY, hitZ, _, _, _, hitDistance = raycast(px, fromY, pz, x, y, z, player)
	if hitX ~= nil and hitDistance < distance - ROPE_CLEAR_STUDS then
		return
	end

	local ropeX, ropeY, ropeZ = x - px, y - py, z - pz
	state.anchor = { x, y, z }
	state.ropeLength = math.sqrt(ropeX * ropeX + ropeY * ropeY + ropeZ * ropeZ)
	state.sinceChainMS = CHAIN_PERIOD_MS
end

function grappleProjectileHit(projectile, hit, x, y, z, tag)
	if tag == HOOK_TAG then
		local hook = hooks[projectile.id]
		if hook ~= nil then
			hooks[projectile.id] = nil
			hookLanded(hook, x, y, z)
		end
	elseif tag == CHAIN_TAG then
		chains[projectile.id] = nil
	end

	return projectile, hit, x, y, z, tag
end
registerEventListener("ProjectileHit", "grappleProjectileHit")

--[[
	The Fire state: the sound, the shape's fire sequence, and the hook out of the barrel toward
	whatever the crosshair is on, then Hold until the button comes up
]]
local function fireHook(client, item, state)
	local player = client:getControlledIdx(0)
	if player == nil then
		return
	end

	item:playSound("BowFire")
	item:playAnimation("fire")

	local muzzleX, muzzleY, muzzleZ = weaponPointFromHand(client, muzzleFromHand)
	if muzzleX == nil then
		return
	end

	--correctMuzzleVector: the hook heads from the barrel to the spot the crosshair is on, or the
	--way the camera looks if that's nothing within reach
	local camX, camY, camZ = client:getCameraPosition()
	local dirX, dirY, dirZ = client:getCameraDirection()
	local _, aimX, aimY, aimZ = raycast(camX, camY, camZ,
		camX + dirX * HOOK_RANGE, camY + dirY * HOOK_RANGE, camZ + dirZ * HOOK_RANGE, player)

	if aimX ~= nil then
		local toX, toY, toZ = aimX - muzzleX, aimY - muzzleY, aimZ - muzzleZ
		local length = math.sqrt(toX * toX + toY * toY + toZ * toZ)
		if length > 0.5 then
			dirX, dirY, dirZ = toX / length, toY / length, toZ / length
		end
	end

	local hook = addProjectile(grappleChain, muzzleX, muzzleY, muzzleZ,
		dirX * HOOK_SPEED, dirY * HOOK_SPEED, dirZ * HOOK_SPEED, HOOK_TAG, player)
	if hook == nil then
		return
	end

	--gravityMod 0
	hook:setGravity(0, 0, 0)

	hooks[hook.id] = { projectile = hook, clientID = client:getID(), generation = state.generation }
	schedule(HOOK_LIFETIME_MS, "grappleHookExpired", hook.id)
end

function grappleClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	--Left mouse only
	if (mask & 1) == 0 then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local item = heldGrapple(client)
	if item == nil then
		return client, posX, posY, posZ, dirX, dirY, dirZ, mask
	end

	local clientID = client:getID()
	local state = ropes[clientID]
	if state == nil then
		state = { client = client, generation = 0, holding = false }
		ropes[clientID] = state
	end

	--Whatever the last pull was doing is over. Items are compared by ID: getHeldItem gives back a
	--new wrapper each call
	releaseRope(state)
	state.itemID = item.id
	state.holding = true
	state.sinceChainMS = 0

	fireHook(client, item, state)
	schedule(ROPE_TICK_MS, "grappleRopeTick", clientID, state.generation)

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "grappleClick")

function grappleClickRelease(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if (mask & 1) ~= 0 then
		local state = ropes[client:getID()]
		if state ~= nil and state.holding then
			releaseRope(state)
		end
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClickRelease", "grappleClickRelease")

function grappleClientLeft(client)
	ropes[client:getID()] = nil
	return client
end
registerEventListener("ClientLeave", "grappleClientLeft")

--Where a client's rope is hooked and how long it is, or nil while they aren't hanging from one
function grappleGetRope(client)
	local state = ropes[client:getID()]
	if state == nil or not state.holding or state.anchor == nil then
		return nil
	end
	return state.anchor[1], state.anchor[2], state.anchor[3], state.ropeLength
end

--[[
	Drops a Grapple Rope onto the ground, from just above whatever is under that spot so it lands
	on bricks too, and returns it
]]
function dropGrappleRope(x, z)
	x = x or 20
	z = z or -10

	local _, _, groundY = raycast(x, 300, z, x, -300, z)
	local dropY = groundY and (groundY + 1) or 25

	return createItem(grappleRopeItem, x, dropY, z)
end

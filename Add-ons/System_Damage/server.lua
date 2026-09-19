--[[
	Damage

	Health, dying, and respawning, all of it on the server: clients are never told anyone's health, only shown what
	happens because of it.

	What's kept is kept on the objects themselves, since an object is the same Lua table every time a script gets it:

		player.health         what they have left, DEFAULT_MAX_HEALTH to start with
		player.maxHealth      what health regenerates back up to
		player.canBeDamaged   false makes damagePlayer leave them alone
		client.score          0 as they join, up one for every other player they kill. Change it with setScore, which also
		                      puts it next to their name in everyone's player list (F2) with client:setScoreText
		client.dead           true from when their player dies until they respawn
		client.corpse         the body their last player left, until they respawn or leave

	damagePlayer(player, amount, attacker, x, y, z) is the way in for anything that hurts someone, the attacking client and
	where they were hit both being optional. The weapon add-ons
	call it for a shot that lands (Support_Weapons.lua's hurtIfPlayer, with each weapon's damage field), and damageByImpulse
	below calls it for every player a radiusImpulse pushes, like the launcher's shells. setHealth and killPlayer are
	there for scripts that want them.

	Someone who hasn't been hurt for HEALTH_REGEN_DELAY_MS gets HEALTH_REGEN_PER_SECOND back each second up to their max.

	A player whose health reaches 0 dies. Whatever they carried is dropped the way it is when someone leaves (System_Inventory's
	dropCarriedItems), and their player is swapped for a body: a new dynamic that looks the same, which nobody controls, so it
	flops over away from whatever killed them and plays Death. Their camera is left hanging over it, free to look around, with
	no player. A countdown on their screen runs RESPAWN_DELAY_MS down, after which a left click respawns them: serverstart.lua's
	spawnPlayer makes them a new player, which plays Spawn, and they get their starting tools again. Their old body disappears
	in a puff of smoke with BodyRemove when they respawn, or when they leave without having.

	Run from serverstart.lua with dofile("System_Damage"), after System_Inventory.
]]


--[[
	System_Damage loads after System_Inventory so that a click which respawns someone isn't also a click
	with their new tools, and needs the players it hands health to
]]
requireAddOn("System_Inventory")
requireAddOn("System_Players")

--Being hurt, dying, and the body left behind being cleared away
newSoundType("Pain","Assets/sound/pain.wav")
newSoundType("Death","Assets/sound/death.wav")

--[[
	What being hurt looks and sounds like, which damagePlayer below shows on a player it takes health from: the
	old game's ouch particles and the Pain sound where it happened, and for whoever the player belongs to a red vignette
	that closes in from the edges of their screen and wobbles their picture, fading away over a second
	x, y, z is where they were hit, or nothing to put the particles at about chest height
]]
HURT_VIGNETTE_MS = 1000
--How hard the picture wobbles, 1 being about as much as being underwater
HURT_VIGNETTE_WAVE = 0.5

function hurtPlayer(player, x, y, z)
	if player == nil then
		return
	end

	if x == nil then
		x, y, z = player:getPosition()
		y = y + 3
	end

	addEmitter("ouchEmitter", x, y, z)
	player:playSound("Pain")

	if player:getNumControllers() > 0 then
		player:getControllerIdx(0):setVignette(1, 0, 0, 0.5, HURT_VIGNETTE_WAVE, HURT_VIGNETTE_MS)
	end
end

--From NetTypes/NetType.h's SimObjectType enum
local DYNAMIC_TYPE_ID = 1

--What a new player's health and max health are
DEFAULT_MAX_HEALTH = 100

--How much health comes back each second, and how long after last being hurt it starts to, in milliseconds
HEALTH_REGEN_PER_SECOND = 2
HEALTH_REGEN_DELAY_MS = 5000
local REGEN_TICK_MS = 1000

--How long someone stays dead before a click respawns them, in milliseconds
RESPAWN_DELAY_MS = 10000

--Health a radiusImpulse takes is this times the square of the push that reaches a player, since a push fades evenly all the
--way out to its reach, which is a long way: a launcher shell (140) reaches 30 studs. One landing at someone's feet takes
--nearly all their health (98), one 10 studs off 43, and one 20 studs off 10
IMPULSE_DAMAGE_SCALE = 0.005

--How fast a body tips over in radians per second, and how fast it's thrown along and up in studs per second
local CORPSE_TIP_SPEED = 6
local CORPSE_PUSH_SPEED = 6
local CORPSE_LIFT_SPEED = 8
--How often a falling body is checked on in milliseconds, how far over in degrees counts as lying down, and how many checks it gets to get there
local CORPSE_SETTLE_TICK_MS = 50
local CORPSE_LYING_DEGREES = 86
local CORPSE_SETTLE_TICKS = 60

--Where the camera of someone dead hangs: this far back from their body the way they were looking, and this far above it
local DEATH_CAMERA_BACK = 12
local DEATH_CAMERA_UP = 7
--About the middle of a body, above its position
local BODY_MIDDLE = 3

--The puff of smoke a body disappears in, one burst in every direction
addParticleType("bodyRemoveParticle", {
	lit = true,
	texture = "Assets/particles/cloud.png",
	color0 = {1.0, 1.0, 1.0, 0.5},
	color1 = {1.0, 1.0, 1.0, 0.4},
	color2 = {0.9, 0.9, 0.9, 0.2},
	color3 = {0.8, 0.8, 0.8, 0.0},
	size0 = 1.5, size1 = 3, size2 = 4.5, size3 = 5.5,
	time0 = 0, time1 = 0.2, time2 = 0.6, time3 = 1,
	drag = 3,
	gravity = {0, 4, 0},
	spinSpeed = 40,
	lifetimeMS = 1100,
	lifetimeVarianceMS = 300,
	useInvAlpha = true,
	needsSorting = true
})

addEmitterType("bodyRemoveEmitter", {
	particles = "bodyRemoveParticle",
	lifetimeMS = 120,
	ejectionPeriodMS = 4,
	ejectionVelocity = 9,
	velocityVariance = 4,
	ejectionOffset = 0.5,
	thetaMin = 0,
	thetaMax = 180,
	phiVariance = 360
})

--The client's player, the first dynamic they control, or nil
local function playerOf(client)
	if client:getNumControlled() == 0 then
		return nil
	end
	return client:getControlledIdx(0)
end

--A vector turned by a rotation
local function rotateBy(w, x, y, z, vx, vy, vz)
	local tx = 2 * (y * vz - z * vy)
	local ty = 2 * (z * vx - x * vz)
	local tz = 2 * (x * vy - y * vx)
	return vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx)
end

--Full health and able to be damaged, for a new player. Pass a max health to give them something other than the default
function giveHealth(player, maxHealth)
	player.maxHealth = maxHealth or DEFAULT_MAX_HEALTH
	player.health = player.maxHealth
	player.canBeDamaged = true
	player.sinceHurtMS = 0
end

--Someone's score, which is also what everyone's player list shows next to their name
function setScore(client, score)
	client.score = score
	client:setScoreText(tostring(score))
end

function startScore(client)
	setScore(client, 0)
	return client
end
registerEventListener("ClientJoin", "startScore")

--Takes someone's old body away in a puff of smoke, if they left one
local function removeCorpse(client)
	local corpse = client.corpse
	if corpse == nil then
		return
	end
	client.corpse = nil
	corpse.removed = true

	local x, y, z = corpse:getPosition()
	addEmitter("bodyRemoveEmitter", x, y + 1, z)
	playSound("BodyRemove", x, y, z)
	corpse:destroy()
end

--[[
	A dynamic's weight is all at its feet, so a body that's tipped over stands itself back up like a roly-poly toy
	This watches one fall and stops it turning any more once it's lying down, or has bounced and started coming back up, or has had
	long enough. One that got most of the way over is laid the rest of the way flat as it's stopped
	It still slides, falls, and gets pushed around after that, it just can't turn
]]
function corpseSettle(corpse, ticksLeft, lastTilt)
	if corpse.removed then
		return
	end

	--Which way its own up points, and how far that has tipped away from straight up
	local w, x, y, z = corpse:getRotation()
	local upX, upY, upZ = 2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)
	local tilt = math.deg(math.acos(math.max(-1, math.min(1, upY))))

	if tilt < CORPSE_LYING_DEGREES and tilt >= lastTilt - 0.5 and ticksLeft > 0 then
		schedule(CORPSE_SETTLE_TICK_MS, "corpseSettle", corpse, ticksLeft - 1, tilt)
		return
	end

	--Turned the rest of the way around the level axis it's already falling around, so its up ends up level
	local level = math.sqrt(upX * upX + upZ * upZ)
	if tilt >= 45 and level > 0.01 then
		local half = math.rad(90 - tilt) / 2
		local axisX, axisZ = upZ / level, -upX / level
		local turnW, turnX, turnZ = math.cos(half), axisX * math.sin(half), axisZ * math.sin(half)
		--The turn times the rotation it has, the turn having no y part
		corpse:setRotation(
			turnW * w - turnX * x - turnZ * z,
			turnW * x + turnX * w - turnZ * y,
			turnW * y + turnZ * x - turnX * z,
			turnW * z + turnZ * w + turnX * y)
	end

	corpse:setAngularVelocity(0, 0, 0)
	corpse:setAngularFactor(0, 0, 0)
end

--[[
	Counts down the time until someone dead can respawn on their screen, then tells them to click, once a second until they do
	Each print is there a little longer than a second in case the next is late, and takes the last one away first
	(a center print of 0 ms clears what's showing) so they never stack up
	deaths is how many times they'd died when this countdown started, which stops it once they're alive again, even if they've died again since
]]
function respawnCountdown(client, deaths, secondsLeft)
	if client.left or not client.dead or client.deaths ~= deaths then
		return
	end

	client:centerPrint("", 0)
	if secondsLeft > 0 then
		client:centerPrint("You died. You can respawn in " .. secondsLeft .. (secondsLeft == 1 and " second." or " seconds."), 1500, 1, 0.3, 0.3)
	else
		client.canRespawn = true
		client:centerPrint("You died. Click to respawn.", 1500, 1, 0.3, 0.3)
	end

	schedule(1000, "respawnCountdown", client, deaths, math.max(0, secondsLeft - 1))
end

--[[
	Someone's player dies, whatever health it had: see the top of this file for everything that happens
	attacker is the client whose score goes up for it, or nil. fromX, fromY, fromZ is where what killed them hit, which their body falls away from
	Does nothing for a dynamic that isn't a client's player
]]
function killPlayer(player, attacker, fromX, fromY, fromZ)
	if player == nil or player.type ~= DYNAMIC_TYPE_ID or player:getNumControllers() == 0 then
		return
	end

	local client = player:getControllerIdx(0)
	player.health = 0

	--Out of the driver's seat first, so they die next to their vehicle rather than wherever a player in one is kept
	if client:getVehicle() then
		client:exitVehicle()
	end

	--While they still have a player to leave their things next to
	if dropCarriedItems ~= nil then
		dropCarriedItems(client)
	end
	if sittingClients ~= nil then
		sittingClients[client:getID()] = nil
	end

	local x, y, z = player:getPosition()
	local rotW, rotX, rotY, rotZ = player:getRotation()
	local velX, velY, velZ = player:getVelocity()

	--Which way the body falls, level: away from what hit them, or any way at all
	local awayX, awayZ = 0, 0
	if fromX ~= nil then
		awayX, awayZ = x - fromX, z - fromZ
	end
	local awayLength = math.sqrt(awayX * awayX + awayZ * awayZ)
	if awayLength < 0.05 then
		local angle = math.random() * math.pi * 2
		awayX, awayZ, awayLength = math.cos(angle), math.sin(angle), 1
	end
	awayX, awayZ = awayX / awayLength, awayZ / awayLength

	--A body is a box, and one that fell cornerways would end up lying on an edge, so it goes over forwards, backwards, or to one
	--side, whichever is nearest to that. Someone already lying down to crawl carries on the way they were tipped
	local upX, upY, upZ = rotateBy(rotW, rotX, rotY, rotZ, 0, 1, 0)
	if upY > 0.9 then
		local best = -2
		local fallX, fallZ = awayX, awayZ
		for _, axis in ipairs({{1, 0, 0}, {0, 0, 1}}) do
			local axisX, _, axisZ = rotateBy(rotW, rotX, rotY, rotZ, axis[1], axis[2], axis[3])
			for sign = -1, 1, 2 do
				local along = (axisX * awayX + axisZ * awayZ) * sign
				if along > best then
					best, fallX, fallZ = along, axisX * sign, axisZ * sign
				end
			end
		end
		local fallLength = math.sqrt(fallX * fallX + fallZ * fallZ)
		awayX, awayZ = fallX / fallLength, fallZ / fallLength
	elseif upX * upX + upZ * upZ > 0.0001 then
		local level = math.sqrt(upX * upX + upZ * upZ)
		awayX, awayZ = upX / level, upZ / level
	end

	--Their camera hangs behind and above where they died, pulled in front of anything that would be in the way
	local lookX, _, lookZ = client:getCameraDirection()
	local lookLength = math.sqrt(lookX * lookX + lookZ * lookZ)
	if lookLength < 0.05 then
		lookX, lookZ, lookLength = awayX, awayZ, 1
	end
	local middleY = y + BODY_MIDDLE
	local camX = x - lookX / lookLength * DEATH_CAMERA_BACK
	local camY = middleY + DEATH_CAMERA_UP
	local camZ = z - lookZ / lookLength * DEATH_CAMERA_BACK
	local _, hitX, hitY, hitZ = raycast(x, middleY, z, camX, camY, camZ, player)
	if hitX ~= nil then
		camX, camY, camZ = x + (hitX - x) * 0.8, middleY + (hitY - middleY) * 0.8, z + (hitZ - z) * 0.8
	end
	--Aimed at the body, then let go so they can look around: mouse look carries on from wherever the camera was pointed
	local aimX, aimY, aimZ = x - camX, middleY - 1.5 - camY, z - camZ
	local aimLength = math.sqrt(aimX * aimX + aimY * aimY + aimZ * aimZ)
	if aimLength > 0.05 then
		client:staticCamera(camX, camY, camZ, aimX / aimLength, aimY / aimLength, aimZ / aimLength)
	end
	client:staticCamera(camX, camY, camZ)

	--Their player goes, leaving them with none, and a body nobody controls takes its place
	local woreHat = player:getPart("hat") ~= nil
	player:destroy()

	local corpse = createDynamic(brickhead, x, y, z)
	corpse:setRotation(rotW, rotX, rotY, rotZ)
	client:applyAppearance(corpse)
	--Less the hat, if theirs was shot off, see System_Hats
	if not woreHat then
		corpse:setPart("hat", "")
	end
	corpse:setVelocity(velX + awayX * CORPSE_PUSH_SPEED, velY + CORPSE_LIFT_SPEED, velZ + awayZ * CORPSE_PUSH_SPEED)
	--Turning around the level axis square to the way it falls tips its head that way
	corpse:setAngularVelocity(awayZ * CORPSE_TIP_SPEED, 0, -awayX * CORPSE_TIP_SPEED)
	corpse:playSound("Death")
	schedule(CORPSE_SETTLE_TICK_MS, "corpseSettle", corpse, CORPSE_SETTLE_TICKS, 0)

	--Someone who somehow still has a body lying around from before only ever has the one
	removeCorpse(client)
	client.corpse = corpse
	client.dead = true
	client.canRespawn = false
	client.deaths = (client.deaths or 0) + 1

	if attacker ~= nil and not attacker.left and attacker ~= client then
		setScore(attacker, (attacker.score or 0) + 1)
		messageAll(attacker:getName() .. " killed " .. client:getName() .. ".")
	else
		messageAll(client:getName() .. " died.")
	end

	respawnCountdown(client, client.deaths, math.ceil(RESPAWN_DELAY_MS / 1000))
end

--[[
	Takes health off someone's player, showing them being hurt (serverstart.lua's hurtPlayer), and kills them if that was the last of it
	attacker is the client to blame, or nil, and x, y, z is where they were hit, or nothing
	Returns whether they were damaged: not if it isn't a client's player, canBeDamaged is false, or amount isn't above 0
]]
function damagePlayer(player, amount, attacker, x, y, z)
	if player == nil or player.type ~= DYNAMIC_TYPE_ID or player:getNumControllers() == 0 then
		return false
	end

	--A player some other script made without giveHealth
	if player.health == nil then
		giveHealth(player)
	end

	if not player.canBeDamaged or amount == nil or amount <= 0 then
		return false
	end

	player.health = player.health - amount
	player.sinceHurtMS = 0

	if hurtPlayer ~= nil then
		hurtPlayer(player, x, y, z)
	end

	if player.health <= 0 then
		killPlayer(player, attacker, x, y, z)
	end

	return true
end

--Sets someone's health, up to their max, killing them at 0 or less whether or not they can be damaged
function setHealth(player, health)
	if player == nil or player.type ~= DYNAMIC_TYPE_ID then
		return
	end

	if player.health == nil then
		giveHealth(player)
	end

	player.health = math.min(health, player.maxHealth)
	if player.health <= 0 then
		killPlayer(player)
	end
end

--Every player a radiusImpulse pushes is damaged by how hard it pushed them. A pull doesn't hurt
--Whoever set the impulse off is blamed if they said so in impulseAttacker first, see System_Inventory's launcherShellHit
--A blast that does its own damage sets impulseHarmless around its radiusImpulse instead, so its push only pushes, like
--the Rocket Launcher add-on's
function damageByImpulse(dynamic, x, y, z, strength)
	if strength > 0 and not impulseHarmless then
		damagePlayer(dynamic, strength * strength * IMPULSE_DAMAGE_SCALE, impulseAttacker)
	end
	return dynamic, x, y, z, strength
end
registerEventListener("RadiusImpulseHit", "damageByImpulse")

--Health coming back, a bit each second for everyone who hasn't been hurt lately
function healthRegenTick()
	for i = 0, getNumClients() - 1 do
		local player = playerOf(getClientIdx(i))
		if player ~= nil and player.health ~= nil and player.health < player.maxHealth then
			player.sinceHurtMS = (player.sinceHurtMS or 0) + REGEN_TICK_MS
			if player.sinceHurtMS > HEALTH_REGEN_DELAY_MS then
				player.health = math.min(player.maxHealth, player.health + HEALTH_REGEN_PER_SECOND * REGEN_TICK_MS / 1000)
			end
		end
	end

	schedule(REGEN_TICK_MS, "healthRegenTick")
end
schedule(REGEN_TICK_MS, "healthRegenTick")

--Someone dead gets a new player, their old body goes, and they start over with the tools everyone joins with
function respawnPlayer(client)
	if not client.dead then
		return
	end

	client.dead = false
	client.canRespawn = false
	client:centerPrint("", 0)

	removeCorpse(client)
	spawnPlayer(client)
	if giveStartingItems ~= nil then
		giveStartingItems(client)
	end
end

--A left click respawns someone who's been dead long enough
function respawnClick(client, posX, posY, posZ, dirX, dirY, dirZ, mask)
	if client.dead and client.canRespawn and (mask & 1) ~= 0 then
		respawnPlayer(client)
	end

	return client, posX, posY, posZ, dirX, dirY, dirZ, mask
end
registerEventListener("ClientClick", "respawnClick")

--Someone who leaves while dead takes their body with them, and their countdown stops
function removeCorpseOnLeave(client)
	client.left = true
	client.dead = false
	removeCorpse(client)
	return client
end
registerEventListener("ClientLeave", "removeCorpseOnLeave")

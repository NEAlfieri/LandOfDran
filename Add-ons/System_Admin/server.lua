--[[
	System_Admin

	Slash commands: the chat listener that picks them out of what someone says, the ones anyone can use
	(/clearbricks, /clearvehicles), and the admin ones, from /kick to /setWaterLevel.

	Two tables anything else can add to, both keyed by the lowercased command with its slash:

		playerCommands["/sit"] = function(client, argText, words) ... end
		adminCommands["/fallingtiles"] = function(client, argText, words) ... end

	An admin is single player's host, or anyone who logged into the eval console. Add a line to the chat
	window's list of commands with registerChatSuggestion, see LuaAPI.md.
]]

--Removes every brick planted by the client with that net ID, returns how many
function clearBricksOwnedBy(ownerID)
	local count = 0
	--Backwards, so removing a brick never moves one we haven't looked at yet
	for i = getNumBricks() - 1, 0, -1 do
		local brick = getBrickIdx(i)
		if brick:getOwner() == ownerID then
			brick:remove()
			count = count + 1
		end
	end
	return count
end

--Removes every vehicle the client with that net ID sliced, loaded, or was given by spawnJeep, returns how many
function clearVehiclesOwnedBy(ownerID)
	local count = 0
	for i = getNumVehicles() - 1, 0, -1 do
		local vehicle = getVehicleIdx(i)
		if vehicle:getBuilderID() == ownerID then
			vehicle:remove()
			count = count + 1
		end
	end
	return count
end

--Commands anyone can use, by their lowercased name, the same arguments as an admin command below.
--Another add-on puts its own in here, see System_Emote's /sit
playerCommands = {}

playerCommands["/clearbricks"] = function(client)
	local count = clearBricksOwnedBy(client:getID())
	if count == 0 then
		client:message("You don't have any bricks to clear.")
	else
		messageAll(client:getName() .. " cleared their " .. count .. (count == 1 and " brick." or " bricks."))
		playSound("BrickClear")
	end
end

playerCommands["/clearvehicles"] = function(client)
	local count = clearVehiclesOwnedBy(client:getID())
	if count == 0 then
		client:message("You don't have any vehicles to clear.")
	else
		messageAll(client:getName() .. " cleared their " .. count .. (count == 1 and " vehicle." or " vehicles."))
		playSound("BrickClear")
	end
end

--Commands only admins (single player's host, or anyone who logged into the eval console) can use, by their
--lowercased name. Each gets the client, the arguments as one string, and the message split into words (the
--command first). Add-ons add their own, the way FallingTiles.lua adds /fallingTiles
adminCommands = {}

--The message arrives as "Name: text". A command that runs isn't shown in chat
function chatCommands(client, message)
	local text = string.sub(message, string.len(client:getName()) + 3)

	--The command word and its arguments, the server already lowercased the word
	local words = {}
	for word in string.gmatch(text, "%S+") do
		table.insert(words, word)
	end
	local command = words[1]

	--Arguments as typed, in case a name or number has spaces or capitals in it
	local argText = string.match(text, "^%S+%s+(.-)%s*$") or ""

	if playerCommands[command] then
		playerCommands[command](client, argText, words)
		return client, ""
	end

	if adminCommands[command] then
		if not client:isAdmin() then
			client:message("You need to be an admin to use " .. command .. ".")
		else
			adminCommands[command](client, argText, words)
		end
		return client, ""
	end

	return client, message
end
registerEventListener("ClientChat","chatCommands")
--Listed in the chat window while typing a slash command, see registerChatSuggestion in LuaAPI.md
registerChatSuggestion("clearbricks", "/clearbricks - remove every brick you planted")
registerChatSuggestion("clearvehicles", "/clearvehicles - remove every vehicle you made")

--Finds a connected client by name, ignoring case. An exact match wins, then the only one whose name starts with it.
--Returns the client, or nil and a message saying why not
function findClientByName(name)
	if name == "" then
		return nil, "Give a player's name."
	end
	local lowered = string.lower(name)
	local partial = nil
	local partialCount = 0
	for i = 0, getNumClients() - 1 do
		local other = getClientIdx(i)
		local otherName = string.lower(other:getName())
		if otherName == lowered then
			return other
		end
		if string.sub(otherName, 1, string.len(lowered)) == lowered then
			partial = other
			partialCount = partialCount + 1
		end
	end
	if partialCount == 1 then
		return partial
	elseif partialCount > 1 then
		return nil, "More than one player's name starts with " .. name .. "."
	end
	return nil, "No player named " .. name .. " is here."
end

--Where a client is, for /find and /fetch: their vehicle if they're in one, else their player
function clientPosition(client)
	local vehicle = client:getVehicle()
	if vehicle then
		return vehicle:getPosition()
	end
	if client:getNumControlled() > 0 then
		return client:getControlledIdx(0):getPosition()
	end
	return nil
end

--Puts a client's player next to a position, first getting them out of any vehicle. False if they have no player
function teleportClient(client, x, y, z)
	if client:getNumControlled() == 0 then
		return false
	end
	if client:getVehicle() then
		client:exitVehicle()
	end
	--A little up so they don't land inside whoever they're put next to
	client:getControlledIdx(0):setPosition(x, y + 3, z)
	return true
end

adminCommands["/clearallvehicles"] = function(client)
	local count = getNumVehicles()
	if count == 0 then
		client:message("There are no vehicles to clear.")
		return
	end
	for i = count - 1, 0, -1 do
		getVehicleIdx(i):remove()
	end
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " vehicle." or " vehicles."))
	playSound("BrickClear")
end

adminCommands["/clearallbricks"] = function(client)
	local count = getNumBricks()
	if count == 0 then
		client:message("There are no bricks to clear.")
		return
	end
	clearAllBricks()
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " brick." or " bricks."))
	playSound("BrickClear")
end

--Items lying around on the ground, not ones anyone carries or the ones bricks offer
adminCommands["/clearallitems"] = function(client)
	local count = 0
	for i = getNumItems() - 1, 0, -1 do
		local item = getItemIdx(i)
		if not item:isHeld() and not item:isDisplay() then
			item:destroy()
			count = count + 1
		end
	end
	if count == 0 then
		client:message("There are no items on the ground to clear.")
		return
	end
	messageAll(client:getName() .. " cleared all " .. count .. (count == 1 and " item" or " items") .. " on the ground.")
	playSound("BrickClear")
end

adminCommands["/kick"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You can't kick yourself.")
		return
	end
	messageAll(target:getName() .. " was kicked by " .. client:getName() .. ".")
	target:kick()
end

adminCommands["/rain"] = function(client)
	if getRain() > 0 then
		setRain(0)
		messageAll(client:getName() .. " stopped the rain.")
	else
		setRain(1)
		messageAll(client:getName() .. " made it rain.")
	end
end

adminCommands["/settimescale"] = function(client, argText)
	local scale = tonumber(argText)
	if not scale then
		client:message("Usage: /setTimeScale <in-game seconds per real second>, 1 is normal and 0 freezes time.")
		return
	end
	setTimeScale(scale)
	messageAll(client:getName() .. " set the time scale to " .. scale .. ".")
end

adminCommands["/settimeofday"] = function(client, argText)
	local fraction = tonumber(argText)
	if not fraction then
		client:message("Usage: /setTimeOfDay <0-1>, 0 is midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset.")
		return
	end
	setTimeOfDay(fraction)
	messageAll(client:getName() .. " set the time of day to " .. fraction .. ".")
end

adminCommands["/setwaterlevel"] = function(client, argText)
	if argText == "" or string.lower(argText) == "off" or string.lower(argText) == "none" then
		setWaterLevel()
		messageAll(client:getName() .. " removed the water.")
		return
	end
	local level = tonumber(argText)
	if not level then
		client:message("Usage: /setWaterLevel <height>, or /setWaterLevel off to remove the water.")
		return
	end
	setWaterLevel(level)
	messageAll(client:getName() .. " set the water level to " .. level .. ".")
end

--Brings a player to the admin
adminCommands["/fetch"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You're already here.")
		return
	end
	--Their camera if it's off flying, so a fetched player lands where the admin is looking from
	local x, y, z
	if client:getFreeCamera() then
		x, y, z = client:getCameraPosition()
	else
		x, y, z = clientPosition(client)
	end
	if not x then
		client:message("You don't have a player to fetch them to.")
		return
	end
	if not teleportClient(target, x, y, z) then
		client:message(target:getName() .. " doesn't have a player to fetch.")
		return
	end
	target:message(client:getName() .. " fetched you.")
	client:message("Fetched " .. target:getName() .. ".")
end

--Takes the admin to a player
adminCommands["/find"] = function(client, name)
	local target, why = findClientByName(name)
	if not target then
		client:message(why)
		return
	end
	if target:getID() == client:getID() then
		client:message("You found yourself.")
		return
	end
	local x, y, z = clientPosition(target)
	if not x then
		client:message(target:getName() .. " doesn't have a player to find.")
		return
	end
	if not teleportClient(client, x, y, z) then
		client:message("You don't have a player to go there with.")
		return
	end
	client:message("Found " .. target:getName() .. ".")
end

registerChatSuggestion("clearallvehicles", "/clearAllVehicles - admins: remove every vehicle")
registerChatSuggestion("clearallbricks", "/clearAllBricks - admins: remove every brick")
registerChatSuggestion("clearallitems", "/clearAllItems - admins: remove every item lying on the ground")
registerChatSuggestion("kick", "/kick <player> - admins: disconnect a player")
registerChatSuggestion("rain", "/rain - admins: start or stop the rain")
registerChatSuggestion("settimescale", "/setTimeScale <scale> - admins: 1 is normal, 0 freezes time")
registerChatSuggestion("settimeofday", "/setTimeOfDay <0-1> - admins: 0 midnight, 0.5 noon")
registerChatSuggestion("setwaterlevel", "/setWaterLevel <height|off> - admins: put water at that height")
registerChatSuggestion("fetch", "/fetch <player> - admins: bring a player to you")
registerChatSuggestion("find", "/find <player> - admins: go to a player")

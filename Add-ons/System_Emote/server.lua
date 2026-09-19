--[[
	System_Emote

	Things a player does with their own body rather than with anything in the world. For now that's /sit,
	which puts them in the player model's sitting pose until they stand back up.
]]

--The sit animation and the players it's put on, and the table /sit is registered in
requireAddOn("System_Players")
requireAddOn("System_Admin")

--Who's sitting down by /sit, by client ID. Riders of a model vehicle sit on their own, see LuaAPI.md's Model vehicles
sittingClients = {}

function toggleSitting(client)
	local player = client:getNumControlled() > 0 and client:getControlledIdx(0) or nil
	if not player then
		client:message("You don't have a player to sit down.")
		return
	end

	if client:getVehicle() then
		client:message("You're already sitting in a vehicle.")
		return
	end

	if sittingClients[client:getID()] then
		sittingClients[client:getID()] = nil
		player:stopAnimation("sit")
	else
		sittingClients[client:getID()] = true
		player:playAnimation("sit", true)
	end
end

--Forgotten when they leave, their player goes with them
function forgetSitting(client)
	sittingClients[client:getID()] = nil
	return client
end
registerEventListener("ClientLeave","forgetSitting")

playerCommands["/sit"] = function(client)
	toggleSitting(client)
end
registerChatSuggestion("sit", "/sit - sit down, or stand back up")

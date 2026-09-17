--[[
	Audio test area

	Builds a few stations for hearing whether sounds get muffled behind bricks, 300 studs east of spawn,
	starts sounds at each one, and saves the bricks to Saves/AudioTest.lod.

	Stations, on a grey floor. /audiotest drops you at the floor's edge near the first two:
		Closed room (light blue, near, -X side): a ClickMove loop inside. Its brown door faces you, and /audiodoor
			opens and closes it, so you can stand in front of it and hear the same sound with and without a wall in the way.
		Lone wall (orange, near, +X side): ClickPlant every second on its far side. Walk around either end
			and it should go from muffled to clear, partly muffled right at the corner.
		Brick box (purple, far, -X side): Lua keeps planting a yellow brick inside a sealed box and breaking it again,
			for the ClickPlant and BrickBreak sounds.
		Open control (green marker, far, +X side): the same ClickMove loop, plus the plant/break cycle a little
			toward the middle, with no walls at all, to compare against.

	Chat commands:
		/audiotest   teleport to the test area (and start the sounds again if they were stopped)
		/audiodoor   open or close the closed room's door
		/audiostop   stop every test sound

	Add this to serverstart.lua with:
		dofile("AudioTest.lua")
	It needs serverstart.lua's ClickMove, ClickPlant, and BrickBreak sound types, so put it after those.
	Bricks it builds are skipped if something is already in the way.
]]

local OX = 300
local OZ = 0

--Brick heights are in plates, sound positions are in world units (a plate is 0.4 units tall)
local WALL_PLATES = 20

audioTestRunning = false
audioTestLoops = {}
audioTestDoor = nil
audioTestDoorOpen = false

--Spots where a brick keeps being planted and broken, one sealed in the purple box and one out in the open
audioTestBrickSpots = {
	{ x = OX - 27, z = OZ + 21, brick = nil },
	{ x = OX + 13, z = OZ + 21, brick = nil },
}

local function wall(x, y, z, width, height, length, r, g, b)
	addBrick(x, y, z, width, height, length, r, g, b, 1)
end

function buildAudioTest()
	--Floor
	wall(OX - 40, 0, OZ - 40, 80, 1, 80, 0.5, 0.5, 0.5)

	--Closed room, 12 by 12 with a roof, the door is the gap in the wall facing where /audiotest drops you
	wall(OX - 30, 1, OZ - 30, 5, WALL_PLATES, 1, 0.6, 0.8, 1)
	wall(OX - 23, 1, OZ - 30, 5, WALL_PLATES, 1, 0.6, 0.8, 1)
	wall(OX - 30, 1, OZ - 19, 12, WALL_PLATES, 1, 0.6, 0.8, 1)
	wall(OX - 30, 1, OZ - 29, 1, WALL_PLATES, 10, 0.6, 0.8, 1)
	wall(OX - 19, 1, OZ - 29, 1, WALL_PLATES, 10, 0.6, 0.8, 1)
	wall(OX - 30, WALL_PLATES + 1, OZ - 30, 12, 1, 12, 0.6, 0.8, 1)
	closeAudioTestDoor()

	--Lone wall, 16 wide
	wall(OX + 10, 1, OZ - 25, 16, WALL_PLATES, 1, 1, 0.6, 0.2)

	--Brick box, 8 by 8 with a roof
	wall(OX - 30, 1, OZ + 18, 8, 12, 1, 0.6, 0.3, 0.8)
	wall(OX - 30, 1, OZ + 25, 8, 12, 1, 0.6, 0.3, 0.8)
	wall(OX - 30, 1, OZ + 19, 1, 12, 6, 0.6, 0.3, 0.8)
	wall(OX - 23, 1, OZ + 19, 1, 12, 6, 0.6, 0.3, 0.8)
	wall(OX - 30, 13, OZ + 18, 8, 1, 8, 0.6, 0.3, 0.8)

	--Marker under the open control's loop
	wall(OX + 21, 1, OZ + 21, 2, 1, 2, 0.3, 1, 0.3)
end

function closeAudioTestDoor()
	if not audioTestDoor then
		audioTestDoor = addBrick(OX - 25, 1, OZ - 30, 2, WALL_PLATES, 1, 0.45, 0.3, 0.15, 1)
	end
	audioTestDoorOpen = false
end

function openAudioTestDoor()
	if audioTestDoor then
		audioTestDoor:remove()
		audioTestDoor = nil
	end
	audioTestDoorOpen = true
end

function startAudioTest()
	if audioTestRunning then
		return
	end
	audioTestRunning = true

	--Loops sit 3 units up, about head height
	table.insert(audioTestLoops, startSoundLoop("ClickMove", OX - 24, 3, OZ - 24.5, 1, 1))
	table.insert(audioTestLoops, startSoundLoop("ClickMove", OX + 22, 3, OZ + 22, 1, 1))

	schedule(1000, "audioTestWallClick")
	schedule(1500, "audioTestBrickCycle")
end

function stopAudioTest()
	audioTestRunning = false
	for _, loop in ipairs(audioTestLoops) do
		stopSoundLoop(loop)
	end
	audioTestLoops = {}
end

function audioTestWallClick()
	if not audioTestRunning then
		return
	end
	--On the lone wall's far side from where /audiotest drops you
	playSound("ClickPlant", OX + 18, 3, OZ - 21)
	schedule(1000, "audioTestWallClick")
end

function audioTestBrickCycle()
	if not audioTestRunning then
		return
	end
	for _, spot in ipairs(audioTestBrickSpots) do
		if spot.brick then
			--Breaking with the effect makes clients play BrickBreak where the debris flies off
			spot.brick:remove(true)
			spot.brick = nil
		else
			spot.brick = addBrick(spot.x, 1, spot.z, 2, 3, 2, 1, 1, 0.3, 1)
			if spot.brick then
				playSound("ClickPlant", spot.x + 1, 1, spot.z + 1)
			end
		end
	end
	schedule(1500, "audioTestBrickCycle")
end

function audioTestChat(client, message)
	local text = string.sub(message, string.len(client:getName()) + 3)

	if text == "/audiotest" then
		startAudioTest()
		if client:getNumControlled() > 0 then
			client:getControlledIdx(0):setPosition(OX, 5, OZ - 36)
		end
		client:message("Audio test: blue closed room (/audiodoor toggles its door), orange lone wall, purple brick box, green open control. /audiostop stops the sounds.")
		return client, ""
	elseif text == "/audiodoor" then
		if audioTestDoorOpen then
			closeAudioTestDoor()
			client:message("Closed room door closed.")
		else
			openAudioTestDoor()
			client:message("Closed room door open.")
		end
		return client, ""
	elseif text == "/audiostop" then
		stopAudioTest()
		client:message("Audio test sounds stopped.")
		return client, ""
	end

	return client, message
end
registerEventListener("ClientChat", "audioTestChat")
registerChatSuggestion("audiotest", "/audiotest - build the audio test rooms and stand in them")
registerChatSuggestion("audiodoor", "/audiodoor - open or close the closed room's door")
registerChatSuggestion("audiostop", "/audiostop - stop the audio test sounds")

function setUpAudioTest()
	buildAudioTest()
	saveBuild("AudioTest.lod", true)
	startAudioTest()
	info("Audio test area built at " .. OX .. ", " .. OZ .. ", type /audiotest in chat to go there")
end
schedule(100, "setUpAudioTest")

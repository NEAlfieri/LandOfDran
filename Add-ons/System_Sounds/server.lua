--[[
	System_Sounds

	The sound types the base game plays, with the old game's names and file names, and the two sounds
	the server makes for everyone: a brick being planted, and an admin logging in.

	A system that has sounds of its own registers them itself, so they come and go with it:
	System_Players has the ones a player joining, leaving or spawning makes, System_Damage the ones
	being hurt and dying make, and System_Inventory the tools'.
]]

--Clients play ClickMove, ClickRotate, Jump, and BrickBreak on their own when the server has sounds by
--those names. A file that isn't in Assets/sound/ logs an error and is skipped
newSoundType("ClickMove","Assets/sound/clickMove.wav")
newSoundType("ClickRotate","Assets/sound/clickRotate.wav")
newSoundType("ClickPlant","Assets/sound/clickPlant.wav")
newSoundType("Jump","Assets/sound/jump.wav")
newSoundType("BrickBreak","Assets/sound/breakBrick.wav")
newSoundType("Admin","Assets/sound/admin.wav")
newSoundType("BrickClear","Assets/sound/brickClear.wav")
--The server plays these itself where dynamics fall into or jump out of the water
newSoundType("Splash","Assets/sound/splash1.wav")
newSoundType("ExitWater","Assets/sound/exitWater.wav")
--And these from a player whose flashlight turns on or off
newSoundType("LightOn","Assets/sound/lightOn.wav")
newSoundType("LightOff","Assets/sound/lightOff.wav")
--The horn a new vehicle honks with when its driver left clicks, until its wrench dialog picks another sound
newSoundType("Honk","Assets/sound/434878__mickthemicguy__car-honking.wav")
--Getting into or out of a vehicle, a countdown or a confirmation, and an upload being accepted and then finished
newSoundType("PlayerMount","Assets/sound/playerMount.wav")
newSoundType("Beep","Assets/sound/beep.wav")
newSoundType("UploadStart","Assets/sound/uploadStart.wav")
newSoundType("ProcessComplete","Assets/sound/processComplete.wav")

--Music, which players can put on bricks by holding Insert and clicking one to open the wrench dialog
newSoundType("After School Special","Assets/music/After_School_Special.wav",true)
newSoundType("After School Special","Assets/music/analog.wav",true)
newSoundType("DJGriffen - Euphoria","Assets/music/djgriffinEuphoria.wav",true)
newSoundType("Drums","Assets/music/drums.wav",true)
newSoundType("Police Siren","Assets/music/policeSiren.wav",true)
newSoundType("Spring Birds","Assets/music/springBirds.wav",true)
newSoundType("Vehicle Hover","Assets/music/vehicleHover.wav",true)
newSoundType("Zero Day","Assets/music/zerodaywip.wav",true)
newSoundType("Ambient Deep","Assets/music/Ambient_Deep.ogg",true)
newSoundType("Bass 1","Assets/music/Bass_1.ogg",true)
newSoundType("Bass 2","Assets/music/Bass_2.ogg",true)
newSoundType("Bass 3","Assets/music/Bass_3.ogg",true)
newSoundType("Creepy","Assets/music/Creepy.ogg",true)
newSoundType("Distort","Assets/music/Distort.ogg",true)
newSoundType("Factory","Assets/music/Factory.ogg",true)
newSoundType("Icy","Assets/music/Icy.ogg",true)
newSoundType("Jungle","Assets/music/Jungle.ogg",true)
newSoundType("Paprika - Byakko no","Assets/music/Paprika_-_Byakko_no.ogg",true)
newSoundType("Peaceful","Assets/music/Peaceful.ogg",true)
newSoundType("Piano Bass","Assets/music/Piano_Bass.ogg",true)
newSoundType("Rock","Assets/music/Rock.ogg",true)
newSoundType("Stress","Assets/music/Stress_.ogg",true)
newSoundType("Vartan - Death","Assets/music/Vartan_-_Death.ogg",true)
newSoundType("Fire","Assets/music/fire.wav",true)
--newSoundType("Rain","Assets/music/dragon-studio-calming-rain-loop-398653.mp3",true)
--newSoundType("Rain2","Assets/music/dragon-studio-gentle-rain-01-437305.mp3",true)

--Everyone nearby hears a brick get planted, from its center
function plantSound(client, brick)
	--plantSpawnBrick took it back
	if not brick then
		return client, brick
	end

	local x, y, z = brick:getPosition()
	local width, height, length = brick:getDimensions()
	if brick:getAngleID() % 2 == 1 then
		width, length = length, width
	end
	playSound("ClickPlant", x + width / 2, (y + height / 2) * 0.4, z + length / 2)
	return client, brick
end
registerEventListener("ClientPlantBrick","plantSound")

--Just the player who got the eval password right hears it
function adminLoginSound(client)
	client:playSound("Admin")
	return client
end
registerEventListener("ClientAdminLogin","adminLoginSound")

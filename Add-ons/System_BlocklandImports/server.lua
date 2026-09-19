--What loadBlocklandSave turns the lights and emitters Blockland saves put on bricks into, loaded as an add-on of its own
--Blockland's lights are shared types, ours are settings on each brick, so addBlocklandLight gives each Blockland light type the settings
--(the same table as brick:setLight) its bricks' lights start with. Emitters use the emitter type addBlocklandEmitter names, or else the
--one whose uiName is the Blockland name (see System_Emitters). Music uses a music sound type with the same name (see newSoundType)

--The emitter types the names below are matched against
requireAddOn("System_Emitters")

--Blockland lights have a radius and brightness. A Blockland unit is two studs, so a light is made just bright enough to reach twice its
--Blockland radius before our lights cut off (where brightness / (distance^2 + 1) drops under 0.02 of its brightest color channel), then
--scaled by its Blockland brightness over the basic lights' 9. Corona width is its flare's NearSize. blinkSpeed and blinkStrength are
--left out for lights that don't blink, see brick:setLight
local function blocklandLight(uiName, r, g, b, radius, brightness, coronaWidth, blinkSpeed, blinkStrength)
	local reach = radius * 2
	local brightestChannel = math.max(r, g, b, 0.01)
	addBlocklandLight(uiName, {
		color = {r, g, b},
		brightness = 0.05 * (reach * reach + 1) / brightestChannel * (brightness / 9),
		coronaWidth = coronaWidth,
		blinkSpeed = blinkSpeed,
		blinkStrength = blinkStrength
	})
end

--Light_Basic add-on
blocklandLight("Red Light", 1, 0, 0, 15, 9, 2)
blocklandLight("Orange Light", 1, 0.5, 0, 15, 9, 2)
blocklandLight("Yellow Light", 1, 1, 0, 15, 9, 2)
blocklandLight("Green Light", 0, 1, 0, 15, 9, 2)
blocklandLight("Cyan Light", 0, 1, 1, 15, 9, 2)
blocklandLight("Blue Light", 0, 0, 1, 15, 9, 2)
blocklandLight("Purple Light", 0.5, 0, 1, 15, 9, 2)
blocklandLight("Bright", 0.8, 0.9, 1, 20, 15, 1.8)

--Light_Animated add-on. Its lights step through keys, A as 0 up to Z as 1, spread evenly over a time in seconds. Ours dim and brighten
--smoothly, so the blinking ones get their full brightness, a cycle as long as the time between their flashes, and a strength from how
--dark they get between flashes. RGB and Alarm animate color and radius, which ours can't, so they get about what they average

--Brightness keys AZQZFZA over 1 second: three flashes a second, dropping to 0, 0.64, and 0.2 between them, which average 0.28
blocklandLight("Yellow Blink", 1, 1, 0, 10, 9, 3, 1 / 3, 0.72)
--Brightness keys AZAZAZA over half a second: three flashes all the way on and off every half second
blocklandLight("Strobe", 0.7, 1, 1, 15, 30, 1, 1 / 6, 1)
blocklandLight("RGB", 1, 1, 1, 15, 9, 1)			--Steps through red, green, and blue
blocklandLight("Alarm", 1, 0, 0, 7, 9, 1)			--Pulses red, its radius swinging between 1 and 10

--Blockland only ships these compiled, so they're guesses at how they look
blocklandLight("Player's Light", 1, 1, 1, 10, 9, 0)
blocklandLight("White Ambient", 1, 1, 1, 30, 5, 0)
blocklandLight("White Ambient Dim", 1, 1, 1, 30, 2, 0)

--Blockland's Camera Glow is the admin orb, which System_Emitters has under the old game's uiName
addBlocklandEmitter("Camera Glow", "CameraEmitter")

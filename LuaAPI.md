# Land of Dran Server Lua API

This documents every function the server-side Lua environment exposes to scripts
(`serverstart.lua` and anything `dofile`'d from it, or run through the eval console).
Generated from the current `LuaFunctions/*.cpp` and `Networking/PacketsFromClient/*.cpp`
source - if you add or change a binding, update this file too.

## Conventions

- Every `Dynamic`, `StaticObject`, client, brick, light, and emitter table has an `id` field (its net ID) and a
  `type` field you can compare against: `1` = Dynamic, `2` = Static, `3` = Client, `4` = Brick,
  `5` = Light, `6` = Emitter (`NetTypes/NetType.h`'s `SimObjectType`). `raycast()` and `client:getCursorItem()` can
  return a Dynamic, a Static, or a Brick, so check `.type` before assuming which. Items are Dynamics too, with
  more methods, see [Items](#items).
- Functions documented as `Expected N arguments` in an error message are strict about
  argument count - passing the wrong number logs an error and does nothing (they don't
  throw a Lua error, so a mistake here fails silently unless you're watching the log).
- Color arguments (`r,g,b[,a]`) are floats in the 0-1 range.
- Mouse button masks (used by `ClientClick`, see below) follow SDL's convention:
  `1` = left, `2` = middle, `4` = right. Check with `(mask & 1) ~= 0`, etc. - more than
  one bit can be set if multiple buttons are held.

---

## Logging / misc

| Function | Arguments | Description |
|---|---|---|
| `info(...)` | any number of values | Logs a line to the server's info log. Values are stringified (tables holding a Dynamic/Static/Client/Brick/Light/Emitter print as `[Dynamic N]`/`[Static N]`/`[Client N]`/`[Brick N]`/`[Light N]`/`[Emitter N]`). |
| `error(...)` | any number of values | Same as `info`, but logged as an error and prefixed accordingly. |
| `debug(...)` | any number of values | Same as `info`, but only logged when the `logger/verbose` setting is on. |
| `shutdown()` | none | Stops the main program loop (shuts the whole process down, not just the server). |

## Time of day, sky, and water

The server owns the time of day, the look of the day/night cycle, and the water level. It sends
them to every client once a second, and right away when one of these functions changes them or a
client finishes joining.

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `setTimeOfDay(fraction)` | `fraction`: `0` = midnight, `0.25` = sunrise, `0.5` = noon, `0.75` = sunset. Values outside 0-1 wrap around. | none | Jumps to that time of day. The server starts at noon. |
| `getTimeOfDay()` | none | number, 0-1 | Current time of day, on the same scale as `setTimeOfDay`. |
| `setTimeScale(scale)` | `scale`: in-game seconds that pass per real second | none | A full day is 1000 in-game seconds (`DAY_LENGTH_SECONDS` in `LandOfDran.h`), so the default of `1` is a ~16.7 minute day. `0` freezes time, negative values run it backwards. |
| `getTimeScale()` | none | number | Current time scale. |
| `setWaterLevel([y])` | `y`: world height of the water surface, or no argument / `nil` | none | Puts a water surface at height `y` across the whole world, or removes it when called with no argument. Off by default. Dynamics in the water float or sink depending on their buoyancy (see `dynamic:setBuoyancy`) and are slowed by drag. Players at least half under water swim: W and S follow the camera up and down, A and D stay level, holding jump swims up, and pressing jump with their head above the surface jumps out. The server plays the `Splash` sound where a dynamic falls in fast and `ExitWater` where one comes out fast, if they're registered. Clients draw ripples spreading across the surface where dynamics go in, come out, or move along it. |
| `getWaterLevel()` | none | number, or `nil` if there's no water | Current water height. |
| `setRain(intensity)` | `intensity`: `0` for none up to `1` for a downpour, clamped to that range | none | Makes it rain everywhere. Off by default. Clients ease into the new intensity over a few seconds. They play a rain loop that gets quieter and muffled the less open sky is above the camera, and, unless their `graphics/rainquality` is off, draw falling drops and splashes only where nothing is overhead, and darken and add shine to surfaces the rain reaches. Surfaces dry off slowly after the rain stops. Rain doesn't change the sky or sun, pair it with `setSunColor` / `setFogColor` / `setFogDistance` for an overcast look. |
| `getRain()` | none | number, 0-1 | Current rain intensity. |

All of these except the getters use the strict `Expected 1 number argument` check described
above (`setWaterLevel` also accepts no arguments).

The day/night cycle blends between four phases: `"night"`, `"dawn"`, `"day"`, and `"dusk"` (phase
names ignore case). Each has its own sky, fog, and sun color, and the colors you see at any moment
are a mix of the phases on either side of the current time. Everything is lit by the ambient color,
light from the sky itself, and the sun adds its light on top wherever it reaches, so shadows are
the ambient color with the sun taken away. At night the "sun" is the moon, shining from the opposite
side of the sky.

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `setSkyColor(phase, r, g, b)` | phase name, color | none | Color of the sky straight up during that phase. It fades into the fog color toward the horizon. |
| `getSkyColor(phase)` | phase name | r, g, b | That phase's sky color. |
| `setFogColor(phase, r, g, b)` | phase name, color | none | Color of the fog, and of the sky near the horizon, during that phase. |
| `getFogColor(phase)` | phase name | r, g, b | That phase's fog color. |
| `setSunColor(phase, r, g, b[, brightness])` | phase name, color, optional brightness | none | Color of the sunlight (moonlight for `"night"`) during that phase. Sunlight is much brighter than a screen color, so the color is multiplied by `brightness`; leave it out to keep the phase's current brightness. Defaults: `day` is `1, 0.7, 0.5` at brightness `15`, `dawn` and `dusk` are about `6`, `night` is `0.5, 0.6, 1` at `0.35`. |
| `getSunColor(phase)` | phase name | r, g, b, brightness | That phase's sunlight, split into a color whose brightest channel is 1 and its brightness. |
| `setAmbientColor(phase, r, g, b)` | phase name, color | none | Light from the sky that reaches everything, shadows included, during that phase. It's dim next to sunlight, so small values go a long way. Defaults: `day` is about `0.45, 0.32, 0.22`, `dawn` and `dusk` about `0.25, 0.2, 0.2`, `night` about `0.015, 0.018, 0.03`. Values above 1 are allowed. |
| `getAmbientColor(phase)` | phase name | r, g, b | That phase's ambient color. |
| `setFogDistance(start, end)` | distances from the camera in world units | none | Fog begins at `start` and completely hides everything past `end`. Needs `0 <= start < end <= 900`. Defaults to `150, 290`. Grass and water always reach past `end`, and shadows cover the view out to `end`, so a longer fog distance spreads the same shadow detail over more ground. |
| `getFogDistance()` | none | start, end | Current fog distances. |
| `resetDayCycle()` | none | none | Puts every phase's sky, fog, sun, and ambient color and the fog distances back to their defaults. Doesn't change the time of day or time scale. |

The setters log an error and do nothing if the phase name is unknown or the arguments are the wrong
count or type. Negative colors and brightness are treated as 0.

### Skyboxes

By default the sky is the gradient from the colors above. A server can replace it with a day skybox
and a night skybox instead. They cross fade while the sun is near the horizon (starting a little
before sunrise, done a little after). Leaving one out keeps the gradient for that part of the day.
Clients load skyboxes from their own copy of the game folder, so every player needs the same files.

A skybox path is one of:

- A `.hdr` file (Radiance HDR, equirectangular like `Assets/ibl/main.hdr`), with straight up at the
  top of the image. Clients with the **Image Based Lighting** graphics setting on (the default) also
  light everything with it: bricks, models, water, and lit particles get their ambient light from
  the sky around them in place of the ambient color, and shiny materials (Chrome, Pearl, Foil,
  Slippery) reflect it. The day cycle's sun still adds its light and shadows on top, and the
  brightest parts of the image (such as a sun in the photo) are capped for lighting so that sunlight
  isn't counted twice. The sun and moon discs aren't drawn over a `.hdr` sky. With the setting off,
  a `.hdr` sky is only drawn, and the ambient color lights the world as usual.
- Five `.png` face images in the old game's layout, given as the path without the `_N.png` ending,
  e.g. `"Assets/skyboxes/bluecloud"` for `bluecloud_0.png` to `bluecloud_4.png`: `_0` is the top,
  `_1` +x, `_2` -x, `_3` +z, `_4` -z. An optional `_5.png` is the bottom, otherwise the top is used
  again. Images are drawn as they are and never light anything. The sun and moon are drawn over them.

Every skybox fades into the fog color toward the horizon, so the fogged edge of the world blends into it.

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `setSkybox([day[, night]])` | `day`, `night`: skybox paths, or `nil` for the gradient | none | Sets both skyboxes at once and sends them to every client. `setSkybox()` goes back to the gradient all day. Logs an error and changes nothing if a path isn't relative, reaches outside the game folder, is longer than 255 characters, or the server doesn't have the file(s). |
| `getSkybox()` | none | day, night | Current skybox paths, `nil` for the gradient. |

```lua
setSkybox("Assets/skyboxes/bluecloud", "Assets/skyboxes/space")  -- clouds by day, stars by night
setSkybox("Assets/ibl/main.hdr")                                  -- lit by a photo of a road by day, gradient at night
```

## Scheduling

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `schedule(delayMS, functionName, ...)` | `delayMS`: milliseconds from now to run; `functionName`: **string** name of a global function; any further arguments are passed through to that function when it runs | schedule ID (number) | Calls the named global function once, after `delayMS` milliseconds. Extra arguments after `functionName` are forwarded to it. Runs are one-shot - call `schedule` again inside the callback for a repeating timer (see `PickupSystem.lua`'s bob loop). |
| `cancel(scheduleID)` | `scheduleID`: value returned by `schedule` | none | Cancels a pending scheduled call before it fires. No-op if it already fired or was already cancelled. |

## Events

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `registerEventListener(eventName, functionName)` | both strings | none | Binds a global function as a listener for a built-in event. Multiple listeners can be bound to the same event; they run in registration order, each receiving whatever the previous one returned (see below), so every listener for a given event must accept and return the same argument list. |
| `unregisterEventListener(eventName, functionName)` | both strings | none | Removes a previously-registered listener. |
| `getNumListeners(eventName)` | string | count | How many functions are bound to an event. |
| `getListenerIdx(eventName, index)` | string, 0-based index | function name (string) | Name of the listener at that index. |

### Built-in events

| Event | Listener signature | Notes |
|---|---|---|
| `ClientJoin` | `function(client) ... return client end` | Fires once a client finishes phase-1 loading (right after connecting). `serverstart.lua`'s `join()` creates and gives them their player dynamic here. |
| `ClientLeave` | `function(client) ... return client end` | Fires when a client disconnects, before it's removed from the client list. Use this to clean up anything the client owned (see `PickupSystem.lua`'s `dropHeldOnLeave`). |
| `ClientChat` | `function(client, message) ... return client, message end` | Fires when a client sends a chat message, before it's broadcast. Return a modified `message` to alter it, or an empty string to suppress it. |
| `ClientPlantBrick` | `function(client, brick) ... return client, brick end` | Fires after a client plants its ghost brick and the server accepts it. The brick is already placed and sent to clients; call `brick:remove()` to take it back out. |
| `ClientAdminLogin` | `function(client) ... return client end` | Fires when a client enters the right eval console password. Not fired for the single player host, who is made admin automatically. `serverstart.lua` plays the `Admin` sound to them here. |
| `ClientClick` | `function(client, posX, posY, posZ, dirX, dirY, dirZ, mask) ... return client, posX, posY, posZ, dirX, dirY, dirZ, mask end` | Fires on every mouse click. `posX/Y/Z` and `dirX/Y/Z` are the camera's position and look direction *at the moment of the click*; `mask` is the SDL mouse button mask (see Conventions). |
| `ClientStartTalking` | `function(client) ... return client end` | Fires when a client starts sending voice chat. Calling `client:setVoiceMuted(true)` here cuts them off before anyone hears them. |
| `ClientStopTalking` | `function(client) ... return client end` | Fires when a client lets go of push to talk, or half a second after their voice stops arriving (a lost last packet, or muted while talking). Not fired for a client who leaves while talking. |
| `ClientWrenchBrick` | `function(client, brick) ... return client, brick end` | Fires when a client holds Insert and left clicks a brick within 100 studs of their camera, before its wrench dialog opens. Return `client, nil` to keep the dialog closed, or another brick to open that one's dialog instead. Not fired by `client:openWrenchDialog`, which is what the wrench item in `Inventory.lua` uses. |
| `ClientClickRelease` | `function(client, posX, posY, posZ, dirX, dirY, dirZ, mask) ... return client, posX, posY, posZ, dirX, dirY, dirZ, mask end` | Fires when a client lets go of a mouse button in game, even over a window. Same arguments as `ClientClick`, except `mask` is only the button let go. `Inventory.lua` stops swinging the hammer or wrench here. |
| `ClientSliceBricks` | `function(client, brickCount) ... return client, brickCount end` | Fires when a client's selection box would make a vehicle, after every rule it has to follow checks out, with how many bricks (wheels included) would be sliced. Return `client, nil` to stop it, which leaves the bricks where they are and tells the client nothing. See [Vehicles](#vehicles). |
| `VehicleCreated` | `function(vehicle, builder) ... return vehicle, builder end` | Fires once a vehicle is finished being made, by slicing, loading a save (by a client or `loadVehicleFile`), or `sliceBricks`, with the client who made it or `nil` for Lua. Return values are ignored. `serverstart.lua` makes every new vehicle destructable here. |
| `ClientEnterVehicle` | `function(client, vehicle, seat) ... return client, vehicle, seat end` | Fires when a client right clicks a vehicle, before they get in: to drive it (`seat` is `nil`) when nobody is, otherwise onto its free passenger seat nearest where they clicked (`seat` 0 or more). Return `client, nil, seat` to keep them out. Not fired by `client:enterVehicle`. |
| `ClientExitVehicle` | `function(client, vehicle, seat) ... return client, vehicle, seat end` | Fires after a client gets out of a vehicle (`seat` is `nil` for the driver, else the passenger seat they were on) by right clicking, or because their player was destroyed or given to someone else while in it. Not fired by `client:exitVehicle`, `vehicle:ejectDriver`, removing the vehicle, or leaving the server. |
| `ClientWrenchVehicle` | `function(client, vehicle) ... return client, vehicle end` | Fires when a client holds Insert and left clicks a vehicle, before its wrench dialog opens. Return `client, nil` to keep the dialog closed. Not fired by `client:openWrenchDialog`. |
| `ClientLoadVehicle` | `function(client, brickCount, asVehicle) ... return client, brickCount, asVehicle end` | Fires when a vehicle save a client uploaded from their Saved Vehicles window is about to be placed, with how many bricks it has (wheels included) and whether it's loading as a vehicle or as bricks. Return `client, nil` to stop it, which tells the client nothing. Not fired by `loadVehicleFile`. See [Vehicles](#vehicles). |
| `ClientRemoveVehicle` | `function(client, vehicle) ... return client, vehicle end` | Fires when a client confirms Remove vehicle in a vehicle's wrench dialog, before it's removed. Return `client, nil` to keep it. Not fired by `vehicle:destroy` or `clearAllVehicles`. |
| `ClientDropItem` | `function(client, slot) ... return client, slot end` | Fires when a client presses their drop item key with Ctrl (Ctrl+W by default), with the slot their item bar has picked (0-4), whether or not there's an item in it or their items are out. Nothing is dropped unless a listener does it; `Inventory.lua` throws the item in their hand. |
| `ProjectileHit` | `function(projectile, hit, x, y, z, tag) ... return projectile, hit, x, y, z, tag end` | Fires the first time a projectile from `addProjectile` touches something that collides: a Dynamic, Static, Brick, or Vehicle as `hit`, or `nil` for the ground. `x, y, z` is where on `hit` they touched, and `tag` is the tag it was fired with. It's removed right after its listeners run, unless one already removed it. Return values are ignored. `Inventory.lua` bursts launcher shells here. |

---

## Dynamics

Dynamics are physics-simulated objects (players, projectiles, pickups, etc).

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `createDynamic(typeID, x, y, z)` | `typeID` from `newDynamicType`/`getDynamicType`; spawn position | Dynamic | Spawns a new dynamic of the given type at the given position. |
| `getDynamicId(netId)` | net ID | Dynamic | Looks up a dynamic by its net ID. Errors if it doesn't exist. |
| `getDynamicIdx(index)` | 0-based index | Dynamic | Looks up a dynamic by its position in the internal list (see `getNumDynamics`). |
| `getNumDynamics()` | none | count | How many dynamics currently exist. |
| `newDynamicType(scriptName, modelFilePath, scaleX, scaleY, scaleZ)` | `scriptName`: unique name used to refer to this type later; `modelFilePath`: path to the model file; scale on each axis | typeID | Registers a new kind of dynamic (model + scale). Call once at startup per type. |
| `getDynamicType(scriptName)` | string | typeID | Looks up a previously-registered type's ID by its script name. |
| `addAnimation(typeID, animationName, startFrame, endFrame, speed, fadeInMS, fadeOutMS)` | type to attach the animation to; frame range (the model file's animation ticks, which for an FBX are its frame numbers minus 1); playback speed in ticks per ms; fade in/out durations in ms | none | Adds a named animation clip to a dynamic type. The first animation added to a type is used as its walk cycle. One named `grab` plays on a player whenever its client left clicks in game, for everyone. While several play at once, animations added later play over earlier ones, but only on the parts of the model they actually move (a grab only takes over the arm it swings, the legs keep walking). Players' heads also turn to show where their camera looks, if the model has a node named `Head`. |
| `raycast(startX, startY, startZ, endX, endY, endZ[, dynamicToIgnore])` | ray start/end points; optionally a Dynamic to exclude from the hit test | hit object, x, y, z, normalX, normalY, normalZ, distance; or `nil` | Casts a ray through the physics world. Returns the Dynamic, Static, or Brick it hit first, then the world position of the hit, the normal of the surface it hit (pointing out of it), and the distance from the start point. If it hit the ground, which has no object, the hit object is `nil` and the rest still follow. Returns just `nil` if it hit nothing. `local hit = raycast(...)` still works if you only need the object. |
| `addProjectile(typeID, x, y, z, velX, velY, velZ[, tag[, shooter]])` | dynamic type ID; position; velocity in studs per second; any string, `""` by default, or `nil`; a Dynamic, or `nil` | Dynamic | Fires a dynamic that falls with gravity and is turned every tick so its model's +Y points the way it's going (while faster than 8 studs a second). It never falls asleep, and is swept along each physics step so it doesn't skip through thin bricks. It passes through `shooter`, usually the player who fired it. Clients only draw it where the server has it, it never bumps into their own player. The first time it touches anything that collides, the ground included, `ProjectileHit` fires with `tag` and it's removed. Bricks and statics with collision off don't count. |

### `dynamic:` methods

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `dynamic:destroy()` | none | none | Removes the dynamic from the world and un-controls it for any client controlling it. |
| `dynamic:getPosition()` | none | x, y, z | Current world position. |
| `dynamic:setPosition(x, y, z)` | position | none | Teleports the dynamic. |
| `dynamic:getVelocity()` | none | x, y, z | Current linear velocity. |
| `dynamic:setVelocity(x, y, z)` | velocity | none | Sets linear velocity directly. |
| `dynamic:getAngularVelocity()` | none | x, y, z | Current angular velocity. |
| `dynamic:setAngularVelocity(x, y, z)` | angular velocity | none | Sets angular velocity directly. |
| `dynamic:setAngularFactor(x, y, z)` | per-axis multiplier (0 = locked) | none | Restricts which axes the physics engine is allowed to rotate the object around, e.g. `(0,0,0)` to stop it tipping over. |
| `dynamic:activate()` | none | none | Wakes the physics body up if it was asleep. |
| `dynamic:isActive()` | none | bool | Whether the physics body is currently active (not asleep). |
| `dynamic:getGravity()` | none | x, y, z | Current per-object gravity vector. |
| `dynamic:setGravity(x, y, z)` | gravity vector | none | Overrides gravity for just this object. |
| `dynamic:getFriction()` | none | value | Current friction coefficient. |
| `dynamic:setFriction(friction)` | 0-10, clamped | none | Sets friction. |
| `dynamic:getRestitution()` | none | value | Current restitution (bounciness). |
| `dynamic:setRestitution(restitution)` | 0-10, clamped | none | Sets restitution. |
| `dynamic:getRotation()` | none | w, x, y, z | Current orientation as a quaternion. |
| `dynamic:setRotation(w, x, y, z)` | full quaternion | none | Sets orientation from an explicit quaternion. |
| `dynamic:setRotation(yaw, pitch, roll)` | 3 args instead of 4 | none | Alternate overload: sets orientation from Euler angles instead of a quaternion. |
| `dynamic:getMass()` | none | value | Current mass. |
| `dynamic:setMassProps(mass, centerX, centerY, centerZ)` | mass and local center of mass | none | Sets mass and center of mass together. |
| `dynamic:setMeshColor(meshName, r, g, b, a)` | mesh name within the model, color | none | Recolors one mesh of the model and broadcasts the change to clients. |
| `dynamic:setMeshDecal(meshName, decalName)` | mesh name within the model; file name of an image in `Assets/faces` or `Assets/shirts` (e.g. `"smiley.png"` or `"Mod-Police.png"`, up to 64 characters), or `""` to remove it | none | Shows a face or shirt on one mesh, drawn over its color, and broadcasts the change. The image covers the mesh's texture coordinates from 0 to 1, or only the rectangle a `decalarea` line in the model's `.txt` gives that mesh (`decalarea`, the mesh name, then the texture coordinates of the image's top left and bottom right corners, all tab separated), with nothing outside it; the default player's `Torso` has one covering its front. A model's face plate (a mesh named `Face1`, or `Face` without one) is see-through except for the face, so without a face it isn't drawn at all, and it casts no shadow or outline. Clients look the name up in their own `Assets/faces` folder, then `Assets/shirts`, so one they don't have isn't shown. |
| `dynamic:setHighlight(r, g, b, a, thickness)` | color; `thickness` is how far (in world units) the outline extends past the model's surface | none | Applies an outline/highlight effect around the whole object and broadcasts it to clients. |
| `dynamic:clearHighlight()` | none | none | Removes the outline/highlight effect. |
| `dynamic:setNameTag(text, r, g, b)` | text up to 64 characters, `""` for none; color, 0-1 each | none | Puts floating text over the object for every client, drawn over the world above its collision box, and broadcasts it. Clients don't draw the tag on the object they control, so you never see your own, and a tag fades out past 150 world units and is left off past 250. `serverstart.lua` gives each player their client's name in `ClientJoin`. |
| `dynamic:getNumControllers()` | none | count | How many clients currently control this dynamic (usually 0 or 1; 0 means it's a normal server-simulated object, not a player). |
| `dynamic:getControllerIdx(index)` | 0-based index | Client | The client controlling this dynamic at that index. |
| `dynamic:snapToCursor(client, xOffset, yOffset, zOffset)` | client to attach to; view-space offset: `x` = right, `y` = up, `z` = distance in front of the camera | none | Attaches the dynamic to a client's cursor: every physics tick its position is recomputed from that client's live camera position/direction plus this offset, and its gravity is disabled. Calling this again while already snapped just updates the client/offset. |
| `dynamic:unsnap()` | none | none | Detaches from the cursor (if snapped) and restores the gravity it had before snapping. No-op if not snapped. |
| `dynamic:isSnapped()` | none | bool | Whether the dynamic is currently snapped to any client's cursor. |
| `dynamic:getSnapClient()` | none | Client or `nil` | The client it's snapped to, or `nil` if not snapped. |
| `dynamic:playSound(name[, pitch, volume])` | sound type name; see [Sounds](#sounds) | none | Plays a sound once for everyone, following the dynamic as it moves. |
| `dynamic:startSoundLoop(name[, pitch, volume])` | sound type name; see [Sounds](#sounds) | loop ID | Starts a looping sound that follows the dynamic. It stops by itself when the dynamic is destroyed. |
| `dynamic:setBuoyancy(buoyancy)` | 0-10, clamped; default 1.3 | none | How hard water pushes the dynamic up, as a multiple of its weight when it's fully under. `0` sinks (slowed by drag), `1` hangs wherever it is, higher values float with less of it under. Sent to clients too, since they simulate the dynamics they control (players) in water themselves. A swimming player holds their depth while moving, so buoyancy only decides whether they sink or float up while they aren't swimming. |
| `dynamic:getBuoyancy()` | none | number | Current buoyancy. |
| `dynamic:isItem()` | none | bool | Whether it's an item, which has the `item:` methods below too. |
| `dynamic:isProjectile()` | none | bool | Whether `addProjectile` made it. |

---

## Items

Items are tools like the hammer: dynamics that players can carry in their inventory. On the ground an item is an
ordinary dynamic. It falls, collides, and floats, every `dynamic:` method works on it, and `raycast()` and
`client:getCursorItem()` can hit it. Item tables are Dynamic tables (`type` is `1`) that have the `item:` methods
below as well, so check with `dynamic:isItem()`.

Each client can carry 5 items, in slots 0 to 4. While an item is carried its body is out of the physics world. It
doesn't collide, fall, or float, and `setPosition`, `setRotation`, `setVelocity`, `setAngularVelocity`, `activate`,
and `snapToCursor` do nothing. `getPosition` gives the position of the player carrying it. Settings like gravity,
friction, and buoyancy are kept for when it's back on the ground. Items a leaving client still carries go back into the
world where they were, after `ClientLeave` listeners run.

Players press Q (the "Show/Hide Items" key) to slide their items out on the right of the screen, which puts the item in
the picked slot in their player's right hand for everyone to see, or in front of their camera in first person. The mouse
wheel picks another slot while their items are out. Pressing Q again, or a brick hot bar slot's key, puts them away. A
carried item is held by the first dynamic `client:setDefaultController` gave its client, and isn't drawn anywhere
without one. Pressing Ctrl+W fires `ClientDropItem`, and letting go of a mouse button fires `ClientClickRelease`.

`Inventory.lua`, run from `serverstart.lua`, gives every player who joins the `hammer`, `wrench`, `paintCan`, and
`dranLauncher` item types `serverstart.lua` adds, and removes those when they leave (other items they carry are dropped). Left clicking an
item on the ground within 10 studs picks it up into the first empty slot. Holding left mouse with the hammer or wrench
in hand swings it, hitting right away and then about once a second for as long as it's held, except the wrench stops once
it opens a dialog. The hammer knocks loose a brick it's clicked on (`brick:remove(true)`), and the wrench opens the
brick's wrench dialog, playing `WrenchHit`. Hitting anything else within reach, the ground included, plays `HammerHit` or
`WrenchMiss` there. Every hit makes the tool's spark and explosion emitters (`hammerSparkEmitter` and
`hammerExplosionEmitter`, or the wrench's) where it hit. Holding left mouse with the paint can sprays a `paintEmitter` stream in the player's paint color from
the can to what they look at, with the `SprayLoop` sound, and paints every brick within 13 studs the crosshair passes
over with their paint color and material (`client:getPaintColor`, `client:getPaintMaterial`), checking about every 30 ms.
Left clicking with the launcher in hand plays its `fire` animation and the `Launch` sound, puts a `gunSmokeEmitter` at the
end of its barrel, and fires a `launcherShell` (`addProjectile`, tagged `"launcherShell"`) at 90 studs a second toward
whatever the crosshair is on, trailing a `shellTrailEmitter`, at most once every 650 ms. Where a shell lands it makes a
`radiusImpulse` of 140, three `hammerExplosionEmitter` puffs, and two `FogEmitterA` that stop after a second.
Ctrl+W throws the item in hand the way the player looks.

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `newItemType(scriptName, modelFilePath, scaleX, scaleY, scaleZ, uiName, iconPath)` | same as `newDynamicType`; the name shown in the item bar; an image for its slot, relative to the game folder, or `""` for none, which shows the name instead | typeID | Registers a kind of item. The type ID works anywhere a dynamic type's does, like `addAnimation` and `getDynamicType`. Call it at startup, before anyone joins. An icon that isn't a file in the game folder logs an error and the type gets none. Clients load the icon from their own game folder. A model with no `Collision` mesh collides as a box around the whole model. |
| `setItemHand(typeID, gripX, gripY, gripZ, pitch, yaw, roll)` | item type ID; the point on the model that goes in the hand, in world units after scaling; degrees around the x, y, and z axes | none | How items of a type sit in a hand. Unturned, the model's +Y points up out of the hand and its -Z the way its holder faces, and a negative pitch leans its top forward. Call it at startup, before anyone joins. By default the model's origin is in the hand, unturned. |
| `createItem(typeID, x, y, z)` | item type ID from `newItemType`; position | Item | Spawns an item on the ground. Logs an error for a type that isn't an item type. |
| `getNumItems()` | none | count | How many items exist, carried or not. |
| `getItemIdx(index)` | 0-based index | Item | The item at that position among all items. |

### `item:` methods

Along with every `dynamic:` method.

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `item:isHeld()` | none | bool | Whether it's in someone's inventory. |
| `item:getHolder()` | none | Client or `nil` | The client carrying it. |
| `item:getSlot()` | none | slot or `nil` | Which of its carrier's slots it's in, 0-4. |
| `item:isEquipped()` | none | bool | Whether it's in its carrier's hand: their items are out with its slot picked. |
| `item:playAnimation(name[, loop])` | `"swing"`, or the name of an animation `addAnimation` gave its type; `loop` defaults to false | none | Plays the animation for everyone, once or over and over. Every item can `"swing"`, tipping forward around its grip until its top points 90 degrees further toward the ground and back, a bit over a fifth of a second each time. Only one animation loops at a time, starting a loop replaces the last. Logs an error for an animation it doesn't have. |
| `item:stopAnimation([name])` | animation name, or nothing | none | Stops the looping animation if it's the one named, or whatever loops without a name. A swing finishes the one it's partway through. |
| `item:getItemName()` | none | string | Its type's name in the item bar, like `"Hammer"`. |
| `item:getTypeName()` | none | string | Its type's script name, like `"hammer"`. |

---

## Statics

Statics are non-moving objects that still have a mesh and physics presence (walls, floor tiles, buttons, etc). They reuse the same "dynamic type" definitions (model + scale) as dynamics.

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `createStatic(typeID, x, y, z)` | type ID (from `newDynamicType`/`getDynamicType`); position | Static | Spawns a static object of the given type. |
| `getStaticId(netId)` | net ID | Static | Looks up a static by its net ID. |
| `getStaticIdx(index)` | 0-based index | Static | Looks up a static by its position in the internal list. |
| `getNumStatics()` | none | count | How many statics currently exist. |

### `static:` methods

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `static:destroy()` | none | none | Removes the static from the world. |
| `static:getPosition()` | none | x, y, z | Current position. |
| `static:getRotation()` | none | w, x, y, z | Current orientation as a quaternion. |
| `static:getFriction()` | none | value | Current friction coefficient. |
| `static:setFriction(friction)` | 0-10, clamped | none | Sets friction. |
| `static:getRestitution()` | none | value | Current restitution. |
| `static:setRestitution(restitution)` | 0-10, clamped | none | Sets restitution. |
| `static:setMeshColor(meshName, r, g, b, a)` | mesh name, color | none | Recolors one mesh and broadcasts the change. |
| `static:setHighlight(r, g, b, a, thickness)` | color; `thickness` is how far (in world units) the outline extends past the model's surface | none | Applies an outline/highlight effect around the whole object and broadcasts it to clients. |
| `static:clearHighlight()` | none | none | Removes the outline/highlight effect. |
| `static:setColliding(bool)` | true/false | none | Enables or disables collision for the object without removing it. |
| `static:setHidden(bool)` | true/false | none | Shows or hides the object client-side. |

---

## Lights

Lights that shine from a spot in the world, either in every direction or, as spotlights, in a cone
(see `light:setConeAngle`). They have no model or collision.
Clients light everything around them (bricks, models, grass, and the water surface, which also shows
glints of them on its waves) with inverse square falloff, draw a
glowing corona where they are, and give the lights nearest the camera shadows. How many get shadows
is each player's `graphics/pointshadows` setting (0 to 8, default 4). Up to 32 lights light the view
at once, the nearest ones win. A brick a light is inside doesn't cast that light's shadows, so lights
in the middle of bricks (like the ones on bricks, see `brick:setLight`) shine out of them.

A light reaches until it's too dim to see: roughly `sqrt(brightness * 50)` studs for a light whose
brightest color channel is 1, up to 500 (`light:getRange()` gives the exact value). As a guide,
`brightness` 50 at 5 studs is about an eighth of noon sunlight, so a lamp is around 20-100 and a
floodlight a few thousand.

Players' flashlights (see `client:setFlashlightEnabled`) are lights too: an 80 degree spotlight with
brightness 150 and a 0.8 stud corona, which shows up in `getNumLights` and `getLightIdx` while it's on.
Each client shines it from just past the holding player's `Right_Hand` mesh as it's drawn (or in
front of their eyes if their model has no such mesh) toward where that player looks. Like any
spotlight's, its corona only shows to people inside the beam, so its owner doesn't see their own.
`light:getPosition()` gives the player's position, and the server keeps pointing it with
`setDirection`. `light:setPosition` takes it out of the player's hand, and `light:destroy()`
switches it off without the `LightOff` sound.

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `createLight(x, y, z, r, g, b, brightness, flicker, coronaWidth)` | position; color 0-1 per channel; `brightness` 0 or more (clamped to 100000); `flicker` in world units (0-16); `coronaWidth` in world units (0-256, 0 for no corona) | Light | Places a point light. `flicker` is how far the light wanders around its position: every 40-160 ms it picks a new random spot within that distance and glides there, easing in and out, which makes its lighting and shadows waver like a flame. Each client picks its own spots. A flickering light with shadows redraws them every frame. |
| `getLightId(netId)` | net ID | Light | Looks up a light by its net ID. |
| `getLightIdx(index)` | 0-based index | Light | Looks up a light by its position in the internal list. |
| `getNumLights()` | none | count | How many lights currently exist. |

### `light:` methods

All of these use the strict argument count check, and setters log an error and do nothing if an
argument isn't a finite number.

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `light:destroy()` | none | none | Removes the light. |
| `light:getPosition()` | none | x, y, z | Position, without flicker. |
| `light:setPosition(x, y, z)` | position | none | Moves the light. |
| `light:getColor()` | none | r, g, b | Color. |
| `light:setColor(r, g, b)` | 0-1, clamped | none | Sets the color. |
| `light:getBrightness()` | none | number | Brightness. |
| `light:setBrightness(brightness)` | 0-100000, clamped | none | Sets the brightness. `0` turns the light and its corona off. |
| `light:getFlicker()` | none | number | Flicker distance. |
| `light:setFlicker(distance)` | world units, 0-16, clamped | none | How far the light wanders around its position. |
| `light:getBlink()` | none | speed, strength | Blink cycle time and strength. A new light has speed `0` and strength `1`. |
| `light:setBlink(speed, strength)` | `speed` seconds, 0-60, clamped; `strength` 0-1, clamped | none | Makes the light and its corona dim and brighten again smoothly. `speed` is how long one whole cycle takes, `0` for no blinking. `strength` is how much of its brightness it loses at the dimmest point, halfway through each cycle: `1` goes fully dark, `0.5` drops to half. How far it reaches (`getRange`) doesn't change. Each client runs the cycle on its own clock, so players see it at different points, but lights with the same speed stay in step for any one player. |
| `light:getCoronaWidth()` | none | number | Corona width. |
| `light:setCoronaWidth(width)` | world units, 0-256, clamped | none | Width of the glow drawn at the light, `0` for none. |
| `light:getRange()` | none | number | How far the light reaches, from its brightness and color. |
| `light:getDirection()` | none | x, y, z | Which way a spotlight points, normalized, not counting spin. Defaults to straight down. |
| `light:setDirection(x, y, z)` | any length but zero | none | Points the spotlight. Also resets how far it has spun. Does nothing for a light with a cone angle of 0. |
| `light:getConeAngle()` | none | degrees | Full width of the beam, 0 for a light that shines every way. |
| `light:setConeAngle(degrees)` | 0, or 1-179 (clamped) | none | Turns the light into a spotlight with a beam this many degrees wide, softening toward its edge. `0` (the default) makes it shine every way again. A spotlight's corona only shows from inside its beam, and its shadows only draw the directions the beam can reach, so they cost less. |
| `light:getSpin()` | none | degrees per second | How fast the direction turns. |
| `light:setSpin(degreesPerSecond)` | -3600 to 3600, clamped | none | Turns the spotlight's direction around the vertical axis, like a lighthouse. Negative spins the other way. Each client spins it on its own, so players can see it at slightly different angles. A spinning spotlight redraws its shadows every frame. |

---

## Emitters

Emitters eject particles: small camera-facing images that move, spin, and change color and size over
their short lives, like Blockland's. A **particle type** says what one particle looks like and how it
moves, an **emitter type** says how particles are ejected, and an **emitter** is one of those placed in
the world, at a spot or following a dynamic. Only the types and emitters are sent to clients, each
client ejects, moves, and draws the particles on its own, so two players never see exactly the same ones.

Types are made by name, and adding one with a name that's taken replaces it: clients already in the
game get the change right away and existing emitters of that type carry on with it. `EmitterDefaults.lua`,
run from `serverstart.lua`, adds the old game's types (converted to the units below) and a
`fountainEmitter` for testing. The server puts a `playerJetEmitter` under each foot of a jetting player
(see `client:setJetsEnabled`), and makes a `playerBubbleEmitter` wherever a dynamic splashes
into the water, if a type by that name exists. `emitterTest()` in `serverstart.lua` places one of every
default type.

Units are Blockland's: angles in degrees, speeds in studs per second, times in milliseconds. Ejection
directions are relative to the emitter: world space for one at a spot or on a brick, and turning with the
dynamic (or the mesh) for one that follows a dynamic. Gravity is always world space. Each client keeps at
most `graphics/maxparticles` particles alive (0 to 100000, default 20000), and emitters much further from
its camera than the end of the fog don't eject any. Particles fade into the fog and show in water
reflections. Particle types with `lit` set are lit and shadowed like smoke or bubbles would be, the rest
keep their colors day and night like fire and sparks.

### Particle type fields

The table passed to `addParticleType`. Anything left out keeps its default. Vectors can be tables like
`{1, 0.5, 0, 1}` or, like the old game's scripts, strings like `"1 0.5 0 1"`. An unknown field name or
a value of the wrong kind logs an error and the type isn't added. Numbers out of range are clamped.

| Field | Default | Description |
|---|---|---|
| `texture` | required | Image path relative to the game folder, like `"Assets/particles/cloud.png"`. Clients load it from their own copy, so it has to exist there too. |
| `color0` - `color3` | `{1, 1, 1, 1}` | RGBA, 0-1, multiplied by the texture. |
| `size0` - `size3` | `1` | Width in studs, 0-256. |
| `time0` - `time3` | `0`, `0.33`, `0.66`, `1` | When over a particle's life (0 to 1) each color and size key applies, blended in between. Kept in order. |
| `drag` | `0` | Fraction of its velocity a particle loses per second, one number or `{x, y, z}`, 0-1000. |
| `gravity` | `{0, 0, 0}` | Acceleration in studs per second squared. `{0, -20, 0}` falls, `{0, 15, 0}` rises like smoke. |
| `inheritedVelFactor` | `0` | How much of the velocity of the dynamic the emitter follows particles start with. |
| `lifetimeMS` | `1000` | How long each particle lives, 1-60000. |
| `lifetimeVarianceMS` | `0` | Each particle lives up to this much longer or shorter, kept under `lifetimeMS`. |
| `spinSpeed` | `0` | Degrees per second the image turns. |
| `useInvAlpha` | `false` | `true` blends by alpha, for smoke and anything that should cover what's behind it. `false` adds its color on top, for glows, sparks, and fire. |
| `needsSorting` | `false` | Draws this type's particles back to front each frame. Alpha blended particles that overlap need it to look right. |
| `lit` | `false` | Lights the particle like a surface of its color would be: by the sun or moon, ambient light, and point lights, darker in sun and point light shadows. For smoke, dust, and bubbles. Leave it off for anything that glows, like fire, sparks, and jets. The whole particle gets the light at its middle, and shadows don't pick up colors from transparent bricks. |

### Emitter type fields

The table passed to `addEmitterType`.

| Field | Default | Description |
|---|---|---|
| `particles` | required | Names of 1-16 particle types, separated by spaces (`"a b"`) or as a table (`{"a", "b"}`). Each particle is one of them picked at random. They have to be added first. |
| `ejectionPeriodMS` | `100` | Milliseconds between particles, at least 1. Each frame an emitter ejects every particle it owes, spread along the way it moved. |
| `periodVarianceMS` | `0` | Each gap is up to this much longer or shorter, kept under `ejectionPeriodMS`. |
| `ejectionVelocity` | `2` | Studs per second away from the emitter. |
| `velocityVariance` | `1` | Up to this much faster or slower, at most `ejectionVelocity`. |
| `ejectionOffset` | `0` | How far out from the emitter, along the way they go, particles start. |
| `thetaMin`, `thetaMax` | `0`, `90` | Degrees down from the emitter's up each particle goes out at, picked between these (0-180). `0, 0` shoots straight up, `90, 90` flat outward, `180, 180` straight down. |
| `phiReferenceVel` | `0` | Degrees per second the direction particles go out in turns around the vertical, for spirals. |
| `phiVariance` | `360` | Degrees around the vertical past that direction a particle can go, `360` for every way. |
| `lifetimeMS` | `0` | Emitters of this type remove themselves this long after they're made, for one-off bursts. `0` lasts until removed. Emitters on a brick ignore this and last as long as the brick, though their particles still use their own `lifetimeMS`. |
| `uiName` | `""` | Blockland's name for it: `loadBlocklandSave` puts an emitter of this type on bricks whose saved emitter has this name, ignoring case, the last added type winning if several share it, unless `addBlocklandEmitter` gave that name a type. |

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `addParticleType(name, table)` | name of 1-255 characters; fields above | none | Adds a particle type, or replaces the one with that name. Does nothing if its texture doesn't exist on the server. |
| `addEmitterType(name, table)` | name of 1-255 characters; fields above | none | Adds an emitter type, or replaces the one with that name. |
| `getParticleTable(name)` | particle type name | table | A particle type's fields, with vectors as tables. |
| `getEmitterTable(name)` | emitter type name | table | An emitter type's fields, with `particles` as a string of names. |
| `addEmitter(typeName[, x, y, z])` | emitter type name; position, default `0, 0, 0` | Emitter | Places an emitter. |
| `getEmitterId(netId)` | net ID | Emitter | Looks up an emitter by its net ID. |
| `getEmitterIdx(index)` | 0-based index | Emitter | Looks up an emitter by its position in the internal list. |
| `getNumEmitters()` | none | count | How many emitters currently exist. |

### `emitter:` methods

These use the strict argument count check.

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `emitter:destroy()` | none | none | Removes the emitter. Particles it already ejected live out their lifetimes. `emitter:remove()` does the same, it's the old game's name. |
| `emitter:getPosition()` | none | x, y, z | Where it is, or where the dynamic it follows is. |
| `emitter:setPosition(x, y, z)` | position | none | Moves it there, no longer following a dynamic or on a brick. |
| `emitter:getTypeName()` | none | string | Its emitter type's name. |
| `emitter:setType(typeName)` | emitter type name | none | Switches it to another emitter type. |
| `emitter:attachToDynamic(dynamic[, meshName][, offsetX, offsetY, offsetZ])` | dynamic; name of one of its model's meshes; studs along the dynamic's (or mesh's) own x, y, and z axes | none | Follows the dynamic, or the middle of that mesh as it animates, ejecting particles turned the way the dynamic (or mesh) is turned. An offset moves it that far from the middle, turning with it, like to the end of a gun's barrel. Removed along with the dynamic. |
| `emitter:attachToBrick(brick)` | brick, or `nil` | none | Moves it to the middle of the brick, and it's removed along with the brick instead of after its type's `lifetimeMS`. `nil` leaves it where it is, no longer on or following anything. |
| `emitter:setColor(r, g, b[, a])` | 0-1, clamped; `a` defaults to 1 | none | Multiplies its particles' colors and opacity by this, white by default. Only sent to clients if it changed, so it's cheap to call often. Particles already out keep the color they left with. |
| `emitter:getColor()` | none | r, g, b, a | Its color. |
| `emitter:aimWith(dynamic, range)` / `emitter:aimWith(nil)` | a dynamic, usually a player; how far its aim reaches in studs, 0-1000 | none | Sends particles toward whatever the dynamic looks at, up to `range` studs from its eyes (its client's crosshair for a player's own game, where a third person camera reaches that much further), and they only last until they get there. The type's `thetaMin` and `thetaMax` spread particles around that direction instead of around up. Other clients use the way the player's head turns. `nil` ejects normally again. The paint can in `Inventory.lua` uses this. |

---

## Bricks

Bricks on a grid of 1 stud (1 world unit) horizontally by 1 plate (0.4 world units)
vertically. Positions are a brick's **min corner** in whole studs/plates, not its center. Bricks
can never overlap. A table for a brick that has since been removed stays valid Lua, but its
methods log an error and do nothing.

Besides basic boxes of any size there are special bricks, like ramps, with their own shapes from
Blockland `.blb` files. Their types come from `Assets/brick/types`: `fxDTSBrickData` datablocks in
`bricks.txt` files there (Blockland add-on syntax, with `brickFile`, `uiName`, `iconName`,
`category`, and `subCategory`) and any `.blb` not named elsewhere, by its file name. A special brick fills its type's size on the grid like
a basic brick and turns around its middle. It collides using the collision boxes listed in its
`.blb`, or a convex hull of its shape if it lists none. Clients match the server's types by name
as they join, and draw bricks of types they don't have as plain boxes.

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `addBrick(x, y, z, width, height, length, r, g, b, a[, angleID])` | min corner in studs/plates; `width` and `length` in studs, `height` in plates, each 1-255; color; `angleID` is 0-3 quarter turns (default 0), where 1 and 3 swap width and length | Brick, or `nil` | Adds a brick. Returns `nil` without an error if it would overlap another brick or be out of bounds (`y` below 0). |
| `addSpecialBrick(x, y, z, typeName, r, g, b, a[, angleID])` | min corner in studs/plates; a special brick type's name (like `"45° Ramp 2x"`, case-insensitive, or its `.blb` file name); color; `angleID` 0-3 | Brick, or `nil` | Adds a special brick, which takes its type's size. Logs an error if there's no type by that name. Returns `nil` without an error if it would overlap another brick or be out of bounds. The shape's color faces (like a pine tree's green) keep their own color. |
| `getNumBricks()` | none | count | How many bricks exist. |
| `getBrickIdx(index)` | 0-based index | Brick | Looks up a brick by its position in the internal list. Removing bricks changes the order. |
| `getBrickId(id)` | net ID | Brick or `nil` | Looks up a brick by its net ID. |
| `getBrickAt(x, y, z)` | one stud/plate grid cell | Brick or `nil` | The brick filling that cell, if any. |
| `clearAllBricks()` | none | none | Removes every brick. |
| `saveBuild(fileName[, omitOwnership])` | file name inside the `Saves` folder; `omitOwnership` writes every owner as `-1` | bool | Saves every brick, with its name, material, collision, music, light, emitter, and print, in the Land of Dran binary format. Saves are written under a newer version number than the old game's, so the old game can't load them. |
| `loadLodSave(fileName[, x, y, z])` | file name inside `Saves`; optional offset in studs/plates | count, or `nil` | Loads a Land of Dran binary save (either of the old game's versions, or ours) on top of the current bricks, returning how many were added. Special bricks of types in `Assets/brick/types` are loaded, and so are names, collision, materials, prints, and our saves' music, lights, and emitters. The old game let undulo or bouncy go on top of another material; those bricks keep only the undulo or bouncy. Prints come by name, from the old game's saves too, however many faces its print mask covered; ones the server doesn't have are dropped and listed in the log. Other special types, and the old game's lights and music, are skipped. A brick's music or emitter of a type the server doesn't have is kept (and saved again) but doesn't play. |
| `loadBlocklandSave(fileName)` | file name inside `Saves` | count, or `nil` | Imports a Blockland `.bls` save using its own color palette, returning how many bricks were added. Brick names are matched against `Assets/brick/types`, special bricks included; unrecognized names are skipped and listed in the log. Pearl, chrome, glow, blink, swirl (as `Hologram`), rainbow, and undulo effects become materials, undulo winning on a brick that has a color effect too; water effects are dropped. Brick names, collision, prints, lights, emitters, and music come along, the last three as the brick's own like the wrench dialog's (saved by `saveBuild`). Prints are matched by the name in the save, like `Letters/X`; ones the server doesn't have are dropped and listed in the log. Lights become the light `addBlocklandLight` gave their Blockland type. Emitters use the emitter type `addBlocklandEmitter` gave their name, or else the one whose `uiName` matches, ignoring case, and always point up. Music uses a music sound type (see `newSoundType`) with the same name, ignoring case and with underscores as spaces. Anything without a match is skipped and listed in the log. `BlocklandImports.lua` and `EmitterDefaults.lua`, run from `serverstart.lua`, cover every light and emitter type Blockland's default add-ons have. |
| `addBlocklandLight(uiName, table)` / `addBlocklandLight(uiName, nil)` | a Blockland light type's name, like `"Red Light"`, 1-255 characters, case-insensitive; light fields as for `brick:setLight` | none | Sets the light `loadBlocklandSave` puts on bricks that had this Blockland light type, replacing any set before. Fields left out get a new light's defaults, so without an `offset` the light sits in the middle of its brick, like Blockland's. An unknown field or a value of the wrong kind logs an error and changes nothing. `nil` forgets the type, so its lights are skipped. Bricks already loaded keep their lights. |
| `addBlocklandEmitter(uiName, typeName)` / `addBlocklandEmitter(uiName, nil)` | a Blockland emitter's name, like `"Fog A"`, 1-255 characters, case-insensitive; an emitter type's name | none | Makes `loadBlocklandSave` put an emitter of that type on bricks that had this Blockland emitter, instead of looking for a type with that `uiName`. Logs an error if there's no emitter type by that name. `nil` goes back to matching by `uiName`. |

Save and load functions only accept a plain file name, with no folders, since saves always live
directly in `Saves/`. Bricks that would overlap an existing brick are skipped when loading.

### Brick materials

Every brick has one material, painted on like its color. Players pick theirs next to the color in the brick
selector. Shape effects are only drawn: a brick always collides as its plain shape.

| Material | Effect |
|---|---|
| `None` | Plain brick. |
| `Undulo` | Its corners wiggle and dance around, up to 0.3 studs along each axis. |
| `Bouncy` | Stretches up to 35% taller and back down, once every 1.25 seconds. Anything landing on it bounces back up as fast as it came down (100% restitution), like a trampoline. |
| `Pearl` | Mostly metallic and fairly smooth, a soft blurred sheen. |
| `Chrome` | Almost fully metallic and very smooth, a mirror of a `.hdr` sky when clients have image based lighting on. |
| `Blink` | Pulses once a second, like the part under the mouse in the appearance editor. |
| `Hologram` | See-through bars walk around its sides. |
| `Glow` | Never drawn darker than its own color, however dark it is. |
| `Slippery` | Perfectly smooth to look at, and has a friction of 0.01 with whatever touches it. |
| `Foil` | Metallic, with crinkled rainbow highlights that shift as you look at it from different directions. |
| `Rainbow` | Its color is replaced by one that cycles through the rainbow every 5 seconds, in bands that flow diagonally across a build. |

### Prints

A print is a picture drawn over the paint of a print brick's printed faces, the ones a `.blb` marks `TEX:PRINT`,
like `1x1 Print` or `2x2F Print`. Prints load from Blockland style folders under `Assets/brick/prints`
(`Print_<group>[_Default]/prints/<image>.png`) and are named `<group>/<image>`, so
`Assets/brick/prints/Print_Letters_Default/prints/X.png` is `Letters/X`, the same name Blockland saves use.
Each brick wears one print, on every printed face it has. Players pick one in the wrench dialog, Lua with
`brick:setPrint`, and a print's see-through parts show the brick's own color.

Clients load their own copy of the folder and match the server's prints by name as they join, so a print a
client doesn't have leaves that brick plain for them. Prints come along in `saveBuild` files and are read
back from the old game's saves and from Blockland `.bls` saves by name.

A print can also be a **`.webm` video** in the same folders, named the same way (`Print_Screens/prints/news.webm`
is `Screens/news`), which plays on the brick and loops. Everything else treats it as an ordinary print: the
server only ever knows its name, so a dedicated server needs no video support at all. Each client plays it
from its own clock, so players don't see exactly the same frame, and any audio in the file is ignored (a brick
plays sound with `brick:setMusic`). Only videos a brick in the world is actually wearing are decoded, at most
`graphics/maxvideoprints` (4 by default) of them at once, and the rest hold a still frame. VP8 and VP9 both
play; a client built without libvpx, or one missing the file, draws those bricks plain.

Making one, at the size prints are drawn (256x256) and a sensible bitrate:

```
ffmpeg -i clip.mp4 -an -vf "scale=256:256" -c:v libvpx-vp9 -b:v 600k -r 20 Print_Screens/prints/news.webm
```

### `brick:` methods

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `brick:getPosition()` | none | x, y, z | Min corner, in studs/plates. |
| `brick:getDimensions()` | none | width, height, length | Size before rotation. |
| `brick:getAngleID()` | none | 0-3 | Quarter turns around the vertical axis. |
| `brick:getColor()` | none | r, g, b, a | Color, 0-1. |
| `brick:setColor(r, g, b, a)` | color | none | Recolors the brick. |
| `brick:getMaterial()` | none | material name | The brick's material, like `"Chrome"`, or `"None"`. |
| `brick:setMaterial(name)` | a material name, case-insensitive | none | Paints a material onto the brick, see Brick materials above. `"None"` takes it off. Logs an error listing the materials for an unknown name. |
| `brick:isColliding()` | none | bool | Whether players and objects collide with it. |
| `brick:setColliding(collides)` | bool | none | Turns collision on or off. Non-colliding bricks can still be hit by `raycast()`. |
| `brick:getOwner()` | none | client net ID, or `-1` | Who planted it. `-1` for bricks added by Lua or loaded from a save. |
| `brick:getName()` | none | string | The brick's name, empty by default. |
| `brick:setName(name)` | string | none | Sets the brick's name. |
| `brick:remove([showEffect])` | optional bool | none | Removes the brick. With `true`, clients show it popping loose and flying off like an undone brick. Leave it off when removing many bricks at once. |
| `brick:isSpecial()` | none | bool | Whether it's a special brick with its own shape, rather than a basic box. |
| `brick:getTypeName()` | none | string | A special brick's type name, like `"45° Ramp 2x"`. Empty for basic bricks. |
| `brick:getMusic()` | none | sound name, volume, pitch; or `nil` | The music loop playing from the brick. |
| `brick:setMusic(soundName[, volume, pitch])` / `brick:setMusic(nil)` | a sound type's name (any sound, not just ones marked as music); `volume` 0-1 and `pitch` 0.05-10, clamped | none | Plays the sound on a loop from the middle of the brick for everyone, like `startSoundLoop`, until it's changed or the brick is removed. Leaving out volume and pitch keeps the brick's current ones (1 and 1 at first). Changing anything starts the loop over. `nil` or `""` stops it. |
| `brick:getLight()` | none | table, or `nil` | The brick's light settings, with every field below. |
| `brick:setLight(table)` / `brick:setLight(nil)` | light fields, see below | none | Puts a light on the brick, or changes it. Fields left out keep the brick's current values, or a new light's defaults. An unknown field or a value of the wrong kind logs an error and changes nothing. `nil` takes the light off. |
| `brick:getEmitter()` | none | emitter type name, or `nil` | The emitter on the brick. |
| `brick:setEmitter(typeName)` / `brick:setEmitter(nil)` | an emitter type's name | none | Puts an emitter of that type in the middle of the brick, replacing any it had. `nil` takes it off. |
| `brick:getPrint()` | none | print name, or `""` | The print on the brick, like `"Letters/X"`. |
| `brick:setPrint(name)` / `brick:setPrint("")` | a print's name, case-insensitive | none | Puts a print on the brick, drawn on the printed faces of a print brick type. A `.webm` print plays there, see Prints above. Logs an error and changes nothing for a name no print has. `""` takes it off. Prints on a brick whose type has no printed face are kept but never drawn. |

### Wrench dialog and brick attachments

Players wrench a brick to open its wrench dialog, where they can change whether it collides, its name,
its music loop with volume and pitch, its light, its emitter, and, on a print brick, its print. Wheel and steering wheel bricks also get a section for
how they drive once sliced into a vehicle, see [Vehicles](#vehicles). Wrenching is left clicking a brick with the
wrench item in hand (see [Items](#items)), or holding Insert (the `Wrench` key bind) and left clicking one. Lua can
veto or redirect the Insert way with the `ClientWrenchBrick` event, or open a dialog itself with `client:openWrenchDialog`.
Anyone can currently wrench any brick; there are no build permissions yet. The music list only shows
sounds registered with `newSoundType(name, file, true)` and the emitter list every emitter type, but a
brick keeps any sound or emitter Lua put on it when a player applies the dialog without changing it.
The Print section only shows on a brick whose type has a printed face, and lists every print the server
loaded from `Assets/brick/prints`, `.webm` videos included; players whose own copy is missing one see the
brick plain.

The music loop, light, and emitter are real sound loops, lights, and emitters: they show up in
`getNumLights`/`getLightIdx` and `getNumEmitters`/`getEmitterIdx`, are sent to players who join later,
and are removed along with the brick. If Lua destroys one, the brick makes it again the next time its
settings are changed. They're saved with the brick by `saveBuild`.

Light fields for `brick:setLight` and `brick:getLight` (see [Lights](#lights) for what each does):

| Field | Default | Description |
|---|---|---|
| `color` | `{1, 1, 1}` | RGB, 0-1. |
| `brightness` | `50` | 0-100000. |
| `flicker` | `0` | World units, 0-16. |
| `blinkSpeed` | `0` | Seconds for one full blink cycle, 0-60, `0` for no blinking. |
| `blinkStrength` | `1` | 0-1, how much it dims at the low point of each blink. |
| `coronaWidth` | `0` | World units, 0-256. |
| `coneAngle` | `0` | 0 shines every way, 1-179 makes a spotlight this many degrees wide. |
| `direction` | `{0, -1, 0}` | Which way a spotlight points, any length but zero. |
| `spin` | `0` | Degrees per second, -3600 to 3600. |
| `offset` | `{0, 0, 0}` | Where the light is from the middle of the brick, in world units, -32 to 32 on each axis. Point light shadows leave out any brick a light is inside, so a light in the middle of its brick shines out through it, though bricks right next to it still cast shadows. |

---

## Vehicles

Players build a vehicle out of bricks where it stands, then slice it out of the world into one body that drives on
wheels, like the old game's brick cars. Pressing G (the `Select Bricks for Vehicle` key bind) and clicking starts a
selection box on whatever the crosshair is on; hovering one of its faces and dragging with left mouse held stretches
it, up to 64 studs wide and 160 plates tall, and Enter slices every brick with any part inside it. Escape, or G again,
puts the box away. Lua does the same with `sliceBricks`.

A vehicle needs exactly one steering wheel brick and at least one wheel brick (at most 24), up to 10000 other bricks,
and can reach at most 40 studs along any axis. Special brick types are vehicle parts through our own `vehiclePart`
datablock field in `bricks.txt` (`"wheel"`, `"steering"`, or `"seat"`); `Assets/brick/types/vehicle` has the old game's
wheels and steering wheel, plus our own `2x4 Seat` (a plate that looks like any 2x4 plate), listed under Special,
Vehicle in the brick selector. A vehicle can have up to 32 seats, which are part of its body. The steering wheel decides which way the vehicle
drives (the way its rim faces from its column, `+x` unturned, with the driver standing on its column's side), and every
wheel has to roll that way: a wheel rolls along its longer side. Wheel bricks become wheels drawn with
`Assets/tire/tire.txt`, half as tall as the brick in radius, and everything else, the steering wheel included, becomes
the body. Bricks that don't collide are drawn but aren't part of the body. Lights and emitters on the bricks carry over and
move with the vehicle. Music on the steering wheel brick becomes the vehicle's music; music on other bricks doesn't carry
over. Like the old game, the body weighs one per colliding brick, so a very small vehicle is light enough to wheelie and
flip easily.

Players right click a vehicle within 30 studs of their camera to get in, standing behind its steering wheel. W and S
run the engine, A and D steer, jump brakes, left click plays the `Honk` sound if one is registered, and right click
gets out just above the seat. Right clicking a vehicle someone is already driving stands the player on its free seat
nearest the crosshair, or center prints that every seat is taken. A passenger is locked in place on the seat's top but
turns to face wherever they look, uses items and clicks like normal, and right clicks to get off where they stand;
their movement keys do nothing and jets are off. Passengers stay on when the driver gets out. A vehicle nobody drives holds its brakes. The engine stops pushing past 200 studs a second.
Wheels on the ground going faster than 50 km/h (about 14 studs a second) while turning or braking throw up the
`setVehicleDirtEmitter` emitter type (`vehicleDirtEmitter` from `EmitterDefaults.lua` by default), tinted a darker shade
of the brick under them, or brown when there's no brick under them. Wheels in the water float the vehicle and splash like the old game. A vehicle going faster than
1000 studs a second, spinning faster than 300 radians a second, more than 10000 studs from the middle of the world, or
with a position that isn't a number is removed, with an error logged.

Wrenching a wheel brick before slicing adds a Wheel section to its wrench dialog, and a steering wheel brick a Vehicle
section, which are saved with the brick by `saveBuild`. Wrenching a vehicle (with the wrench item, or Insert and left
click) opens a dialog with its music loop and a Save section, which saves the vehicle to `Saves/Vehicles/<name>.lod` on
the player's own computer: the server sends its bricks as they were before slicing, wheels and their settings included,
with its music on its steering wheel. Its red Remove vehicle button, once confirmed, fires `ClientRemoveVehicle` and
removes it. The `Saved Vehicles` window, opened from the escape menu while in a server, lists those files; picking one
to load as a vehicle ready to drive, or as plain bricks the player owns and can undo, change, and slice again, shows a
ghost of it at the crosshair (up to 100 studs away, 15 studs out in the air otherwise), and left clicking places it there
(Escape cancels). The file is uploaded to the server, which checks the spot is within 140 studs of the player's camera,
fires `ClientLoadVehicle`, and lets a player load one every 5 seconds. Anyone can currently slice, drive, wrench, or load
anything; use the events to limit that.

| Wheel setting | Default | Range | Description |
|---|---|---|---|
| Engine force | `200` | -2000 to 2000 | How hard the wheel drives the vehicle forward, negative backward. |
| Brake force | `400` | 0 to 2000 | How hard it stops while the driver holds jump, and while nobody drives. |
| Steering | `0.5` radians | -pi to pi | How far it turns while steering, 0 doesn't steer, negative turns the other way. |
| Suspension length | `0.7` | 0.1 to 5 | World units the wheel hangs down when resting. |
| Stiffness | `100` | 1 to 1000 | How hard the suspension pushes back. |
| Compression / relaxation damping | `6` / `10` | 1 to 100 | How much the suspension resists moving in and out. |
| Grip | `1.2` | 0.1 to 10 | Friction slip, higher slides less. |
| Roll influence | `0.6` | 0.1 to 10 | How much cornering tips the vehicle, lower is steadier. |

| Steering wheel setting | Default | Range | Description |
|---|---|---|---|
| Mass | `1.5` | 1.5 to 30 | How heavy each brick is to turn or tip over. |
| Spin damping | `0.03` | 0 to 1 | How quickly spinning slows down. |
| Realistic center of mass | off | | Off, the vehicle turns around a point down near its wheels, which keeps it from flipping. On, around the middle of its bricks. |

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `sliceBricks(x1, y1, z1, x2, y2, z2)` | two opposite grid corners in studs/plates, both inclusive | Vehicle, or `nil` and why | Slices every brick with any part in that box into a vehicle, without firing `ClientSliceBricks`. The box can be at most 64 by 160 by 64. |
| `getNumVehicles()` | none | count | How many vehicles exist. |
| `getVehicleIdx(index)` | 0-based index | Vehicle | Looks up a vehicle by its position in the internal list. |
| `getVehicleId(id)` | net ID | Vehicle or `nil` | Looks up a vehicle by its net ID. |
| `clearAllVehicles()` | none | none | Removes every vehicle, letting their drivers out. |
| `radiusImpulse(x, y, z, strength)` | world position; impulse, positive pushes away and negative pulls in | objects pushed, bricks broken | Pushes every dynamic in the world (players, items on the ground, and the rest) and every vehicle whose bounding box is within reach, which is `2.5 * sqrt(abs(strength))` studs (at most 200), along the line from the position to its center, fading to nothing at the edge of its reach. It's an impulse: something weighing 1, like a player or an item, gets `strength` studs a second right at the middle, and a vehicle weighs one per brick. Carried items and players in vehicles aren't pushed themselves. Destructable vehicles (see `vehicle:setDestructable`) also lose every brick, except the steering wheel, where `abs(strength) * vehicleBrickBreakScale / (1 + distance^2)` is at least its volume in cubic studs (a plate is 0.4 tall), with the distance to the nearest part of the brick; they fly off for everyone like hammered bricks, pushed the same way, taking their lights and emitters with them, and a broken seat lets its rider off and can't be used again. The tuning constants are in `Physics/RadiusImpulse.h`. |
| `setVehicleDirtEmitter(typeName)` / `setVehicleDirtEmitter(nil)` | an emitter type's name | none | The emitter type wheels of vehicles sliced from now on throw dirt with. `nil` for none. |
| `loadVehicleFile(fileName, x, y, z[, asBricks])` | a name in the server's `Saves/Vehicles` without `.lod`; a grid spot in studs/plates; `asBricks` | Vehicle (or `true` as bricks) and a message, or `nil` and why | Places a vehicle save, from `vehicle:saveToFile` or one a player saved, with the middle of its bottom at the spot. As a vehicle it follows the same rules as slicing; as bricks, ones in the way of other bricks are left out. Doesn't fire `ClientLoadVehicle`. |

### `vehicle:` methods

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `vehicle:destroy()` / `vehicle:remove()` | none | none | Lets out its driver and passengers, removes its lights, emitters, and music, and removes it. Its bricks don't come back. |
| `vehicle:saveToFile(fileName)` | a name without `.lod` | bool | Saves it to `Saves/Vehicles/<fileName>.lod` on the server, for `loadVehicleFile`. Names can't have folders in them. |
| `vehicle:getNumBricks()` | none | count | Bricks in its body, the steering wheel included. |
| `vehicle:getNumWheels()` | none | count | |
| `vehicle:getPosition()` / `vehicle:setPosition(x, y, z)` | world position | x, y, z / none | Its body's origin, which is near its wheels' tops unless its steering wheel asks for a realistic center of mass. |
| `vehicle:getRotation()` / `vehicle:setRotation(w, x, y, z)` | quaternion | w, x, y, z / none | |
| `vehicle:getVelocity()` / `vehicle:setVelocity(x, y, z)` | studs per second | x, y, z / none | |
| `vehicle:getAngularVelocity()` / `vehicle:setAngularVelocity(x, y, z)` | radians per second | x, y, z / none | |
| `vehicle:setGravity(x, y, z)` | acceleration | none | |
| `vehicle:getDriver()` | none | Client or `nil` | Who's driving it. |
| `vehicle:ejectDriver()` | none | none | Lets its driver out, without `ClientExitVehicle`. |
| `vehicle:setDestructable(bool)` / `vehicle:isDestructable()` | bool | none / bool | Whether `radiusImpulse` breaks its bricks off. Off for a new vehicle until a script turns it on, like `serverstart.lua` does from `VehicleCreated`. |
| `vehicle:getNumSeats()` | none | count | How many passenger seats (seat bricks) it has, not counting the driver's, including seats that were broken off. |
| `vehicle:getPassenger(seat)` | 0 to `getNumSeats() - 1` | Client or `nil` | Who's riding on that seat. Use `client:exitVehicle` to get them off. |
| `vehicle:getBuilder()` | none | Client or `nil` | Who sliced it, `nil` if Lua did or they left. |
| `vehicle:getBuilderID()` | none | client net ID, or `-1` | |
| `vehicle:getMusic()` | none | sound name, volume, pitch; or `nil` | The loop playing from it. |
| `vehicle:setMusic(soundName[, volume, pitch])` / `vehicle:setMusic(nil)` | a sound type's name; `volume` 0-1, `pitch` 0.05-10 | none | Plays the sound on a loop from the vehicle for everyone, following it, until it's changed or the vehicle is removed. Changing anything starts the loop over. |

Vehicles also come back from `raycast()` and `client:getCursorItem`, with `type` 7.

---

## Sounds

Sounds are registered by name with `newSoundType`, then played by that name. Clients load the
file from their own copy of the game folder when they join (or right away if they're already
connected), so the file has to exist on the clients too. `.wav` (any bit depth), `.ogg`
(Vorbis), and `.mp3` files work, mono or stereo.

Sounds with no position play at the same volume wherever the listener is. Sounds with a position
pan left and right, are at full volume within 5 studs, and past that lose about 10 dB every time
the distance doubles, getting duller as well, so they're close to silent a couple hundred studs
away. Sounds moving toward or away from the listener, or a listener moving toward or away from
them, shift in pitch (the Doppler effect, with sound traveling 343 studs a second). Closing in or
pulling apart slower than 12 studs a second doesn't shift pitch at all, and the shift eases in
up to its full amount at 30 studs a second, so walking around doesn't bend music. The listener
is the client's camera, but its movement for the Doppler effect is that of whatever the camera
follows, so swinging the camera around doesn't change pitch. Someone in a vehicle moves along
with it, so a car's own music doesn't bend for whoever is driving or riding on it.

Unless Lua picks a preset with `setAudioEffect`, each client's reverb follows the space around
their camera: out in the open there's almost none, and it gets louder and longer the more
closed in and bigger the space is (bricks, statics, and dynamics all count as walls). Under
the water level everything is muffled and sounds like the `underwater` preset. Positioned
sounds with bricks or objects between them and the camera are muffled too, more the thicker
the bricks in the way, and their echo is muffled along with them. Players can turn
these off or change how many raycasts they use in the audio settings.

Clients play a few sounds by name on their own when the server has registered them: `ClickMove`
and `ClickRotate` when the ghost brick moves or turns, `Jump` when their player jumps, and
`BrickBreak` where a removed brick pops loose. `serverstart.lua` registers these along with
`ClickPlant`, `PlayerConnect`, `PlayerLeave`, `Admin` (played to a client who logs into the eval
console), and `BrickClear` (played to everyone when someone types `/clearbricks` in chat to remove
all of their own bricks). It also registers `Splash` and `ExitWater`, which the server plays by
name where dynamics hit or leave the water, louder the faster they're moving and lower pitched
the bigger they are, and `LightOn` and `LightOff`, which the server plays from a player whose
flashlight turns on or off. `Inventory.lua` plays `HammerHit`, `WrenchHit`, and `WrenchMiss` where tools hit, loops
`SprayLoop` from a spraying paint can, and plays `Launch` from a firing launcher. `Honk` is what drivers play with left click.

In the functions below, `pitch` is a playback speed multiplier (default `1`, clamped to 0.05-10)
and `volume` is 0-1 (default `1`). They can only be given together.

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `newSoundType(name, filePath[, isMusic])` | unique name and a path relative to the game folder, each 1-255 characters; `isMusic` marks it as music | none | Registers a sound. Logs an error and skips it if the name is taken or the file doesn't exist on the server. Sounds marked as music are the ones players can pick for a brick in the wrench dialog. `serverstart.lua` registers `After School Special` from `Assets/music` as music. |
| `playSound(name[, x, y, z][, pitch, volume])` | sound type name; optional world position | none | Plays a sound once for every client, with no position or at `x, y, z`. Sent unreliably, so a client can occasionally miss one. |
| `startSoundLoop(name[, x, y, z][, pitch, volume])` | sound type name; optional world position | loop ID | Starts a sound that repeats until `stopSoundLoop`, with no position or at `x, y, z`. Clients who join later hear it too. Each client only plays the 16 loops closest to them at once; farther ones pause and pick up where they left off. Loops use the music volume setting on top of `volume`. |
| `stopSoundLoop(loopID)` | ID from `startSoundLoop` or `dynamic:startSoundLoop` | none | Stops a loop. Does nothing if it already ended. |
| `setAudioEffect(preset)` | preset name, case insensitive | none | Puts a reverb effect on every sound for every client, including ones who join later. `auto`, the default, has each client's reverb follow the space around them (see above). `none` turns reverb off. Muffling underwater and behind walls happens either way. Presets: `generic`, `paddedcell`, `auditorium`, `concerthall`, `cave`, `forest`, `plain`, `underwater`, `drugged`, `dizzy`, `psychotic`, `outhouse`, `heaven`, `hell`, `memory`, `dustyroom`, `waterroom`, `racer`, `tunnel`. |

See also `dynamic:playSound`, `dynamic:startSoundLoop`, `client:playSound`, and `client:setAudioEffect`.

---

## Voice chat

Players hold push to talk (V by default) to talk. Their voice comes from their player (the dynamic
their movement keys control, otherwise the first dynamic they control), or from their camera if they
have neither, and is only sent to clients whose camera is within the voice range. For the people
listening it works like a positioned sound: full volume within 10 studs, then about 10 dB quieter each
time the distance doubles, muffled behind bricks and underwater, with the same reverb as every other
sound. Each client plays up to 8 people talking at once. Players set their microphone, microphone
volume, and voice chat volume in the audio settings.

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `setVoiceRange(studs)` | distance, at least 0; default 128 | none | How far a talker can be from a client's camera and still be heard. `0` turns voice chat off for everyone. |
| `getVoiceRange()` | none | studs | The current voice range. |

See also `client:setVoiceMuted`, `client:isVoiceMuted`, `client:isTalking`, and the `ClientStartTalking` and `ClientStopTalking` events.

---

## Clients

A "client" represents one connected player/connection.

### Global functions

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `getNumClients()` | none | count | How many clients are currently connected. |
| `getClientIdx(index)` | 0-based index | Client | Looks up a connected client by index. |
| `messageAll(text)` | text (max 255 chars) | none | Broadcasts a chat message from the server to every connected client, as a single packet. Empty strings are silently ignored, same as `client:message`. |
| `centerPrintAll(text)` / `centerPrintAll(text, durationMS)` / `centerPrintAll(text, durationMS, red, green, blue)` | text (max 255 chars); duration in ms (default 3000, clamped to 60000); color 0-1 (default white) | none | Broadcasts a temporary message to the center of every connected client's screen, as a single packet. |

### `client:` methods

| Method | Arguments | Returns | Description |
|---|---|---|---|
| `client:message(text)` | string (max 255 chars) | none | Sends a chat message from the server to just this client. |
| `client:kick()` | none | none | Disconnects the client. |
| `client:getName()` | none | string | The client's display name. |
| `client:getIP()` | none | string | The client's IP address. |
| `client:getID()` | none | net ID | The client's unique net ID. |
| `client:getPing()` | none | ms | Round-trip ping. |
| `client:getPacketLoss()` | none | value | Current packet loss. |
| `client:isAdmin()` | none | bool | Whether the client logged into the eval console as admin. |
| `client:giveControl(dynamic)` | Dynamic | none | Gives the client physics-simulation authority over the dynamic (e.g. their player). |
| `client:removeControl(dynamic)` | Dynamic | none | Takes physics-simulation authority for the dynamic back from the client. |
| `client:getNumControlled()` | none | count | How many dynamics this client currently controls. |
| `client:getControlledIdx(index)` | 0-based index | Dynamic | The controlled dynamic at that index (index 0 is typically their player). |
| `client:setDefaultController(dynamic)` | Dynamic | none | Sets up movement-key/camera-direction input handling for this dynamic (walking, jumping). Currently the only way to stop this is to destroy the dynamic. Also required before `getCursorItem`/`snapToCursor` will have live camera data for this client. |
| `client:bindCamera(dynamic, fixUpVector, maxFollowDistance)` | Dynamic to follow; whether to lock the camera's up vector; max third-person follow distance | none | Binds the client's camera to follow a dynamic. |
| `client:staticCamera(posX, posY, posZ)` | fixed camera position | none | Detaches the camera and locks it to a fixed position (direction stays free/mouse-controlled). |
| `client:staticCamera(posX, posY, posZ, dirX, dirY, dirZ)` | fixed camera position and direction | none | Same, but also locks the look direction. |
| `client:getCursorItem(maxDistance)` | max ray distance | hit object, x, y, z, normalX, normalY, normalZ, distance; or `nil` | Same return values as `raycast()`. Raycasts from the client's *live* camera position/direction (updated continuously, not just on click) out to `maxDistance`, ignoring the client's own first controlled object. Requires `setDefaultController` to have been called for this client. |
| `client:centerPrint(text)` / `client:centerPrint(text, durationMS)` / `client:centerPrint(text, durationMS, red, green, blue)` | text (max 255 chars); duration in ms (default 3000, clamped to 60000); color 0-1 (default white) | none | Shows a temporary message in the center of just this client's screen. |
| `client:playSound(name[, x, y, z][, pitch, volume])` | same as `playSound` | none | Plays a sound once for just this client. |
| `client:setAudioEffect(preset)` | same as `setAudioEffect` | none | Sets the reverb effect for just this client, until something sets it again. Not remembered: `setAudioEffect`'s preset is what a client gets when they join. |
| `client:setVoiceMuted(muted)` | bool | none | Mutes or unmutes the client's voice chat. The server drops their voice while they're muted, and their game shows "Voice Muted" and stops sending. If they were talking, `ClientStopTalking` fires half a second later. Not remembered if they reconnect. |
| `client:isVoiceMuted()` | none | bool | Whether `setVoiceMuted` muted the client. |
| `client:isTalking()` | none | bool | Whether the client is talking right now, between `ClientStartTalking` and `ClientStopTalking`. |
| `client:setJetsEnabled(enabled)` | bool | none | Whether the client can jet, on by default. Holding right mouse cancels gravity and lifts the player from `setDefaultController` up to 30 studs a second, moving at twice walking speed while they walk, not while swimming. Each foot (`Left_Foot` and `Right_Foot` meshes, or the middle of a model without them) gets a `playerJetEmitter` while they jet, if Lua added that emitter type. Turning it off mid-jet drops them and removes the flames. Not remembered if they reconnect. |
| `client:getJetsEnabled()` | none | bool | Whether `setJetsEnabled` lets the client jet. |
| `client:setFlashlightEnabled(enabled)` | bool | none | Whether the client can use their flashlight, on by default. Players tap their flashlight key (`]` by default) to switch it on or off, and hold it to cycle through colors starting from white. The `LightOn` and `LightOff` sounds play from their player. Turning it off switches off a flashlight that's on. Needs a player from `setDefaultController` to hold it (see Lights). Not remembered if they reconnect. |
| `client:getFlashlightEnabled()` | none | bool | Whether `setFlashlightEnabled` lets the client use a flashlight. |
| `client:addItem(item[, slot])` | an item on the ground; slot 0-4, or the first empty one | slot or `nil` | Puts the item in the client's inventory. Returns `nil` if that slot is taken or none are free, and logs an error too for an item someone already carries. See [Items](#items). |
| `client:removeItem(slot)` | 0-4 | Item or `nil` | Takes the item out of the slot and puts it back into the world just in front of the client's player, not moving, or where it was without a player. `nil` for an empty slot. |
| `client:getItem(slot)` | 0-4 | Item or `nil` | The item in that slot. |
| `client:getSelectedSlot()` | none | slot, open | The slot the client's item bar has picked (0-4, kept while it's put away), and whether their items are out. |
| `client:getHeldItem()` | none | Item or `nil` | The item in the client's hand: the one in the picked slot while their items are out. |
| `client:getCameraPosition()` | none | x, y, z | Where the client's camera was as of their last movement update, which comes about every 100 ms. Needs `setDefaultController`. |
| `client:getCameraDirection()` | none | x, y, z | Which way their camera looked then, normalized. Needs `setDefaultController`. While they hold left mouse, their camera is sent about every 30 ms instead. |
| `client:getPaintColor()` | none | r, g, b, a | The color their paint palette (E, or Right Shift's custom color) has picked, 0-1. Their game sends it as they connect and whenever it changes. White until then. |
| `client:getPaintMaterial()` | none | material name | The brick material their paint palette has picked, like `"Chrome"`, see [Brick materials](#brick-materials). |
| `client:openWrenchDialog(brick)` / `client:openWrenchDialog(vehicle)` | Brick or Vehicle | none | Opens the wrench dialog for the brick (or vehicle, with just its music) on the client's screen, as if they'd wrenched it, without firing `ClientWrenchBrick` or `ClientWrenchVehicle`. What they apply only reaches the brick or vehicle in the last dialog of that kind they were sent, once. See [Wrench dialog and brick attachments](#wrench-dialog-and-brick-attachments) and [Vehicles](#vehicles). |
| `client:getVehicle()` | none | Vehicle and seat, or `nil` | The vehicle the client is driving (seat `nil`) or riding on (the passenger seat, 0 or more). |
| `client:enterVehicle(vehicle[, seat])` | Vehicle; a passenger seat, 0 to `getNumSeats() - 1`, or none to drive | bool | Puts the player from `setDefaultController` in the vehicle's driver's seat, or on that passenger seat, without firing `ClientEnterVehicle`. `false` if someone's already in that seat, the seat doesn't exist, the client is already in a vehicle, or their player isn't standing in the world (like one Lua took out of it). |
| `client:exitVehicle()` | none | none | Lets the client out of whatever they're driving (just above its seat) or riding (just above where they stood), without firing `ClientExitVehicle`. |
| `client:applyAppearance(dynamic)` | Dynamic | none | Puts the colors, face, and shirt the client picked in their appearance editor on the dynamic, usually their player in `ClientJoin`. Their game sends their appearance as they connect: each painted part is matched to a mesh by name ignoring case (parts the model doesn't have are skipped), the face goes on the `Face1` mesh, or `Face` or `Head` if there's no `Face1`, and the shirt goes on the `Torso` mesh, like `dynamic:setMeshDecal`. If they save a change while connected, it's put on the last dynamic this was called with, and parts they no longer paint go back to the model's own look. |

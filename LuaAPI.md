# Land of Dran Server Lua API

This documents every function the server-side Lua environment exposes to scripts
(`serverstart.lua` and anything `dofile`'d from it, or run through the eval console).
Generated from the current `LuaFunctions/*.cpp` and `Networking/PacketsFromClient/*.cpp`
source - if you add or change a binding, update this file too.

## Conventions

- Every `Dynamic`, `StaticObject`, and client table has an `id` field (its net ID) and a
  `type` field you can compare against: `1` = Dynamic, `2` = Static, `3` = Client
  (`NetTypes/NetType.h`'s `SimObjectType`). `raycast()` and `client:getCursorItem()` can
  return either a Dynamic or a Static, so check `.type` before assuming which.
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
| `info(...)` | any number of values | Logs a line to the server's info log. Values are stringified (tables holding a Dynamic/Static/Client print as `[Dynamic N]`/`[Static N]`/`[Client N]`). |
| `error(...)` | any number of values | Same as `info`, but logged as an error and prefixed accordingly. |
| `debug(...)` | any number of values | Same as `info`, but only logged when the `logger/verbose` setting is on. |
| `shutdown()` | none | Stops the main program loop (shuts the whole process down, not just the server). |

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
| `ClientClick` | `function(client, posX, posY, posZ, dirX, dirY, dirZ, mask) ... return client, posX, posY, posZ, dirX, dirY, dirZ, mask end` | Fires on every mouse click. `posX/Y/Z` and `dirX/Y/Z` are the camera's position and look direction *at the moment of the click*; `mask` is the SDL mouse button mask (see Conventions). |

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
| `addAnimation(typeID, animationName, startFrame, endFrame, speed, fadeInMS, fadeOutMS)` | type to attach the animation to; frame range; playback speed; fade in/out durations in ms | none | Adds a named animation clip to a dynamic type. The first animation added to a type is used as its walk cycle. |
| `raycast(startX, startY, startZ, endX, endY, endZ[, dynamicToIgnore])` | ray start/end points; optionally a Dynamic to exclude from the hit test | Dynamic, Static, or `nil` | Casts a ray through the physics world and returns whatever it hit first (or `nil`). |

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
| `dynamic:getNumControllers()` | none | count | How many clients currently control this dynamic (usually 0 or 1; 0 means it's a normal server-simulated object, not a player). |
| `dynamic:getControllerIdx(index)` | 0-based index | Client | The client controlling this dynamic at that index. |
| `dynamic:snapToCursor(client, xOffset, yOffset, zOffset)` | client to attach to; view-space offset: `x` = right, `y` = up, `z` = distance in front of the camera | none | Attaches the dynamic to a client's cursor: every physics tick its position is recomputed from that client's live camera position/direction plus this offset, and its gravity is disabled. Calling this again while already snapped just updates the client/offset. |
| `dynamic:unsnap()` | none | none | Detaches from the cursor (if snapped) and restores the gravity it had before snapping. No-op if not snapped. |
| `dynamic:isSnapped()` | none | bool | Whether the dynamic is currently snapped to any client's cursor. |
| `dynamic:getSnapClient()` | none | Client or `nil` | The client it's snapped to, or `nil` if not snapped. |

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
| `static:setColliding(bool)` | true/false | none | Enables or disables collision for the object without removing it. |
| `static:setHidden(bool)` | true/false | none | Shows or hides the object client-side. |

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
| `client:getCursorItem(maxDistance)` | max ray distance | Dynamic, Static, or `nil` | Raycasts from the client's *live* camera position/direction (updated continuously, not just on click) out to `maxDistance`, ignoring the client's own first controlled object. Requires `setDefaultController` to have been called for this client. |
| `client:centerPrint(text)` / `client:centerPrint(text, durationMS)` / `client:centerPrint(text, durationMS, red, green, blue)` | text (max 255 chars); duration in ms (default 3000, clamped to 60000); color 0-1 (default white) | none | Shows a temporary message in the center of just this client's screen. |

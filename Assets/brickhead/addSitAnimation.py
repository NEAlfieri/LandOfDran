"""
Adds the sitting pose to Brickhead.fbx, run from the project root with a headless Blender:

	blender -b --python Assets/brickhead/addSitAnimation.py [-- input.fbx output.fbx]

Brickhead has no armature: each body part is its own mesh object parented under Torso, and the one
animation take is rotation keys on those objects around their own rotation pivots, in ticks that
are the frame numbers minus one. Frames 1 to 31 are the walk, 36 the rest pose (everything at
zero), and 57 to 66 the grab, which is how serverstart.lua's addAnimation lines slice it up.

This appends, after a rest key at frame 70, the Blockland sit: legs straight out in front, arms
reaching forward and down, held at frames 71 and 72. The pose is looped as a "sit" animation and
faded in and out, which is how Blockland's own sit sequence (one frame, blended) sits a player
down and stands them up. Positive rotation around a limb's X axis is forward, the same sign the
walk's front leg and the grab's raised arm use. The model's origin is at its feet, which is where
a seat or the ground puts it, so the root is also dropped until the lowest point of the posed
legs is back at the origin, or the player would sit in the air with their feet where their rear
should be.

The keys are written straight into the file's animation curves, through the FBX binary parser and
writer that ship with Blender's FBX add-on (fbx2json / json2fbx), so nothing else about the file
changes: not the rotation pivots, which the game leans on to blend animations in and out and to
turn a player's head, not the mesh names, not the materials. Blender's own exporter can't be used
for this: it bakes each pivot into location keys that move with the rotation, and the game then
blends those locations linearly, which makes limbs bounce as animations fade and turns the head
about the wrong point.

FBX rotation curves are in degrees, translations in the file's centimetres, and a frame at the
file's 60 fps is 46186158000 / 60 KTime units.
"""
import sys, os, json, shutil, tempfile

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
inPath = argv[0] if len(argv) > 0 else "Assets/brickhead/Brickhead.fbx"
outPath = argv[1] if len(argv) > 1 else inPath

import bpy
sys.path.insert(0, os.path.join(bpy.utils.resource_path('LOCAL'), "scripts", "addons_core", "io_scene_fbx"))
import fbx2json, json2fbx

FRAME = 46186158000 // 60
REST_FRAME = 36
SIT_REST_KEY = 70
SIT_FRAMES = (71, 72)

# Model name -> Lcl Rotation X in degrees while sitting, everything else keeps its rest pose
sitRotationX = {
	"Left_Leg": 90.0,
	"Right_Leg": 90.0,
	"LeftShoulder": 55.0,
	"RightShoulder": 55.0,
}

# How far the root comes down, centimetres: with the legs turned forward about the hips the heels are
# the lowest point of the body, 61.1 above the origin (the thighs' underside is 61.4)
ROOT = "Torso"
ROOT_DROP = 61.1

def ktime(frame):
	return (frame - 1) * FRAME

def evaluate(times, values, time):
	"""The curve's value at a time, linear between keys like the game reads it"""
	if time <= times[0]:
		return values[0]
	for i in range(1, len(times)):
		if times[i] >= time:
			span = times[i] - times[i - 1]
			t = (time - times[i - 1]) / span if span > 0 else 1.0
			return values[i - 1] + (values[i] - values[i - 1]) * t
	return values[-1]

work = tempfile.mkdtemp()
workFbx = os.path.join(work, "brickhead.fbx")
shutil.copyfile(inPath, workFbx)
fbx2json.fbx2json(workFbx)
workJson = os.path.join(work, "brickhead.json")
root = json.load(open(workJson))

top = {e[0]: e for e in root}
objects = top["Objects"][3]
byId = {o[1][0]: o for o in objects}

# Each connection is ["C", [kind, child, parent, property?], ...]: a curve hangs off a curve node by
# its component ("d|X"), the curve node off its model by the property it drives ("Lcl Rotation")
parents = {}
for c in top["Connections"][3]:
	p = c[1]
	parents.setdefault(p[1], []).append((p[2], p[3] if len(p) > 3 else None))

def modelName(model):
	return model[1][1].split("::")[0]

keyed = 0
for curve in objects:
	if curve[0] != "AnimationCurve":
		continue
	elems = {e[0]: e for e in curve[3]}
	times = elems["KeyTime"][1][0]
	values = elems["KeyValueFloat"][1][0]
	flags = elems["KeyAttrFlags"][1][0]
	attrData = elems["KeyAttrDataFloat"][1][0]
	refCount = elems["KeyAttrRefCount"][1][0]
	if len(refCount) != len(times) or len(attrData) != 4 * len(times):
		raise RuntimeError("A curve doesn't keep one tangent record per key, which this script assumes")
	if times[-1] >= ktime(SIT_REST_KEY):
		raise RuntimeError("The file already has keys at frame %d or later, has the sit been added already?" % SIT_REST_KEY)

	curveNodeId, component = parents[curve[1][0]][0]
	prop, modelId = [(pp, pid) for pid, pp in parents[curveNodeId] if byId[pid][0] == "Model"][0]
	name = modelName(byId[modelId])

	rest = evaluate(times, values, ktime(REST_FRAME))
	pose = rest
	if prop == "Lcl Rotation" and component == "d|X" and name in sitRotationX:
		pose = sitRotationX[name]
		keyed += 1
	if prop == "Lcl Translation" and component == "d|Y" and name == ROOT:
		pose = rest - ROOT_DROP
		keyed += 1

	lastAttr = attrData[-4:]
	for frame, value in ((SIT_REST_KEY, rest), (SIT_FRAMES[0], pose), (SIT_FRAMES[1], pose)):
		times.append(ktime(frame))
		values.append(value)
		flags.append(flags[-1])
		attrData.extend(lastAttr)
		refCount.append(1)

if keyed != len(sitRotationX) + 1:
	raise RuntimeError("Expected to pose %d curves but found %d" % (len(sitRotationX) + 1, keyed))

# The take now runs to the last pose
end = ktime(SIT_FRAMES[-1])
for o in objects:
	if o[0] == "AnimationStack":
		for sub in o[3]:
			if sub[0] == "Properties70":
				for p in sub[3]:
					if p[1][0] in ("LocalStop", "ReferenceStop"):
						p[1][4] = end
for sub in top["GlobalSettings"][3]:
	if sub[0] == "Properties70":
		for p in sub[3]:
			if p[1][0] == "TimeSpanStop":
				p[1][4] = end
for take in top["Takes"][3]:
	if take[0] == "Take":
		for sub in take[3]:
			if sub[0] in ("LocalTime", "ReferenceTime"):
				sub[1][1] = end

json.dump(root, open(workJson, "w"))
json2fbx.json2fbx(workJson)
shutil.copyfile(workFbx, outPath)
shutil.rmtree(work)
print("Wrote", outPath, "with the sit at frames", SIT_FRAMES, "and the take ending at frame", SIT_FRAMES[-1])

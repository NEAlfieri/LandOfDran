#include <set>
#include <limits>
#include "Mesh.h"
#include "../Utility/ContentFiles.h"
#include "DtsShape.h"

#include <tuple>

/*
	The scuffed plastic a DTS material with nothing but a flat colour of its own is drawn with, the same as
	the Brickhead's, so a Blockland shape looks like the plastic it is rather than like a mirror. Small on
	purpose: every material that uses them gets its own copy of the array, and an add-on folder holds dozens
*/
static const std::string dtsDefaultNormal = "Assets/dts/scuffedNormal.png";
static const std::string dtsDefaultRoughness = "Assets/dts/scuffedRoughness.png";
static const std::string dtsDefaultOcclusion = "Assets/dts/scuffedOcclusion.png";

//Used in Model::Model to read what flags we want to load our model with from a text file
std::map<std::string, int> aiProcessMap = {
	{"CalcTangentSpace",aiProcess_CalcTangentSpace},
	{"JoinIdenticalVertices",aiProcess_JoinIdenticalVertices},
	{"MakeLeftHanded",aiProcess_MakeLeftHanded},
	{"Triangulate",aiProcess_Triangulate},
	{"RemoveComponent",aiProcess_RemoveComponent},
	{"GenNormals",aiProcess_GenNormals},
	{"GenSmoothNormals",aiProcess_GenSmoothNormals},
	{"SplitLargeMeshes",aiProcess_SplitLargeMeshes},
	{"PreTransformVertices",aiProcess_PreTransformVertices},
	{"LimitBoneWeights",aiProcess_LimitBoneWeights},
	{"ValidateDataStructure",aiProcess_ValidateDataStructure},
	{"ImproveCacheLocality",aiProcess_ImproveCacheLocality},
	{"RemoveRedundantMaterials",aiProcess_RemoveRedundantMaterials},
	{"FixInfacingNormals",aiProcess_FixInfacingNormals},
	{"SortByPType",aiProcess_SortByPType},
	{"FindDegenerates",aiProcess_FindDegenerates},
	{"FindInvalidData",aiProcess_FindInvalidData},
	{"GenUVCoords",aiProcess_GenUVCoords},
	{"TransformUVCoords",aiProcess_TransformUVCoords},
	{"FindInstances",aiProcess_FindInstances},
	{"OptimizeMeshes",aiProcess_OptimizeMeshes},
	{"OptimizeGraph",aiProcess_OptimizeGraph},
	{"FlipUVs",aiProcess_FlipUVs},
	{"FlipWindingOrder",aiProcess_FlipWindingOrder},
	{"SplitByBoneCount",aiProcess_SplitByBoneCount},
	{"Debone",aiProcess_Debone},
	{"GlobalScale",aiProcess_GlobalScale},
	{"EmbedTextures",aiProcess_EmbedTextures},
	{"ForceGenNormals",aiProcess_ForceGenNormals},
	{"DropNormals",aiProcess_DropNormals},
	{"GenBoundingBoxes",aiProcess_GenBoundingBoxes},
};

bool ModelInstance::getMeshColor(int meshIdx, glm::vec4& color) const 
{ 
	if (meshIdx < 0) 
		return false; 
	if (meshIdx >= MeshColors.size()) 
		return false; 

	color = MeshColors[meshIdx]; 
	return MeshColorUsed[meshIdx];
}

int Model::getMeshIdx(const std::string& name) const
{
	for (unsigned int a = 0; a < allMeshes.size(); a++)
	{
		if (allMeshes[a]->name == name)
			return a;
	}
	return -1;
}

int Model::getMeshIdxIgnoringCase(const std::string& name) const
{
	std::string lowerName = lowercase(name);
	for (unsigned int a = 0; a < allMeshes.size(); a++)
	{
		if (lowercase(allMeshes[a]->name) == lowerName)
			return a;
	}
	return -1;
}

int Model::getShirtMeshIdx() const
{
	return getMeshIdxIgnoringCase("Torso");
}

int Model::getFaceMeshIdx() const
{
	for (const char* name : { "Face1", "Face", "Head" })
	{
		int meshIdx = getMeshIdxIgnoringCase(name);
		if (meshIdx != -1)
			return meshIdx;
	}
	return -1;
}


void Model::addAnimation(Animation& animation,int id)
{
	animation.serverID = (id < 0) ? animations.size() : id;

	//A server side model has the nodes but never loads a keyframe, so nothing comes out as affected there
	animation.affectedNodes.assign(allNodes.size(), false);
	for (Node* node : allNodes)
	{
		std::vector<float> times = { animation.startTime, animation.endTime };
		for (float time : node->posTimes)
			if (time > animation.startTime && time < animation.endTime)
				times.push_back(time);
		for (float time : node->rotTimes)
			if (time > animation.startTime && time < animation.endTime)
				times.push_back(time);

		/*
			Does the animation move this node at all, measured against a rest pose of no translation and no
			rotation, which is what ModelInstance::calculateNodeTransform falls back to. A DTS node rests at
			its own default instead, see Model::nodeDefaultsAreRestPose, so a sequence that holds one still
			away from the origin counts as moving it, which costs nothing but a mix that changes nothing.
		*/
		for (float time : times)
		{
			glm::vec3 pos;
			glm::quat rot;
			if (node->sample(time, pos, rot) && (glm::length(pos) > 0.01f || std::abs(rot.w) < 0.99999f))
			{
				animation.affectedNodes[node->nodeIndex] = true;
				break;
			}
		}
	}

	animations.push_back(std::move(animation));
}

//Walks down from a node looking for one by that name, building up the transform each node sits under as it goes
bool Model::findNodePosition(Node const * const node, const std::string& lowerName, const glm::mat4& above, glm::vec3& position)
{
	//The same multiplication calculateMeshTransforms does, minus the animations a rest pose doesn't have
	glm::mat4 here = above * node->defaultTransform;

	if (lowercase(node->name) == lowerName)
	{
		position = glm::vec3(here[3]);
		return true;
	}

	for (Node const * const child : node->children)
	{
		if (findNodePosition(child, lowerName, here, position))
			return true;
	}

	return false;
}

bool Model::getNodePosition(const std::string& name, glm::vec3& position) const
{
	if (!rootNode)
		return false;

	glm::vec3 found;
	if (!findNodePosition(rootNode, lowercase(name), glm::mat4(1.0), found))
		return false;

	position = found * baseScale;
	return true;
}

int Model::getAnimationID(const std::string& name) const
{
	std::string lowerName = lowercase(name);
	for (const Animation& animation : animations)
		if (lowercase(animation.name) == lowerName)
			return animation.serverID;
	return -1;
}

void Model::printHierarchy(Node * node,int layer) const
{
	if (!node)
		node = rootNode;

	for (int a = 0; a < layer; a++)
		std::cout << "\t";
	std::cout << node->name << " ID: " << node->nodeIndex << "\n";

	glm::vec3 scale, skew, trans;
	glm::vec4 perspective;
	glm::quat rot;
	glm::decompose(node->defaultTransform, scale, rot, trans, skew, perspective);

	for (int a = 0; a < layer + 1; a++)
		std::cout << "\t";
	std::cout << "--Translation: " << trans.x << "," << trans.y << "," << trans.z << "\n";

	for (int a = 0; a < layer + 1; a++)
		std::cout << "\t";
	std::cout << "--Rotation: " << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << "\n";

	for (int a = 0; a < layer + 1; a++)
		std::cout << "\t";
	std::cout << "--Scale: " << scale.x << ", " << scale.y << ", " << scale.z << "\n";

	for (int a = 0; a < layer + 1; a++)
		std::cout << "\t";
	std::cout << "--Pivot: " << node->rotationPivot.x << "," << node->rotationPivot.y << "," << node->rotationPivot.z << "\n";

	for (unsigned int a = 0; a < node->meshes.size(); a++)
	{
		for (int a = 0; a < layer+1; a++)
			std::cout << "\t";
		std::cout << node->meshes[a]->name << ": " << node->meshes[a]->vertexCount << " verts, index: "<<node->meshes[a]->meshIndex<<"\n";
		if (node->meshes[a]->material)
		{
			for (int a = 0; a < layer + 2; a++)
				std::cout << "\t";
			std::cout << "Material: " <<node->meshes[a]->material->getName() << "\n";
		}
	}

	for (unsigned int a = 0; a < node->children.size(); a++)
		printHierarchy(node->children[a], layer + 1);
}

void ModelInstance::setHidden(bool _hidden, bool castShadow)
{
	hidden = _hidden;
	hiddenCastsShadow = castShadow;
	wholeModelTransformUpdated = true;

	//Casting shadows keeps the real transform and uses a flag instead, so both need uploading again
	transformUpdated = true;
	anythingUpdated = true;
	for (unsigned int a = 0; a < flagsUpdated.size(); a++)
		flagsUpdated[a] = true;
}

void ModelInstance::setModelTransform(glm::mat4 &&transform)
{
	//Used to update transformUpdated and anythingUpdated, but now wholeModelTransform is exempt
	//wholeModelTransform updates apply during any compile call
	wholeModelTransformUpdated = true;
	wholeModelTransform = transform;
}

glm::vec3 ModelInstance::getMeshCenter(int meshIdx) const
{
	return glm::vec3(wholeModelTransform * MeshTransforms[meshIdx] * glm::vec4(type->allMeshes[meshIdx]->center, 1.0f));
}

glm::quat ModelInstance::getMeshRotation(int meshIdx) const
{
	glm::mat3 rotation(wholeModelTransform * MeshTransforms[meshIdx]);
	for (int axis = 0; axis < 3; axis++)
		rotation[axis] = glm::normalize(rotation[axis]);
	return glm::quat_cast(rotation);
}

void ModelInstance::setNodeRotation(int nodeId,const glm::quat &rotation)
{
	if (nodeId < 0 || nodeId >= NodeRotationFixes.size())
	{
		scope("ModeInstance::setNodeRotation");
		error("Node index out of range.");
		return;
	}

	NodeRotationFixes[nodeId] = rotation;
	UseNodeRotationFix[nodeId] = true;
	transformUpdated = true;
	anythingUpdated = true;
}

void ModelInstance::setFlags(int meshId, unsigned int flags)
{
	if (meshId < 0 || meshId >= MeshFlags.size())
	{
		scope("ModeInstance::setFlags");
		error("Mesh index out of range.");
		return;
	}

	MeshFlags[meshId] = flags;
	flagsUpdated[meshId] = true;
	anythingUpdated = true;
}

void ModelInstance::setColor(int meshId, glm::vec4 color)
{
	if (meshId < 0 || meshId >= MeshFlags.size())
	{
		scope("ModelInstance::setColor");
		error("Mesh index out of range.");
		std::cout<<"Given: "<<meshId<<" max: "<<MeshFlags.size()<<"\n";
		return;
	}

	MeshColorUsed[meshId] = true;
	MeshColors[meshId] = color;
	colorsUpdated[meshId] = true;
	anythingUpdated = true;
}

void ModelInstance::update(float deltaT,bool debug)
{
	calculateMeshTransforms(deltaT,glm::mat4(1.0), 0, debug ? 0 : -1);
	performMeshBufferUpdates();
}

void Node::getFrame(float time, glm::vec3& pos, glm::mat4& rot) const
{
	//Get the key frame after and before our current time
	for (unsigned int a = 1; a < posFrames.size(); a++)
	{
		//Find the first frame with a time greater than our own
		if (posTimes[a] >= time)
		{
			pos += posFrames[a];
			break;
		}
	}

	//Get the key frame after and before our current time
	for (unsigned int a = 1; a < rotFrames.size(); a++)
	{
		//Find the first frame with a time greater than our own
		if (rotTimes[a] >= time)
		{
			rot = glm::toMat4(rotFrames[a]) * rot;
			break;
		}
	}
}

void ModelInstance::setDecal(int meshId, int decalId)
{
	if (meshId < 0 || meshId >= MeshFlags.size())
	{
		scope("ModeInstance::setDecal");
		error("Mesh index out of range.");
		return;
	}
	unsigned int flagsExceptDecal = MeshFlags[meshId] & 4294705663;
	unsigned int decalFlags = decalId == -1 ? 0 : (131072 + (decalId << 9));
	MeshFlags[meshId] = flagsExceptDecal | decalFlags;
	flagsUpdated[meshId] = true;
	anythingUpdated = true;
}

void ModelInstance::removeDecal(unsigned int meshId)
{
	setDecal(meshId, -1);
}

bool ModelInstance::getHighlight(glm::vec4& color, float& thickness) const
{
	color = highlightColor;
	thickness = highlightThickness;
	return highlightColor.a > 0;
}

std::vector<ModelInstance*> ModelInstance::highlightedInstances;

void ModelInstance::setHighlight(const glm::vec4& color, float thickness)
{
	bool wasActive = highlightColor.a > 0;
	bool nowActive = color.a > 0;

	highlightColor = color;
	highlightThickness = thickness;
	highlightUpdated = true;
	anythingUpdated = true;

	//highlightedInstances is only for the client's outline render pass. Server-side instances still track their
	//highlight (for networking) but have nothing to render, and in single player they share this static list
	//with the client, where they'd outlive their Model after the embedded server shuts down
	if (!type || type->isServerSide())
		return;

	if (nowActive && !wasActive)
		highlightedInstances.push_back(this);
	else if (!nowActive && wasActive)
	{
		auto pos = std::find(highlightedInstances.begin(), highlightedInstances.end(), this);
		if (pos != highlightedInstances.end())
			highlightedInstances.erase(pos);
	}
}

void ModelInstance::clearHighlight()
{
	setHighlight(glm::vec4(0, 0, 0, 0), 0);
}

void ModelInstance::renderSelfOutline() const
{
	/*
		Deliberately not using the bufferOffset field here. It's maintained incrementally (decremented on a
		sibling instance's destruction - see ~ModelInstance), which can drift from this instance's true
		position under bursts of creation/destruction, particularly client-side where object lifetime is
		driven by network packets that aren't guaranteed to finish arriving in lockstep with the order they
		were sent (confirmed empirically: spawning/despawning ~100 instances in quick succession left
		bufferOffset corrupted). The main render path (Mesh::recompileInstances, called every frame) never
		notices this because it rewrites its entire buffer from the live instances list by direct array
		position every frame regardless of bufferOffset, silently self-healing any drift. This path draws
		exactly one instance by index though, so a stale offset here doesn't cause a one-frame glitch, it
		draws the wrong instance (or an out-of-bounds one) every frame. Look up the true current position
		instead - cheap, since only a small number of instances are ever highlighted at once.
	*/
	if (type->allMeshes.empty())
		return;

	const std::vector<ModelInstance*>& liveInstances = type->allMeshes[0]->instances;
	auto pos = std::find(liveInstances.begin(), liveInstances.end(), this);
	if (pos == liveInstances.end())
		return;

	type->renderSingleInstance((unsigned int)(pos - liveInstances.begin()));
}

//The first key at or after time, and how far time is past the key before it, holding the first or last key outside the keys' range
static void findKeys(const std::vector<float>& times, float time, size_t& next, float& progress)
{
	next = times.size() - 1;
	progress = 1;

	for (size_t a = 0; a < times.size(); a++)
	{
		if (times[a] >= time)
		{
			next = a;
			float span = a > 0 ? times[a] - times[a - 1] : 0;
			progress = span > 0 ? (time - times[a - 1]) / span : 1;
			return;
		}
	}
}

bool Node::sample(float time, glm::vec3& pos, glm::quat& rot) const
{
	if (posFrames.empty() && rotFrames.empty())
		return false;

	size_t next;
	float progress;

	pos = glm::vec3(0, 0, 0);
	if (!posFrames.empty())
	{
		findKeys(posTimes, time, next, progress);
		pos = next > 0 ? glm::mix(posFrames[next - 1], posFrames[next], progress) : posFrames[next];
	}

	rot = glm::quat(1, 0, 0, 0);
	if (!rotFrames.empty())
	{
		findKeys(rotTimes, time, next, progress);
		rot = next > 0 ? glm::slerp(rotFrames[next - 1], rotFrames[next], progress) : rotFrames[next];
	}

	return true;
}

void ModelInstance::progressAnimations(float deltaT)
{
	auto iter = playingAnimations.begin();
	while (iter != playingAnimations.end())
	{
		AnimationPlayback* anim = &(*iter);

		if (anim->animationEnding)
		{
			//Animation is truely over, remove it from this instance
			if (anim->animationFadeOut <= 0 || anim->animation->fadeOutMS == 0)
			{
				iter = playingAnimations.erase(iter);
				transformUpdated = true;
				continue;
			}
			else
				anim->animationFadeOut -= deltaT / anim->animation->fadeOutMS;
		}
		else if (anim->animationStarting)
		{
			if (anim->animationFadeOut >= 1.0 || anim->animation->fadeInMS == 0)
			{
				anim->animationFadeOut = 1.0;
				anim->animationStarting = false;
			}
			else
				anim->animationFadeOut += deltaT / anim->animation->fadeInMS;
		}

		//Advance animation based on passed time
		anim->animationTime += deltaT * anim->animationSpeed * anim->animation->defaultSpeed;

		//The animation has ended
		if (anim->animationTime >= anim->animation->endTime)
		{
			//Start over
			if (anim->animationLooping)
			{
				float animationLength = anim->animation->endTime - anim->animation->startTime;
				anim->animationTime -= animationLength;
			}
			else //Begin ending fade-out
				anim->animationEnding = true;
		}

		++iter;
	}
}

void Model::setDefaultFrame(float time)
{
	for (unsigned int a = 0; a < allNodes.size(); a++)
	{
		if (allNodes[a]->posFrames.size() < 1 || allNodes[a]->rotFrames.size() < 1)
			continue;

		glm::vec3 pos = glm::vec3(0, 0, 0);
		glm::mat4 rot = glm::mat4(1.0);
		allNodes[a]->getFrame(time, pos, rot);
		allNodes[a]->defaultTransform = rot * glm::translate(pos);
		allNodes[a]->defaultRot = getRotationFromMatrix(rot);
		allNodes[a]->defaultPos = pos;
	}
}

glm::mat4 ModelInstance::calculateNodeTransform(Node const * const node)
{
	glm::mat4 ret = glm::mat4(1.0);

	//Default transform if no animation data available at all
	if (node->posFrames.size() < 1 && node->rotFrames.size() < 1)
	{
		ret = node->defaultTransform;
	}
	else
	{
		//Also what's used when no animations are playing
		glm::vec3 pos = node->defaultPos;
		glm::quat rot = node->defaultRot;

		//playingAnimations is kept in server ID order, so animations added later in Lua play over earlier ones, like a grab over the walk cycle
		//Each one mixes over what's under it as it fades in and out, but only on the nodes it actually moves
		for (const AnimationPlayback& anim : playingAnimations)
		{
			const std::vector<bool>& affected = anim.animation->affectedNodes;
			if (node->nodeIndex >= affected.size() || !affected[node->nodeIndex])
				continue;

			glm::vec3 animPos;
			glm::quat animRot;
			float time = std::clamp(anim.animationTime, anim.animation->startTime, anim.animation->endTime);
			if (!node->sample(time, animPos, animRot))
				continue;

			float weight = std::clamp(anim.animationFadeOut, 0.0f, 1.0f);
			pos = glm::mix(pos, animPos, weight);
			rot = glm::slerp(rot, animRot, weight);
		}

		if(type->rotationPivotsApplied)
			ret = glm::translate(node->rotationPivot) * glm::toMat4(rot) * glm::translate(-node->rotationPivot) * glm::translate(pos);
		else
			ret = glm::translate(pos) * glm::toMat4(rot);
	}

	//Apply node rotation fixes, for example the tilt of a player's head when they look up or down
	if (UseNodeRotationFix[node->nodeIndex])
	{
		ret = ret * glm::translate(node->fixPivot) * glm::toMat4(NodeRotationFixes[node->nodeIndex]) * glm::translate(-node->fixPivot);
	}

	return ret;
}

void ModelInstance::calculateMeshTransforms(float deltaT,glm::mat4 currentTransform,Node const * currentNode,int debugLayer)
{
	//The only update was perhaps the entire model as a whole moving around
	//This would be a change to wholeModelTransform which is applied right before sending to a GL buffer
	if (playingAnimations.size() < 1 && !transformUpdated)
		return;

	if (!currentNode)
	{
		currentNode = type->rootNode;
		progressAnimations(deltaT);
	} 

	if (debugLayer != -1)
	{
		for (int a = 0; a < debugLayer; a++)
			std::cout << "\t";
		std::cout << currentNode->name << "\n";
	}

	currentTransform = currentTransform * calculateNodeTransform(currentNode);

	for (unsigned int a = 0; a < currentNode->meshes.size(); a++)
	{
		//Meshes that are never drawn too: a part can be worn on one, like the hat on a hat lying on the ground, see placeAttachment
		MeshTransforms[currentNode->meshes[a]->meshIndex] = glm::scale(type->baseScale) * currentTransform;

		if (debugLayer != -1)
		{
			for (int a = 0; a < debugLayer + 1; a++)
				std::cout << "\t";
			std::cout << currentNode->meshes[a]->name << "\n";

			glm::vec3 scale, skew, trans;
			glm::vec4 perspective;
			glm::quat rot;
			glm::decompose(currentTransform, scale, rot, trans, skew, perspective);

			for (int a = 0; a < debugLayer + 1; a++)
				std::cout << "\t";
			std::cout << "--Translation: " << trans.x << "," << trans.y << "," << trans.z << "\n";

			for (int a = 0; a < debugLayer + 1; a++)
				std::cout << "\t";
			std::cout << "--Rotation: " << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << "\n";

			for (int a = 0; a < debugLayer + 1; a++)
				std::cout << "\t";
			std::cout << "--Scale: " << scale.x << ", " << scale.y << ", " << scale.z << "\n";
		}
	}

	//deltaT is only used at the start of this function when currentNode == 0 to calculate animation changes
	for (unsigned int a = 0; a < currentNode->children.size(); a++)
		calculateMeshTransforms(0,currentTransform, currentNode->children[a],debugLayer == -1 ? -1 : debugLayer + 1);
}

void ModelInstance::performMeshBufferUpdates()
{
	if (!anythingUpdated && playingAnimations.size() < 0 && !wholeModelTransformUpdated && !highlightUpdated)
		return;
	wholeModelTransformUpdated = false;

	//Mesh by mesh, update what has changed since last call of this function
	for (unsigned int a = 0; a < type->allMeshes.size(); a++)
	{
		if (type->allMeshes[a]->nonRenderingMesh)
			continue;

		if (highlightUpdated)
		{
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[HighlightColor]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * bufferOffset, sizeof(glm::vec4), &highlightColor[0]);

			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[HighlightThickness]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * bufferOffset, sizeof(float), &highlightThickness);
		}

		if (hidden && !hiddenCastsShadow)
		{
			//Setting all points of GL_POSITION in a prim to the same value is defined behavior in opengl shaders and will discard the prim
			glm::mat4 meshTransform = glm::mat4(0.0);
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[ModelTransform]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * bufferOffset, sizeof(glm::mat4), &meshTransform[0][0]);

			continue;
		}

		if (transformUpdated || playingAnimations.size() > 0)
		{
			glm::mat4 meshTransform = wholeModelTransform * MeshTransforms[a];
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[ModelTransform]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * bufferOffset, sizeof(glm::mat4), &meshTransform[0][0]);
		}

		if (flagsUpdated[a])
		{
			unsigned int flags = MeshFlags[a] | (hidden ? hiddenCastingShadowFlag : 0);
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[InstanceFlags]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(int) * bufferOffset, sizeof(int), &flags);

			flagsUpdated[a] = false;
		}

		if (colorsUpdated[a])
		{
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[PreColor]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * bufferOffset, sizeof(glm::vec4), &MeshColors[a][0]);

			colorsUpdated[a] = false;
		}
	}

	highlightUpdated = false;
	transformUpdated = false;
	anythingUpdated = false;
}

void Mesh::fillBuffer(LayoutSlot slot, void* data, int size, int elements)
{
	glBindBuffer(GL_ARRAY_BUFFER, buffers[slot]);
	glEnableVertexAttribArray(slot);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
	glVertexAttribDivisor(slot, 0);
	glVertexAttribPointer(slot, elements, GL_FLOAT, GL_FALSE, 0, 0);
}

void ModelInstance::stopAnimation(int id)
{
	for (unsigned int a = 0; a < playingAnimations.size(); a++)
	{
		if (playingAnimations[a].animation->serverID == id)
		{
			playingAnimations[a].animationStarting = false;
			playingAnimations[a].animationEnding = true;
			return;
		}
	}
}

void ModelInstance::playAnimation(int id,bool loop)
{
	for (unsigned int a = 0; a < playingAnimations.size(); a++)
	{
		if (playingAnimations[a].animation->serverID == id)
		{
			playingAnimations[a].animationStarting = true;
			playingAnimations[a].animationEnding = false;
			playingAnimations[a].animationLooping = loop;
			return;
		}
	}

	Animation* anim = nullptr;
	for (unsigned int a = 0; a < type->animations.size(); a++)
	{
		if (type->animations[a].serverID == id)
		{
			anim = &type->animations[a];
			continue;
		}
	}

	if (!anim)
		return;

	AnimationPlayback play;
	play.animation = anim;
	play.animationLooping = loop;
	play.animationStarting = true;
	play.animationEnding = false;
	play.animationFadeOut = 0.0;
	play.animationTime = play.animation->startTime;

	//In server ID order, see calculateNodeTransform
	auto pos = std::find_if(playingAnimations.begin(), playingAnimations.end(), [id](const AnimationPlayback& playing) { return playing.animation->serverID > id; });
	playingAnimations.insert(pos, std::move(play));
}

void ModelInstance::restartAnimation(int id)
{
	for (AnimationPlayback& playing : playingAnimations)
	{
		if (playing.animation->serverID == id)
		{
			playing.animationTime = playing.animation->startTime;
			playing.animationStarting = true;
			playing.animationEnding = false;
			playing.animationLooping = false;
			return;
		}
	}

	playAnimation(id, false);
}

bool ModelInstance::isPlaying(int id) const
{
	for (unsigned int a = 0; a < playingAnimations.size(); a++)
	{
		if (playingAnimations[a].animation->serverID == id)
			return !playingAnimations[a].animationEnding;
	}
	return false;
}

Mesh::Mesh(aiMesh const* const src, Model const* const parent,bool serverSide)
{
	scope("Mesh::Mesh");

	if (serverSide)
	{
		name = src->mName.C_Str();
		//Servers don't draw, but getMeshAtPoint still needs to know which meshes aren't part of how the model looks
		nonRenderingMesh = lowercase(name) == "collision";
		return;
	}

	debug("Loading mesh: " + std::string(src->mName.C_Str()));

	//If we have one material, make sure we just use that, could be result of materialoverride setting in descriptor file
	if (parent->allMaterials.size() == 1)
		material = parent->allMaterials[0];
	else if (parent->allMaterials.size() > 1)	//Just use whatever materials were baked into the model file itself
	{
		if (src->mMaterialIndex < parent->allMaterials.size())
			material = parent->allMaterials[src->mMaterialIndex];
		else
			error("Material index for mesh out of range! Wanted: " + std::to_string(src->mMaterialIndex) + " total : " + std::to_string(parent->allMaterials.size()));
	}

	name = src->mName.C_Str();

	if (lowercase(name) == "collision")
	{
		nonRenderingMesh = true;
		valid = true;
		//TODO: processCollisionMesh();
		return;
	}

	glGenVertexArrays(1, &vao);

	glGenBuffers(MeshBufferCount, buffers);
	glGenBuffers(1, &indexBuffer);

	//From here on, vao/buffers/indexBuffer are real GL objects that the destructor needs to clean up
	hasGLResources = true;

	glBindVertexArray(vao);

	//We're going to allocate space and set properties for the 3 per-instance buffers now:

	//A vec4 RGBA color. One per instance of the mesh rendered.
	glBindBuffer(GL_ARRAY_BUFFER, buffers[PreColor]);
	glEnableVertexAttribArray(PreColor);
	glVertexAttribPointer(PreColor, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(PreColor, 1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * InstanceBufferPageSize, (void*)0, GL_DYNAMIC_DRAW);

	//A 32bit int that represents at least a few things:
	//Color-based mouse-picking IDs when mouse-picking
	//Whether to use a decal and if so which one
	//Various special effects and stuff 
	glBindBuffer(GL_ARRAY_BUFFER, buffers[InstanceFlags]);
	glEnableVertexAttribArray(InstanceFlags);
	glVertexAttribIPointer(InstanceFlags, 1, GL_UNSIGNED_INT, 0, (void*)0);
	glVertexAttribDivisor(InstanceFlags, 1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(unsigned int) * InstanceBufferPageSize, (void*)0, GL_DYNAMIC_DRAW);

	//In OpenGL you *can* pass a mat4 as a vertex buffer based layout binding
	//But you technically need to use 4 different layout locations to achieve it, 
	//as if each one passed one row of a 4x4 matrix
	//This is per-instance as well btw 
	glBindBuffer(GL_ARRAY_BUFFER, buffers[ModelTransform]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * InstanceBufferPageSize, (void*)0, GL_DYNAMIC_DRAW);
	for (int i = 0; i < 4; i++)
	{
		glEnableVertexAttribArray(ModelTransform + i);
		glVertexAttribPointer(    ModelTransform + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(4 * sizeof(float) * i));
		glVertexAttribDivisor(    ModelTransform + i, 1);
	}

	//Outline/highlight color for the whole model instance, alpha <= 0 means no highlight (per-instance, only read by the outline shader)
	glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightColor]);
	glEnableVertexAttribArray(HighlightColor);
	glVertexAttribPointer(HighlightColor, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(HighlightColor, 1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * InstanceBufferPageSize, (void*)0, GL_DYNAMIC_DRAW);

	//How far to extrude the outline pass along vertex normals, in world units (per-instance, only read by the outline shader)
	glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightThickness]);
	glEnableVertexAttribArray(HighlightThickness);
	glVertexAttribPointer(HighlightThickness, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(HighlightThickness, 1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * InstanceBufferPageSize, (void*)0, GL_DYNAMIC_DRAW);

	instancesAllocated = InstanceBufferPageSize;

	//For the other 5 layout mapped buffers as well as the index buffer, we need to get data from Assimp:

	//Model space positions:
	if (!src->HasPositions())
	{
		error("Assimp Mesh had no position vertex data!");
		return;
	}

	if (src->mNumVertices > 0)
	{
		glm::vec3 low(src->mVertices[0].x, src->mVertices[0].y, src->mVertices[0].z);
		glm::vec3 high = low;
		for (unsigned int v = 1; v < src->mNumVertices; v++)
		{
			glm::vec3 vertex(src->mVertices[v].x, src->mVertices[v].y, src->mVertices[v].z);
			low = glm::min(low, vertex);
			high = glm::max(high, vertex);
		}
		center = (low + high) * 0.5f;
		this->low = low;
	}

	fillBuffer(ModelSpace, src->mVertices, src->mNumVertices * sizeof(aiVector3D), 3);

	if (src->HasNormals())
	{
		fillBuffer(NormalVector, src->mNormals, src->mNumVertices * sizeof(aiVector3D), 3);
		hasNormals = true;

		/*
			See the SmoothNormal comment in Mesh.h: NormalVector above can be a hard per-face normal, which
			would make the outline pass's extruded shell visibly tear apart at every edge. Build a second
			normal channel by averaging together the normals of every vertex that shares a position, so
			adjacent faces agree on which way to push out and the shell stays welded.
		*/
		std::vector<aiVector3D> smoothNormals(src->mNumVertices, aiVector3D(0, 0, 0));
		std::map<std::tuple<float, float, float>, std::vector<unsigned int>> positionGroups;
		for (unsigned int v = 0; v < src->mNumVertices; v++)
		{
			const aiVector3D& p = src->mVertices[v];
			positionGroups[std::make_tuple(p.x, p.y, p.z)].push_back(v);
		}

		for (auto& group : positionGroups)
		{
			aiVector3D sum(0, 0, 0);
			for (unsigned int idx : group.second)
				sum += src->mNormals[idx];

			if (sum.SquareLength() > 0.0001f)
				sum.Normalize();

			for (unsigned int idx : group.second)
				smoothNormals[idx] = sum;
		}

		fillBuffer(SmoothNormal, smoothNormals.data(), (unsigned int)smoothNormals.size() * sizeof(aiVector3D), 3);
	}

	if (src->HasTangentsAndBitangents())
	{
		fillBuffer(TangentVector,   src->mTangents,   src->mNumVertices * sizeof(aiVector3D), 3);
		fillBuffer(BitangentVector, src->mBitangents, src->mNumVertices * sizeof(aiVector3D), 3);
		hasTangents = true;

		if (!hasNormals)
			error("A model had tangents and bitangents but not normals somehow.");
	}

	if (src->GetNumUVChannels() == 1)
	{
		std::vector<glm::vec2> data;
		for (unsigned int b = 0; b < src->mNumVertices; b++)
			data.push_back(glm::vec2(src->mTextureCoords[0][b].x, src->mTextureCoords[0][b].y));

		fillBuffer(TextureCoords, &data[0][0], ((unsigned int)data.size()) * sizeof(glm::vec2), 2);
	}
	else if(src->GetNumUVChannels() > 1)
		error("File has more than one UV channel which is not supported!");

	if (!src->HasFaces())
	{
		error("Model does not have indicies / faces even though it does have position vertices.");
		return;
	}

	std::vector<unsigned short> indices;
	for (unsigned int b = 0; b < src->mNumFaces; b++)
	{
		if (src->mFaces[b].mNumIndices != 3)
			error("Mesh has non-triangle face, ignoring. Did you know you can specify aiProcess_Triangulate to fix this?");
		else
		{
			indices.push_back(src->mFaces[b].mIndices[0]);
			indices.push_back(src->mFaces[b].mIndices[1]); 
			indices.push_back(src->mFaces[b].mIndices[2]);
		}
	}
	vertexCount = (unsigned int)indices.size();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
	
	glBindVertexArray(0);

	valid = true;
}

Mesh::~Mesh()
{
	//Server-side meshes (no GL context ever exists on a dedicated server) and non-rendering meshes
	//(e.g. "collision") never allocated these - calling glDelete* on them would either crash (null
	//GL function pointers with no context) or delete whatever garbage handle happened to be left in
	//these uninitialized fields
	if (!hasGLResources)
		return;

	glDeleteBuffers(1, &indexBuffer);
	glDeleteBuffers(MeshBufferCount, buffers);

	glDeleteVertexArrays(1, &vao);
}

void Mesh::render(std::shared_ptr<ShaderManager> graphics, bool useMaterials) const
{
	if (nonRenderingMesh)
		return;

	if(instances.size() < 1)
		return;

	if (graphics->basicUniforms.DecalArea != decalArea)
	{
		graphics->basicUniforms.DecalArea = decalArea;
		graphics->updateBasicUBO();
	}

	if (useMaterials && material)
		material->use(graphics);

	glBindVertexArray(vao);
	glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, (void*)0, (GLsizei)instances.size());
	glBindVertexArray(0);
}

void Mesh::renderOnce(std::shared_ptr<ShaderManager> graphics) const
{
	if (nonRenderingMesh || !valid)
		return;

	if (graphics->basicUniforms.DecalArea != decalArea)
	{
		graphics->basicUniforms.DecalArea = decalArea;
		graphics->updateBasicUBO();
	}

	if (material)
		material->use(graphics);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, (void*)0);
	glBindVertexArray(0);
}

void Mesh::renderSingleInstance(unsigned int bufferOffset) const
{
	if (nonRenderingMesh)
		return;

	//Every mesh of a model is kept parallel with the others (see ModelInstance::renderSelfOutline, which is the
	//only caller and computes this index fresh each call), so this should never actually trigger - just a guard
	//against reading past the buffer if that invariant is ever broken
	if (bufferOffset >= instances.size())
		return;

	glBindVertexArray(vao);
	glDrawElementsInstancedBaseInstance(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, (void*)0, 1, bufferOffset);
	glBindVertexArray(0);
}

ModelInstance::ModelInstance(Model* _type)
{
	if (!_type)
		return;

	type = _type;

	//Something messed up loading the model?
	if(type->allMeshes.size() < 1)
		return;

	//Create values for each mesh and node so we can always just access them by model's mesh/node index without worring about size
	for(unsigned int a = 0 ; a<type->allMeshes.size(); a++)
	{
		MeshTransforms.push_back(glm::mat4(1.0));
		MeshFlags.push_back(0);
		MeshColorUsed.push_back(false);
		MeshColors.push_back(glm::vec4(0,0,0,0));
		flagsUpdated.push_back(true);
		colorsUpdated.push_back(true);
	}

	for(unsigned int a = 0; a<type->allNodes.size(); a++)
	{
		NodeRotationFixes.push_back(glm::quat(1.0,0.0,0.0,0.0));
		UseNodeRotationFix.push_back(false);
	}

	//A player's face plate, like Face1 in front of the default player's head, is see-through except for the face on it
	int faceMesh = type->getFaceMeshIdx();
	if (faceMesh != -1 && lowercase(type->allMeshes[faceMesh]->name) != "head")
		MeshFlags[faceMesh] |= MeshFlag_DecalCutout;

	//The offset should be the same for all meshes of a given model
	bufferOffset = (unsigned int)type->allMeshes[0]->instances.size();
	
	//Used for Model::updateAll
	type->instances.push_back(this);

	//Add this instance to each of its Model's Meshes 
	for (unsigned int a = 0; a < type->allMeshes.size(); a++)
		type->allMeshes[a]->instances.push_back(this);
}

ModelInstance::~ModelInstance()
{
	//Safety net: an object shouldn't normally be destroyed while still highlighted (callers clear it first),
	//but avoid ever leaving a dangling pointer in this list if one is
	auto highlightPos = std::find(highlightedInstances.begin(), highlightedInstances.end(), this);
	if (highlightPos != highlightedInstances.end())
		highlightedInstances.erase(highlightPos);

	if (!type)
		return;

	std::vector<ModelInstance*>::iterator pos = std::find(
		type->instances.begin(),
		type->instances.end(),
		this
	);

	if (pos != type->instances.end()) //Shouldn't happen
		type->instances.erase(pos);
	
	//Remove this instance from all of its Model's Meshes
	for (unsigned int a = 0; a < type->allMeshes.size(); a++)
	{
		std::vector<ModelInstance*>::iterator pos = std::find(
			type->allMeshes[a]->instances.begin(),
			type->allMeshes[a]->instances.end(),
			this
		);

		if (pos == type->allMeshes[a]->instances.end()) //Shouldn't happen
			continue;
		
		pos = type->allMeshes[a]->instances.erase(pos);

		//Update all other instances bufferOffsets since after this instance will need to be decremented by one
		//TODO: Maybe preallocate a certain amount of instances at a time, reallocate more if needed, and only soft delete with a hidden flag most of the time
		while(pos != type->allMeshes[a]->instances.end())
		{
			--(*pos)->bufferOffset;
			++pos;
		}
	}
}

/*
	Assimp does this nasty thing when importing some models
	It will add a billion superfluous nodes on top of each node
	If you had Torso -> Left Arm -> Left Hand it would give you
	Torso_RotationPivot -> Torso_Translation -> Torso->Rotation -> Torso_Scale -> LeftArm_RotationPivot...
	This just makes parsing the hierarchy less efficient and we can generally get rid of all these
*/
std::string stripSillyAssimpNodeNames(std::string in)
{
	if (in.find("_$AssimpFbx$_") != std::string::npos)
		return in.substr(0, in.find("_$AssimpFbx$_"));
	else
		return in;
}

//Recursivly searches model's node tree looking for a collision mesh using default node transforms, returns true if mesh found
bool getCollisionTransformMatrix(const aiScene* scene,const aiNode* node, aiMatrix4x4 &startTransform)
{
	startTransform = startTransform * node->mTransformation;

	for (unsigned int a = 0; a < node->mNumMeshes; a++)
	{
		std::string name = lowercase(scene->mMeshes[node->mMeshes[a]]->mName.C_Str());
		if (name.find("collision") != std::string::npos)
			return true;
	}
	for (unsigned int a = 0; a < node->mNumChildren; a++)
	{
		aiMatrix4x4 childTransform = startTransform;
		if (getCollisionTransformMatrix(scene, node->mChildren[a], childTransform))
		{
			startTransform = childTransform;
			return true;
		}
	}

	return false;
}

//Grows minPos and maxPos to hold every vertex of every mesh under node, moved into model space by the node transforms above them
//Meshes whose index into the scene's meshes is in skip are left out
static void growToMeshes(const aiScene* scene, const aiNode* node, aiMatrix4x4 transform, aiVector3D& minPos, aiVector3D& maxPos, const std::set<unsigned int>& skip)
{
	transform = transform * node->mTransformation;

	for (unsigned int a = 0; a < node->mNumMeshes; a++)
	{
		if (skip.count(node->mMeshes[a]))
			continue;

		const aiMesh* mesh = scene->mMeshes[node->mMeshes[a]];
		for (unsigned int v = 0; v < mesh->mNumVertices; v++)
		{
			aiVector3D position = transform * mesh->mVertices[v];
			minPos.x = std::min(position.x, minPos.x);
			minPos.y = std::min(position.y, minPos.y);
			minPos.z = std::min(position.z, minPos.z);
			maxPos.x = std::max(position.x, maxPos.x);
			maxPos.y = std::max(position.y, maxPos.y);
			maxPos.z = std::max(position.z, maxPos.z);
		}
	}

	for (unsigned int a = 0; a < node->mNumChildren; a++)
		growToMeshes(scene, node->mChildren[a], transform, minPos, maxPos, skip);
}

//The same walk as growToMeshes, except each mesh gets its own box rather than one around the lot of them
void Model::calculateMeshBounds(const aiScene* scene)
{
	//Each node left to visit, with the transform that puts its meshes in the whole model's space
	std::vector<std::pair<const aiNode*, aiMatrix4x4>> toVisit;
	toVisit.emplace_back(scene->mRootNode, aiMatrix4x4());

	while (!toVisit.empty())
	{
		const aiNode* node = toVisit.back().first;
		aiMatrix4x4 transform = toVisit.back().second * node->mTransformation;
		toVisit.pop_back();

		for (unsigned int a = 0; a < node->mNumMeshes; a++)
		{
			unsigned int meshIdx = node->mMeshes[a];
			if (meshIdx >= allMeshes.size())
				continue;

			Mesh* mesh = allMeshes[meshIdx];
			const aiMesh* src = scene->mMeshes[meshIdx];
			for (unsigned int v = 0; v < src->mNumVertices; v++)
			{
				aiVector3D position = transform * src->mVertices[v];
				glm::vec3 vertex(position.x, position.y, position.z);

				if (mesh->hasBounds())
				{
					mesh->boundsLow = glm::min(mesh->boundsLow, vertex);
					mesh->boundsHigh = glm::max(mesh->boundsHigh, vertex);
				}
				else
				{
					mesh->boundsLow = vertex;
					mesh->boundsHigh = vertex;
				}
			}
		}

		for (unsigned int a = 0; a < node->mNumChildren; a++)
			toVisit.emplace_back(node->mChildren[a], transform);
	}
}

int Model::getMeshAtPoint(const glm::vec3& point) const
{
	//The face plate sits right in front of the head, and is see-through apart from the face, so the head is what's really there
	int facePlate = getFaceMeshIdx();
	if (facePlate != -1 && lowercase(allMeshes[facePlate]->name) == "head")
		facePlate = -1;

	int best = -1;
	float bestDistance = 0;
	float bestVolume = 0;

	for (int a = 0; a < (int)allMeshes.size(); a++)
	{
		const Mesh* mesh = allMeshes[a];
		if (a == facePlate || mesh->nonRenderingMesh || !mesh->hasBounds())
			continue;

		//How far outside the box the point is along each axis, all zero for one inside it
		glm::vec3 away = glm::max(glm::max(mesh->boundsLow - point, point - mesh->boundsHigh), glm::vec3(0));
		float distance = glm::dot(away, away);

		glm::vec3 size = mesh->boundsHigh - mesh->boundsLow;
		float volume = size.x * size.y * size.z;

		//A smaller box inside a bigger one, like a head within a whole body, is the more useful answer
		if (best == -1 || distance < bestDistance || (distance == bestDistance && volume < bestVolume))
		{
			best = a;
			bestDistance = distance;
			bestVolume = volume;
		}
	}

	return best;
}

void Model::calculateCollisionBox(const aiScene* scene)
{
	/*
		How big the thing actually looks, whatever it collides as. A Blockland shape's collision detail is
		often only its body (the stunt plane's leaves out its wings entirely), so anything that wants to
		know how much room the model takes up on screen, like where a plane's camera sits, asks for this
		rather than for the collision box. Hidden meshes are left out the same way
	*/
	{
		std::set<unsigned int> hidden;
		for (unsigned int a = 0; a < allMeshes.size() && a < scene->mNumMeshes; a++)
			if (allMeshes[a]->nonRenderingMesh)
				hidden.insert(a);

		aiVector3D high = aiVector3D(-9999, -9999, -9999);
		aiVector3D low = aiVector3D(9999, 9999, 9999);
		growToMeshes(scene, scene->mRootNode, aiMatrix4x4(), low, high, hidden);

		if (high.x >= low.x)
			drawnHalfExtents = glm::vec3(high.x - low.x, high.y - low.y, high.z - low.z) * 0.5f;
	}

	//Find the collision mesh, kinda redundant since we do it in getCollisionTransformMatrix
	aiMesh* colMesh = nullptr;
	for (unsigned int a = 0; a < scene->mNumMeshes; a++)
	{
		std::string name = scene->mMeshes[a]->mName.C_Str();
		name = lowercase(name);
		if (name.find("collision") != std::string::npos)
		{
			colMesh = scene->mMeshes[a];
			break;
		}
	}

	if (!colMesh)
	{
		/*
			No collision mesh, a box around everything the model draws instead. Not around a mesh a hide line
			took out: that's a part that can't be drawn the way it was meant, like the flame behind a Blockland
			rocket, which would otherwise make the rocket collide as a box sixteen studs long. allMeshes is in
			the same order as the scene's meshes
		*/
		std::set<unsigned int> hidden;
		for (unsigned int a = 0; a < allMeshes.size() && a < scene->mNumMeshes; a++)
			if (allMeshes[a]->nonRenderingMesh)
				hidden.insert(a);

		aiVector3D maxPos = aiVector3D(-9999, -9999, -9999);
		aiVector3D minPos = aiVector3D(9999, 9999, 9999);
		growToMeshes(scene, scene->mRootNode, aiMatrix4x4(), minPos, maxPos, hidden);

		//Every mesh it has is hidden, so there's nothing else to go by
		if (maxPos.x < minPos.x)
			growToMeshes(scene, scene->mRootNode, aiMatrix4x4(), minPos, maxPos, {});

		if (maxPos.x < minPos.x)
		{
			error("No collision mesh found, and no vertices to put a box around.");
			return;
		}

		aiVector3D halfSize = maxPos - minPos;
		halfSize /= 2.0;
		collisionHalfExtents = glm::vec3(halfSize.x, halfSize.y, halfSize.z);
		collisionOffset = glm::vec3(halfSize.x + minPos.x, halfSize.y + minPos.y, halfSize.z + minPos.z);
		eyePosition = glm::vec3(0, halfSize.y * 1.85, 0);
		return;
	}

	//Find the default transform for that collision mesh within the larger model
	aiMatrix4x4 collisionMeshTransform = aiMatrix4x4();
	if (!getCollisionTransformMatrix(scene, scene->mRootNode, collisionMeshTransform))
	{
		//We should have bailed above at this point
		error("No collision mesh found.");
		return; 
	}

	//Find the highest and lowest point in all 3 dimensions for the collision mesh
	aiVector3D maxPos = aiVector3D(-9999, -9999, -9999);
	aiVector3D minPos = aiVector3D(9999, 9999, 9999);

	for (unsigned int a = 0; a < colMesh->mNumVertices; a++)
	{
		aiVector3D v = colMesh->mVertices[a];

		maxPos.x = std::max(v.x, maxPos.x);
		maxPos.y = std::max(v.y, maxPos.y);
		maxPos.z = std::max(v.z, maxPos.z);

		minPos.x = std::min(v.x, minPos.x);
		minPos.y = std::min(v.y, minPos.y);
		minPos.z = std::min(v.z, minPos.z);
	}

	//Move coordinates from mesh space to model space
	maxPos = collisionMeshTransform * maxPos;
	minPos = collisionMeshTransform * minPos;

	if (maxPos.x < minPos.x)
		std::swap(maxPos.x, minPos.x);
	if (maxPos.y < minPos.y)
		std::swap(maxPos.y, minPos.y);
	if (maxPos.z < minPos.z)
		std::swap(maxPos.z, minPos.z);

	aiVector3D size = maxPos - minPos;

	//Bullet uses *half* extents
	size /= 2.0;
	collisionHalfExtents = glm::vec3(size.x, size.y, size.z);

	aiVector3D offset = size + minPos;
	collisionOffset = glm::vec3(offset.x, offset.y, offset.z);

	//This can be manually set later, or you can add a mesh with the name "eye" to your model
	eyePosition = glm::vec3(0, size.y * 1.85, 0);
}

//Abridged version for server-side loading, namely for collision meshes
Model::Model(std::string filePath, bool _serverSide, glm::vec3 _baseScale) : loadedPath(filePath), serverSide(_serverSide), baseScale(_baseScale)
{ 
	scope("Model::Model");

	if (!serverSide)
	{
		error("Server-side constructor called from client possibly?");
		return;
	}

	debug("Loading model descriptor file: " + filePath);

	/*
		Blockland add-ons ship their models as .dts files, which say everything about themselves a
		descriptor file would, so one can be handed straight to newDynamicType or newItemType on its own.
		Reading it as a descriptor that only points at itself keeps the rest of this the same, and
		still lets a .txt point its file line at a .dts when it does want decalarea or material lines.
	*/
	std::istringstream selfDescriptor("file\t" + getFileFromPath(filePath) + "\n");

	std::ifstream descriptorFile;
	if (!isDtsPath(filePath))
		descriptorFile.open(contentPath(filePath).c_str());

	std::istream& descriptor = isDtsPath(filePath) ? (std::istream&)selfDescriptor : (std::istream&)descriptorFile;

	if (!isDtsPath(filePath) && !descriptorFile.is_open())
	{
		error("Could not open file " + filePath);
		return;
	}

	//What Assimp flags does the model creator wish for us to import their model with
	unsigned int desiredImporterFlags = 0;

	//Lower case names of meshes hide lines asked not to draw, see the client side constructor
	std::set<std::string> hiddenMeshes;

	//Lower case names of meshes the descriptor's fixedcolor lines keep from being painted, see Mesh::fixedColor
	std::set<std::string> fixedColorMeshes;

	//The relative file path to the actual 3d model file
	std::string modelPath = "";

	std::string line = "";
	while (!descriptor.eof())
	{
		getline(descriptor, line);

		//Every line is just two arguments separated by a tab

		if (line.length() < 1)
			continue;

		if (line.substr(0, 1) == "#")
			continue;

		size_t firstTab = line.find("\t");

		if (firstTab == std::string::npos)
		{
			error("Malformed model import text file: " + line);
			continue;
		}

		std::string argument = line.substr(0, firstTab);
		std::string value = line.substr(firstTab + 1, line.length() - (firstTab + 1)); //value needs to maintain case for flags

		//Getting a few non-importer-flags out of the way:
		if (argument == "file")
		{
			modelPath = getFolderFromPath(filePath) + value;
			continue;
		}

		if (argument == "singlematerial")
		{
			//Server-side doesn't need materials
			continue;
		}

		if (argument == "material")
		{
			//Server-side doesn't need materials
			continue;
		}

		if (argument == "decalarea")
		{
			//Server-side doesn't draw decals
			continue;
		}

		if (argument == "hide")
		{
			hiddenMeshes.insert(lowercase(value.substr(0, value.find_last_not_of(" \t\r\n") + 1)));
			continue;
		}

		//A mesh that keeps its material's look however the instance is colored, by name
		if (argument == "fixedcolor")
		{
			fixedColorMeshes.insert(lowercase(value.substr(0, value.find_last_not_of(" \t\r\n") + 1)));
			continue;
		}

		//How a model worn on another sits on it, see attachMesh
		if (parseAttachLine(argument, value))
			continue;

		auto flagSearchResult = aiProcessMap.find(argument);
		//It wasn't a valid assimp flag
		if (flagSearchResult == aiProcessMap.end())
		{
			error("Invalid process flag: " + argument);
			continue;
		}

		//To be fair an argument field for these isn't really required since they're false by default...
		if (lowercase(value) != "true")
			continue;

		//They specified an additional Assimp import flag to import their model with
		desiredImporterFlags += flagSearchResult->second;
	}

	descriptorFile.close();

	if (modelPath.length() < 1)
	{
		error("No 'file' line specified to point to the actual model file!");
		return;
	}

	debug("Loading model " + filePath + " with flags " + std::to_string(desiredImporterFlags));

	Assimp::Importer importer;
	const aiScene* scene = nullptr;

	/*
		Only matters to a descriptor asking for GenSmoothNormals (with DropNormals for a file that came with flat
		ones, like an .stl): faces meeting at a sharper angle than this keep their edge, so the dome of a hat is
		smooth while the edge of its brim stays an edge. Assimp's default of 175 degrees smooths everything
	*/
	importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

	/*
		A DTS shape is read by us rather than by Assimp, but comes out of it looking like anything
		else Assimp would have handed back, so everything below here treats the two the same
	*/
	std::unique_ptr<aiScene> dtsScene;
	std::vector<DtsSequence> dtsSequences;

	if (isDtsPath(modelPath))
	{
		dtsScene.reset(loadDtsScene(modelPath, &dtsSequences));
		scene = dtsScene.get();
	}
	else
		scene = importer.ReadFile(contentPath(modelPath), desiredImporterFlags);

	if (!scene)
	{
		error("Problem loaded model file " + modelPath);
		if (!isDtsPath(modelPath))
			error(importer.GetErrorString());
		return;
	}

	for (unsigned int a = 0; a < scene->mNumMeshes; a++)
	{
		aiMesh *src = scene->mMeshes[a];
		Mesh* tmp = new Mesh(src, this,serverSide);
		if (hiddenMeshes.count(lowercase(tmp->name)))
			tmp->nonRenderingMesh = true;
		if (fixedColorMeshes.count(lowercase(tmp->name)))
			tmp->fixedColor = true;
		tmp->meshIndex = allMeshes.size();
		allMeshes.push_back(tmp);
	}

	/*
		The server builds the node tree too, which costs it very little: it loads no animation channels,
		so every node here is just a name and the rest transform the file was exported with. That's what
		getNodePosition reads, which is how a script finds the spots an add-on's shape names, like where
		a jeep's wheels hang, see Lua's getTypeNodePosition
	*/
	nodeDefaultsAreRestPose = isDtsPath(modelPath);
	projectileForward = isDtsPath(modelPath) ? glm::vec3(0, 0, -1) : glm::vec3(0, 1, 0);
	rootNode = new Node(scene->mRootNode, this);

	calculateMeshBounds(scene);
	calculateCollisionBox(scene);

	/*
		A DTS shape brings its animations along with it, under whatever names the add-on gave them,
		so nothing has to add them by hand the way an addAnimation line does for an FBX model. Both
		sides load the same file in the same order, so the IDs they hand out line up.
	*/
	for (const DtsSequence& sequence : dtsSequences)
	{
		Animation animation;
		animation.name = sequence.name;
		animation.startTime = sequence.startTime;
		animation.endTime = sequence.endTime;
		animation.defaultSpeed = sequence.defaultSpeed;
		addAnimation(animation);
	}

	//The file was read, which is all a server needs of it. Every way out above this leaves valid false,
	//so isValid means the same thing here as it does on the client, see DynamicType::serverSideLoad
	valid = true;
}

//Full constructor for client-side loading, includes materials and animations
Model::Model(std::string filePath, std::shared_ptr<TextureManager> textures,glm::vec3 _baseScale) : loadedPath(filePath), serverSide(false), baseScale(_baseScale)
{
	scope("Model::Model");

	valid = false;

	debug("Loading model descriptor file: " + filePath);

	//A .dts is its own descriptor, see the note on this in the server-side constructor above
	std::istringstream selfDescriptor("file\t" + getFileFromPath(filePath) + "\n");

	std::ifstream descriptorFile;
	if (!isDtsPath(filePath))
		descriptorFile.open(contentPath(filePath).c_str());

	std::istream& descriptor = isDtsPath(filePath) ? (std::istream&)selfDescriptor : (std::istream&)descriptorFile;

	if (!isDtsPath(filePath) && !descriptorFile.is_open())
	{
		error("Could not open file " + filePath);
		return;
	}

	/*
		Allows us to force there to be just one material
		whose channels and texture paths we manually specify with included text file
		Empty string if not doing the override
	*/
	std::string overrideMaterialPath = "";

	//What Assimp flags does the model creator wish for us to import their model with
	unsigned int desiredImporterFlags = 0;
	
	//The relative file path to the actual 3d model file
	std::string modelPath = "";

	//First string is material name from model, second string is path to material descriptor text file
	std::map<std::string, std::string> materialOverrides;

	//Lower case mesh names and their decalarea lines, see Mesh::decalArea
	std::map<std::string, glm::vec4> decalAreas;

	/*
		Lower case names of meshes the descriptor's hide lines ask not to draw. They're loaded, just never
		drawn, like the Collision mesh, and a model with no Collision mesh is boxed around the rest of it,
		see calculateCollisionBox. For a part of a model that can't be drawn the way it was meant, like
		the see-through trail on a Blockland bullet
	*/
	std::set<std::string> hiddenMeshes;

	//Lower case names of meshes the descriptor's fixedcolor lines keep from being painted, see Mesh::fixedColor
	std::set<std::string> fixedColorMeshes;

	std::string line = "";
	while (!descriptor.eof())
	{
		getline(descriptor, line);

		//Every line is just two arguments separated by a tab

		if (line.length() < 1)
			continue;

		if (line.substr(0, 1) == "#")
			continue;

		size_t firstTab = line.find("\t");

		if (firstTab == std::string::npos)
		{
			error("Malformed model import text file: " + line);
			continue;
		}

		std::string argument = line.substr(0, firstTab);
		std::string value = line.substr(firstTab + 1, line.length() - (firstTab+1)); //value needs to maintain case for flags

		//Getting a few non-importer-flags out of the way:
		if (argument == "file")
		{
			modelPath = getFolderFromPath(filePath) + value;
			continue;
		}

		//Only use one material, whose textures we manually define in an external text file:
		if (argument == "singlematerial")
		{
			overrideMaterialPath = value;

			continue;
		}

		if (argument == "material")
		{
			//Okay actually this line has 3 tabs
			size_t secondTab = value.find("\t");

			if (secondTab == std::string::npos)
			{
				error("material line needs 2 arguments");
				continue;
			}

			std::string materialName = value.substr(0, secondTab);
			std::string materialPath = value.substr(secondTab + 1, value.length() - (secondTab + 1));

			materialOverrides.insert(std::pair<std::string,std::string>(materialName, materialPath));

			continue;
		}

		//A mesh not to draw, by name
		if (argument == "hide")
		{
			hiddenMeshes.insert(lowercase(value.substr(0, value.find_last_not_of(" \t\r\n") + 1)));
			continue;
		}

		//A mesh that keeps its material's look however the instance is colored, by name
		if (argument == "fixedcolor")
		{
			fixedColorMeshes.insert(lowercase(value.substr(0, value.find_last_not_of(" \t\r\n") + 1)));
			continue;
		}

		//Where decals go on a mesh: its name, then the texture coordinates of a decal's top left and bottom right corners
		if (argument == "decalarea")
		{
			std::istringstream values(value);
			std::string meshName;
			glm::vec4 area;
			if (!(values >> meshName >> area.x >> area.y >> area.z >> area.w) || area.x == area.z || area.y == area.w)
			{
				error("decalarea line needs a mesh name and two corners that aren't in line: " + value);
				continue;
			}

			decalAreas[lowercase(meshName)] = area;
			continue;
		}

		//How a model worn on another sits on it, see attachMesh
		if (parseAttachLine(argument, value))
			continue;

		auto flagSearchResult = aiProcessMap.find(argument);
		//It wasn't a valid assimp flag
		if (flagSearchResult == aiProcessMap.end())
		{
			error("Invalid process flag: " + argument);
			continue;
		}

		//To be fair an argument field for these isn't really required since they're false by default...
		if (lowercase(value) != "true")
			continue;

		//They specified an additional Assimp import flag to import their model with
		desiredImporterFlags += flagSearchResult->second;
	}

	descriptorFile.close();

	if (modelPath.length() < 1)
	{
		error("No 'file' line specified to point to the actual model file!");
		return;
	}

	debug("Loading model " + filePath + " with flags " + std::to_string(desiredImporterFlags));

	Assimp::Importer importer;
	const aiScene* scene = nullptr;

	/*
		Only matters to a descriptor asking for GenSmoothNormals (with DropNormals for a file that came with flat
		ones, like an .stl): faces meeting at a sharper angle than this keep their edge, so the dome of a hat is
		smooth while the edge of its brim stays an edge. Assimp's default of 175 degrees smooths everything
	*/
	importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

	//A DTS shape is read by us rather than by Assimp, see the same branch in the constructor above
	std::unique_ptr<aiScene> dtsScene;
	std::vector<DtsSequence> dtsSequences;

	if (isDtsPath(modelPath))
	{
		dtsScene.reset(loadDtsScene(modelPath, &dtsSequences));
		scene = dtsScene.get();
	}
	else
		scene = importer.ReadFile(contentPath(modelPath), desiredImporterFlags);

	if (!scene)
	{
		error("Problem loaded model file " + modelPath);
		if (!isDtsPath(modelPath))
			error(importer.GetErrorString());
		return;
	}

	//We're using singlematerial to have one material for the whole mesh!
	if(overrideMaterialPath.length() > 0)
	{
		Material* overrideMaterial = new Material(overrideMaterialPath, textures);
		if (!overrideMaterial->isValid())
			error("Material : " + overrideMaterial->getName() + " was not valid!");
		allMaterials.push_back(overrideMaterial);
	}
	else
	{
		std::string fileName = getFileFromPath(filePath);
		std::string folder = getFolderFromPath(filePath);

		debug("Expected materials: " + std::to_string(scene->mNumMaterials));

		for (unsigned int a = 0; a < scene->mNumMaterials; a++)
		{
			aiMaterial* src = scene->mMaterials[a];

			std::cout<<src->GetName().C_Str()<<std::endl;	

			auto matPair = materialOverrides.find(src->GetName().C_Str());
			if (matPair != materialOverrides.end())
			{
				//A specific material override was defined for this material
				debug("Using override for " + matPair->first);

				Material* overrideMaterial = new Material(folder + matPair->second, textures);
				if (!overrideMaterial->isValid())
					error("Material : " + overrideMaterial->getName() + " was not valid!");
				allMaterials.push_back(overrideMaterial);

				continue;
			}

			//Here we're going to load the textures that are embedded within the model file itself
			//Assimp categorizes its textures weird, and I wouldn't be surprised if it's not even consistant across files
			//aiTextureType_DIFFUSE   == our albedo
			//aiTextureType_NORMALS   == our normal map
			//aiTextureType_SHININESS == our roughness
			//aiTextureType_METALNESS == our metalness
			//Haven't found anything for ambient occlusion yet, but that may be because FBX files just don't use it

			int numDiffuse = src->GetTextureCount(aiTextureType_DIFFUSE);
			int numNormal = src->GetTextureCount(aiTextureType_NORMALS);
			int numRough = src->GetTextureCount(aiTextureType_SHININESS);
			int numMetal = src->GetTextureCount(aiTextureType_METALNESS);

			aiString albedo;
			aiString normal;
			aiString rough;
			aiString metal;
			
			if (numDiffuse == 1)
				src->GetTexture(aiTextureType_DIFFUSE, 0, &albedo);
			else if (numDiffuse > 1)
				error("More than one diffuse texture for material " + std::string(src->GetName().C_Str()));

			if (numNormal == 1)
				src->GetTexture(aiTextureType_NORMALS, 0, &normal);
			else if (numNormal > 1)
				error("More than one normal texture for material " + std::string(src->GetName().C_Str()));

			if (numRough == 1)
				src->GetTexture(aiTextureType_SHININESS, 0, &rough);
			else if (numRough > 1)
				error("More than one roughness texture for material " + std::string(src->GetName().C_Str()));

			if (numMetal == 1)
				src->GetTexture(aiTextureType_METALNESS, 0, &metal);
			else if (numMetal > 1)
				error("More than one metalness texture for material " + std::string(src->GetName().C_Str()));

			std::string albedoPath = albedo.length > 0 ? folder + albedo.C_Str() : "";
			std::string normalPath = normal.length > 0 ? folder + normal.C_Str() : "";
			std::string roughPath  = rough.length  > 0 ? folder + rough.C_Str()  : "";
			std::string metalPath  = metal.length  > 0 ? folder + metal.C_Str()  : "";

			if (albedoPath.length() < 1 && normalPath.length() < 1)
			{
				error("Neither albedo for normal map texture specified for material " + std::string(src->GetName().C_Str()));
				continue;
			}

			debug("Loading material " + std::string(src->GetName().C_Str()));

			/*
				Materials are kept by name, and a DTS one is named by the add-on that wrote it, which
				means names as common as "black" or "white". Putting its folder in front keeps two
				add-ons that both have a "black" from being handed each other's texture. The override
				lookup above still goes by the plain name the shape uses.
			*/
			std::string materialName = isDtsPath(modelPath)
				? folder + std::string(src->GetName().C_Str())
				: std::string(src->GetName().C_Str());

			/*
				A DTS material is only a name with a flat texture under it, and the ones add-ons use for
				shades of grey are all pure black differing only in how see-through they are, so its
				alpha has to be read rather than dropped, see Texture::addLayer
			*/
			/*
				A Blockland shape's textures are nearly all a single flat colour, since Torque wanted a file
				where a number would have done. Read as a colour instead of loaded as a texture, which costs
				nothing to draw and, more to the point, leaves the material's texture array free: every layer
				of one has to be the same size, so a 16x16 flat colour would stop any real map joining it
			*/
			glm::vec4 flatAlbedo(1.0f);
			bool albedoIsFlat = isDtsPath(modelPath) && !albedoPath.empty() && normalPath.empty()
				&& textures->flatColor(albedoPath, true, flatAlbedo);

			/*
				Which lets a shape that brought no maps of its own be drawn as the same scuffed plastic the
				Brickhead is, rather than as a flat colour. A perfectly flat colour with a perfectly even
				normal turns every big panel into one smooth specular gradient, a highlight the size of the
				panel, however rough it's set; what makes the Brickhead read as plastic is its normal map
				breaking that up into grain, so that is what these get, with a roughness kept matte and a
				gentle occlusion. The shape's own texture coordinates are sparse, so the grain comes out
				coarse, which is still a long way better than a mirror. A .txt descriptor naming any of
				these, by file or by number, still wins
			*/
			bool scuffed = albedoIsFlat && roughPath.empty() && metalPath.empty();
			if (scuffed)
			{
				albedoPath = "";
				normalPath = dtsDefaultNormal;
				roughPath = dtsDefaultRoughness;
			}

			Material* tmp = new Material(materialName,albedoPath,normalPath,roughPath,metalPath,
				scuffed ? dtsDefaultOcclusion : "",textures, isDtsPath(modelPath));

			if (albedoIsFlat)
				tmp->constantAlbedo = flatAlbedo;

			allMaterials.push_back(tmp);

			if (!tmp->isValid())
				error("Material : " + tmp->getName() + " was not valid!");
		}
	}

	debug(std::to_string(allMaterials.size()) + " materials loaded");

	for (unsigned int a = 0; a < scene->mNumMeshes; a++)
	{
		aiMesh* src = scene->mMeshes[a];
		Mesh* tmp = new Mesh(src, this);

		if (hiddenMeshes.count(lowercase(tmp->name)))
		{
			tmp->nonRenderingMesh = true;
			hiddenMeshes.erase(lowercase(tmp->name));
		}

		if (fixedColorMeshes.count(lowercase(tmp->name)))
		{
			tmp->fixedColor = true;
			fixedColorMeshes.erase(lowercase(tmp->name));
		}

		tmp->meshIndex = allMeshes.size();
		allMeshes.push_back(tmp);

		auto area = decalAreas.find(lowercase(tmp->name));
		if (area != decalAreas.end())
		{
			tmp->decalArea = area->second;
			decalAreas.erase(area);
		}
	}

	for (const auto& [meshName, area] : decalAreas)
		error("decalarea line for " + meshName + " but " + filePath + " has no mesh by that name");

	for (const std::string& meshName : hiddenMeshes)
		error("hide line for " + meshName + " but " + filePath + " has no mesh by that name");

	for (const std::string& meshName : fixedColorMeshes)
		error("fixedcolor line for " + meshName + " but " + filePath + " has no mesh by that name");

	//See the note on this member, it has to be known before any Node is constructed
	nodeDefaultsAreRestPose = isDtsPath(modelPath);
	projectileForward = isDtsPath(modelPath) ? glm::vec3(0, 0, -1) : glm::vec3(0, 1, 0);

	rootNode = new Node(scene->mRootNode, this);

	if (scene->mNumAnimations == 1)
	{
		aiAnimation* anim = scene->mAnimations[0];

		//For each 'channel' aka node this animation affects
		for (unsigned int a = 0; a < anim->mNumChannels; a++)
		{
			//Find the node by name
			//Keeping in mind we've collapsed the hierarchy ourselves quite a bit
			aiNodeAnim* nodeAnim = anim->mChannels[a];
			std::string channelName = nodeAnim->mNodeName.C_Str();
			std::string targetName = stripSillyAssimpNodeNames(channelName);

			//Assimp splits a pivoted FBX node's keys into Translation, Rotation, and Scaling channels, each with a filler key for the parts it isn't
			//Folded into one node those filler keys would land among the real ones, so only take the part each channel is named for
			static const std::string assimpMarker = "_$AssimpFbx$_";
			size_t markerPos = channelName.find(assimpMarker);
			std::string part = markerPos == std::string::npos ? "" : channelName.substr(markerPos + assimpMarker.length());
			bool usePositions = part.empty() || part == "Translation";
			bool useRotations = part.empty() || part == "Rotation";
			if (!usePositions && !useRotations)
				continue;
			Node* target = nullptr;
			for (unsigned int b = 0; b < allNodes.size(); b++)
			{
				if (allNodes[b]->name == targetName)
				{
					target = allNodes[b];
					break;
				}
			}

			if (!target)
			{
				error("Animation target node " + targetName + " could not be found");
				continue;
			}

			//Copy position keys for all possible animations to the given node
			for (unsigned int b = 0; usePositions && b < nodeAnim->mNumPositionKeys; b++)
			{
				glm::vec3 pos(nodeAnim->mPositionKeys[b].mValue.x, nodeAnim->mPositionKeys[b].mValue.y, nodeAnim->mPositionKeys[b].mValue.z);

				//TODO: Actually applying scaling keys and letting baseScale cancel it out would be the right way to do it
				if (nodeAnim->mNumScalingKeys > b)
				{
					glm::vec3 scale(nodeAnim->mScalingKeys[b].mValue.x, nodeAnim->mScalingKeys[b].mValue.y, nodeAnim->mScalingKeys[b].mValue.z);
					pos /= scale;
				}

				target->posFrames.push_back(pos);
				target->posTimes.push_back((float)nodeAnim->mPositionKeys[b].mTime);
			}

			//Copy rotation keys for all possible animations to the given node
			for (unsigned int b = 0; useRotations && b < nodeAnim->mNumRotationKeys; b++)
			{
				glm::quat rot(nodeAnim->mRotationKeys[b].mValue.w, nodeAnim->mRotationKeys[b].mValue.x, nodeAnim->mRotationKeys[b].mValue.y, nodeAnim->mRotationKeys[b].mValue.z);

				target->rotFrames.push_back(rot);
				target->rotTimes.push_back((float)nodeAnim->mRotationKeys[b].mTime);
			}
		}
	}
	else if (scene->mNumAnimations > 1)
	{
		error("More than one animation imported by Assimp for model " + filePath);
		error("For now animations should all be on the same track, but LoD animations are then");
		error("defined in script as being certain time slices of the original animation track.");
	}

	for (Node* node : allNodes)
	{
		node->fixPivot = node->rotationPivot;

		if (headNodeIdx == -1 && lowercase(node->name) == "head")
		{
			headNodeIdx = (int)node->nodeIndex;

			//The bottom middle of the head, where it sits on the neck, its vertices are in the same space as the pivot
			for (const Mesh* mesh : node->meshes)
			{
				if (lowercase(mesh->name) == "head")
				{
					node->fixPivot = glm::vec3(mesh->center.x, mesh->low.y, mesh->center.z);
					break;
				}
			}
		}
	}

	calculateMeshBounds(scene);
	calculateCollisionBox(scene);

	/*
		A DTS shape brings its animations along with it, under whatever names the add-on gave them,
		so nothing has to add them by hand the way an addAnimation line does for an FBX model. Both
		sides load the same file in the same order, so the IDs they hand out line up.
	*/
	for (const DtsSequence& sequence : dtsSequences)
	{
		Animation animation;
		animation.name = sequence.name;
		animation.startTime = sequence.startTime;
		animation.endTime = sequence.endTime;
		animation.defaultSpeed = sequence.defaultSpeed;
		addAnimation(animation);
	}

	valid = true;
}

void Mesh::recompileInstances(const glm::vec3& cullFrom, float cullDistance)
{
	if (nonRenderingMesh)
		return;

	bool cullFar = cullDistance > 0;
	float cullDistance2 = cullDistance * cullDistance;

	std::vector<glm::mat4> transforms;
	std::vector<unsigned int> flags;
	std::vector<glm::vec4> colors;
	std::vector<glm::vec4> highlightColors;
	std::vector<float> highlightThicknesses;

	for (unsigned int a = 0; a < instances.size(); a++)
	{
		//A zero matrix puts every corner of the primitive on the same point, which discards it, see setHidden
		glm::mat4 meshTransform(0.0);
		if (!instances[a]->hidden || instances[a]->hiddenCastsShadow)
		{
			meshTransform = instances[a]->wholeModelTransform * instances[a]->MeshTransforms[meshIndex];

			//Past the draw distance it's dropped the same way, which takes it out of the shadow passes too
			if (cullFar)
			{
				glm::vec3 away = glm::vec3(meshTransform[3]) - cullFrom;
				if (glm::dot(away, away) > cullDistance2)
					meshTransform = glm::mat4(0.0);
			}
		}

		transforms.push_back(meshTransform);
		flags.push_back(instances[a]->MeshFlags[meshIndex] | (instances[a]->hidden ? ModelInstance::hiddenCastingShadowFlag : 0));
		colors.push_back(instances[a]->MeshColors[meshIndex]);
		highlightColors.push_back(instances[a]->highlightColor);
		highlightThicknesses.push_back(instances[a]->highlightThickness);
	}

	if (transforms.size() == 0)
		return;

	//Allocated buffer space used up
	//Allocate at least one more 'page' of instances in this meshes buffers
	if (transforms.size() >= instancesAllocated)
	{
		size_t preExpansionSize = transforms.size();
		//Round up to the next multiple of page size
		instancesAllocated = (unsigned int)(std::ceil(((float)preExpansionSize) / ((float)InstanceBufferPageSize)) * InstanceBufferPageSize);

		//Dummy data for unused part of last page
		for (size_t a = preExpansionSize; a < instancesAllocated; a++)
		{
			transforms.push_back(glm::mat4(1.0));
			flags.push_back(0);
			colors.push_back(glm::vec4(0, 0, 0, 0));
			highlightColors.push_back(glm::vec4(0, 0, 0, 0));
			highlightThicknesses.push_back(0);
		}

		glBindBuffer(GL_ARRAY_BUFFER, buffers[ModelTransform]);
		glBufferData(GL_ARRAY_BUFFER,sizeof(glm::mat4) * instancesAllocated, &transforms[0][0][0],GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[InstanceFlags]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int) * instancesAllocated, &flags[0], GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[PreColor]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * instancesAllocated, &colors[0][0], GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightColor]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * instancesAllocated, &highlightColors[0][0], GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightThickness]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * instancesAllocated, &highlightThicknesses[0], GL_DYNAMIC_DRAW);

		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, buffers[ModelTransform]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * transforms.size(), &transforms[0][0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[InstanceFlags]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(int) * flags.size(), &flags[0]);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[PreColor]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4) * colors.size(), &colors[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightColor]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4) * highlightColors.size(), &highlightColors[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[HighlightThickness]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * highlightThicknesses.size(), &highlightThicknesses[0]);
}

void Model::updateAll(float deltaT, const glm::vec3& cullFrom, float cullDistance)
{
	for (unsigned int a = 0; a < instances.size(); a++)
		instances[a]->calculateMeshTransforms(deltaT);

	/*
		The cull measures to each mesh's own origin, so the radius is opened up by however far the model reaches
		away from there. Without that a big model, or one whose meshes sit well off their origin, would wink out
		while part of it was still inside the draw distance
	*/
	if (cullDistance > 0)
	{
		glm::vec3 low, high;
		if (getDrawnBounds(low, high))
		{
			float scale = std::max(std::max(std::abs(baseScale.x), std::abs(baseScale.y)), std::abs(baseScale.z));
			cullDistance += glm::length(high - low) * 0.5f * scale;
		}
	}

	for (unsigned int a = 0; a < allMeshes.size(); a++)
		allMeshes[a]->recompileInstances(cullFrom, cullDistance);
}

void Model::render(std::shared_ptr<ShaderManager> graphics,bool useMaterials) const
{
	for (unsigned int a = 0; a < allMeshes.size(); a++)
		allMeshes[a]->render(graphics, useMaterials);
}

void Model::renderMesh(std::shared_ptr<ShaderManager> graphics, int meshIdx) const
{
	if (meshIdx >= 0 && meshIdx < (int)allMeshes.size())
		allMeshes[meshIdx]->render(graphics);
}

void Model::renderMeshOnce(std::shared_ptr<ShaderManager> graphics, int meshIdx) const
{
	if (meshIdx >= 0 && meshIdx < (int)allMeshes.size())
		allMeshes[meshIdx]->renderOnce(graphics);
}

bool Model::getDrawnBounds(glm::vec3& low, glm::vec3& high) const
{
	bool any = false;

	for (unsigned int a = 0; a < allMeshes.size(); a++)
	{
		if (allMeshes[a]->nonRenderingMesh || !allMeshes[a]->hasBounds())
			continue;

		low = any ? glm::min(low, allMeshes[a]->boundsLow) : allMeshes[a]->boundsLow;
		high = any ? glm::max(high, allMeshes[a]->boundsHigh) : allMeshes[a]->boundsHigh;
		any = true;
	}

	if (!any)
		return false;

	//A negative scale on an axis would turn that side of the box inside out
	glm::vec3 scaledLow = low * baseScale;
	glm::vec3 scaledHigh = high * baseScale;
	low = glm::min(scaledLow, scaledHigh);
	high = glm::max(scaledLow, scaledHigh);
	return true;
}

bool Model::getMeshBounds(const std::string& meshName, glm::vec3& low, glm::vec3& high) const
{
	int meshIdx = getMeshIdxIgnoringCase(meshName);
	if (meshIdx == -1 || !allMeshes[meshIdx]->hasBounds())
		return false;

	//A negative scale on an axis would turn that side of the box inside out
	glm::vec3 scaledLow = allMeshes[meshIdx]->boundsLow * baseScale;
	glm::vec3 scaledHigh = allMeshes[meshIdx]->boundsHigh * baseScale;
	low = glm::min(scaledLow, scaledHigh);
	high = glm::max(scaledLow, scaledHigh);
	return true;
}

bool Model::parseAttachLine(const std::string& argument, const std::string& value)
{
	std::istringstream values(value);

	if (argument == "attach")
	{
		std::string meshName;
		values >> meshName;
		attachMesh = lowercase(meshName);
		return true;
	}

	if (argument == "attachoffset")
	{
		glm::vec3 offset;
		if (values >> offset.x >> offset.y >> offset.z)
			attachOffset = offset;
		else
			error("attachoffset line needs three numbers: " + value);
		return true;
	}

	if (argument == "attachrotation")
	{
		glm::vec3 degrees;
		if (values >> degrees.x >> degrees.y >> degrees.z)
			attachRotation = glm::quat(glm::radians(degrees));
		else
			error("attachrotation line needs three angles in degrees: " + value);
		return true;
	}

	if (argument == "attachscale")
	{
		float scale = 0;
		if (values >> scale && scale > 0)
			baseScale = glm::vec3(scale);
		else
			error("attachscale line needs one number above zero: " + value);
		return true;
	}

	return false;
}

glm::mat4 Model::getAttachTransform(float scale) const
{
	glm::mat4 turn = glm::toMat4(attachRotation);

	//Its lowest point once turned is what sits on the mesh it's worn on
	float bottom = 0;
	glm::vec3 low, high;
	if (getDrawnBounds(low, high))
	{
		bottom = std::numeric_limits<float>::max();
		for (int corner = 0; corner < 8; corner++)
		{
			glm::vec3 point((corner & 1) ? high.x : low.x, (corner & 2) ? high.y : low.y, (corner & 4) ? high.z : low.z);
			bottom = std::min(bottom, (turn * glm::vec4(point, 1.0f)).y);
		}
	}

	return glm::translate((attachOffset - glm::vec3(0, bottom, 0)) * scale) * turn * glm::scale(glm::vec3(scale));
}

bool ModelInstance::placeAttachment(ModelInstance* part, float scale) const
{
	if (!part || !part->type || !type)
		return false;

	int meshIdx = type->getMeshIdxIgnoringCase(part->type->attachMesh);
	if (meshIdx == -1 || part->type->allMeshes.empty())
	{
		if (!part->hidden)
			part->setHidden(true);
		return false;
	}

	//The top middle of the mesh in its own space, the same space its vertices are in
	const Mesh* mesh = type->allMeshes[meshIdx];
	glm::vec3 top(mesh->center.x, mesh->center.y * 2.0f - mesh->low.y, mesh->center.z);

	part->setModelTransform(getMeshWorldTransform(meshIdx) * glm::translate(top) * part->type->getAttachTransform(scale));

	if (part->hidden != hidden || part->hiddenCastsShadow != hiddenCastsShadow)
		part->setHidden(hidden, hiddenCastsShadow);

	return true;
}

void Model::renderSingleInstance(unsigned int bufferOffset) const
{
	for (unsigned int a = 0; a < allMeshes.size(); a++)
		allMeshes[a]->renderSingleInstance(bufferOffset);
}

Model::~Model()
{
	//Deletion is recursive, trying to do it through allNodes would cause a crash
	if(rootNode)
		delete rootNode;

	for (unsigned int a = 0; a < allMeshes.size(); a++)
		delete allMeshes[a];

	for (unsigned int a = 0; a < allMaterials.size(); a++)
		delete allMaterials[a]; //Calls markForCleanup on associated textures
}

Node::Node(aiNode const* const src, Model * parent)
{
	nodeIndex = parent->allNodes.size();
	parent->allNodes.push_back(this);
	name = src->mName.C_Str();

	if (name.find("_RotationPivot") != std::string::npos)
	{
		parent->rotationPivotsApplied = true;
		glm::mat4 to;
		CopyaiMat(src->mTransformation, to);
		rotationPivot = getTransformFromMatrix(to);
	}

	name = stripSillyAssimpNodeNames(name);

	//What is this node's default state before any animations
	CopyaiMat(src->mTransformation, defaultTransform);

	/*
		The same thing split into a position and a rotation, which is what ModelInstance::calculateNodeTransform
		rebuilds a node from once it has any animation keys at all. Left at zero and identity, any node an
		animation touches snaps to its parent's origin the moment one plays, or as one fades, which a DTS
		shape's slides and bolts would do because it bakes each part's rest position into its node.

		Only worth taking where the file means it as a rest pose, see Model::nodeDefaultsAreRestPose: Assimp
		hands back an FBX node as the artist left it, which for Brickhead's right arm is a frame of the grab.
		setDefaultFrame overwrites both of these when it's used.
	*/
	if (parent->nodeDefaultsAreRestPose)
	{
		defaultPos = getTransformFromMatrix(defaultTransform);
		defaultRot = getRotationFromMatrix(defaultTransform);
	}

	//Assimp gives us meshes as indicies to an array of meshes loaded earlier
	for (unsigned int a = 0; a < src->mNumMeshes; a++)
		meshes.push_back(parent->allMeshes[src->mMeshes[a]]);

	//Recursivly collapse the bloated node hierarchy Assimp gives us
	for (unsigned int a = 0; a < src->mNumChildren; a++)
	{
		//Is a child node one of the nodes Assimp split a single original node into during importing
		if (stripSillyAssimpNodeNames(src->mChildren[a]->mName.C_Str()) == name)
			foldNodeInto(src->mChildren[a], parent);					//Collapse it into this node
		else
			children.push_back(new Node(src->mChildren[a], parent));	//Allow it to be its own node
	}
}

void Node::foldNodeInto(aiNode const* const src, Model* parent)
{
	//Just in case the rotation pivot is not the first of the node's assimp separates an origional node into
	std::string tmpname = src->mName.C_Str();
	if (tmpname.find("_RotationPivot") != std::string::npos)
	{
		parent->rotationPivotsApplied = true;
		glm::mat4 to;
		CopyaiMat(src->mTransformation, to);
		rotationPivot = getTransformFromMatrix(to);
	}

	//If assimp splits a single origonal node into many smaller nodes for each part of the model matrix the org. node had
	//*Probably* the last one will have all of the meshes for the original node
	//Make sure every mesh that was assigned to the original node is assigned to our reconstructed node
	for (unsigned int a = 0; a < src->mNumMeshes; a++)
	{
		//Do we already have this mesh in our node?
		std::vector<Mesh*>::iterator pos = std::find(
			meshes.begin(), 
			meshes.end(), 
			parent->allMeshes[src->mMeshes[a]]);

		//If not, add it
		if (pos == meshes.end())
			meshes.push_back(parent->allMeshes[src->mMeshes[a]]);
	}

	//Continue the folding process recursivly 
	for (unsigned int a = 0; a < src->mNumChildren; a++)
	{
		//Is a child node one of the nodes Assimp split a single original node into during importing
		if (stripSillyAssimpNodeNames(src->mChildren[a]->mName.C_Str()) == name)
			foldNodeInto(src->mChildren[a], parent);					//Collapse it into this node
		else
			children.push_back(new Node(src->mChildren[a], parent));	//Allow it to be its own node
	}
}

Node::~Node()
{
	//Recursive!
	for (unsigned int a = 0; a < children.size(); a++)
		delete children[a];
}

#include "Mesh.h"

#include <tuple>

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


void Model::addAnimation(Animation& animation,int id)
{
	animation.serverID = (id < 0) ? animations.size() : id;
	animations.push_back(std::move(animation));
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

void ModelInstance::setHidden(bool _hidden)
{
	hidden = _hidden;
	wholeModelTransformUpdated = true;
}

void ModelInstance::setModelTransform(glm::mat4 &&transform)
{
	//Used to update transformUpdated and anythingUpdated, but now wholeModelTransform is exempt
	//wholeModelTransform updates apply during any compile call
	wholeModelTransformUpdated = true;
	wholeModelTransform = transform;
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

void Node::getFrame(const AnimationPlayback& anim, glm::vec3& pos, glm::mat4& rot) const
{
	//Get the key frame after and before our current time
	for (int a = 1; a < (int)posFrames.size(); a++)
	{
		//Find the first frame with a time greater than our own
		if (posTimes[a] >= anim.animationTime)
		{
			float nextTime = posTimes[a];
			glm::vec3 nextFrame = posFrames[a];

			int prev = a - 1; //Just needed to suppress a warning, really
			if (prev < 0)
				break;

			//Figure out what the previous frame is...
			//We start by assuming it was literally the previous frame, but...
			float prevTime = posTimes[prev];
			glm::vec3 prevFrame = posFrames[prev];

			//How far are we between frames: for interpolation
			float totalTimeBetween = nextTime - prevTime;
			float timeProgress = anim.animationTime - prevTime;

			pos += anim.animationFadeOut * lerp(prevFrame, nextFrame, timeProgress / totalTimeBetween);

			break;
		}
	}

	//Get the key frame after and before our current time
	for (int a = 1; a < (int)rotFrames.size(); a++)
	{
		//Find the first frame with a time greater than our own
		if (rotTimes[a] >= anim.animationTime)
		{
			float nextTime = rotTimes[a];
			glm::quat nextFrame = rotFrames[a];

			int prev = a - 1; //Just needed to suppress a warning, really
			if (prev < 0)
				break;

			//Figure out what the previous frame is...
			//We start by assuming it was literally the previous frame, but...
			float prevTime = rotTimes[prev];
			glm::quat prevFrame = rotFrames[prev];

			//How far are we between frames: for interpolation
			float totalTimeBetween = nextTime - prevTime;
			float timeProgress = anim.animationTime - prevTime;

			glm::quat animationRotation = glm::slerp(prevFrame, nextFrame, timeProgress / totalTimeBetween);
			rot = glm::toMat4(glm::slerp(glm::quat(1,0,0,0), animationRotation, anim.animationFadeOut)) * rot;

			break;
		}
	}
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
	if (node->posFrames.size() < 1 || node->rotFrames.size() < 1)
	{
		ret = node->defaultTransform;
	}
	else
	{
		glm::vec3 pos = glm::vec3(0, 0, 0);
		glm::mat4 rot = glm::mat4(1.0);

		//There's one or more playing animations
		if (playingAnimations.size() > 0)
		{
			for (unsigned int a = 0; a < playingAnimations.size(); a++)
				node->getFrame(playingAnimations[a], pos, rot);
		}
		//There are animations but we aren't playing any, use default frame
		else
		{
			pos = node->defaultPos;
			rot = glm::toMat4(node->defaultRot);
		}

		if(type->rotationPivotsApplied)
			ret = glm::translate(node->rotationPivot) * rot * glm::translate(-node->rotationPivot) * glm::translate(pos);
		else
			ret = glm::translate(pos) * rot;
	}

	//Apply node rotation fixes, for example the tilt of a player's head when they look up or down
	if (UseNodeRotationFix[node->nodeIndex])
	{
		ret = ret * glm::translate(node->rotationPivot) * glm::toMat4(NodeRotationFixes[node->nodeIndex]) * glm::translate(-node->rotationPivot);
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
		if (currentNode->meshes[a]->nonRenderingMesh)
			continue;

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

		if (hidden)
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
			glBindBuffer(GL_ARRAY_BUFFER, type->allMeshes[a]->buffers[InstanceFlags]);
			glBufferSubData(GL_ARRAY_BUFFER, sizeof(int) * bufferOffset, sizeof(int), &MeshFlags[a]);

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

	playingAnimations.push_back(std::move(play));
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

	if (useMaterials && material)
		material->use(graphics);

	glBindVertexArray(vao);
	glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, (void*)0, (GLsizei)instances.size());
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

void Model::calculateCollisionBox(const aiScene* scene)
{
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
		//No collision mesh, give it a tiny box by default
		error("No collision mesh found.");
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

	std::ifstream descriptorFile(filePath.c_str());

	if (!descriptorFile.is_open())
	{
		error("Could not open file " + filePath);
		return;
	}

	//What Assimp flags does the model creator wish for us to import their model with
	unsigned int desiredImporterFlags = 0;

	//The relative file path to the actual 3d model file
	std::string modelPath = "";

	std::string line = "";
	while (!descriptorFile.eof())
	{
		getline(descriptorFile, line);

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
	const aiScene* scene = importer.ReadFile(modelPath, desiredImporterFlags);

	if (!scene)
	{
		error("Problem loaded model file " + modelPath);
		error(importer.GetErrorString());
		return;
	}

	for (unsigned int a = 0; a < scene->mNumMeshes; a++)
	{
		aiMesh *src = scene->mMeshes[a];
		Mesh* tmp = new Mesh(src, this,serverSide);
		tmp->meshIndex = allMeshes.size();
		allMeshes.push_back(tmp);
	}

	//rootNode = new Node(scene->mRootNode, this);*/

	calculateCollisionBox(scene);
}

//Full constructor for client-side loading, includes materials and animations
Model::Model(std::string filePath, std::shared_ptr<TextureManager> textures,glm::vec3 _baseScale) : loadedPath(filePath), serverSide(false), baseScale(_baseScale)
{
	scope("Model::Model");

	valid = false;

	debug("Loading model descriptor file: " + filePath);

	std::ifstream descriptorFile(filePath.c_str());

	if (!descriptorFile.is_open())
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

	std::string line = "";
	while (!descriptorFile.eof())
	{
		getline(descriptorFile, line);

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
	const aiScene* scene = importer.ReadFile(modelPath, desiredImporterFlags);

	if (!scene)
	{
		error("Problem loaded model file " + modelPath);
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
			Material* tmp = new Material(std::string(src->GetName().C_Str()),albedoPath,normalPath,roughPath,metalPath, "",textures);
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

		tmp->meshIndex = allMeshes.size();
		allMeshes.push_back(tmp);
	}

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
			std::string targetName = stripSillyAssimpNodeNames(nodeAnim->mNodeName.C_Str());
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
			for (unsigned int b = 0; b < nodeAnim->mNumPositionKeys; b++)
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
			for (unsigned int b = 0; b < nodeAnim->mNumRotationKeys; b++)
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

	calculateCollisionBox(scene);

	valid = true;
}

void Mesh::recompileInstances()
{
	if (nonRenderingMesh)
		return;

	std::vector<glm::mat4> transforms;
	std::vector<unsigned int> flags;
	std::vector<glm::vec4> colors;
	std::vector<glm::vec4> highlightColors;
	std::vector<float> highlightThicknesses;

	for (unsigned int a = 0; a < instances.size(); a++)
	{
		if (instances[a]->hidden)
			transforms.push_back(glm::mat4(0.0));
		else
			transforms.push_back(instances[a]->wholeModelTransform * instances[a]->MeshTransforms[meshIndex]);
		flags.push_back(instances[a]->MeshFlags[meshIndex]);
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

void Model::updateAll(float deltaT)
{
	for (unsigned int a = 0; a < instances.size(); a++)
		instances[a]->calculateMeshTransforms(deltaT);
	for (unsigned int a = 0; a < allMeshes.size(); a++)
		allMeshes[a]->recompileInstances();
}

void Model::render(std::shared_ptr<ShaderManager> graphics,bool useMaterials) const
{
	for (unsigned int a = 0; a < allMeshes.size(); a++)
		allMeshes[a]->render(graphics, useMaterials);
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

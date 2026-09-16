#include "InstancedBrickRenderer.h"

#include "../Utility/GlobalStartup.h"

#include <array>
#include <cfloat>

//Position, normal, tangent, bitangent, uv
static constexpr int cubeVertexFloats = 14;
//Min corner, size, color, material
static constexpr int instanceFloats = 11;
//Min corner, size, color, material, quarter turns, print decal layer
static constexpr int specialInstanceFloats = 13;
//In studs horizontally and plates vertically
static constexpr int chunkSize = 64;

//How far shape effects move bricks past their grid box, the same as brick.vert, so chunks aren't culled while bricks still show
static constexpr float unduloAmplitude = 0.3f;
static constexpr float bouncyStretch = 0.35f;

//Where each face group sits in the shared cube, see makeCube
static constexpr GLint topFirst = 0;
static constexpr GLsizei topCount = 6;
static constexpr GLint bottomFirst = 6;
static constexpr GLsizei bottomCount = 6;
static constexpr GLint sidesFirst = 12;
static constexpr GLsizei sidesCount = 24;

static const glm::vec3 gridScale = glm::vec3(STUD_SIZE, PLATE_SIZE, STUD_SIZE);

static int floorDiv(int value, int divisor)
{
	return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

//A brick belongs to the chunk holding its min corner, it never moves so that never changes
static int64_t chunkKey(const Brick* brick)
{
	int64_t x = floorDiv(brick->x, chunkSize) & 0x1FFFFF;
	int64_t y = floorDiv(brick->y, chunkSize) & 0x1FFFFF;
	int64_t z = floorDiv(brick->z, chunkSize) & 0x1FFFFF;
	return (x << 42) | (y << 21) | z;
}

static glm::vec3 brickSize(const Brick& brick)
{
	return glm::vec3(brick.footprintWidth(), brick.height, brick.footprintLength());
}

//Corner in studs/plates, relative to wherever the brick transform puts it
static void appendInstance(std::vector<float>& instances, const glm::vec3& corner, const Brick& brick, float alpha)
{
	glm::vec3 size = brickSize(brick);
	glm::vec3 color = glm::vec3(brick.color) / 255.0f;
	instances.insert(instances.end(), { corner.x, corner.y, corner.z, size.x, size.y, size.z, color.r, color.g, color.b, alpha, (float)brick.material });
}

//printLayer is which layer of the decal array holds the brick's print, or -1 for a brick with none
static void appendSpecialInstance(std::vector<float>& instances, const glm::vec3& corner, const Brick& brick, float alpha, int printLayer)
{
	appendInstance(instances, corner, brick, alpha);
	instances.push_back((float)brick.angleID);
	instances.push_back((float)printLayer);
}

static std::vector<float> makeCube()
{
	struct Face
	{
		glm::vec3 origin;
		glm::vec3 u;
		glm::vec3 v;
	};

	//Top, bottom, then the 4 sides, matching the draw ranges above
	//u cross v points outward, so every face winds counter-clockwise, and u/v double as tangent/bitangent
	const Face faces[6] =
	{
		{ glm::vec3(0, 1, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1) },
		{ glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1) },
		{ glm::vec3(1, 0, 1), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0) },
		{ glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0) },
		{ glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0) },
		{ glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0) }
	};

	const glm::vec2 corners[6] = { glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(0, 1) };

	std::vector<float> data;
	for (const Face& face : faces)
	{
		glm::vec3 normal = glm::cross(face.u, face.v);
		for (const glm::vec2& uv : corners)
		{
			glm::vec3 position = face.origin + face.u * uv.x + face.v * uv.y;
			data.insert(data.end(), { position.x, position.y, position.z, normal.x, normal.y, normal.z,
				face.u.x, face.u.y, face.u.z, face.v.x, face.v.y, face.v.z, uv.x, uv.y });
		}
	}

	return data;
}

//Gribb-Hartmann: each plane is a sum or difference of the matrix's last row with one of the others
static std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& viewProjection)
{
	glm::vec4 rows[4];
	for (int row = 0; row < 4; row++)
		rows[row] = glm::vec4(viewProjection[0][row], viewProjection[1][row], viewProjection[2][row], viewProjection[3][row]);

	return { rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1], rows[3] - rows[1], rows[3] + rows[2], rows[3] - rows[2] };
}

static bool boxVisible(const std::array<glm::vec4, 6>& planes, const glm::vec3& min, const glm::vec3& max)
{
	for (const glm::vec4& plane : planes)
	{
		glm::vec3 farthest(plane.x > 0 ? max.x : min.x, plane.y > 0 ? max.y : min.y, plane.z > 0 ? max.z : min.z);
		if (glm::dot(glm::vec3(plane), farthest) + plane.w < 0)
			return false;
	}
	return true;
}

//World bounds of a box after a transform, from its 8 corners
static void transformBounds(const glm::mat4& transform, const glm::vec3& min, const glm::vec3& max, glm::vec3& outMin, glm::vec3& outMax)
{
	outMin = glm::vec3(FLT_MAX);
	outMax = glm::vec3(-FLT_MAX);
	for (int corner = 0; corner < 8; corner++)
	{
		glm::vec3 local((corner & 1) ? max.x : min.x, (corner & 2) ? max.y : min.y, (corner & 4) ? max.z : min.z);
		glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
		outMin = glm::min(outMin, world);
		outMax = glm::max(outMax, world);
	}
}

void InstancedBrickRenderer::createInstancedVao(GLuint& vao, GLuint& instanceBuffer) const
{
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &instanceBuffer);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	const int cubeAttributeSizes[5] = { 3, 3, 3, 3, 2 };
	int offset = 0;
	for (int attribute = 0; attribute < 5; attribute++)
	{
		glEnableVertexAttribArray(attribute);
		glVertexAttribPointer(attribute, cubeAttributeSizes[attribute], GL_FLOAT, GL_FALSE, cubeVertexFloats * sizeof(float), (void*)(offset * sizeof(float)));
		offset += cubeAttributeSizes[attribute];
	}

	glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	const GLuint instanceAttributes[4] = { 5, 6, 7, 10 };
	const int instanceAttributeSizes[4] = { 3, 3, 4, 1 };
	offset = 0;
	for (int a = 0; a < 4; a++)
	{
		GLuint attribute = instanceAttributes[a];
		glEnableVertexAttribArray(attribute);
		glVertexAttribPointer(attribute, instanceAttributeSizes[a], GL_FLOAT, GL_FALSE, instanceFloats * sizeof(float), (void*)(offset * sizeof(float)));
		glVertexAttribDivisor(attribute, 1);
		offset += instanceAttributeSizes[a];
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedBrickRenderer::createSpecialVao(GLuint& vao, GLuint& instanceBuffer) const
{
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &instanceBuffer);
	glBindVertexArray(vao);

	//Position, normal, tangent, bitangent, uv at 0-4, then vertex color at 8
	glBindBuffer(GL_ARRAY_BUFFER, specialMeshBuffer);
	const GLuint meshAttributes[6] = { 0, 1, 2, 3, 4, 8 };
	const int meshAttributeSizes[6] = { 3, 3, 3, 3, 2, 4 };
	int offset = 0;
	for (int a = 0; a < 6; a++)
	{
		glEnableVertexAttribArray(meshAttributes[a]);
		glVertexAttribPointer(meshAttributes[a], meshAttributeSizes[a], GL_FLOAT, GL_FALSE, specialVertexFloats * sizeof(float), (void*)(offset * sizeof(float)));
		offset += meshAttributeSizes[a];
	}

	glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	for (GLuint attribute : { 5, 6, 7, 9, 10, 11 })
	{
		glEnableVertexAttribArray(attribute);
		glVertexAttribDivisor(attribute, 1);
	}
	pointSpecialInstances(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedBrickRenderer::pointSpecialInstances(GLsizei firstInstance) const
{
	const GLsizei stride = specialInstanceFloats * sizeof(float);
	size_t offset = (size_t)firstInstance * stride;

	glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
	glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offset + 3 * sizeof(float)));
	glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offset + 6 * sizeof(float)));
	glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE, stride, (void*)(offset + 10 * sizeof(float)));
	glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, stride, (void*)(offset + 11 * sizeof(float)));
	glVertexAttribPointer(11, 1, GL_FLOAT, GL_FALSE, stride, (void*)(offset + 12 * sizeof(float)));
}

int InstancedBrickRenderer::printLayer(const Brick& brick) const
{
	return prints ? prints->getDecalLayer(brick.printID) : -1;
}

void InstancedBrickRenderer::countPrints(Chunk* chunk, const std::vector<std::pair<uint16_t, int>>& newCounts)
{
	//What the chunk had before comes out of the total, whatever it has now goes in
	for (const auto& entry : chunk->prints)
	{
		auto found = printCounts.find(entry.first);
		if (found != printCounts.end() && (found->second -= entry.second) <= 0)
			printCounts.erase(found);
	}

	for (const auto& entry : newCounts)
		printCounts[entry.first] += entry.second;

	chunk->prints = newCounts;
}

const SpecialBrickType* InstancedBrickRenderer::specialType(const Brick& brick) const
{
	if (!brick.isSpecial() || !types || brick.typeID - 1 >= (int)specialTypeOffsets.size())
		return nullptr;
	return types->getSpecial(brick.typeID - 1);
}

InstancedBrickRenderer::Chunk* InstancedBrickRenderer::getChunk(const Brick* brick)
{
	int64_t key = chunkKey(brick);

	Chunk*& chunk = chunks[key];
	if (chunk)
		return chunk;

	chunk = new Chunk;
	chunk->key = key;

	for (int transparency = 0; transparency < 2; transparency++)
	{
		createInstancedVao(chunk->vao[transparency], chunk->instanceBuffer[transparency]);
		createSpecialVao(chunk->specialVao[transparency], chunk->specialInstanceBuffer[transparency]);
	}

	//Its bounds are only worked out once upload has its bricks, until then it's an empty box at the origin
	addToList(chunk);

	return chunk;
}

void InstancedBrickRenderer::markDirty(Chunk* chunk)
{
	if (chunk->dirty)
		return;

	chunk->dirty = true;
	dirtyChunks.push_back(chunk);
}

void InstancedBrickRenderer::addBrick(Brick* brick)
{
	Chunk* chunk = getChunk(brick);
	brick->renderIndex = chunk->bricks.size();
	chunk->bricks.push_back(brick);
	markDirty(chunk);
}

void InstancedBrickRenderer::removeBrick(Brick* brick)
{
	auto it = chunks.find(chunkKey(brick));
	if (it == chunks.end())
		return;

	Chunk* chunk = it->second;
	Brick* last = chunk->bricks.back();
	chunk->bricks[brick->renderIndex] = last;
	last->renderIndex = brick->renderIndex;
	chunk->bricks.pop_back();
	markDirty(chunk);
}

void InstancedBrickRenderer::updateBrick(Brick* brick)
{
	auto it = chunks.find(chunkKey(brick));
	if (it != chunks.end())
		markDirty(it->second);
}

void InstancedBrickRenderer::rebuild(Chunk* chunk)
{
	generation++;

	if (chunk->bricks.empty())
	{
		chunks.erase(chunk->key);
		removeFromList(chunk);
		destroyChunk(chunk);
		return;
	}

	upload(chunk);
}

void InstancedBrickRenderer::upload(Chunk* chunk)
{
	std::vector<float> instances[2];
	std::vector<std::pair<int, const Brick*>> specials[2];
	std::vector<std::pair<uint16_t, int>> prints;
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);
	bool anyUndulo = false;

	for (const Brick* brick : chunk->bricks)
	{
		glm::vec3 corner = glm::vec3(brick->x, brick->y, brick->z);
		glm::vec3 top = corner + brickSize(*brick);
		if (brick->material == BrickMaterial_Bouncy)
			top.y = corner.y + brick->height * (1.0f + bouncyStretch);
		anyUndulo |= brick->material == BrickMaterial_Undulo;

		min = glm::min(min, corner);
		max = glm::max(max, top);

		if (const SpecialBrickType* type = specialType(*brick))
		{
			//Only a print that's really drawn counts, a brick can wear one Lua gave it with no face to show it on
			if (brick->printID != 0 && type->groupCount[BrickTexturePrint] > 0)
			{
				auto found = std::find_if(prints.begin(), prints.end(), [&](const auto& entry) { return entry.first == brick->printID; });
				if (found == prints.end())
					prints.push_back({ brick->printID, 1 });
				else
					found->second++;
			}

			//A type with see-through faces is drawn with the transparent bricks even when it's painted opaque
			specials[brick->color.a < 255 || type->hasTransparency ? 1 : 0].push_back({ brick->typeID - 1, brick });
			continue;
		}

		appendInstance(instances[brick->color.a < 255 ? 1 : 0], corner, *brick, brick->color.a / 255.0f);
	}

	countPrints(chunk, prints);

	glm::vec3 wiggle = glm::vec3(anyUndulo ? unduloAmplitude : 0.0f);
	chunk->min = min * gridScale - wiggle;
	chunk->max = max * gridScale + wiggle;

	//A brick group's chunk isn't in chunkList, its bounds are in its own space and it's drawn by transform
	if (chunk->listIndex >= 0)
	{
		chunkList[chunk->listIndex].min = chunk->min;
		chunkList[chunk->listIndex].max = chunk->max;
	}

	for (int transparency = 0; transparency < 2; transparency++)
	{
		glBindBuffer(GL_ARRAY_BUFFER, chunk->instanceBuffer[transparency]);
		glBufferData(GL_ARRAY_BUFFER, instances[transparency].size() * sizeof(float), instances[transparency].data(), GL_DYNAMIC_DRAW);
		chunk->count[transparency] = instances[transparency].size() / instanceFloats;

		std::vector<std::pair<int, const Brick*>>& sorted = specials[transparency];
		std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::vector<float> specialInstances;
		std::vector<SpecialRun>& runs = chunk->specialRuns[transparency];
		runs.clear();

		for (size_t a = 0; a < sorted.size(); a++)
		{
			const Brick* brick = sorted[a].second;
			if (runs.empty() || runs.back().type != sorted[a].first)
				runs.push_back({ sorted[a].first, (GLsizei)a, 0 });
			runs.back().count++;

			appendSpecialInstance(specialInstances, glm::vec3(brick->x, brick->y, brick->z), *brick, brick->color.a / 255.0f, printLayer(*brick));
		}

		glBindBuffer(GL_ARRAY_BUFFER, chunk->specialInstanceBuffer[transparency]);
		glBufferData(GL_ARRAY_BUFFER, specialInstances.size() * sizeof(float), specialInstances.data(), GL_DYNAMIC_DRAW);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedBrickRenderer::rebuildDirty(float budgetMS)
{
	float start = getTicksMS();

	while (!dirtyChunks.empty())
	{
		Chunk* chunk = dirtyChunks.back();
		dirtyChunks.pop_back();
		chunk->dirty = false;
		rebuild(chunk);

		if (getTicksMS() - start > budgetMS)
			break;
	}
}

int InstancedBrickRenderer::addBrickGroup(const std::vector<Brick>& bricks)
{
	BrickGroup* group = new BrickGroup;
	group->bricks = bricks;

	group->chunk = new Chunk;
	for (int transparency = 0; transparency < 2; transparency++)
	{
		createInstancedVao(group->chunk->vao[transparency], group->chunk->instanceBuffer[transparency]);
		createSpecialVao(group->chunk->specialVao[transparency], group->chunk->specialInstanceBuffer[transparency]);
	}

	for (Brick& brick : group->bricks)
		group->chunk->bricks.push_back(&brick);

	if (!group->bricks.empty())
		upload(group->chunk);

	generation++;
	int id = nextGroup++;
	groups[id] = group;
	return id;
}

void InstancedBrickRenderer::removeBrickGroup(int group)
{
	auto found = groups.find(group);
	if (found == groups.end())
		return;

	destroyChunk(found->second->chunk);
	delete found->second;
	groups.erase(found);
	generation++;
}

void InstancedBrickRenderer::setTransform(const glm::mat4& transform) const
{
	glUniformMatrix4fv(brickTransformUniform, 1, GL_FALSE, &transform[0][0]);
}

void InstancedBrickRenderer::uploadSingleInstance(const glm::vec3& corner, const Brick& brick, float alpha) const
{
	std::vector<float> instance;
	appendInstance(instance, corner, brick, alpha);

	glBindBuffer(GL_ARRAY_BUFFER, singleInstanceBuffer);
	glBufferData(GL_ARRAY_BUFFER, instance.size() * sizeof(float), instance.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedBrickRenderer::uploadSingleSpecialInstance(const glm::vec3& corner, const Brick& brick, float alpha) const
{
	std::vector<float> instance;
	appendSpecialInstance(instance, corner, brick, alpha, printLayer(brick));

	glBindBuffer(GL_ARRAY_BUFFER, singleSpecialInstanceBuffer);
	glBufferData(GL_ARRAY_BUFFER, instance.size() * sizeof(float), instance.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedBrickRenderer::drawInstances(std::shared_ptr<ShaderManager> shaders, const std::vector<InstanceSet>& sets, const std::function<void(size_t)>& beforeEach) const
{
	struct FaceGroup
	{
		const Material* material;
		GLint first;
		GLsizei count;
		bool tileByStuds;
	};

	const FaceGroup faceGroups[3] =
	{
		{ topMaterial, topFirst, topCount, true },
		{ bottomMaterial, bottomFirst, bottomCount, true },
		{ sideMaterial, sidesFirst, sidesCount, false }
	};

	for (const FaceGroup& faceGroup : faceGroups)
	{
		faceGroup.material->use(shaders);
		glUniform1i(tileByStudsUniform, faceGroup.tileByStuds);

		for (size_t a = 0; a < sets.size(); a++)
		{
			if (beforeEach)
				beforeEach(a);

			glBindVertexArray(sets[a].vao);
			glDrawArraysInstanced(GL_TRIANGLES, faceGroup.first, faceGroup.count, sets[a].count);
		}
	}

	glBindVertexArray(0);
}

void InstancedBrickRenderer::drawSpecial(std::shared_ptr<ShaderManager> shaders, const std::vector<SpecialSet>& sets, const std::function<void(size_t)>& beforeEach) const
{
	if (sets.empty())
		return;

	//Indexed by BrickFaceTexture
	const Material* materials[BrickTextureCount] = { topMaterial, bottomMaterial, sideMaterial, rampMaterial, printMaterial };

	glUniform1i(specialMeshUniform, 1);
	glUniform1i(tileByStudsUniform, 0);

	for (int faceGroup = 0; faceGroup < BrickTextureCount; faceGroup++)
	{
		bool materialInUse = false;
		glUniform1i(printFaceUniform, faceGroup == BrickTexturePrint);

		for (size_t a = 0; a < sets.size(); a++)
		{
			if (beforeEach)
				beforeEach(a);

			glBindVertexArray(sets[a].vao);
			glBindBuffer(GL_ARRAY_BUFFER, sets[a].instanceBuffer);

			for (const SpecialRun& run : *sets[a].runs)
			{
				const SpecialBrickType* type = types->getSpecial(run.type);
				if (!type || type->groupCount[faceGroup] == 0)
					continue;

				if (!materialInUse)
				{
					//A print covers its whole face, however DecalArea was left by the last mesh drawn, see brick.vert
					//Set right here so Material::use uploads it, since Mesh::render only re-uploads when it sees a different one
					if (faceGroup == BrickTexturePrint)
						shaders->basicUniforms.DecalArea = glm::vec4(0, 0, 1, 1);

					materials[faceGroup]->use(shaders);
					materialInUse = true;
				}

				pointSpecialInstances(run.first);
				glDrawArraysInstanced(GL_TRIANGLES, specialTypeOffsets[run.type] + type->groupFirst[faceGroup], type->groupCount[faceGroup], run.count);
			}
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUniform1i(specialMeshUniform, 0);
	glUniform1i(printFaceUniform, 0);
}

void InstancedBrickRenderer::visibleChunks(const std::shared_ptr<ShaderManager>& shaders, bool transparent, std::vector<const Chunk*>& out) const
{
	int transparency = transparent ? 1 : 0;

	std::array<glm::vec4, 6> planes = frustumPlanes(shaders->cameraUniforms.CameraProjection * shaders->cameraUniforms.CameraView);
	const glm::vec3& eye = shaders->cameraUniforms.CameraPosition;

	//Chunks come out of the map in no particular order, so they're sorted before drawing. Opaque bricks go
	//nearest first, which lets the depth test throw away everything behind a wall before model.frag ever runs
	//on it, and transparent ones farthest first, since they blend and have to arrive in back to front order
	std::vector<std::pair<float, const Chunk*>> sorted;
	sorted.reserve(chunkList.size());
	for (const ChunkBounds& bounds : chunkList)
	{
		const Chunk* chunk = bounds.chunk;
		if (chunk->count[transparency] == 0 && chunk->specialRuns[transparency].empty())
			continue;
		if (!boxVisible(planes, bounds.min, bounds.max))
			continue;

		//Squared distance to the nearest point of the chunk, 0 for the one the camera is inside
		glm::vec3 toBox = glm::clamp(eye, bounds.min, bounds.max) - eye;
		sorted.push_back({ glm::dot(toBox, toBox), chunk });
	}

	std::sort(sorted.begin(), sorted.end(), [transparent](const std::pair<float, const Chunk*>& a, const std::pair<float, const Chunk*>& b)
		{ return transparent ? a.first > b.first : a.first < b.first; });

	out.clear();
	out.reserve(sorted.size());
	for (const auto& entry : sorted)
		out.push_back(entry.second);
}

void InstancedBrickRenderer::renderDepth(std::shared_ptr<ShaderManager> shaders) const
{
	std::vector<const Chunk*> visible;
	visibleChunks(shaders, false, visible);
	if (visible.empty())
		return;

	const glm::mat4 identity(1.0f);
	glUniformMatrix4fv(brickDepthTransformUniform, 1, GL_FALSE, &identity[0][0]);

	//Nothing here writes color, so the whole cube goes in one draw instead of three by face group
	for (const Chunk* chunk : visible)
	{
		if (chunk->count[0] > 0)
		{
			glBindVertexArray(chunk->vao[0]);
			glDrawArraysInstanced(GL_TRIANGLES, 0, topCount + bottomCount + sidesCount, chunk->count[0]);
		}

		if (chunk->specialRuns[0].empty())
			continue;

		/*
			Culling stays exactly as the shading pass has it. The shadow pass turns it off for these, since a shape
			that isn't closed has to cast from both sides, but here the pre-pass must lay down the depth of the very
			triangles the shading pass will draw and no others. With back faces let through, a thin shape like a
			fence rail has its two sides within a depth quantum of each other, they interpolate depth a little
			differently, and the back face wins some pixels - where the front face then fails the equal test and
			leaves a speckled hole with the ground showing through it
		*/
		glUniform1i(brickDepthSpecialMeshUniform, 1);
		glBindVertexArray(chunk->specialVao[0]);
		glBindBuffer(GL_ARRAY_BUFFER, chunk->specialInstanceBuffer[0]);
		for (const SpecialRun& run : chunk->specialRuns[0])
		{
			const SpecialBrickType* type = types->getSpecial(run.type);
			if (!type)
				continue;

			pointSpecialInstances(run.first);
			glDrawArraysInstanced(GL_TRIANGLES, specialTypeOffsets[run.type], type->vertexCount(), run.count);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUniform1i(brickDepthSpecialMeshUniform, 0);
	}

	glBindVertexArray(0);
}

void InstancedBrickRenderer::render(std::shared_ptr<ShaderManager> shaders, bool transparent) const
{
	int transparency = transparent ? 1 : 0;

	std::vector<const Chunk*> chunkOrder;
	visibleChunks(shaders, transparent, chunkOrder);
	if (chunkOrder.empty())
		return;

	std::vector<InstanceSet> visible;
	std::vector<SpecialSet> visibleSpecial;
	visible.reserve(chunkOrder.size());
	for (const Chunk* chunk : chunkOrder)
	{
		if (chunk->count[transparency] > 0)
			visible.push_back({ chunk->vao[transparency], chunk->count[transparency] });
		if (!chunk->specialRuns[transparency].empty())
			visibleSpecial.push_back({ chunk->specialVao[transparency], chunk->specialInstanceBuffer[transparency], &chunk->specialRuns[transparency] });
	}

	setTransform(glm::mat4(1.0f));

	if (transparent)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
	}

	if (!visible.empty())
		drawInstances(shaders, visible);
	drawSpecial(shaders, visibleSpecial);

	if (transparent)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
}

void InstancedBrickRenderer::renderGroups(std::shared_ptr<ShaderManager> shaders, const std::vector<GroupDraw>& draws, bool transparent) const
{
	int transparency = transparent ? 1 : 0;
	std::array<glm::vec4, 6> planes = frustumPlanes(shaders->cameraUniforms.CameraProjection * shaders->cameraUniforms.CameraView);
	bool drewAny = false;

	for (const GroupDraw& draw : draws)
	{
		auto found = groups.find(draw.group);
		if (found == groups.end())
			continue;

		const Chunk* chunk = found->second->chunk;
		if (chunk->count[transparency] == 0 && chunk->specialRuns[transparency].empty())
			continue;

		glm::vec3 min, max;
		transformBounds(draw.transform, chunk->min, chunk->max, min, max);
		if (!boxVisible(planes, min, max))
			continue;

		if (!drewAny && transparent)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_FALSE);
		}
		drewAny = true;

		setTransform(draw.transform);
		if (chunk->count[transparency] > 0)
			drawInstances(shaders, { { chunk->vao[transparency], chunk->count[transparency] } });
		if (!chunk->specialRuns[transparency].empty())
			drawSpecial(shaders, { { chunk->specialVao[transparency], chunk->specialInstanceBuffer[transparency], &chunk->specialRuns[transparency] } });
	}

	if (!drewAny)
		return;

	setTransform(glm::mat4(1.0f));

	if (transparent)
	{
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
}

void InstancedBrickRenderer::renderGhost(std::shared_ptr<ShaderManager> shaders, const Brick& ghost, float pulse) const
{
	pulse = std::clamp(pulse, 0.0f, 1.0f);
	float alpha = 0.35f + 0.35f * pulse;

	setTransform(glm::mat4(1.0f));
	glUniform1f(glowUniform, 0.1f + 0.35f * pulse);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	if (specialType(ghost))
	{
		std::vector<SpecialRun> runs = { { ghost.typeID - 1, 0, 1 } };
		uploadSingleSpecialInstance(glm::vec3(ghost.x, ghost.y, ghost.z), ghost, alpha);
		drawSpecial(shaders, { { singleSpecialVao, singleSpecialInstanceBuffer, &runs } });
	}
	else
	{
		uploadSingleInstance(glm::vec3(ghost.x, ghost.y, ghost.z), ghost, alpha);
		drawInstances(shaders, { { singleVao, 1 } });
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUniform1f(glowUniform, 0.0f);
}

void InstancedBrickRenderer::renderLoose(std::shared_ptr<ShaderManager> shaders, const std::vector<LooseBrick>& bricks) const
{
	if (bricks.empty())
		return;

	std::vector<const LooseBrick*> basic;
	std::vector<const LooseBrick*> special;
	std::vector<std::vector<SpecialRun>> specialRuns;
	for (const LooseBrick& loose : bricks)
	{
		if (specialType(*loose.brick))
		{
			special.push_back(&loose);
			specialRuns.push_back({ { loose.brick->typeID - 1, 0, 1 } });
		}
		else
			basic.push_back(&loose);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::vector<InstanceSet> sets(basic.size(), { singleVao, 1 });
	if (!sets.empty())
	{
		drawInstances(shaders, sets, [&](size_t index)
		{
			const LooseBrick& loose = *basic[index];
			uploadSingleInstance(-brickSize(*loose.brick) * 0.5f, *loose.brick, loose.alpha);
			setTransform(loose.transform);
		});
	}

	std::vector<SpecialSet> specialSets;
	for (const std::vector<SpecialRun>& runs : specialRuns)
		specialSets.push_back({ singleSpecialVao, singleSpecialInstanceBuffer, &runs });

	drawSpecial(shaders, specialSets, [&](size_t index)
	{
		//The transform already has the brick's turn in it
		Brick unturned = *special[index]->brick;
		unturned.angleID = 0;
		uploadSingleSpecialInstance(-brickSize(unturned) * 0.5f, unturned, special[index]->alpha);
		setTransform(special[index]->transform);

		//Uploading unbinds the instance buffer drawSpecial just bound
		glBindBuffer(GL_ARRAY_BUFFER, singleSpecialInstanceBuffer);
	});

	setTransform(glm::mat4(1.0f));

	glDisable(GL_BLEND);
}

std::string InstancedBrickRenderer::getShadowStats() const
{
	return std::to_string(shadowChunksDrawn) + " chunks drawn of " + std::to_string(shadowChunksTested) + " looked at";
}

bool InstancedBrickRenderer::hasTransparentBricks() const
{
	for (const ChunkBounds& bounds : chunkList)
	{
		if (bounds.chunk->count[1] > 0 || !bounds.chunk->specialRuns[1].empty())
			return true;
	}
	for (const auto& entry : groups)
	{
		if (entry.second->chunk->count[1] > 0 || !entry.second->chunk->specialRuns[1].empty())
			return true;
	}
	return false;
}

void InstancedBrickRenderer::drawChunkShadow(const Chunk* chunk, bool opaque, bool transparent, GLint specialUniform) const
{
	//brickShadowCascade.vert drops transparent bricks under its minOpacity
	for (int transparency = 0; transparency < 2; transparency++)
	{
		if (chunk->count[transparency] < 1 || !(transparency ? transparent : opaque))
			continue;

		glBindVertexArray(chunk->vao[transparency]);
		glDrawArraysInstanced(GL_TRIANGLES, 0, topCount + bottomCount + sidesCount, chunk->count[transparency]);
	}

	//Special shapes aren't always closed, so unlike boxes both of their sides cast
	bool culling = glIsEnabled(GL_CULL_FACE);
	bool anySpecial = false;

	for (int transparency = 0; transparency < 2; transparency++)
	{
		if (chunk->specialRuns[transparency].empty() || !(transparency ? transparent : opaque))
			continue;

		if (!anySpecial)
		{
			glUniform1i(specialUniform, 1);
			glDisable(GL_CULL_FACE);
			anySpecial = true;
		}

		glBindVertexArray(chunk->specialVao[transparency]);
		glBindBuffer(GL_ARRAY_BUFFER, chunk->specialInstanceBuffer[transparency]);
		for (const SpecialRun& run : chunk->specialRuns[transparency])
		{
			const SpecialBrickType* type = types->getSpecial(run.type);
			if (!type)
				continue;

			pointSpecialInstances(run.first);
			glDrawArraysInstanced(GL_TRIANGLES, specialTypeOffsets[run.type], type->vertexCount(), run.count);
		}
	}

	if (anySpecial)
	{
		glUniform1i(specialUniform, 0);
		if (culling)
			glEnable(GL_CULL_FACE);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glBindVertexArray(0);
}

void InstancedBrickRenderer::renderShadowCascade(const glm::mat4& lightSpaceMatrix, bool opaque, bool transparent, bool tintProgram, const std::vector<GroupDraw>* draws, const glm::vec3* skipPoint) const
{
	std::array<glm::vec4, 6> planes = frustumPlanes(lightSpaceMatrix);
	//Chunks between the light and the cascade still shadow it, GL_DEPTH_CLAMP flattens them onto its near plane
	planes[4] = glm::vec4(0, 0, 0, 1);

	GLint specialUniform = tintProgram ? tintSpecialMeshUniform : shadowSpecialMeshUniform;
	GLint transformUniform = tintProgram ? tintTransformUniform : shadowTransformUniform;
	GLint skipPointUniform = tintProgram ? tintSkipPointUniform : shadowSkipPointUniform;

	const glm::mat4 identity(1.0f);
	glUniformMatrix4fv(transformUniform, 1, GL_FALSE, &identity[0][0]);

	for (const ChunkBounds& bounds : chunkList)
	{
		if (boxVisible(planes, bounds.min, bounds.max))
		{
			shadowChunksDrawn++;
			drawChunkShadow(bounds.chunk, opaque, transparent, specialUniform);
		}
	}
	shadowChunksTested += (int)chunkList.size();

	if (!draws || draws->empty())
		return;

	bool drewAny = false;
	for (const GroupDraw& draw : *draws)
	{
		auto found = groups.find(draw.group);
		if (found == groups.end())
			continue;

		glm::vec3 min, max;
		transformBounds(draw.transform, found->second->chunk->min, found->second->chunk->max, min, max);
		if (!boxVisible(planes, min, max))
			continue;

		drewAny = true;
		glUniformMatrix4fv(transformUniform, 1, GL_FALSE, &draw.transform[0][0]);

		//The shader compares it to brick corners in the group's own space
		if (skipPoint)
		{
			glm::vec3 local = glm::vec3(glm::inverse(draw.transform) * glm::vec4(*skipPoint, 1.0f));
			glUniform3fv(skipPointUniform, 1, &local[0]);
		}

		drawChunkShadow(found->second->chunk, opaque, transparent, specialUniform);
	}

	if (!drewAny)
		return;

	glUniformMatrix4fv(transformUniform, 1, GL_FALSE, &identity[0][0]);
	if (skipPoint)
		glUniform3fv(skipPointUniform, 1, &(*skipPoint)[0]);
}

void InstancedBrickRenderer::addToList(Chunk* chunk)
{
	chunk->listIndex = (int)chunkList.size();
	chunkList.push_back({ chunk->min, chunk->max, chunk });
}

void InstancedBrickRenderer::removeFromList(Chunk* chunk)
{
	if (chunk->listIndex < 0)
		return;

	//The last entry takes the leaving one's place, so this doesn't shuffle thousands of chunks along
	chunkList[chunk->listIndex] = chunkList.back();
	chunkList[chunk->listIndex].chunk->listIndex = chunk->listIndex;
	chunkList.pop_back();
	chunk->listIndex = -1;
}

void InstancedBrickRenderer::destroyChunk(Chunk* chunk)
{
	countPrints(chunk, {});

	glDeleteVertexArrays(2, chunk->vao);
	glDeleteBuffers(2, chunk->instanceBuffer);
	glDeleteVertexArrays(2, chunk->specialVao);
	glDeleteBuffers(2, chunk->specialInstanceBuffer);
	delete chunk;
}

void InstancedBrickRenderer::clear()
{
	for (auto& entry : chunks)
		destroyChunk(entry.second);

	chunks.clear();
	chunkList.clear();
	dirtyChunks.clear();
}

InstancedBrickRenderer::InstancedBrickRenderer(std::shared_ptr<ShaderManager> shaders, std::shared_ptr<TextureManager> textures, const BrickTypes* _types,
	const PrintTypes* _prints) : types(_types), prints(_prints)
{
	std::vector<float> cube = makeCube();

	glGenBuffers(1, &cubeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	glBufferData(GL_ARRAY_BUFFER, cube.size() * sizeof(float), cube.data(), GL_STATIC_DRAW);

	std::vector<float> specialMeshes;
	for (size_t a = 0; types && a < types->getSpecialCount(); a++)
	{
		const SpecialBrickType* type = types->getSpecial((int)a);
		specialTypeOffsets.push_back((GLint)(specialMeshes.size() / specialVertexFloats));
		specialMeshes.insert(specialMeshes.end(), type->vertices.begin(), type->vertices.end());
	}

	glGenBuffers(1, &specialMeshBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, specialMeshBuffer);
	glBufferData(GL_ARRAY_BUFFER, specialMeshes.size() * sizeof(float), specialMeshes.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	createInstancedVao(singleVao, singleInstanceBuffer);
	createSpecialVao(singleSpecialVao, singleSpecialInstanceBuffer);

	topMaterial = new Material("Assets/brick/studs.txt", textures);
	bottomMaterial = new Material("Assets/brick/bottoms.txt", textures);
	sideMaterial = new Material("Assets/brick/sides.txt", textures);
	rampMaterial = new Material("Assets/brick/ramps.txt", textures);
	//A flat normal map, since a material's first layer can't be only roughness
	printMaterial = new Material("BrickPrintPlain", "", "Assets/brick/printNormal.png", "Assets/brick/scuffed-plastic-rough.png", "", "", textures);

	if (!topMaterial->isValid() || !bottomMaterial->isValid() || !sideMaterial->isValid() || !rampMaterial->isValid() || !printMaterial->isValid())
		error("Could not load brick materials from Assets/brick/");

	tileByStudsUniform = shaders->brickShader->getUniformLocation("tileByStuds");
	printFaceUniform = shaders->brickShader->getUniformLocation("printFace");
	brickTransformUniform = shaders->brickShader->getUniformLocation("brickTransform");
	glowUniform = shaders->brickShader->getUniformLocation("glow");
	specialMeshUniform = shaders->brickShader->getUniformLocation("specialMesh");
	brickDepthSpecialMeshUniform = shaders->brickDepthShader->getUniformLocation("specialMesh");
	brickDepthTransformUniform = shaders->brickDepthShader->getUniformLocation("brickTransform");
	shadowSpecialMeshUniform = shaders->brickShadowCascadeShader->getUniformLocation("specialMesh");
	tintSpecialMeshUniform = shaders->brickShadowTintShader->getUniformLocation("specialMesh");
	shadowTransformUniform = shaders->brickShadowCascadeShader->getUniformLocation("brickTransform");
	tintTransformUniform = shaders->brickShadowTintShader->getUniformLocation("brickTransform");
	shadowSkipPointUniform = shaders->brickShadowCascadeShader->getUniformLocation("skipPoint");
	tintSkipPointUniform = shaders->brickShadowTintShader->getUniformLocation("skipPoint");

	info("Uploaded " + std::to_string(specialMeshes.size() / specialVertexFloats) + " vertices for " + std::to_string(specialTypeOffsets.size()) + " special brick types");
}

InstancedBrickRenderer::~InstancedBrickRenderer()
{
	clear();

	for (auto& entry : groups)
	{
		destroyChunk(entry.second->chunk);
		delete entry.second;
	}
	groups.clear();

	glDeleteVertexArrays(1, &singleVao);
	glDeleteBuffers(1, &singleInstanceBuffer);
	glDeleteVertexArrays(1, &singleSpecialVao);
	glDeleteBuffers(1, &singleSpecialInstanceBuffer);
	glDeleteBuffers(1, &cubeBuffer);
	glDeleteBuffers(1, &specialMeshBuffer);

	delete topMaterial;
	delete bottomMaterial;
	delete sideMaterial;
	delete rampMaterial;
	delete printMaterial;
}

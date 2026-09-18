#include "WorldDecals.h"

//How far off its surface a decal is drawn, on top of the polygon offset, so it never fights the face under it
static constexpr float surfaceLift = 0.01f;

//How far past its brick's sides a decal may reach before it's cut off
static constexpr float brickOverhang = 0.02f;

void WorldDecal::write(unsigned char* data) const
{
	size_t at = 0;
	memcpy(data + at, &typeID, sizeof(uint16_t));
	at += sizeof(uint16_t);
	memcpy(data + at, &position.x, sizeof(float) * 3);
	at += sizeof(float) * 3;
	memcpy(data + at, &normal.x, sizeof(float) * 3);
	at += sizeof(float) * 3;
	memcpy(data + at, &size, sizeof(float));
	at += sizeof(float);
	memcpy(data + at, &roll, sizeof(float));
	at += sizeof(float);
	memcpy(data + at, &brickID, sizeof(netIDType));
}

WorldDecal WorldDecal::read(const unsigned char* data)
{
	WorldDecal decal;
	size_t at = 0;
	memcpy(&decal.typeID, data + at, sizeof(uint16_t));
	at += sizeof(uint16_t);
	memcpy(&decal.position.x, data + at, sizeof(float) * 3);
	at += sizeof(float) * 3;
	memcpy(&decal.normal.x, data + at, sizeof(float) * 3);
	at += sizeof(float) * 3;
	memcpy(&decal.size, data + at, sizeof(float));
	at += sizeof(float);
	memcpy(&decal.roll, data + at, sizeof(float));
	at += sizeof(float);
	memcpy(&decal.brickID, data + at, sizeof(netIDType));
	return decal;
}

void WorldDecals::setType(uint16_t id, const std::string& name, const std::string& materialPath, std::shared_ptr<TextureManager> textures)
{
	scope("WorldDecals::setType");

	if (id >= types.size())
		types.resize((size_t)id + 1);

	delete types[id].material;
	types[id].material = nullptr;
	types[id].name = name;

	if (!doesFileExist(materialPath))
	{
		error("No material " + materialPath + " for decal type " + name + ", its decals won't be drawn");
		return;
	}

	Material* material = new Material(materialPath, textures);
	if (!material->isValid())
	{
		error("Could not load material " + materialPath + " for decal type " + name);
		delete material;
		return;
	}

	types[id].material = material;
}

void WorldDecals::trim(unsigned int maxDecals)
{
	while (decals.size() > maxDecals)
		decals.pop_front();
}

void WorldDecals::prune(const BrickHolder* bricks)
{
	if (!bricks)
		return;

	for (auto iter = decals.begin(); iter != decals.end();)
	{
		if (iter->brickID != NO_ID && !bricks->find(iter->brickID))
			iter = decals.erase(iter);
		else
			++iter;
	}
}

void WorldDecals::clear()
{
	decals.clear();
}

struct DecalCorner
{
	glm::vec3 position;
	glm::vec2 uv;
};

//Sutherland-Hodgman against one side of a box: keeps what's on the inside of position[axis] = limit
static void clipToSide(std::vector<DecalCorner>& polygon, int axis, float limit, bool keepBelow)
{
	std::vector<DecalCorner> kept;
	kept.reserve(polygon.size() + 1);

	for (size_t a = 0; a < polygon.size(); a++)
	{
		const DecalCorner& from = polygon[a];
		const DecalCorner& to = polygon[(a + 1) % polygon.size()];

		float fromDistance = keepBelow ? limit - from.position[axis] : from.position[axis] - limit;
		float toDistance = keepBelow ? limit - to.position[axis] : to.position[axis] - limit;

		if (fromDistance >= 0)
			kept.push_back(from);

		if ((fromDistance >= 0) != (toDistance >= 0))
		{
			float along = fromDistance / (fromDistance - toDistance);
			kept.push_back({ glm::mix(from.position, to.position, along), glm::mix(from.uv, to.uv, along) });
		}
	}

	polygon = std::move(kept);
}

void WorldDecals::appendVertices(std::vector<float>& vertices, const WorldDecal& decal, const Brick* brick) const
{
	glm::vec3 normal = glm::normalize(decal.normal);

	//Any two directions across the surface, then turned about the normal by the decal's roll
	glm::vec3 across = std::abs(normal.y) < 0.99f ? glm::normalize(glm::cross(glm::vec3(0, 1, 0), normal)) : glm::vec3(1, 0, 0);
	glm::vec3 up = glm::cross(normal, across);
	glm::vec3 tangent = across * std::cos(decal.roll) + up * std::sin(decal.roll);
	glm::vec3 bitangent = glm::cross(normal, tangent);

	glm::vec3 center = decal.position;
	glm::vec4 color = glm::vec4(1);

	glm::vec3 low, high;
	if (brick)
	{
		low = glm::vec3(brick->x * STUD_SIZE, brick->y * PLATE_SIZE, brick->z * STUD_SIZE);
		high = low + glm::vec3(brick->footprintWidth() * STUD_SIZE, brick->height * PLATE_SIZE, brick->footprintLength() * STUD_SIZE);

		/*
			A hole right at the edge of a brick would hang half off it, so it's slid along the face until the
			part of it that shows, a circle inside the square, fits. That's at most half its size, far less than
			anyone could tell the shot from. A face too small to fit it at all is dealt with by the cutting below
		*/
		float radius = decal.size * 0.5f;
		for (int axis = 0; axis < 3; axis++)
		{
			if (std::abs(normal[axis]) > 0.5f || high[axis] - low[axis] < radius * 2.0f)
				continue;
			center[axis] = glm::clamp(center[axis], low[axis] + radius, high[axis] - radius);
		}

		//The rim of the hole is the brick's own plastic pushed out of shape, so it takes the brick's color
		color = glm::vec4(glm::vec3(brick->color) / 255.0f, 1.0f);
	}

	std::vector<DecalCorner> polygon;
	const float corners[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
	for (int a = 0; a < 4; a++)
	{
		glm::vec2 uv(corners[a][0], corners[a][1]);
		polygon.push_back({ center + tangent * ((uv.x - 0.5f) * decal.size) + bitangent * ((uv.y - 0.5f) * decal.size), uv });
	}

	if (brick)
	{
		for (int axis = 0; axis < 3 && polygon.size() >= 3; axis++)
		{
			clipToSide(polygon, axis, low[axis] - brickOverhang, false);
			if (polygon.size() >= 3)
				clipToSide(polygon, axis, high[axis] + brickOverhang, true);
		}
	}

	if (polygon.size() < 3)
		return;

	//A fan from its first corner, what's left of a square after cutting is always convex
	for (size_t a = 1; a + 1 < polygon.size(); a++)
	{
		const DecalCorner* triangle[3] = { &polygon[0], &polygon[a], &polygon[a + 1] };
		for (const DecalCorner* corner : triangle)
		{
			glm::vec3 position = corner->position + normal * surfaceLift;
			vertices.insert(vertices.end(), {
				position.x, position.y, position.z,
				normal.x, normal.y, normal.z,
				tangent.x, tangent.y, tangent.z,
				bitangent.x, bitangent.y, bitangent.z,
				corner->uv.x, corner->uv.y,
				color.r, color.g, color.b, color.a });
		}
	}
}

void WorldDecals::update(const BrickHolder* bricks)
{
	//Whatever they were on is gone, so they are too
	prune(bricks);

	draws.assign(types.size(), TypeDraw());
	if (decals.empty())
		return;

	//One run of triangles per type, since each type draws with its own material
	std::vector<float> vertices;
	for (size_t type = 0; type < types.size(); type++)
	{
		if (!types[type].material)
			continue;

		draws[type].first = (GLint)(vertices.size() / vertexFloats);
		for (const WorldDecal& decal : decals)
		{
			if (decal.typeID == type)
				appendVertices(vertices, decal, decal.brickID != NO_ID && bricks ? bricks->find(decal.brickID) : nullptr);
		}
		draws[type].count = (GLsizei)(vertices.size() / vertexFloats) - draws[type].first;
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WorldDecals::render(std::shared_ptr<ShaderManager> shaders, const glm::mat4* lightSpaceMatricies, int shadowSoftness, bool coloredShadows) const
{
	if (decals.empty() || !shaders->decalShader || !shaders->decalShader->isCompiled())
		return;

	bool anything = false;
	for (const TypeDraw& draw : draws)
		anything = anything || draw.count > 0;
	if (!anything)
		return;

	Program* program = shaders->decalShader;
	program->use();
	glUniformMatrix4fv(program->getUniformLocation("lightSpaceMatricies"), 3, GL_FALSE, (const GLfloat*)lightSpaceMatricies);
	glUniform1i(program->getUniformLocation("shadowSoftness"), shadowSoftness);
	glUniform1i(program->getUniformLocation("coloredShadows"), coloredShadows);

	//A print's area, which model.frag would otherwise read off whatever mesh was drawn last
	shaders->basicUniforms.DecalArea = glm::vec4(0, 0, 1, 1);

	//Water's reflection draws everything mirrored, and nothing can get behind a decal to see its back anyway
	bool culling = glIsEnabled(GL_CULL_FACE);
	if (culling)
		glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -1.0f);

	glBindVertexArray(vao);
	for (size_t type = 0; type < draws.size() && type < types.size(); type++)
	{
		if (draws[type].count <= 0 || !types[type].material)
			continue;

		//Also sends the basic uniforms
		types[type].material->use(shaders);
		glDrawArrays(GL_TRIANGLES, draws[type].first, draws[type].count);
	}
	glBindVertexArray(0);

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	if (culling)
		glEnable(GL_CULL_FACE);
}

WorldDecals::WorldDecals()
{
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	//Position, normal, tangent, bitangent at 0 to 3, uv at 4, color at 5, see decal.vert
	const int widths[6] = { 3, 3, 3, 3, 2, 4 };
	size_t offset = 0;
	for (int a = 0; a < 6; a++)
	{
		glEnableVertexAttribArray(a);
		glVertexAttribPointer(a, widths[a], GL_FLOAT, GL_FALSE, vertexFloats * sizeof(float), (void*)(offset * sizeof(float)));
		offset += widths[a];
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

WorldDecals::~WorldDecals()
{
	for (DecalType& type : types)
		delete type.material;

	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

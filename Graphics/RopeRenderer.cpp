#include "RopeRenderer.h"
#include "../SimObjects/Rope.h"

//The world's gravity, see PhysicsWorld's constructor
static constexpr float ropeGravity = -70.0f;

//Nodes move in steps of this many seconds, however long the frame was, so a rope hangs the same at any frame rate
static constexpr float stepSeconds = 1.0f / 60.0f;

//A long frame is caught up on with at most this many steps, the rest of it is forgotten
static constexpr int maxStepsPerFrame = 4;

//How much of its speed a node keeps each step, which is what lets a swinging rope come to rest
static constexpr float nodeDamping = 0.985f;

//Passes over the sections per step pulling them back to their length, more keeps a rope of many links from looking stretchy
static constexpr int relaxPasses = 12;

//Pulled out to this much of its length, a rope is drawn as the straight line it all but is
static constexpr float tautFraction = 0.995f;

//An end that moves further than this in one frame was moved there, not swung, and the rope starts over from a straight line
static constexpr float teleportDistance = 40.0f;

void RopeRenderer::simulate(Rope& rope, float deltaT) const
{
	//What the rope is pinned to, in order: its first end, its bends, its other end
	std::vector<glm::vec3> pins;
	pins.push_back(rope.getDrawnEnd(0));
	for (const glm::vec3& bend : rope.getBends())
		pins.push_back(bend);
	pins.push_back(rope.getDrawnEnd(1));

	size_t sections = pins.size() - 1;
	size_t links = std::max((size_t)rope.getLinks(), sections);

	std::vector<float> reach(pins.size(), 0.0f);
	for (size_t a = 1; a < pins.size(); a++)
		reach[a] = reach[a - 1] + glm::distance(pins[a], pins[a - 1]);
	float span = reach.back();

	//Which node sits on each pin: as far along the chain as the pin is along the path, and at least a link past the pin before
	std::vector<size_t> pinNodes(pins.size(), 0);
	for (size_t a = 1; a < pins.size(); a++)
	{
		size_t node = span > 0.0001f ? (size_t)std::lround(reach[a] / span * links) : a * links / sections;
		size_t leftAfter = sections - a;
		pinNodes[a] = std::clamp(node, pinNodes[a - 1] + 1, links - leftAfter);
	}
	pinNodes.back() = links;

	//Laid straight from pin to pin
	auto layStraight = [&]()
	{
		rope.nodes.resize(links + 1);
		for (size_t a = 0; a < sections; a++)
		{
			size_t first = pinNodes[a], last = pinNodes[a + 1];
			for (size_t node = first; node <= last; node++)
				rope.nodes[node] = glm::mix(pins[a], pins[a + 1], (float)(node - first) / (float)(last - first));
		}
		rope.lastNodes = rope.nodes;
		rope.simulationDebt = 0;
	};

	bool startOver = rope.nodes.size() != links + 1 || rope.lastNodes.size() != links + 1;
	if (!startOver)
	{
		startOver = glm::distance(rope.nodes.front(), pins.front()) > teleportDistance || glm::distance(rope.nodes.back(), pins.back()) > teleportDistance;
		for (const glm::vec3& node : rope.nodes)
			startOver = startOver || glm::any(glm::isnan(node));
	}

	/*
		How much rope there is to spare is measured between where its ends are tied, and that's the slack it's drawn
		with too: an end drawn somewhere else, like the barrel of a tool, makes the drawn rope that much longer
		or shorter rather than looser or tighter
	*/
	float tiedSpan = 0;
	{
		glm::vec3 last = rope.getDrawnEnd(0, true);
		for (const glm::vec3& bend : rope.getBends())
		{
			tiedSpan += glm::distance(last, bend);
			last = bend;
		}
		tiedSpan += glm::distance(last, rope.getDrawnEnd(1, true));
	}
	float slack = std::max(rope.getLength() - tiedSpan, 0.0f);
	float drawnLength = span + slack;

	if (startOver || slack <= rope.getLength() * (1.0f - tautFraction))
	{
		layStraight();
		return;
	}

	//How long each link is, the slack shared out between the sections by how long they are
	std::vector<float> rest(links, 0.0f);
	for (size_t a = 0; a < sections; a++)
	{
		size_t first = pinNodes[a], last = pinNodes[a + 1];
		float share = span > 0.0001f ? (reach[a + 1] - reach[a]) / span : 1.0f / sections;
		for (size_t link = first; link < last; link++)
			rest[link] = drawnLength * share / (float)(last - first);
	}

	std::vector<bool> pinned(links + 1, false);
	for (size_t a = 0; a < pins.size(); a++)
		pinned[pinNodes[a]] = true;

	rope.simulationDebt += deltaT / 1000.0f;
	int steps = std::min((int)(rope.simulationDebt / stepSeconds), maxStepsPerFrame);
	rope.simulationDebt = std::min(rope.simulationDebt - steps * stepSeconds, stepSeconds);

	for (int step = 0; step < steps; step++)
	{
		for (size_t a = 0; a <= links; a++)
		{
			if (pinned[a])
				continue;

			glm::vec3 moved = (rope.nodes[a] - rope.lastNodes[a]) * nodeDamping;
			rope.lastNodes[a] = rope.nodes[a];
			rope.nodes[a] += moved + glm::vec3(0, ropeGravity * stepSeconds * stepSeconds, 0);
		}

		for (int pass = 0; pass < relaxPasses; pass++)
		{
			for (size_t a = 0; a < pins.size(); a++)
				rope.nodes[pinNodes[a]] = pins[a];

			//Back and forth, so a correction at one end reaches the other in one pass instead of one link a pass
			for (size_t b = 0; b < links; b++)
			{
				size_t link = (pass & 1) ? links - 1 - b : b;
				glm::vec3 between = rope.nodes[link + 1] - rope.nodes[link];
				float distance = glm::length(between);

				//A rope can fold up shorter than it is, it just can't stretch
				if (distance <= rest[link] || distance < 0.00001f)
					continue;

				glm::vec3 correction = between * ((distance - rest[link]) / distance);
				bool firstPinned = pinned[link], secondPinned = pinned[link + 1];
				if (firstPinned && secondPinned)
					continue;

				if (firstPinned)
					rope.nodes[link + 1] -= correction;
				else if (secondPinned)
					rope.nodes[link] += correction;
				else
				{
					rope.nodes[link] += correction * 0.5f;
					rope.nodes[link + 1] -= correction * 0.5f;
				}
			}
		}
	}

	//Every frame, not only on a step, so the ends never trail what they're tied to
	for (size_t a = 0; a < pins.size(); a++)
		rope.nodes[pinNodes[a]] = pins[a];
}

void RopeRenderer::begin()
{
	vertices.clear();
	firsts.clear();
	counts.clear();
}

void RopeRenderer::add(Rope& rope, float deltaT)
{
	//Tied to something that hasn't arrived yet, or is gone with the rope's removal on its way
	if (!rope.isDrawable())
		return;

	simulate(rope, deltaT);

	const std::vector<glm::vec3>& nodes = rope.nodes;
	if (nodes.size() < 2)
		return;

	glm::vec4 color = glm::vec4(rope.getColor()) / 255.0f;

	firsts.push_back((GLint)(vertices.size() / vertexFloats));
	counts.push_back((GLsizei)nodes.size() * 2);

	float along = 0;
	glm::vec3 lastTangent(0, 1, 0);
	for (size_t a = 0; a < nodes.size(); a++)
	{
		if (a > 0)
			along += glm::distance(nodes[a], nodes[a - 1]);

		//The way from the node before to the node after, links folded onto each other keep the last good one
		glm::vec3 tangent = nodes[std::min(a + 1, nodes.size() - 1)] - nodes[a > 0 ? a - 1 : 0];
		float tangentLength = glm::length(tangent);
		if (tangentLength > 0.00001f)
			lastTangent = tangent / tangentLength;

		for (float edge : { -1.0f, 1.0f })
		{
			const float vertex[vertexFloats] = {
				nodes[a].x, nodes[a].y, nodes[a].z,
				lastTangent.x, lastTangent.y, lastTangent.z,
				edge, along, rope.getWidth(),
				color.r, color.g, color.b, color.a
			};
			vertices.insert(vertices.end(), vertex, vertex + vertexFloats);
		}
	}
}

void RopeRenderer::upload()
{
	if (vertices.empty())
		return;

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RopeRenderer::render(std::shared_ptr<ShaderManager> shaders, const glm::mat4* lightSpaceMatricies) const
{
	if (firsts.empty() || !shaders->ropeShader || !shaders->ropeShader->isCompiled())
		return;

	Program* program = shaders->ropeShader;
	program->use();
	glUniformMatrix4fv(program->getUniformLocation("lightSpaceMatricies"), 3, GL_FALSE, &lightSpaceMatricies[0][0][0]);

	//A ribbon facing the camera has no back, and water's reflection draws everything mirrored
	bool culling = glIsEnabled(GL_CULL_FACE);
	if (culling)
		glDisable(GL_CULL_FACE);

	glBindVertexArray(vao);
	glMultiDrawArrays(GL_TRIANGLE_STRIP, firsts.data(), counts.data(), (GLsizei)firsts.size());
	glBindVertexArray(0);

	if (culling)
		glEnable(GL_CULL_FACE);
}

RopeRenderer::RopeRenderer()
{
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	const GLsizei stride = vertexFloats * sizeof(float);
	const int sizes[4] = { 3, 3, 3, 4 };
	size_t offset = 0;
	for (int a = 0; a < 4; a++)
	{
		glEnableVertexAttribArray(a);
		glVertexAttribPointer(a, sizes[a], GL_FLOAT, GL_FALSE, stride, (void*)offset);
		offset += sizes[a] * sizeof(float);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

RopeRenderer::~RopeRenderer()
{
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

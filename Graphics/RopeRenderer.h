#pragma once

#include "../LandOfDran.h"
#include "ShaderSpecification.h"

class Rope;

/*
	Client only: works out how every rope hangs and draws them all from one buffer
	A rope's curve is never sent. Each client hangs a chain of nodes between wherever the rope's ends are drawn for
	it, pinned there and at the rope's bends, and lets it fall and settle, see simulate. A rope pulled to its full
	length is a straight line, which skips all of that
	They're drawn as ribbons facing the camera, see rope.vert, in the opaque pass so they're in water's reflection too
*/
class RopeRenderer
{
	GLuint vao = 0;
	GLuint vbo = 0;

	//Position, tangent, edge + distance along + width, color, which is what rope.vert reads
	static constexpr int vertexFloats = 13;

	std::vector<float> vertices;

	//One triangle strip per rope
	std::vector<GLint> firsts;
	std::vector<GLsizei> counts;

	//Moves a rope's nodes along by deltaT milliseconds
	void simulate(Rope& rope, float deltaT) const;

	public:

	//Once a frame, after everything's drawn position is known: starts over with no ropes
	void begin();

	//Hangs the rope for this frame and adds it to what gets drawn
	void add(Rope& rope, float deltaT);

	//After the last add
	void upload();

	void render(std::shared_ptr<ShaderManager> shaders, const glm::mat4* lightSpaceMatricies) const;

	RopeRenderer();
	~RopeRenderer();
};

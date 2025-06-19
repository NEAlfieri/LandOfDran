#include "BrickRenderer.h"

BrickChunk* foundChunk = nullptr;

bool StopAtFirstChunk(BrickChunk* val)
{
	foundChunk = val;
	return val;
}

void BrickRenderer::addBrick(BrickRenderData* brick)
{
	int chunkX = brick->x / 32;
	int chunkY = brick->y / 32;
	int chunkZ = brick->z / 32;

	//std::cout<< "Adding brick at " << chunkX << " " << chunkY << " " << chunkZ << "\n";

	int pos[3] = { chunkX, chunkY, chunkZ };

	foundChunk = nullptr;
	chunkTree.Search(pos, pos, StopAtFirstChunk);

	if (!foundChunk)
	{
		foundChunk = new BrickChunk(chunkX*32, chunkY*32, chunkZ*32, lastChunkId);
		lastChunkId++;
		chunkTree.Insert(pos, pos, foundChunk);
		chunks.push_back(foundChunk);
	}

	foundChunk->addBrick(brick);
}

void BrickRenderer::recompile()
{
	for (BrickChunk* chunk : chunks)
	{
		chunk->getFaces();
	}
}

void BrickRenderer::render(GLint brickChunkPos)
{
	for (BrickChunk* chunk : chunks)
	{
		chunk->render(brickChunkPos);
	}
}

void BrickRenderer::removeBrick(BrickRenderData* brick)
{
	int chunkX = brick->x / 32;
	int chunkY = brick->y / 32;
	int chunkZ = brick->z / 32;

	int pos[3] = { chunkX, chunkY, chunkZ };

	foundChunk = nullptr;
	chunkTree.Search(pos, pos, StopAtFirstChunk);

	if (!foundChunk)
	{
		error("Error: Tried to remove brick from non-existant chunk\n");
		return;
	}

	foundChunk->removeBrick(brick);
	if(foundChunk->getNumBricks() < 1)
	{
		chunks.erase(std::remove(chunks.begin(), chunks.end(), foundChunk), chunks.end());
		chunkTree.Remove(pos, pos, foundChunk);
		delete foundChunk;
	}
}

void BrickRenderData::debugPrint()
{
	std::cout << "BrickRenderData\n";
	std::cout << "x: " << x << " y: " << y << " z: " << z << "\n";
	std::cout << "w: " << w << " h: " << h << " l: " << l << "\n";
	std::cout << "brickColor: " << (int)brickColor << "\n";
	std::cout << "chunkId: " << chunkId << "\n";
	std::cout << "chunkIter: " << chunkIter << "\n";
	std::cout << "\n\n";
}

void BrickChunk::deleteAllBricks()
{
	for (BrickRenderData* brick : bricks)
		delete brick;
	bricks.clear();
	brickTree.RemoveAll();
}

void BrickChunk::addBrick(BrickRenderData* brick)
{
	char min[3] = { brick->x - chunkPosX, brick->y - chunkPosY, brick->z - chunkPosZ };
	char max[3] = { (brick->x - chunkPosX) + brick->w - 1, (brick->y - chunkPosY) + brick->h - 1, (brick->z - chunkPosZ) + brick->l - 1 };

	//std::cout<<"Chunk pos: " << chunkPosX << " " << chunkPosY << " " << chunkPosZ << "\n";
	//std::cout<< "Adding brick in chunk at " << (int)min[0] << " " << (int)min[1] << " " << (int)min[2] << "\n";

	brickTree.Insert(min, max, brick);
	brick->chunkIter = bricks.size();
	brick->chunkId = chunkId;
	bricks.push_back(brick);
}

void BrickChunk::removeBrick(BrickRenderData* brick)
{
	char min[3] = { brick->x - chunkPosX, brick->y - chunkPosY, brick->z - chunkPosZ };
	char max[3] = { (brick->x - chunkPosX) + brick->w, (brick->y - chunkPosY) + brick->h, (brick->z - chunkPosZ) + brick->l };
	brickTree.Remove(min, max, brick);
	bricks.erase(std::remove(bricks.begin(), bricks.end(), brick), bricks.end());
}

BrickRenderData *foundBrick = nullptr;

bool StopAtFirstHit(BrickRenderData* val) 
{ 
	foundBrick = val;
	return val;
}

void BrickChunk::getFaces(int dir, std::vector<glm::vec3>& positions, std::vector<glm::vec2>& sizes, std::vector<unsigned int>& directions)
{
	if (dir == 0)
		return; 

	for (unsigned int layer = 0; layer < 32; layer++)
	{
		unsigned int layerFaceMask[32];
		std::fill(layerFaceMask, layerFaceMask + 32, 0);

		for (unsigned int i = 0; i < 32 * 32; i++)
		{
			unsigned int row = i % 32;
			unsigned int col = (i / 32);

			//Get the brick at this position
			char pos[3] = { row, col, layer };

			if (abs(dir) == 2)
			{
				pos[0] = col;
				pos[1] = layer;
				pos[2] = row;
			}
			else if (abs(dir) == 3)
			{
				pos[0] = layer;
				pos[1] = row;
				pos[2] = col;
			}


			foundBrick = nullptr;
			brickTree.Search(pos, pos, StopAtFirstHit);
			BrickRenderData* brick = foundBrick;

			if (!brick)
				continue;

			//Todo: replace with bitwise operation
			if(abs(dir) == 1)
				pos[2] -= dir > 0 ? 1 : -1;
			else if (abs(dir) == 2)
				pos[1] -= dir > 0 ? 1 : -1;
			else if (abs(dir) == 3)
				pos[0] -= dir > 0 ? 1 : -1;

			foundBrick = nullptr;
			brickTree.Search(pos, pos, StopAtFirstHit);
			brick = foundBrick;

			if (brick)
				continue;

			layerFaceMask[row] = (1 << col) | layerFaceMask[row];
		}

		for (unsigned int row = 0; row < 32; row++)
		{
			unsigned int y = 0;
			while (y < 32)
			{
				y += std::countr_zero(layerFaceMask[row] >> y);
				if (y >= 32)
					break;

				unsigned int h = std::countr_one(layerFaceMask[row] >> y);
				unsigned int hAsMask = (h < 32U) ? ((1U << h) - 1U) : ~0U; //hAsMask would be 0b11111 if h is 5, 0b1111 if h is 4, etc...
				unsigned int mask = hAsMask << y;

				unsigned int w = 1;
				while (row + w < 32)
				{
					unsigned int next_row_h = (layerFaceMask[row + w] /* >> y */ ) & mask;

					if (next_row_h != mask)
						break;

					layerFaceMask[row + w] = layerFaceMask[row + w] & ~mask;
					++w;
				}
					
				//std::cout << "Layer: " << layer << " Adding face with width : " << w << " height : " << h << " row: " << row << " y : " << y << "\n";

				glm::vec3 pos = glm::vec3(row, y, layer);
				if (dir < 0)
				{
					pos.z++;
				}
				if (abs(dir) == 2)
				{
					//pos = glm::vec3(pos.y, pos.z, pos.x);
				}
				else if (abs(dir) == 1)
				{
					//pos = glm::vec3(pos.z, pos.x, pos.y);
				}
				positions.push_back(pos);

				sizes.push_back(glm::vec2(w, h));

				int glDir = dir + 3;
				if(glDir > 2)
					glDir--;
				directions.push_back(glDir);

				std::cout << "Size: " << w << "," << h << " direction: " << glDir << " Position: "<<pos.x<<","<<pos.y<<","<<pos.z<<"\n";

				/*if (dir < 0)
				{
					for (int i = 0; i < 6; i++)
						verts[verts.size() - 6 + i].z++;

					//For face culling, switch winding order
					std::swap(verts[verts.size() - 6], verts[verts.size() - 5]);
					std::swap(verts[verts.size() - 3], verts[verts.size() - 2]);
				}

				if (abs(dir) == 2)
				{
					for (int i = 0; i < 6; i++)
						verts[verts.size() - 6 + i] = glm::vec3(verts[verts.size() - 6 + i].y, verts[verts.size() - 6 + i].z, verts[verts.size() - 6 + i].x);
				}

				if (abs(dir) == 3)
				{
					for (int i = 0; i < 6; i++)
						verts[verts.size() - 6 + i] = glm::vec3(verts[verts.size() - 6 + i].y, verts[verts.size() - 6 + i].z, verts[verts.size() - 6 + i].x);
					for (int i = 0; i < 6; i++)
						verts[verts.size() - 6 + i] = glm::vec3(verts[verts.size() - 6 + i].y, verts[verts.size() - 6 + i].z, verts[verts.size() - 6 + i].x);
				}*/

				y += h;
			}
		}
	}
}

void BrickChunk::getFaces()
{
	glGenVertexArrays(1, &vao);
	glGenBuffers(4, buffers);

	std::vector<glm::vec3> facePositions;
	std::vector<glm::vec2> faceSizes;
	std::vector<unsigned int> faceDirections;

	for (int i = -3; i <= 3; i++)
		getFaces(i, facePositions,faceSizes,faceDirections);

	instanceCount = facePositions.size();

	glBindVertexArray(vao);

	std::vector<glm::vec2> verts;
	verts.push_back(glm::vec2(0, 0));
	verts.push_back(glm::vec2(1, 0));
	verts.push_back(glm::vec2(1, 1));
	verts.push_back(glm::vec2(0, 1));

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec2), &verts[0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, facePositions.size() * sizeof(glm::vec3), &facePositions[0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(1, 1);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ARRAY_BUFFER, faceSizes.size() * sizeof(glm::vec2), &faceSizes[0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(2, 1);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[3]);
	glBufferData(GL_ARRAY_BUFFER, faceDirections.size() * sizeof(unsigned int), &faceDirections[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 0, (void*)0);
	glVertexAttribDivisor(3, 1);

	glBindVertexArray(0);
}

void BrickChunk::render(GLint brickChunkPos)
{
	glUniform3f(brickChunkPos, chunkPosX, chunkPosY, chunkPosZ);
	glBindVertexArray(vao);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, instanceCount);
	glBindVertexArray(0);
}
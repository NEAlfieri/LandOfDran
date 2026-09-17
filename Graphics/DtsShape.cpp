#include "DtsShape.h"
#include "../Utility/FileFunctions.h"
#include "../Utility/StringFunctions.h"

#include <assimp/scene.h>

#include <filesystem>
#include <fstream>
#include <cstring>

/*
	A DTS file is a header, then one block of memory holding the whole shape, then the sequences
	and the material list written straight after it.

	That block is read through three cursors at once: one walking 32 bit values from the start of
	the block, one walking 16 bit values from start16, and one walking 8 bit values from start8.
	A given field always comes off the cursor of its own size, so reading the shape means keeping
	all three moving in step. Every so often the exporter wrote the same counter to all three, so
	checkGuard reading three matching numbers is a good sign we haven't lost our place.
*/
namespace
{
	//The only shape version Blockland's exporter writes, and so the only one we read
	static const int DtsVersion = 24;

	//Top two bits of a primitive's material field say how to read its indices
	static const unsigned int PrimitiveTriangles = 0x00000000;
	static const unsigned int PrimitiveStrip     = 0x40000000;
	static const unsigned int PrimitiveFan       = 0x80000000;
	static const unsigned int PrimitiveTypeMask  = 0xC0000000;
	//The rest of the field, once the type bits are off it
	static const unsigned int PrimitiveNoMaterial  = 0x10000000;
	static const unsigned int PrimitiveMaterialMask = 0x0FFFFFFF;

	//Mesh types, the number written in front of each mesh
	static const int MeshTypeStandard = 0;
	static const int MeshTypeSkin     = 1;
	static const int MeshTypeDecal    = 2;
	static const int MeshTypeSorted   = 3;
	static const int MeshTypeNull     = 4;

	struct DtsStream
	{
		const char* data = nullptr;
		size_t length = 0;

		//Byte offsets of each of the three cursors, and where each one has to stop
		size_t at32 = 0, end32 = 0;
		size_t at16 = 0, end16 = 0;
		size_t at8 = 0,  end8 = 0;

		//What the next guard should read, and whether anything has gone wrong yet
		int guard = 0;
		bool failed = false;
		std::string failure = "";

		void fail(const std::string& why)
		{
			if (!failed)
			{
				failed = true;
				failure = why;
			}
		}

		//Every read checks it stays inside its own part of the block, add-ons are not trusted input
		bool take(size_t& at, size_t end, size_t bytes)
		{
			if (failed)
				return false;
			if (bytes > end - at)
			{
				fail("ran off the end of the file");
				return false;
			}
			at += bytes;
			return true;
		}

		int get32()
		{
			size_t from = at32;
			if (!take(at32, end32, 4))
				return 0;
			int ret;
			memcpy(&ret, data + from, 4);
			return ret;
		}

		float getF32()
		{
			size_t from = at32;
			if (!take(at32, end32, 4))
				return 0;
			float ret;
			memcpy(&ret, data + from, 4);
			return ret;
		}

		short get16()
		{
			size_t from = at16;
			if (!take(at16, end16, 2))
				return 0;
			short ret;
			memcpy(&ret, data + from, 2);
			return ret;
		}

		signed char get8()
		{
			size_t from = at8;
			if (!take(at8, end8, 1))
				return 0;
			return (signed char)data[from];
		}

		//Reads a whole array of 32 bit values, or skips one we have no use for
		void skip32(size_t count) { take(at32, end32, count * 4); }
		void skip16(size_t count) { take(at16, end16, count * 2); }
		void skip8(size_t count)  { take(at8,  end8,  count); }

		glm::vec3 getPoint()
		{
			float x = getF32();
			float y = getF32();
			float z = getF32();
			return glm::vec3(x, y, z);
		}

		/*
			A rotation is four 16 bit values. Torque stores the conjugate of the rotation
			it means, so the parts other than w get flipped back on the way in.
		*/
		glm::quat getQuat()
		{
			float x = get16() / 32767.0f;
			float y = get16() / 32767.0f;
			float z = get16() / 32767.0f;
			float w = get16() / 32767.0f;
			return glm::quat(w, -x, -y, -z);
		}

		//Same counter written to all three cursors, so a mismatch means we've lost our place
		void checkGuard(const std::string& where)
		{
			int a = get32();
			int b = get16();
			int c = get8();

			if (failed)
				return;

			if (a != guard || b != guard || c != guard)
				fail("file is not laid out the way a version 24 shape should be, at " + where);

			guard++;
		}

		//Counts are used to size allocations, so nothing is trusted until it's been checked
		bool sane(int count, int most, const std::string& what)
		{
			if (failed)
				return false;
			if (count < 0 || count > most)
			{
				fail("a nonsense number of " + what + " (" + std::to_string(count) + ")");
				return false;
			}
			return true;
		}
	};

	struct DtsShapeNode
	{
		int name = -1;
		int parent = -1;
		glm::vec3 defaultTranslation = glm::vec3(0);
		glm::quat defaultRotation = glm::quat(1, 0, 0, 0);
	};

	struct DtsObject
	{
		int name = -1;
		int numMeshes = 0;
		int startMesh = 0;
		int node = -1;
	};

	struct DtsDetail
	{
		int name = -1;
		int subShape = 0;
		int objectDetail = 0;
		float size = 0;
	};

	struct DtsPrimitive
	{
		int start = 0;
		int count = 0;
		unsigned int material = 0;
	};

	struct DtsMesh
	{
		int type = MeshTypeNull;
		std::vector<glm::vec3> verts;
		std::vector<glm::vec3> norms;
		std::vector<glm::vec2> tverts;
		std::vector<DtsPrimitive> primitives;
		std::vector<unsigned short> indices;
		int vertsPerFrame = 0;
	};

	struct DtsSequenceData
	{
		int name = -1;
		int numKeyframes = 0;
		float duration = 0;
		bool cyclic = false;
		int baseRotation = 0;
		int baseTranslation = 0;
		//Which nodes the sequence has keys for, by node index
		std::vector<bool> rotationMatters;
		std::vector<bool> translationMatters;
	};

	struct DtsShapeData
	{
		std::vector<DtsShapeNode> nodes;
		std::vector<DtsObject> objects;
		std::vector<DtsDetail> details;
		std::vector<DtsMesh> meshes;
		std::vector<std::string> names;
		std::vector<std::string> materials;
		std::vector<DtsSequenceData> sequences;

		//Keyframes every sequence indexes into
		std::vector<glm::quat> nodeRotations;
		std::vector<glm::vec3> nodeTranslations;

		std::vector<int> subShapeFirstNode;
		std::vector<int> subShapeFirstObject;
		std::vector<int> subShapeNumNodes;
		std::vector<int> subShapeNumObjects;

		const std::string& nameOf(int index) const
		{
			static const std::string nothing = "";
			return (index >= 0 && index < (int)names.size()) ? names[index] : nothing;
		}
	};

	//Reads one mesh, which is where nearly all of a shape's bytes are
	void readMesh(DtsStream& s, DtsMesh& mesh)
	{
		mesh.type = s.get32();

		if (mesh.type == MeshTypeNull || s.failed)
			return;

		s.checkGuard("the start of a mesh");

		int numFrames = s.get32();
		int numMatFrames = s.get32();
		int parentMesh = s.get32();
		s.skip32(6 + 3 + 1);				//Its own bounding box, middle, and radius, all of which we work out ourselves

		int numVerts = s.get32();
		if (!s.sane(numVerts, 1 << 22, "vertices"))
			return;
		mesh.verts.resize(numVerts);
		for (int a = 0; a < numVerts; a++)
			mesh.verts[a] = s.getPoint();

		int numTVerts = s.get32();
		if (!s.sane(numTVerts, 1 << 22, "texture coordinates"))
			return;
		mesh.tverts.resize(numTVerts);
		for (int a = 0; a < numTVerts; a++)
		{
			float u = s.getF32();
			float v = s.getF32();
			mesh.tverts[a] = glm::vec2(u, v);
		}

		mesh.norms.resize(numVerts);
		for (int a = 0; a < numVerts; a++)
			mesh.norms[a] = s.getPoint();

		s.skip8(numVerts);					//A cheaper normal per vertex we have no use for, we have the real ones

		int numPrimitives = s.get32();
		if (!s.sane(numPrimitives, 1 << 20, "primitives"))
			return;
		mesh.primitives.resize(numPrimitives);
		//A primitive's start and length come off the 16 bit cursor, but its material off the 32 bit one
		for (int a = 0; a < numPrimitives; a++)
		{
			mesh.primitives[a].start = (unsigned short)s.get16();
			mesh.primitives[a].count = (unsigned short)s.get16();
		}
		for (int a = 0; a < numPrimitives; a++)
			mesh.primitives[a].material = (unsigned int)s.get32();

		int numIndices = s.get32();
		if (!s.sane(numIndices, 1 << 22, "indices"))
			return;
		mesh.indices.resize(numIndices);
		for (int a = 0; a < numIndices; a++)
			mesh.indices[a] = (unsigned short)s.get16();

		int numMergeIndices = s.get32();
		if (!s.sane(numMergeIndices, 1 << 22, "merge indices"))
			return;
		s.skip16(numMergeIndices);			//Only used for shrinking a mesh between detail levels

		mesh.vertsPerFrame = s.get32();
		s.get32();							//Flags, all of which are about billboards

		s.checkGuard("the end of a mesh");

		/*
			A skinned mesh carries its bones after all of that. Nothing in the game moves a DTS
			shape by bones, but the bytes still have to be walked past to find the next mesh.
		*/
		if (mesh.type == MeshTypeSkin)
		{
			int numInitialVerts = s.get32();
			if (!s.sane(numInitialVerts, 1 << 22, "skin vertices"))
				return;
			s.skip32(numInitialVerts * 3);	//Positions before any bone moved them
			s.skip32(numInitialVerts * 3);	//And their normals
			s.skip8(numInitialVerts);

			int numTransforms = s.get32();
			if (!s.sane(numTransforms, 1 << 16, "bone transforms"))
				return;
			s.skip32(numTransforms * 16);

			int numWeights = s.get32();
			if (!s.sane(numWeights, 1 << 22, "bone weights"))
				return;
			s.skip32(numWeights);			//Which vertex
			s.skip32(numWeights);			//Which bone
			s.skip32(numWeights);			//And how much of it

			int numBoneNodes = s.get32();
			if (!s.sane(numBoneNodes, 1 << 16, "bones"))
				return;
			s.skip32(numBoneNodes);

			s.checkGuard("the end of a skinned mesh");
		}
		else if (mesh.type == MeshTypeSorted)
		{
			//Sorted meshes carry draw order for every way you can look at them, which nothing exports any more
			s.fail("has a sorted mesh, which isn't supported");
		}

		//Only the first frame of an animated-by-vertex mesh is kept, nothing in the game plays those
		if (numFrames > 1 || numMatFrames > 1)
		{
			if (mesh.vertsPerFrame > 0 && mesh.vertsPerFrame < numVerts)
			{
				mesh.verts.resize(mesh.vertsPerFrame);
				mesh.norms.resize(mesh.vertsPerFrame);
			}
		}

		//A mesh that borrows another's vertices, which our exporters don't write
		if (parentMesh >= 0)
			mesh.verts.clear();
	}

	bool readShape(const std::string& filePath, DtsShapeData& shape)
	{
		std::ifstream file(filePath.c_str(), std::ios::binary);
		if (!file.is_open())
		{
			error("Could not open DTS file " + filePath);
			return false;
		}

		std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		if (contents.length() < 16)
		{
			error(filePath + " is too short to be a DTS shape");
			return false;
		}

		unsigned int header[4];
		memcpy(header, contents.data(), 16);

		int version = header[0] & 0xFF;
		if (version != DtsVersion)
		{
			error(filePath + " is a version " + std::to_string(version) + " DTS shape, only version "
				+ std::to_string(DtsVersion) + " can be read");
			return false;
		}

		//Sizes are in 32 bit words, and say where in the block the smaller cursors start
		size_t blockWords = header[1];
		size_t start16 = header[2];
		size_t start8 = header[3];

		if (start16 > start8 || start8 > blockWords || 16 + blockWords * 4 > contents.length())
		{
			error(filePath + " has a DTS header that points outside of the file");
			return false;
		}

		DtsStream s;
		s.data = contents.data() + 16;
		s.length = blockWords * 4;
		s.at32 = 0;				s.end32 = start16 * 4;
		s.at16 = start16 * 4;	s.end16 = start8 * 4;
		s.at8 = start8 * 4;		s.end8 = blockWords * 4;

		int numNodes = s.get32();
		int numObjects = s.get32();
		int numDecals = s.get32();
		int numSubShapes = s.get32();
		int numIflMaterials = s.get32();
		int numNodeRotations = s.get32();
		int numNodeTranslations = s.get32();
		int numNodeUniformScales = s.get32();
		int numNodeAlignedScales = s.get32();
		int numNodeArbitraryScales = s.get32();
		int numGroundFrames = s.get32();
		int numObjectStates = s.get32();
		int numDecalStates = s.get32();
		int numTriggers = s.get32();
		int numDetails = s.get32();
		int numMeshes = s.get32();
		int numNames = s.get32();
		s.getF32();							//Smallest size the shape is still drawn at
		s.get32();							//And which detail level that is

		if (!s.sane(numNodes, 1 << 16, "nodes") || !s.sane(numObjects, 1 << 16, "objects")
			|| !s.sane(numSubShapes, 1 << 12, "sub shapes") || !s.sane(numDetails, 1 << 12, "detail levels")
			|| !s.sane(numMeshes, 1 << 16, "meshes") || !s.sane(numNames, 1 << 16, "names")
			|| !s.sane(numDecals, 1 << 16, "decals") || !s.sane(numIflMaterials, 1 << 16, "animated materials")
			|| !s.sane(numNodeRotations, 1 << 22, "rotation keys") || !s.sane(numNodeTranslations, 1 << 22, "translation keys")
			|| !s.sane(numObjectStates, 1 << 20, "object states") || !s.sane(numDecalStates, 1 << 20, "decal states")
			|| !s.sane(numTriggers, 1 << 16, "triggers") || !s.sane(numGroundFrames, 1 << 20, "ground frames")
			|| !s.sane(numNodeUniformScales, 1 << 20, "scale keys") || !s.sane(numNodeAlignedScales, 1 << 20, "scale keys")
			|| !s.sane(numNodeArbitraryScales, 1 << 20, "scale keys"))
		{
			error(filePath + ": " + s.failure);
			return false;
		}

		s.checkGuard("the counts at the start");

		s.skip32(2 + 3 + 6);				//The whole shape's radius, tube radius, middle, and bounding box

		s.checkGuard("the shape's bounds");

		shape.nodes.resize(numNodes);
		for (int a = 0; a < numNodes; a++)
		{
			shape.nodes[a].name = s.get32();
			shape.nodes[a].parent = s.get32();
			s.skip32(3);					//Its first object, first child, and next sibling, which we work out ourselves
		}

		s.checkGuard("the nodes");

		shape.objects.resize(numObjects);
		for (int a = 0; a < numObjects; a++)
		{
			shape.objects[a].name = s.get32();
			shape.objects[a].numMeshes = s.get32();
			shape.objects[a].startMesh = s.get32();
			shape.objects[a].node = s.get32();
			s.skip32(2);					//Next sibling and first decal
		}

		s.checkGuard("the objects");

		s.skip32(numDecals * 5);
		s.checkGuard("the decals");

		s.skip32(numIflMaterials * 5);
		s.checkGuard("the animated materials");

		shape.subShapeFirstNode.resize(numSubShapes);
		shape.subShapeFirstObject.resize(numSubShapes);
		shape.subShapeNumNodes.resize(numSubShapes);
		shape.subShapeNumObjects.resize(numSubShapes);

		for (int a = 0; a < numSubShapes; a++)
			shape.subShapeFirstNode[a] = s.get32();
		for (int a = 0; a < numSubShapes; a++)
			shape.subShapeFirstObject[a] = s.get32();
		s.skip32(numSubShapes);				//First decal of each

		s.checkGuard("where each sub shape starts");

		for (int a = 0; a < numSubShapes; a++)
			shape.subShapeNumNodes[a] = s.get32();
		for (int a = 0; a < numSubShapes; a++)
			shape.subShapeNumObjects[a] = s.get32();
		s.skip32(numSubShapes);				//How many decals each has

		s.checkGuard("how big each sub shape is");

		/*
			Rotations come off the 16 bit cursor and translations off the 32 bit one, so the two
			loops below are really one pass: the order within each cursor is what matters.
		*/
		for (int a = 0; a < numNodes; a++)
			shape.nodes[a].defaultRotation = s.getQuat();
		for (int a = 0; a < numNodes; a++)
			shape.nodes[a].defaultTranslation = s.getPoint();

		shape.nodeTranslations.resize(numNodeTranslations);
		for (int a = 0; a < numNodeTranslations; a++)
			shape.nodeTranslations[a] = s.getPoint();

		shape.nodeRotations.resize(numNodeRotations);
		for (int a = 0; a < numNodeRotations; a++)
			shape.nodeRotations[a] = s.getQuat();

		s.checkGuard("the keyframes");

		s.skip32(numNodeUniformScales);
		s.skip32(numNodeAlignedScales * 3);
		s.skip32(numNodeArbitraryScales * 3);
		s.skip16(numNodeArbitraryScales * 4);

		s.checkGuard("the scale keyframes");

		s.skip32(numGroundFrames * 3);
		s.skip16(numGroundFrames * 4);

		s.checkGuard("the ground frames");

		s.skip32(numObjectStates * 3);
		s.checkGuard("the object states");

		s.skip32(numDecalStates);
		s.checkGuard("the decal states");

		s.skip32(numTriggers * 2);
		s.checkGuard("the triggers");

		shape.details.resize(numDetails);
		for (int a = 0; a < numDetails; a++)
		{
			shape.details[a].name = s.get32();
			shape.details[a].subShape = s.get32();
			shape.details[a].objectDetail = s.get32();
			shape.details[a].size = s.getF32();
			s.skip32(3);					//Average and biggest error, and polygon count, none of which we use
		}

		s.checkGuard("the detail levels");

		shape.meshes.resize(numMeshes);
		for (int a = 0; a < numMeshes; a++)
		{
			readMesh(s, shape.meshes[a]);
			if (s.failed)
			{
				error(filePath + ": " + s.failure);
				return false;
			}
		}

		s.checkGuard("the meshes");

		shape.names.resize(numNames);
		for (int a = 0; a < numNames; a++)
		{
			std::string name = "";
			while (true)
			{
				char letter = (char)s.get8();
				if (letter == 0 || s.failed)
					break;
				name += letter;
			}
			shape.names[a] = name;
		}

		s.checkGuard("the names");

		if (s.failed)
		{
			error(filePath + ": " + s.failure);
			return false;
		}

		/*
			Sequences and the material list were written after the block, one field after another,
			so from here it's an ordinary read through the rest of the file.
		*/
		size_t at = 16 + blockWords * 4;

		auto readInt = [&](int& out) -> bool
		{
			if (at + 4 > contents.length())
				return false;
			memcpy(&out, contents.data() + at, 4);
			at += 4;
			return true;
		};
		auto readFloat = [&](float& out) -> bool
		{
			if (at + 4 > contents.length())
				return false;
			memcpy(&out, contents.data() + at, 4);
			at += 4;
			return true;
		};
		//A set of node numbers: how many it was sized for, how many words that took, then the words
		auto readSet = [&](std::vector<bool>& out) -> bool
		{
			int sized = 0, numWords = 0;
			if (!readInt(sized) || !readInt(numWords))
				return false;
			if (numWords < 0 || numWords > (1 << 16))
				return false;

			out.assign(numNodes, false);
			for (int a = 0; a < numWords; a++)
			{
				int word = 0;
				if (!readInt(word))
					return false;
				for (int bit = 0; bit < 32; bit++)
				{
					int node = a * 32 + bit;
					if (node < numNodes && (((unsigned int)word) & (1u << bit)))
						out[node] = true;
				}
			}
			return true;
		};

		int numSequences = 0;
		if (!readInt(numSequences) || numSequences < 0 || numSequences > (1 << 12))
		{
			error(filePath + " has a nonsense number of sequences");
			return false;
		}

		shape.sequences.resize(numSequences);
		for (int a = 0; a < numSequences; a++)
		{
			DtsSequenceData& sequence = shape.sequences[a];
			int flags = 0, ignored = 0;
			float ignoredFloat = 0;

			bool ok = readInt(sequence.name)
				&& readInt(flags)
				&& readInt(sequence.numKeyframes)
				&& readFloat(sequence.duration)
				&& readInt(ignored)						//Priority
				&& readInt(ignored)						//First ground frame
				&& readInt(ignored)						//And how many of them
				&& readInt(sequence.baseRotation)
				&& readInt(sequence.baseTranslation)
				&& readInt(ignored)						//Where its scale keys start
				&& readInt(ignored)						//Its first object state
				&& readInt(ignored)						//Its first decal state
				&& readInt(ignored)						//Its first trigger
				&& readInt(ignored)						//And how many of those
				&& readFloat(ignoredFloat);				//What frame the exporter started from

			std::vector<bool> unused;
			ok = ok && readSet(sequence.rotationMatters)
				&& readSet(sequence.translationMatters)
				&& readSet(unused)						//Nodes it scales
				&& readSet(unused)						//Decals it changes
				&& readSet(unused)						//Animated materials it changes
				&& readSet(unused)						//Objects it hides or shows
				&& readSet(unused)						//Objects whose vertices it animates
				&& readSet(unused);						//Objects whose material it swaps

			if (!ok || sequence.numKeyframes < 0 || sequence.numKeyframes > (1 << 20))
			{
				error(filePath + " has a sequence that could not be read");
				return false;
			}

			//Bit 1 of the flags marks a sequence that was exported to loop
			sequence.cyclic = (flags & 2) != 0;
		}

		if (at >= contents.length())
		{
			error(filePath + " is missing its material list");
			return false;
		}

		at++;									//What version the material list was written at
		int numMaterials = 0;
		if (!readInt(numMaterials) || numMaterials < 0 || numMaterials > (1 << 12))
		{
			error(filePath + " has a nonsense number of materials");
			return false;
		}

		shape.materials.resize(numMaterials);
		for (int a = 0; a < numMaterials; a++)
		{
			if (at >= contents.length())
			{
				error(filePath + " ends in the middle of its material names");
				return false;
			}
			size_t nameLength = (unsigned char)contents[at];
			at++;
			if (at + nameLength > contents.length())
			{
				error(filePath + " ends in the middle of its material names");
				return false;
			}
			shape.materials[a] = contents.substr(at, nameLength);
			at += nameLength;
		}

		return true;
	}

	/*
		DTS materials are just a name, and the texture is whatever file sits next to the shape under
		that name. Add-ons are nowhere near consistent about capitalisation or which image format
		they used, so everything in the folder gets compared without case.
	*/
	std::string findTexture(const std::string& folder, const std::string& materialName)
	{
		static const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp" };

		std::string wanted = lowercase(materialName);

		//The name may already have an extension on it, in which case it's the whole file name
		std::error_code problem;
		std::filesystem::directory_iterator files(folder.length() > 0 ? folder : ".", problem);
		if (problem)
			return "";

		std::string fallback = "";
		for (const auto& file : files)
		{
			if (!file.is_regular_file())
				continue;

			std::string name = file.path().filename().string();
			std::string lower = lowercase(name);

			if (lower == wanted)
				return folder + name;

			for (const std::string& extension : extensions)
			{
				if (lower == wanted + extension)
					return folder + name;
			}

			//A shape asking for "black" when the folder has "black.1.png" is better than no texture at all
			if (fallback.length() < 1 && lower.rfind(wanted + ".", 0) == 0)
				fallback = folder + name;
		}

		return fallback;
	}

	//Turns one of a mesh's primitives into plain triangles, whichever way it was written
	/*
		Turns one of a mesh's primitives into plain triangles, whichever way it was written.

		Torque winds its triangles the opposite way round from the front face we draw, so every one
		comes out reversed here. Left alone, a shape's faces all end up facing inward: with backface
		culling a closed model still has the right outline, since the far side's back faces stand in
		for the near side's front ones, but it's lit by the normals of the surface behind it, which
		is what makes it look inside out.
	*/
	void addTriangles(const DtsMesh& mesh, const DtsPrimitive& primitive, std::vector<unsigned int>& out)
	{
		unsigned int kind = primitive.material & PrimitiveTypeMask;

		auto index = [&](int at) -> unsigned int
		{
			int which = primitive.start + at;
			if (which < 0 || which >= (int)mesh.indices.size())
				return 0;
			return mesh.indices[which];
		};

		//Every triangle goes in with its last two corners swapped, which is the reversal above
		auto addTriangle = [&](unsigned int one, unsigned int two, unsigned int three)
		{
			out.push_back(one);
			out.push_back(three);
			out.push_back(two);
		};

		if (kind == PrimitiveStrip)
		{
			for (int a = 0; a + 2 < primitive.count; a++)
			{
				unsigned int one = index(a), two = index(a + 1), three = index(a + 2);
				//A strip repeats a vertex to turn a corner, which leaves triangles with no area in it
				if (one == two || two == three || one == three)
					continue;

				//Every other triangle in a strip is wound the other way round
				if (a % 2 == 0)
					addTriangle(one, two, three);
				else
					addTriangle(two, one, three);
			}
		}
		else if (kind == PrimitiveFan)
		{
			for (int a = 1; a + 1 < primitive.count; a++)
				addTriangle(index(0), index(a), index(a + 1));
		}
		else	//PrimitiveTriangles
		{
			for (int a = 0; a + 2 < primitive.count; a += 3)
				addTriangle(index(a), index(a + 1), index(a + 2));
		}
	}

	glm::vec3 toGlm(const aiVector3D& in)
	{
		return glm::vec3(in.x, in.y, in.z);
	}

	aiString toAiString(const std::string& in)
	{
		aiString ret;
		ret.Set(in);
		return ret;
	}
}

aiScene * loadDtsScene(const std::string &filePath, std::vector<DtsSequence> * sequences)
{
	scope("loadDtsScene");

	DtsShapeData shape;
	if (!readShape(filePath, shape))
		return nullptr;

	if (shape.details.size() < 1 || shape.nodes.size() < 1)
	{
		error(filePath + " has no detail levels or no nodes");
		return nullptr;
	}

	std::string folder = getFolderFromPath(filePath);

	/*
		A shape can hold the same thing at several levels of detail. The game has no use for the
		cheaper ones, so the biggest one it was exported at is the one that gets loaded.
	*/
	size_t bestDetail = 0;
	for (size_t a = 1; a < shape.details.size(); a++)
	{
		if (shape.details[a].size > shape.details[bestDetail].size)
			bestDetail = a;
	}

	const DtsDetail& detail = shape.details[bestDetail];
	if (detail.subShape < 0 || detail.subShape >= (int)shape.subShapeFirstObject.size())
	{
		error(filePath + " has a detail level pointing at a sub shape that isn't there");
		return nullptr;
	}

	aiScene* scene = new aiScene();

	//Every material the shape names becomes one of ours, pointing at the image file next to it
	scene->mNumMaterials = (unsigned int)shape.materials.size();
	if (scene->mNumMaterials > 0)
	{
		scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
		for (unsigned int a = 0; a < scene->mNumMaterials; a++)
		{
			aiMaterial* material = new aiMaterial();
			aiString name = toAiString(shape.materials[a]);
			material->AddProperty(&name, AI_MATKEY_NAME);

			std::string texture = findTexture(folder, shape.materials[a]);
			if (texture.length() > 0)
			{
				//Model looks textures up next to the model file, so only the file name goes in
				aiString file = toAiString(getFileFromPath(texture));
				material->AddProperty(&file, AI_MATKEY_TEXTURE_DIFFUSE(0));
			}
			else
				error("No texture next to " + filePath + " for material " + shape.materials[a]);

			scene->mMaterials[a] = material;
		}
	}

	/*
		Each object of the chosen detail level turns into one mesh, named after the object, with
		one exception: a mesh whose triangles don't all use the same material has to be split up,
		since a mesh of ours only has the one.
	*/
	std::vector<aiMesh*> meshes;
	//Which meshes ended up hanging off each node
	std::vector<std::vector<unsigned int>> meshesOfNode(shape.nodes.size());

	int firstObject = shape.subShapeFirstObject[detail.subShape];
	int numObjects = shape.subShapeNumObjects[detail.subShape];

	for (int a = firstObject; a < firstObject + numObjects; a++)
	{
		if (a < 0 || a >= (int)shape.objects.size())
			continue;

		const DtsObject& object = shape.objects[a];
		if (detail.objectDetail >= object.numMeshes)
			continue;						//This object isn't drawn at this level of detail

		int which = object.startMesh + detail.objectDetail;
		if (which < 0 || which >= (int)shape.meshes.size())
			continue;

		const DtsMesh& mesh = shape.meshes[which];
		if (mesh.type == MeshTypeNull || mesh.type == MeshTypeDecal || mesh.verts.size() < 1)
			continue;

		//Gather the mesh's triangles under the material each one uses
		std::map<int, std::vector<unsigned int>> byMaterial;
		for (const DtsPrimitive& primitive : mesh.primitives)
		{
			int material = (primitive.material & PrimitiveNoMaterial)
				? -1 : (int)(primitive.material & PrimitiveMaterialMask);
			if (material >= (int)shape.materials.size())
				material = -1;

			addTriangles(mesh, primitive, byMaterial[material]);
		}

		for (const auto& [material, indices] : byMaterial)
		{
			if (indices.size() < 3)
				continue;

			aiMesh* out = new aiMesh();

			std::string name = shape.nameOf(object.name);
			//Only say which material it was split off for when there was actually a split
			if (byMaterial.size() > 1 && material >= 0)
				name += "_" + shape.materials[material];
			out->mName = toAiString(name);

			out->mMaterialIndex = material >= 0 ? (unsigned int)material : 0;
			out->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

			unsigned int numVerts = (unsigned int)mesh.verts.size();
			out->mNumVertices = numVerts;
			out->mVertices = new aiVector3D[numVerts];
			out->mNormals = new aiVector3D[numVerts];
			for (unsigned int b = 0; b < numVerts; b++)
			{
				out->mVertices[b] = aiVector3D(mesh.verts[b].x, mesh.verts[b].y, mesh.verts[b].z);
				glm::vec3 normal = b < mesh.norms.size() ? mesh.norms[b] : glm::vec3(0, 0, 1);
				out->mNormals[b] = aiVector3D(normal.x, normal.y, normal.z);
			}

			//Texture coordinates run the other way up from ours, the same flip a FlipUVs line does
			if (mesh.tverts.size() >= numVerts)
			{
				out->mNumUVComponents[0] = 2;
				out->mTextureCoords[0] = new aiVector3D[numVerts];
				for (unsigned int b = 0; b < numVerts; b++)
					out->mTextureCoords[0][b] = aiVector3D(mesh.tverts[b].x, 1.0f - mesh.tverts[b].y, 0);
			}

			out->mNumFaces = (unsigned int)(indices.size() / 3);
			out->mFaces = new aiFace[out->mNumFaces];
			for (unsigned int b = 0; b < out->mNumFaces; b++)
			{
				aiFace& face = out->mFaces[b];
				face.mNumIndices = 3;
				face.mIndices = new unsigned int[3];
				for (unsigned int c = 0; c < 3; c++)
				{
					unsigned int index = indices[b * 3 + c];
					face.mIndices[c] = index < numVerts ? index : 0;
				}
			}

			/*
				Tangents, which the shape doesn't carry and which a normal map can't be drawn without. Assimp
				works them out for an FBX whose descriptor asks for CalcTangentSpace, but this scene never
				goes through Assimp's post processing, so they're worked out here the same way: from which
				way the texture coordinates run across each triangle, summed over the triangles at a vertex
				and then squared up to its normal.

				A triangle whose texture coordinates cover no area says nothing about direction and is left
				out, and a flat coloured Blockland shape is full of those since every corner of a panel can
				sit on the same texel. A vertex only such triangles touch gets any direction square to its
				normal: a normal map sampled at a single point is flat there anyway, so the direction it's
				read along makes no visible difference
			*/
			if (out->mTextureCoords[0])
			{
				std::vector<glm::vec3> tangentSum(numVerts, glm::vec3(0));
				std::vector<glm::vec3> bitangentSum(numVerts, glm::vec3(0));
				//Every mesh split off an object carries the object's whole vertex list, so only count the ones this one draws
				std::vector<bool> used(numVerts, false);

				for (unsigned int b = 0; b < out->mNumFaces; b++)
				{
					const unsigned int* corner = out->mFaces[b].mIndices;
					for (unsigned int c = 0; c < 3; c++)
						used[corner[c]] = true;

					glm::vec3 p0 = toGlm(out->mVertices[corner[0]]);
					glm::vec3 p1 = toGlm(out->mVertices[corner[1]]);
					glm::vec3 p2 = toGlm(out->mVertices[corner[2]]);
					glm::vec2 w0(out->mTextureCoords[0][corner[0]].x, out->mTextureCoords[0][corner[0]].y);
					glm::vec2 w1(out->mTextureCoords[0][corner[1]].x, out->mTextureCoords[0][corner[1]].y);
					glm::vec2 w2(out->mTextureCoords[0][corner[2]].x, out->mTextureCoords[0][corner[2]].y);

					glm::vec3 e1 = p1 - p0, e2 = p2 - p0;
					glm::vec2 d1 = w1 - w0, d2 = w2 - w0;
					float area = d1.x * d2.y - d2.x * d1.y;
					if (std::abs(area) < 1e-12f)
						continue;

					float r = 1.0f / area;
					glm::vec3 tangent = (e1 * d2.y - e2 * d1.y) * r;
					glm::vec3 bitangent = (e2 * d1.x - e1 * d2.x) * r;
					for (unsigned int c = 0; c < 3; c++)
					{
						tangentSum[corner[c]] += tangent;
						bitangentSum[corner[c]] += bitangent;
					}
				}

				out->mTangents = new aiVector3D[numVerts];
				out->mBitangents = new aiVector3D[numVerts];
				unsigned int guessed = 0, drawn = 0;

				for (unsigned int b = 0; b < numVerts; b++)
				{
					glm::vec3 normal = toGlm(out->mNormals[b]);
					if (glm::length2(normal) < 1e-12f)
						normal = glm::vec3(0, 0, 1);
					normal = glm::normalize(normal);

					//Gram-Schmidt, so the tangent lies in the surface even where the summed triangles disagree
					glm::vec3 tangent = tangentSum[b] - normal * glm::dot(normal, tangentSum[b]);
					if (glm::length2(tangent) < 1e-12f)
					{
						//Any direction square to the normal, crossing it with whichever axis it leans away from most
						glm::vec3 axis = std::abs(normal.x) < std::abs(normal.y)
							? (std::abs(normal.x) < std::abs(normal.z) ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1))
							: (std::abs(normal.y) < std::abs(normal.z) ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1));
						tangent = glm::cross(normal, axis);
						if (used[b])
							guessed++;
					}
					if (used[b])
						drawn++;
					tangent = glm::normalize(tangent);

					//Handedness from the triangles, so a mirrored texture layout reads its map the right way round
					glm::vec3 bitangent = glm::cross(normal, tangent);
					if (glm::dot(bitangent, bitangentSum[b]) < 0.0f)
						bitangent = -bitangent;

					out->mTangents[b] = aiVector3D(tangent.x, tangent.y, tangent.z);
					out->mBitangents[b] = aiVector3D(bitangent.x, bitangent.y, bitangent.z);
				}

				if (guessed > 0)
					debug(name + ": " + std::to_string(guessed) + " of " + std::to_string(drawn) + " vertices had no texture coordinate area to take a tangent from");
			}

			if (object.node >= 0 && object.node < (int)shape.nodes.size())
				meshesOfNode[object.node].push_back((unsigned int)meshes.size());

			meshes.push_back(out);
		}
	}

	if (meshes.size() < 1)
	{
		error(filePath + " has no meshes that could be drawn");
		delete scene;
		return nullptr;
	}

	scene->mNumMeshes = (unsigned int)meshes.size();
	scene->mMeshes = new aiMesh * [scene->mNumMeshes];
	for (unsigned int a = 0; a < scene->mNumMeshes; a++)
		scene->mMeshes[a] = meshes[a];

	/*
		The shape's nodes become ours underneath one node of our own, which holds the turn from
		the Z up world DTS shapes are built in into the Y up one our models live in.
	*/
	std::vector<aiNode*> nodes(shape.nodes.size(), nullptr);
	std::vector<std::vector<int>> childrenOf(shape.nodes.size());
	std::vector<int> roots;

	for (size_t a = 0; a < shape.nodes.size(); a++)
	{
		int parent = shape.nodes[a].parent;
		if (parent >= 0 && parent < (int)shape.nodes.size() && parent != (int)a)
			childrenOf[parent].push_back((int)a);
		else
			roots.push_back((int)a);
	}

	for (size_t a = 0; a < shape.nodes.size(); a++)
	{
		aiNode* node = new aiNode();
		node->mName = toAiString(shape.nameOf(shape.nodes[a].name));

		glm::mat4 transform = glm::translate(shape.nodes[a].defaultTranslation)
			* glm::toMat4(shape.nodes[a].defaultRotation);

		//Ours are the other way round from a glm matrix, and hold the movement along the top
		node->mTransformation = aiMatrix4x4(
			transform[0][0], transform[1][0], transform[2][0], transform[3][0],
			transform[0][1], transform[1][1], transform[2][1], transform[3][1],
			transform[0][2], transform[1][2], transform[2][2], transform[3][2],
			transform[0][3], transform[1][3], transform[2][3], transform[3][3]);

		node->mNumMeshes = (unsigned int)meshesOfNode[a].size();
		if (node->mNumMeshes > 0)
		{
			node->mMeshes = new unsigned int[node->mNumMeshes];
			for (unsigned int b = 0; b < node->mNumMeshes; b++)
				node->mMeshes[b] = meshesOfNode[a][b];
		}

		nodes[a] = node;
	}

	for (size_t a = 0; a < shape.nodes.size(); a++)
	{
		aiNode* node = nodes[a];
		node->mNumChildren = (unsigned int)childrenOf[a].size();
		if (node->mNumChildren > 0)
		{
			node->mChildren = new aiNode * [node->mNumChildren];
			for (unsigned int b = 0; b < node->mNumChildren; b++)
			{
				node->mChildren[b] = nodes[childrenOf[a][b]];
				node->mChildren[b]->mParent = node;
			}
		}
	}

	aiNode* root = new aiNode();
	root->mName = toAiString("DtsShape");
	//A quarter turn back about x, which puts the shape's up along ours
	root->mTransformation = aiMatrix4x4(
		1, 0, 0, 0,
		0, 0, 1, 0,
		0, -1, 0, 0,
		0, 0, 0, 1);

	root->mNumChildren = (unsigned int)roots.size();
	if (root->mNumChildren > 0)
	{
		root->mChildren = new aiNode * [root->mNumChildren];
		for (unsigned int a = 0; a < root->mNumChildren; a++)
		{
			root->mChildren[a] = nodes[roots[a]];
			root->mChildren[a]->mParent = root;
		}
	}

	scene->mRootNode = root;

	/*
		Every sequence the shape came with goes end to end on one track, since one animation with
		slices cut out of it is what Model expects. A key sits on every whole frame, so a sequence
		with twelve keyframes takes up twelve frames of the track no matter how long it runs for,
		and how fast it should actually be played gets worked out in DtsSequence::defaultSpeed.
	*/
	if (shape.sequences.size() > 0)
	{
		//Keys are gathered per node first, since a channel has to hold all of a node's keys at once
		std::vector<std::vector<aiVectorKey>> positionKeys(shape.nodes.size());
		std::vector<std::vector<aiQuatKey>> rotationKeys(shape.nodes.size());

		float frame = 0;
		for (const DtsSequenceData& sequence : shape.sequences)
		{
			int numKeyframes = sequence.numKeyframes;
			if (numKeyframes < 1)
				continue;

			/*
				The keys of every node a sequence moves sit one node after another, each node's
				keyframes together, starting from where the sequence says its keys begin.
			*/
			int movedSoFar = 0;
			for (size_t node = 0; node < shape.nodes.size(); node++)
			{
				if (node >= sequence.translationMatters.size() || !sequence.translationMatters[node])
					continue;

				for (int key = 0; key < numKeyframes; key++)
				{
					int at = sequence.baseTranslation + movedSoFar * numKeyframes + key;
					if (at < 0 || at >= (int)shape.nodeTranslations.size())
						continue;

					const glm::vec3& position = shape.nodeTranslations[at];
					positionKeys[node].push_back(aiVectorKey(frame + key,
						aiVector3D(position.x, position.y, position.z)));
				}
				movedSoFar++;
			}

			int turnedSoFar = 0;
			for (size_t node = 0; node < shape.nodes.size(); node++)
			{
				if (node >= sequence.rotationMatters.size() || !sequence.rotationMatters[node])
					continue;

				for (int key = 0; key < numKeyframes; key++)
				{
					int at = sequence.baseRotation + turnedSoFar * numKeyframes + key;
					if (at < 0 || at >= (int)shape.nodeRotations.size())
						continue;

					const glm::quat& rotation = shape.nodeRotations[at];
					rotationKeys[node].push_back(aiQuatKey(frame + key,
						aiQuaternion(rotation.w, rotation.x, rotation.y, rotation.z)));
				}
				turnedSoFar++;
			}

			/*
				A sequence usually moves a node one way only: the pistol's slide is given translations
				and no rotations at all. Node::sample fills in whatever a node has no keys for with
				nothing rather than with the node's rest pose, so a node with only translations would
				be forced upright and one with only rotations would be dragged to its parent's origin.
				Giving every node the sequence touches a full set of both, using its rest value for the
				side the sequence leaves alone, keeps it where the shape says it sits.
			*/
			for (size_t node = 0; node < shape.nodes.size(); node++)
			{
				const bool moved = node < sequence.translationMatters.size() && sequence.translationMatters[node];
				const bool turned = node < sequence.rotationMatters.size() && sequence.rotationMatters[node];

				if (moved == turned)
					continue;

				for (int key = 0; key < numKeyframes; key++)
				{
					if (!moved)
					{
						const glm::vec3& rest = shape.nodes[node].defaultTranslation;
						positionKeys[node].push_back(aiVectorKey(frame + key, aiVector3D(rest.x, rest.y, rest.z)));
					}
					else
					{
						const glm::quat& rest = shape.nodes[node].defaultRotation;
						rotationKeys[node].push_back(aiQuatKey(frame + key, aiQuaternion(rest.w, rest.x, rest.y, rest.z)));
					}
				}
			}

			if (sequences)
			{
				DtsSequence slice;
				slice.name = shape.nameOf(sequence.name);
				slice.startTime = frame;
				slice.endTime = frame + (numKeyframes - 1);
				slice.cyclic = sequence.cyclic;

				/*
					A frame of the track lasts as long as one of the sequence's keyframes did, and
					playing speed is in frames per millisecond, the same as an addAnimation line.
				*/
				float length = sequence.duration * 1000.0f;
				slice.defaultSpeed = (numKeyframes > 1 && length > 0)
					? (numKeyframes - 1) / length : 0.04f;

				sequences->push_back(slice);
			}

			//A frame of clear space so the end of one sequence never blends into the start of the next
			frame += numKeyframes;
		}

		std::vector<aiNodeAnim*> channels;
		for (size_t node = 0; node < shape.nodes.size(); node++)
		{
			if (positionKeys[node].size() < 1 && rotationKeys[node].size() < 1)
				continue;

			aiNodeAnim* channel = new aiNodeAnim();
			channel->mNodeName = toAiString(shape.nameOf(shape.nodes[node].name));

			channel->mNumPositionKeys = (unsigned int)positionKeys[node].size();
			if (channel->mNumPositionKeys > 0)
			{
				channel->mPositionKeys = new aiVectorKey[channel->mNumPositionKeys];
				for (unsigned int a = 0; a < channel->mNumPositionKeys; a++)
					channel->mPositionKeys[a] = positionKeys[node][a];
			}

			channel->mNumRotationKeys = (unsigned int)rotationKeys[node].size();
			if (channel->mNumRotationKeys > 0)
			{
				channel->mRotationKeys = new aiQuatKey[channel->mNumRotationKeys];
				for (unsigned int a = 0; a < channel->mNumRotationKeys; a++)
					channel->mRotationKeys[a] = rotationKeys[node][a];
			}

			channels.push_back(channel);
		}

		if (channels.size() > 0)
		{
			aiAnimation* animation = new aiAnimation();
			animation->mName = toAiString("DtsSequences");
			animation->mDuration = frame;
			animation->mTicksPerSecond = 0;
			animation->mNumChannels = (unsigned int)channels.size();
			animation->mChannels = new aiNodeAnim * [animation->mNumChannels];
			for (unsigned int a = 0; a < animation->mNumChannels; a++)
				animation->mChannels[a] = channels[a];

			scene->mNumAnimations = 1;
			scene->mAnimations = new aiAnimation * [1];
			scene->mAnimations[0] = animation;
		}
	}

	debug("Loaded DTS shape " + filePath + " with " + std::to_string(scene->mNumMeshes) + " meshes, "
		+ std::to_string(shape.nodes.size()) + " nodes, " + std::to_string(scene->mNumMaterials)
		+ " materials, and " + std::to_string(shape.sequences.size()) + " sequences");

	return scene;
}

bool isDtsPath(const std::string &filePath)
{
	return lowercase(filePath).ends_with(".dts");
}

#include "Material.h"

#include <filesystem>

//One spelling for a file however a descriptor reached it, so tools/../brickhead/x.png and brickhead/x.png are the same file
static std::string sameFile(const std::string& path)
{
	if (path.empty())
		return path;
	return std::filesystem::path(path).lexically_normal().generic_string();
}

void Material::finishCreation(std::string albedo, std::string normal, std::string roughness, std::string metalness, std::string occlusion, std::shared_ptr<TextureManager>  textures, bool flattenAlbedoAlpha)
{
	int howManyLayers = 0;
	if (albedo.length() > 0)
		howManyLayers++;
	if (normal.length() > 0)
		howManyLayers++;
	if (metalness.length() > 0 || roughness.length() > 0 || occlusion.length() > 0)
		howManyLayers++;

	debug(std::to_string(howManyLayers) + " layers expected for " + name);

	int currentLayer = 0;

	/*
		The texture is kept by what's in it rather than by which material asked for it, so materials made of
		the same files share one copy: TextureManager::createTexture hands back an existing texture of the same
		name, and every flat coloured DTS material in an add-on folder is made of the same three scuff maps.
		Named by material, twenty one of them loaded those maps twenty one times over
	*/
	std::string key = sameFile(albedo) + "|" + sameFile(normal) + "|" + sameFile(metalness) + "|"
		+ sameFile(occlusion) + "|" + sameFile(roughness) + (flattenAlbedoAlpha ? "|flat" : "");
	PBRArrayTexture = textures->createTexture(howManyLayers, key);

	if (!PBRArrayTexture)
	{
		error("Something went wrong allocating space for texture for material: " + name);
		return;
	}

	//In the event a texture already exists with this exact name, use it instead
	if (PBRArrayTexture->isValid())
	{
		//Still gotta set material related uniforms though
		int layer = 0;
		if (albedo.length() > 0)
		{
			useAlbedo = layer;
			layer++;
		}
		if (normal.length() > 0)
		{
			useNormal = layer;
			layer++;
		}
		if (metalness.length() > 0)
			useMetalness = layer;
		if (roughness.length() > 0)
			useRoughness = layer;
		if (occlusion.length() > 0)
			useOcclusion = layer;

		valid = true;
		return;
	}

	if (albedo.length() > 0)
	{
		useAlbedo = currentLayer;
		++currentLayer;
		PBRArrayTexture->addLayer(albedo, flattenAlbedoAlpha);
	}

	if (normal.length() > 0)
	{
		useNormal = currentLayer;
		++currentLayer;
		PBRArrayTexture->addLayer(normal);
	}

	//No point in incrementing currentLayer beyond this point...
	//Create the final layer with various masks used for PBR rendering

	/*
		Bail early, no metalness/roughness/occlusion data, no need for a 3rd layer. Without the
		return the layers this material didn't ask for get added as empty components anyway, which
		is only ever reached by a material that has no PBR maps at all, like the flat textures a
		DTS shape names. The shaders already draw one of those with default values, see useRoughness
		and the rest in model.frag.glsl.
	*/
	if (howManyLayers == currentLayer)
	{
		PBRArrayTexture->setFilter(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);

		if (PBRArrayTexture->isValid())
			valid = true;

		return;
	}

	/*
		With no albedo and no normal map the MOHR layer is the first one, and its metalness and occlusion
		channels can be empty, which leaves nothing to say how big the texture is. Take the size from
		whichever of the three does have a file, which is what a material of one roughness map needs
	*/
	if (currentLayer == 0)
	{
		const std::string& sizeFrom = metalness.length() > 0 ? metalness : (occlusion.length() > 0 ? occlusion : roughness);
		if (!textures->sizeFromFile(PBRArrayTexture, sizeFrom, 3))
			return;
	}

	//Metalness
	if (metalness.length() > 0)
	{
		useMetalness = currentLayer;
		textures->addComponent(PBRArrayTexture, metalness);
	}
	else
		textures->addEmptyComponent(PBRArrayTexture);

	//Ambient occlusion
	if (occlusion.length() > 0)
	{
		useOcclusion = currentLayer;
		textures->addComponent(PBRArrayTexture, occlusion);
	}
	else
		textures->addEmptyComponent(PBRArrayTexture);

	//Roughness
	if (roughness.length() > 0)
	{
		useRoughness = currentLayer;
		textures->addComponent(PBRArrayTexture, roughness);
	}
	else
		textures->addEmptyComponent(PBRArrayTexture);

	//We used to force every layer to have 4 channels and the MOHR channel had a fourth height channel that's not back in yet
	if (PBRArrayTexture->getNumChannels() == 4)
		textures->addEmptyComponent(PBRArrayTexture);

	PBRArrayTexture->setFilter(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);

	if (PBRArrayTexture->isValid())
		valid = true;
}

Material::Material(const std::string &_name, const std::string &albedo, const  std::string &normal, const  std::string &roughness, const  std::string &metalness, const  std::string &occlusion, std::shared_ptr<TextureManager>  textures, bool flattenAlbedoAlpha)
	: name(_name)
{
	scope("Material::Material (explicit)");

	finishCreation(albedo, normal, roughness, metalness, occlusion, textures, flattenAlbedoAlpha);
}

Material::Material(const std::string &filePath, std::shared_ptr<TextureManager>  textures)
{
	scope("Material::Material (from file)");

	name = getFileFromPath(filePath.c_str());
	std::string pathToTextures = getFolderFromPath(filePath.c_str());

	std::ifstream materialDescriptor(filePath.c_str());

	if (!materialDescriptor.is_open())
	{
		error("Could not open " + filePath);
		return;
	}

	/*
		At the moment, a material has the following things for PBR:
		Albedo - aka color
		Normal Map
		Metalness
		Roughness
		Ambient Occlusion

		When making a material you may use or not use any of these
		The only restriction atm is that you can't use metal/rough/ao without also using either albedo and/or normal map
	*/
	
	//Remains "" if not used
	std::string albedoPath = "";
	std::string normalPath = "";
	std::string metalPath = "";
	std::string roughPath = "";
	std::string aoPath = "";

	std::string line = "";
	while (!materialDescriptor.eof())
	{
		getline(materialDescriptor, line);

		if (line.length() < 1)
			continue;

		if (line[0] == '#')
			continue;

		size_t tabPos = line.find("\t");
		if (tabPos == std::string::npos)
		{
			error("Invalid line in file " + filePath);
			return;
		}
		
		//Each line in this file should contain two words separated by a tab
		//The first is the type of texture i.e. 'albedo'
		//The second is a relative path to the image file
		std::string type = line.substr(0, tabPos);
		std::string path = line.substr(tabPos + 1, std::string::npos);

		if (type.length() < 1)
		{
			error("No texture type specified " + filePath);
			return;
		}

		if (path.length() < 1)
		{
			error("Invalid path specified " + filePath);
			return;
		}

		/*
			A line can give plain numbers instead of a file name, which is what a surface of one flat value
			wants: one number for metalness, roughness or occlusion, and one or three for an albedo colour.
			The numbers are on the same 0 to 1 scale a texture's pixels are read on

			Better than a tiny single colour image, which the old materials here used: every layer of the
			PBR array has to be the same size, so a 1x1 metalness beside a real roughness map is refused
		*/
		std::vector<float> numbers;
		bool allNumbers = true;
		{
			std::istringstream values(path);
			std::string word;
			while (values >> word)
			{
				try
				{
					size_t used = 0;
					float value = std::stof(word, &used);
					if (used != word.length())
						throw std::invalid_argument("trailing");
					numbers.push_back(value);
				}
				catch (const std::exception&)
				{
					allNumbers = false;
					break;
				}
			}
		}
		allNumbers = allNumbers && !numbers.empty();

		//Complains about a line like "roughness 0.5 0.5", which is neither a file name nor a value we can use
		auto wrongCount = [&](const std::string& wanted)
		{
			error(type + " in " + filePath + " needs " + wanted + ", not " + std::to_string(numbers.size()) + " numbers");
		};

		//You can optionally specify a name for the material for use with Lua or whatever later
		//It can also prevent reusing the same material
		if (type == "name")
			name = path;
		else if (type == "albedo")
		{
			if (!allNumbers)
				albedoPath = pathToTextures + path;
			else if (numbers.size() == 1)
				constantAlbedo = glm::vec4(numbers[0], numbers[0], numbers[0], 1);
			else if (numbers.size() == 3)
				constantAlbedo = glm::vec4(numbers[0], numbers[1], numbers[2], 1);
			else
				wrongCount("one number for a shade or three for a colour");
		}
		else if (type == "normal")
			normalPath = pathToTextures + path;
		else if (type == "metalness")
		{
			if (!allNumbers)
				metalPath = pathToTextures + path;
			else if (numbers.size() == 1)
				constantMOR.x = numbers[0];
			else
				wrongCount("one number");
		}
		else if (type == "roughness")
		{
			if (!allNumbers)
				roughPath = pathToTextures + path;
			else if (numbers.size() == 1)
				constantMOR.z = numbers[0];
			else
				wrongCount("one number");
		}
		else if (type == "occlusion")
		{
			if (!allNumbers)
				aoPath = pathToTextures + path;
			else if (numbers.size() == 1)
				constantMOR.y = numbers[0];
			else
				wrongCount("one number");
		}
		else
		{
			error("Invalid material texture type: " + type + " file: " + filePath);
			return;
		}
	}

	finishCreation(albedoPath, normalPath, roughPath, metalPath, aoPath, textures);
}

Material::~Material()
{
	if (PBRArrayTexture)
		PBRArrayTexture->markForCleanup();
}

void Material::use(std::shared_ptr<ShaderManager> shaders) const
{
	if (!valid)
		return;

	shaders->basicUniforms.useAlbedo =		useAlbedo;
	shaders->basicUniforms.useNormal =		useNormal;
	shaders->basicUniforms.useMetalness =	useMetalness;
	shaders->basicUniforms.useRoughness =	useRoughness;
	shaders->basicUniforms.useAO =			useOcclusion;

	//Used for whichever of them useAlbedo and the rest say there's no texture for
	shaders->basicUniforms.ConstantAlbedo =	constantAlbedo;
	shaders->basicUniforms.ConstantMOR =	glm::vec4(constantMOR, 0);

	shaders->updateBasicUBO();

	PBRArrayTexture->bind(PBRArray);
}

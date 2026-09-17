#include "Material.h"

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
	
	PBRArrayTexture = textures->createTexture(howManyLayers, name);

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

		//You can optionally specify a name for the material for use with Lua or whatever later
		//It can also prevent reusing the same material
		if (type == "name")
			name = path;
		else if (type == "albedo")
			albedoPath = pathToTextures + path;
		else if (type == "normal")
			normalPath = pathToTextures + path;
		else if (type == "metalness")
			metalPath = pathToTextures + path;
		else if (type == "roughness")
			roughPath = pathToTextures + path;
		else if (type == "occlusion")
			aoPath = pathToTextures + path;
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

	shaders->updateBasicUBO();

	PBRArrayTexture->bind(PBRArray);
}

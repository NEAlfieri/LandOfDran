#pragma once

#include "../LandOfDran.h"
#include "Texture.h"
#include "ShaderSpecification.h"

class Material
{
	/*
		Does PBRArrayTexture contain an albedo layer ? 
		-1 if not, otherwise points to the layer its on
	*/
	int useAlbedo = -1;

	/*
		Does PBRArrayTexture contain a normal map layer ? 
		-1 if not, otherwise points to the layer its on
	*/
	int useNormal = -1;

	/*
		Does PBRArrayTexture contain a MOHR layer with a metalness channel ? 
		-1 if not, otherwise points to the layer its on
		metalness, roughness, and occlusion will always be on the same layer, though they might not all be used
	*/
	int useMetalness = -1;

	/*
		Does PBRArrayTexture contain a MOHR layer with a roughness channel ?
		-1 if not, otherwise points to the layer its on
		metalness, roughness, and occlusion will always be on the same layer, though they might not all be used
	*/
	int useRoughness = -1;

	/*
		Does PBRArrayTexture contain a MOHR layer with a occlusion channel ?
		-1 if not, otherwise points to the layer its on
		metalness, roughness, and occlusion will always be on the same layer, though they might not all be used
	*/
	int useOcclusion = -1;

	//Used in 3d model description files, or possibly Lua
	std::string name = "";

	Texture* PBRArrayTexture = 0;

	bool valid = false;

	//Second half of both constructors
	void finishCreation(std::string albedo, std::string normal, std::string roughness, std::string metalness, std::string occlusion, std::shared_ptr<TextureManager> textures, bool flattenAlbedoAlpha = false);

	public:

	/*
		What this material is drawn with wherever it has no texture, from plain numbers in its descriptor
		instead of file names. The defaults are what the shaders used to hard code, so a material that
		names none of them looks exactly as it did

		A flat colour belongs here rather than in a one pixel image: every layer of the PBR array has to
		be the same size, so a 1x1 metalness next to a real roughness map would be refused
	*/
	glm::vec4 constantAlbedo = glm::vec4(1, 1, 1, 1);
	//Metalness, occlusion, roughness, in the order the MOHR layer packs them
	glm::vec3 constantMOR = glm::vec3(0, 1, 0.5);

	std::string getName() const { return name; }

	bool isValid() const { return valid; }

	/*
		Sets PBRArrayTexture as the active PBRArray texture
		Passes other uniforms describing the texture to shaders
	*/
	void use(std::shared_ptr<ShaderManager> shaders) const;

	/*
		Pass a filepath to a text file containing a description of this material
		and the filepaths of the indivdual textures it has
	*/
	Material(const std::string &filePath, std::shared_ptr<TextureManager>  textures);

	/*
		Pass the paths for the 5 possible channels directly to make a material
		Any you don't want to use can be left as empty strings: ""

		flattenAlbedoAlpha reads a see-through albedo as a shade of what's left over white rather than
		throwing its alpha away, which is what the flat textures a DTS shape names expect, see
		Texture::addLayer
	*/
	Material(const std::string &_name,const std::string &albedo,const std::string &normal,const std::string &roughness,const std::string &metalness,const std::string &occlusion, std::shared_ptr<TextureManager> textures, bool flattenAlbedoAlpha = false);

	~Material();
};

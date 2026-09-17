#include "ItemIconRenderer.h"
#include "Mesh.h"

Texture* ItemIconRenderer::getIcon(int slot) const
{
	if (slot < 0 || slot >= (int)slots.size() || !slots[slot].drawn || !slots[slot].target)
		return nullptr;

	Texture* icon = slots[slot].target->getColorResult();
	return icon && icon->isValid() ? icon : nullptr;
}

void ItemIconRenderer::clear()
{
	slots.clear();
}

void ItemIconRenderer::begin(std::shared_ptr<ShaderManager> shaders)
{
	previousEnvironment = shaders->environmentUniforms;
	previousCamera = shaders->cameraUniforms;
	previousPointLightCount = shaders->pointLightUniforms.PointLightCount;

	//Lit from above and in front of the camera, with no fog, water or point lights, the same turntable
	//lighting the appearance editor uses so an item reads the same whatever time of day the server is on
	EnvironmentUniforms& environment = shaders->environmentUniforms;
	environment.LightDirection = glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f));
	environment.SunDirection = environment.LightDirection;
	environment.LightColor = glm::vec3(3.0f);
	environment.AmbientColor = glm::vec3(0.5f);
	environment.ShadowStrength = 0.0f;
	environment.RainIntensity = 0.0f;
	environment.RainWetness = 0.0f;
	environment.ClipPlane = glm::vec4(0.0f);
	shaders->updateEnvironmentUBO();

	shaders->pointLightUniforms.PointLightCount = 0;
	shaders->updatePointLightUBO();

	Program* program = shaders->modelShader;
	program->use();

	//Nowhere near any shadow cascade, so cascadeLight finds every one out of range and returns fully lit
	glm::mat4 noShadow = glm::translate(glm::vec3(1.0e7f, 1.0e7f, 0.0f));
	glm::mat4 lightSpaceMatricies[3] = { noShadow, noShadow, noShadow };
	glUniformMatrix4fv(program->getUniformLocation("lightSpaceMatricies"), 3, GL_FALSE, &lightSpaceMatricies[0][0][0]);
	glUniform1i(program->getUniformLocation("coloredShadows"), 0);
	glUniform1i(program->getUniformLocation("pickingID"), 0);
	glUniform4f(program->getUniformLocation("editorHighlight"), 0.0f, 0.0f, 0.0f, 0.0f);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}

void ItemIconRenderer::end(std::shared_ptr<ShaderManager> shaders, int screenWidth, int screenHeight)
{
	shaders->environmentUniforms = previousEnvironment;
	shaders->updateEnvironmentUBO();

	shaders->cameraUniforms = previousCamera;
	shaders->updateCameraUBO();

	shaders->pointLightUniforms.PointLightCount = previousPointLightCount;
	shaders->updatePointLightUBO();

	shaders->basicUniforms.nonInstanced = 0;
	shaders->basicUniforms.cameraSpacePosition = 0;
	shaders->updateBasicUBO();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenWidth, screenHeight);
}

void ItemIconRenderer::render(std::shared_ptr<ShaderManager> shaders, const std::vector<Request>& requests, int screenWidth, int screenHeight)
{
	if (slots.size() != requests.size())
		slots.resize(requests.size());

	//Only the slots whose picture would come out different from the one already in their texture
	bool anyToDraw = false;
	std::vector<bool> redraw(requests.size(), false);
	for (size_t a = 0; a < requests.size(); a++)
	{
		Model* model = requests[a].model;
		if (model && !model->isValid())
			model = nullptr;

		//The picked slot turns, so its angle differs every frame and it redraws every frame, which is also
		//what keeps up with whatever its item is animating in hand. The rest hold still and cost nothing
		redraw[a] = model != slots[a].model || (model && (!slots[a].drawn || requests[a].angle != slots[a].angle));
		anyToDraw = anyToDraw || redraw[a];
	}

	if (!anyToDraw)
		return;

	begin(shaders);

	for (size_t a = 0; a < requests.size(); a++)
	{
		if (!redraw[a])
			continue;

		Slot& slot = slots[a];
		Model* model = requests[a].model;
		if (model && !model->isValid())
			model = nullptr;

		slot.model = model;
		slot.angle = requests[a].angle;
		slot.drawn = false;

		if (!model)
			continue;

		//The box the model fills, which the camera is then backed off far enough to fit whichever way it's turned
		glm::vec3 low, high;
		if (!model->getDrawnBounds(low, high))
		{
			low = model->getColOffset() - model->getColHalfExtents();
			high = model->getColOffset() + model->getColHalfExtents();
		}

		glm::vec3 center = (low + high) * 0.5f;
		glm::vec3 size = glm::max(high - low, glm::vec3(0.001f));

		//Its own middle at the origin, turned around its up axis, then tipped toward the camera
		glm::mat4 modelTransform = glm::rotate(-tilt, glm::vec3(1, 0, 0)) * glm::rotate(requests[a].angle, glm::vec3(0, 1, 0)) * glm::translate(-center);

		/*
			However far around it has turned, it never reaches further from the middle sideways than the
			corner of its own footprint, and the tilt trades a little of its height for that. Fitting those
			two rather than a sphere around the whole thing is what stops a long thin item like a hammer
			from being drawn tiny to leave room for a width it never has
		*/
		float spunRadius = glm::length(glm::vec2(size.x, size.z)) * 0.5f;
		float halfUp = size.y * 0.5f * std::cos(tilt) + spunRadius * std::sin(tilt);
		float halfDepth = spunRadius * std::cos(tilt) + size.y * 0.5f * std::sin(tilt);

		/*
			Enough for the widest of those to fit, and then some of however near its front can come. Only some:
			backing off by the whole of it fits the nearest face of a deep item with room to spare and leaves
			the item noticeably smaller in its slot than the drawn icons it stands in for. What sticks out over
			the edge of the picture this way is a corner at most, and the slot has padding around it anyway
		*/
		float fov = glm::radians(40.0f);
		float distance = std::max(spunRadius, halfUp) / std::tan(fov * 0.5f) + halfDepth * 0.5f;
		float radius = std::max(std::max(halfUp, halfDepth), spunRadius);
		glm::vec3 cameraPosition(0.0f, 0.0f, distance);

		CameraUniforms& camera = shaders->cameraUniforms;
		camera.CameraView = glm::lookAt(cameraPosition, glm::vec3(0.0f), glm::vec3(0, 1, 0));
		camera.CameraAngle = glm::mat4(glm::mat3(camera.CameraView));
		camera.CameraPosition = cameraPosition;
		camera.CameraDirection = glm::vec3(0, 0, -1);
		camera.CameraProjection = glm::perspective(fov, 1.0f, std::max(distance - radius * 1.2f, distance * 0.01f), distance + radius * 2.0f);
		shaders->updateCameraUBO();

		//Nothing is ever this far from the model, so it's never fogged whatever the server's fog is set to
		shaders->environmentUniforms.FogDistanceMin = distance * 100.0f;
		shaders->environmentUniforms.FogDistanceMax = distance * 200.0f;
		shaders->updateEnvironmentUBO();

		if (!slot.target)
		{
			RenderTarget::RenderTargetSettings settings;
			settings.width = resolution;
			settings.height = resolution;
			//The fourth channel is what leaves the corners of the slot see-through around the item
			settings.channels = 4;
			settings.useDepth = false;
			settings.useDepthBuffer = true;
			settings.clearColor = glm::vec4(0.0f);
			settings.colorWrap = GL_CLAMP_TO_EDGE;
			slot.target = std::make_shared<RenderTarget>(settings, textures);

			if (!slot.target->isValid())
			{
				slot.target.reset();
				slot.model = nullptr;
				continue;
			}
		}

		slot.target->use();

		shaders->basicUniforms.nonInstanced = 1;
		shaders->basicUniforms.cameraSpacePosition = 0;
		shaders->basicUniforms.RotationMatrix = glm::mat4(1.0f);
		shaders->basicUniforms.ScaleMatrix = glm::mat4(1.0f);

		//Every mesh sits somewhere of its own inside the model, which is per instance data the non instanced
		//path can't read, so each one is drawn on its own with that folded into the model matrix
		for (int meshIdx = 0; meshIdx < model->getNumMeshes(); meshIdx++)
		{
			if (!model->isMeshDrawn(meshIdx))
				continue;

			shaders->basicUniforms.TranslationMatrix = modelTransform *
				(requests[a].instance ? requests[a].instance->getMeshTransform(meshIdx) : glm::scale(model->baseScale));
			shaders->updateBasicUBO();

			model->renderMeshOnce(shaders, meshIdx);
		}

		slot.drawn = true;
	}

	end(shaders, screenWidth, screenHeight);
}

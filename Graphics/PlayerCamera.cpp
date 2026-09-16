#include "PlayerCamera.h"

glm::vec3 Camera::mouseCoordsToWorldSpace(glm::vec2 mouseCoords) const
{
    glm::vec4 homoCoords = glm::vec4(mouseCoords.x,mouseCoords.y,0,1);
    glm::vec4 worldCoords = glm::inverse(projectionMatrix * viewMatrix) * homoCoords;
    return glm::vec3(worldCoords.x,worldCoords.y,worldCoords.z);
}

void Camera::calculateLightSpaceMatricies(glm::vec3 lightDirection, float shadowDistance, int mapResolution, glm::mat4* result, float* radii)
{
    //Where each cascade ends: the practical split scheme, mostly logarithmic (even detail on screen) with some uniform mixed in
    const float logarithmicShare = 0.75f;
    float farthest = std::max(nearPlane * 2.0f, std::min(farPlane, shadowDistance));
    float splits[4];
    for (int a = 0; a <= 3; a++)
    {
        float fraction = a / 3.0f;
        float logarithmic = nearPlane * std::pow(farthest / nearPlane, fraction);
        float uniform = glm::mix(nearPlane, farthest, fraction);
        splits[a] = glm::mix(uniform, logarithmic, logarithmicShare);
    }

    //Light space axes don't depend on the camera, so cascades can be kept to whole texel steps along them, see below
    glm::vec3 up = std::abs(lightDirection.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightRotation = glm::lookAt(glm::vec3(0), -lightDirection, up);

    glm::mat4 inverseView = glm::inverse(viewMatrix);
    glm::vec3 eye = glm::vec3(inverseView[3]);
    glm::vec3 forward = -glm::normalize(glm::vec3(inverseView[2]));

    //Squared distance from the view axis to a corner of the screen, per unit forward
    float tanHalfHeight = std::tan(glm::radians(currentFieldOfVision) * 0.5f);
    float cornerSlopeSquared = tanHalfHeight * tanHalfHeight * (1.0f + aspectRatio * aspectRatio);

    //A little depth past each sphere, anything further toward the light is flattened onto the near plane by GL_DEPTH_CLAMP
    //Keeping the range tight matters since the depth a shadow can be off by grows with it
    const float depthMargin = 1.0f;

    //Room for model.frag's widest filter plus the texel snapping below, so neither reaches past the edge of the slice
    const float paddingTexels = 6.0f;

    for (int a = 0; a < 3; a++)
    {
        float sliceNear = splits[a];
        float sliceFar = splits[a + 1];

        //Smallest sphere around this slice of the view, which stays the same size however the camera turns
        //Its center is on the view axis where the near and far corners are equally far from it, or at the far end for wide views
        float centerDistance = std::min(sliceFar, 0.5f * (sliceNear + sliceFar) * (1.0f + cornerSlopeSquared));
        float radius = std::sqrt((sliceFar - centerDistance) * (sliceFar - centerDistance) + sliceFar * sliceFar * cornerSlopeSquared);
        radius *= mapResolution / (mapResolution - 2.0f * paddingTexels);

        //Moving the cascade only in whole texels keeps casters landing on the same texels, so shadow edges don't crawl as the camera moves
        if (radii)
            radii[a] = radius;

        float texelSize = 2.0f * radius / mapResolution;
        glm::vec3 center = glm::vec3(lightRotation * glm::vec4(eye + forward * centerDistance, 1.0f));
        center.x = std::floor(center.x / texelSize) * texelSize;
        center.y = std::floor(center.y / texelSize) * texelSize;

        //View space looks down -z, so the side toward the light has the larger z
        result[a] = glm::ortho(center.x - radius, center.x + radius, center.y - radius, center.y + radius,
            -(center.z + radius) - depthMargin, -(center.z - radius) + depthMargin) * lightRotation;
    }
}

void Camera::swapPerson()
{
    setFirstPerson(!firstPerson);
}

void Camera::setFirstPerson(bool _firstPerson)
{
	firstPerson = _firstPerson;
}   

void Camera::control(float deltaT,std::shared_ptr<InputMap> input)
{
    if (!(freePosition && target.expired()))
        return;

    //Test camera controls, no-clip camera
    float speed = 0.015f;

    if (input->isCommandKeydown(WalkForward))
        flyStraight(deltaT * speed);
    if (input->isCommandKeydown(WalkBackward))
        flyStraight(-deltaT * speed);
    if (input->isCommandKeydown(WalkRight))
        flySideways(-deltaT * speed);
    if (input->isCommandKeydown(WalkLeft))
        flySideways(deltaT * speed);
}

void Camera::updateSettings(std::shared_ptr<SettingManager> settings)
{
    mouseSensitivity = settings->getFloat("input/mousesensitivity");
    invertMouse = settings->getBool("input/invertmousey");

    //While zooming the view keeps its zoom, and eases to the new field of view once the key is let go
    float fov = (float)std::min(std::max(settings->getInt("graphics/fieldofview"), 60), 120);
    if (zooming)
        fieldOfVision = fov;
    else
        setFOV(fov);
}

void Camera::setPosition(const glm::vec3& pos)
{
    position = pos;
}

void Camera::setDirection(const glm::vec3& dir)
{
    direction = dir;

    //If either of the coords are exactly 0 the shader will stop rendering anything
    if (std::abs(direction.x) < 0.001)
        direction.x = 0.001;
    if (std::abs(direction.y) < 0.001)
        direction.y = 0.001;
    if (std::abs(direction.z) < 0.001)
        direction.z = 0.001;
}

void Camera::setUp(const glm::vec3& up)
{
	nominalUp = up;
}   

glm::vec3 Camera::getPosition()
{
    return position;
}

glm::vec3 Camera::getDirection()
{
    return direction;
}

void Camera::updateProjection()
{
    projectionMatrix = glm::perspective(glm::radians(currentFieldOfVision), aspectRatio, nearPlane, farPlane);
}

void Camera::setFOV(float fov)
{
    fieldOfVision = fov;
    currentFieldOfVision = fov;
    updateProjection();
}

//Call when screen size changed
void Camera::setAspectRatio(float ratio)
{
    aspectRatio = ratio;
    updateProjection();
}

Camera::Camera(float _aspectRatio, float _fieldOfVision, float _nearPlane, float _farPlane)
    : aspectRatio(_aspectRatio), fieldOfVision(_fieldOfVision), nearPlane(_nearPlane), farPlane(_farPlane)
{
    currentFieldOfVision = fieldOfVision;
    updateProjection();
}

//Positive amount forward, negative backward, only use for no-clip camera
void Camera::flyStraight(float amount)
{
	position = position + glm::vec3(direction.x * amount, direction.y * amount, direction.z * amount);
}

//Positive amount right, negative left, only use for no-clip camera
void Camera::flySideways(float amount)
{
    float y = atan2(direction.x, direction.z);
    y += 1.57079633f;
    if (y > 6.28318531f)
        y -= 6.28318531f;

    position = position + glm::vec3(sin(y) * amount, 0, cos(y) * amount);
}

//Look around with the mouse
void Camera::turn(float relMouseX, float relMouseY)
{
    if (!freeDirection)
        return;

    //Turns slower while zoomed in, so things move across the screen about as fast as they do normally
    float zoomScale = std::tan(glm::radians(currentFieldOfVision) * 0.5f) / std::tan(glm::radians(fieldOfVision) * 0.5f);
    relMouseX *= mouseSensitivity / 100.0f * zoomScale;
    relMouseY *= mouseSensitivity / 100.0f * zoomScale;
    if (invertMouse)
        relMouseY = -relMouseY;

    if (isnan(direction.y))
        direction.y = 0;
    if (isnan(direction.x))
        direction.x = 0;
    if (isnan(direction.z))
        direction.z = 0;

    float p = asin(direction.y);
    float y = atan2(direction.x, direction.z);
    float newP = p + relMouseY;
    if (newP >= 1.57f)
        newP = 1.57f;
    if (newP <= -1.57f)
        newP = -1.57f;

    direction = glm::vec3(cos(newP) * sin(y + relMouseX), sin(newP), cos(newP) * cos(y + relMouseX));
}

//Call once per frame
void Camera::render(std::shared_ptr<ShaderManager> graphics,float deltaT,const std::shared_ptr<PhysicsWorld> world)
{
    //Eases toward the zoomed or normal field of view, 95% of the way there in about 180 ms however fast frames come
    float desiredFieldOfVision = zooming ? zoomedFieldOfVision : fieldOfVision;
    if (currentFieldOfVision != desiredFieldOfVision)
    {
        currentFieldOfVision = glm::mix(desiredFieldOfVision, currentFieldOfVision, std::exp(-deltaT / 60.0f));
        if (std::abs(currentFieldOfVision - desiredFieldOfVision) < 0.01f)
            currentFieldOfVision = desiredFieldOfVision;
        updateProjection();
    }

    if(!target.expired())
	{
        std::shared_ptr<Dynamic> targetLock = target.lock();
        if (targetLock->clientControlled)
        {
            btTransform t = targetLock->body->getWorldTransform();
            position = b2g3(t.getOrigin());

            glm::vec3 eyePos = targetLock->getType()->getModel()->getEyePosition();
            glm::vec4 homoEyePos = glm::toMat4(targetLock->interpolator.getRotation()) * glm::vec4(eyePos, 1);
            position += glm::vec3(homoEyePos.x, homoEyePos.y, homoEyePos.z);

            const btQuaternion &q = t.getRotation();
            glm::quat quat = glm::quat(q.getW(), q.getX(), q.getY(), q.getZ());
            glm::vec4 homoDir = glm::toMat4(quat) * glm::vec4(0, 0, 1, 0);
            if (!freeDirection)
                direction = glm::vec3(homoDir.x, homoDir.y, homoDir.z);
            if(freeUpVector)
			{
				glm::vec4 homoUp = glm::toMat4(quat) * glm::vec4(0, 1, 0, 0);
				nominalUp = glm::vec3(homoUp.x, homoUp.y, homoUp.z);
			}
            else
                nominalUp = glm::vec3(0, 1, 0);
        }
        else
        {
            position = targetLock->interpolator.getPosition();

            glm::vec3 eyePos = targetLock->getType()->getModel()->getEyePosition();
            glm::vec4 homoEyePos = glm::toMat4(targetLock->interpolator.getRotation()) * glm::vec4(eyePos, 1);
            position += glm::vec3(homoEyePos.x, homoEyePos.y, homoEyePos.z);

            glm::vec4 homoDir = glm::toMat4(targetLock->interpolator.getRotation()) * glm::vec4(0, 0, 1, 0);
            if(!freeDirection)
                direction = glm::vec3(homoDir.x, homoDir.y, homoDir.z);
            if(freeUpVector)
            {
                glm::vec4 homoUp = glm::toMat4(targetLock->interpolator.getRotation()) * glm::vec4(0, 1, 0, 0);
				nominalUp = glm::vec3(homoUp.x, homoUp.y, homoUp.z);
			}
            else
                nominalUp = glm::vec3(0, 1, 0);
        }

        if (!firstPerson)
        {
            //Bit of a zoom out effect
            thirdPersonDistance += deltaT * 0.1f;
            thirdPersonDistance = std::min(thirdPersonDistance, maxThirdPersonDistance);

            btVector3 hitPos, hitNormal;
            if (world)
            {
                if (world->doRaycast(g2b3(position), g2b3(position - direction * glm::vec3(thirdPersonDistance)), targetLock->body, hitPos, hitNormal, alsoIgnore))
                    thirdPersonDistance = std::min(glm::length(position - b2g3(hitPos)), thirdPersonDistance);
            }
        }
        else
        {
            thirdPersonDistance -= deltaT * 0.1f;
            thirdPersonDistance = std::max(thirdPersonDistance, 0.0f);
        }

        position -= direction * glm::vec3(thirdPersonDistance);

        //Still casts a shadow while hidden, so you can see your own shadow in first person
        if((thirdPersonDistance < 5.0) != targetLock->getHidden())
            targetLock->setHidden(thirdPersonDistance < 5.0, true);
	}

    viewMatrix = glm::lookAt(position, position + direction, nominalUp);
    angleMatrix = glm::lookAt(glm::vec3(0, 0, 0), direction, nominalUp);

    uploadUniforms(graphics);
}

void Camera::uploadUniforms(std::shared_ptr<ShaderManager> graphics) const
{
    graphics->cameraUniforms.CameraDirection = direction;
    graphics->cameraUniforms.CameraPosition = position;
    graphics->cameraUniforms.CameraProjection = projectionMatrix;
    graphics->cameraUniforms.CameraView = viewMatrix;
    graphics->cameraUniforms.CameraAngle = angleMatrix;

    graphics->updateCameraUBO();
}

void Camera::uploadReflectionUniforms(std::shared_ptr<ShaderManager> graphics, float waterLevel) const
{
    glm::vec3 mirroredPosition = glm::vec3(position.x, 2.0f * waterLevel - position.y, position.z);
    glm::vec3 mirroredDirection = glm::vec3(direction.x, -direction.y, direction.z);

    //A true mirror of the up vector would also flip triangle winding, this only flips the image vertically
    glm::vec3 up = glm::vec3(-nominalUp.x, nominalUp.y, -nominalUp.z);

    graphics->cameraUniforms.CameraDirection = mirroredDirection;
    graphics->cameraUniforms.CameraPosition = mirroredPosition;
    graphics->cameraUniforms.CameraProjection = projectionMatrix;
    graphics->cameraUniforms.CameraView = glm::lookAt(mirroredPosition, mirroredPosition + mirroredDirection, up);
    graphics->cameraUniforms.CameraAngle = glm::lookAt(glm::vec3(0, 0, 0), mirroredDirection, up);

    graphics->updateCameraUBO();
}

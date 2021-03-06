//
// Bachelor of Software Engineering
// Media Design School
// Auckland
// New Zealand
//
// (c) 2017 Media Design School
//
// Description  : A system which handles rendering each entity.
// Author       : Lance Chaney
// Mail         : lance.cha7337@mediadesign.school.nz
//

#pragma once

#include "RenderState.h"
#include "EntityEventListener.h"

#include <glad\glad.h>
#include <glm\glm.hpp>

#include <vector>

class Scene;
struct GLFWwindow;
class Entity;
struct ModelComponent;

class RenderSystem : public EntityEventListener {
public:
	RenderSystem(GLFWwindow* glContext, Scene&);
	~RenderSystem();
	RenderSystem(const RenderSystem&) = delete;
	RenderSystem& operator=(const RenderSystem&) = delete;

	// Draws a debugging arrow
	// This object will only be drawn once.
	// To keep or update the arrow, drawDebugArrow must be called every frame.
	static void drawDebugArrow(const glm::vec3& base, const glm::vec3& tip,
		const glm::vec3& color = glm::vec3(1, 0, 0));

	// Draws a debugging arrow
	// This object will only be drawn once.
	// To keep or update the arrow, drawDebugArrow must be called every frame.
	static void drawDebugArrow(const glm::vec3& base, const glm::vec3& direction, 
		float magnitude, const glm::vec3& color = glm::vec3(1, 0, 0));

	// Starts rendering the frame.
	// Should be called before update.
	void beginRender();

	// Renders an entity.
	void update(const Entity&);

	// Ends the frame.
	void endRender();

	// Sets the current camera.
	// Also sets the static debug camera for debug drawing.
	void setCamera(const Entity*);

	// Sets the radiance map for reflections
	void setRadianceMap(GLuint radianceMap);

	// Sets the irradiance map for image based lighting
	void setIrradianceMap(GLuint irradianceMap);

	// Inherited via EntityEventListener
	virtual void onEntityCreation(Entity & entity) override;
	virtual void onEntityDestruction(Entity & entity) override;
	virtual void onAddComponents(Entity & entity, size_t componentMaskAdded) override;
	virtual void onRemoveComponents(Entity & entity, size_t componentMaskToRemove) override;

private:
	static void renderModel(const ModelComponent&, const glm::mat4& transform);

	static RenderState s_renderState;
	RenderState m_renderState;

	Scene& m_scene;
};
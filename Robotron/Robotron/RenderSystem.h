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

#include <glad\glad.h>
#include <glm\glm.hpp>

#include <vector>

struct Scene;
struct GLFWwindow;
struct Entity;
struct ModelComponent;

class RenderSystem {
public:
	RenderSystem(GLFWwindow* glContext, Scene&);
	RenderSystem(const RenderSystem&) = delete;
	RenderSystem& operator=(const RenderSystem&) = delete;

	// Draws a debugging arrow
	// This object will only be drawn once.
	// To keep or update the arrow, drawDebugArrow must be called every frame.
	static void drawDebugArrow(const glm::vec3& base, const glm::vec3& tip);

	// Draws a debugging arrow
	// This object will only be drawn once.
	// To keep or update the arrow, drawDebugArrow must be called every frame.
	static void drawDebugArrow(const glm::vec3& base, const glm::vec3& direction, float magnitude);

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

	// Sets the environment map for reflections
	void setEnvironmentMap(const Entity&);

private:
	static void renderModel(const ModelComponent&, const glm::mat4& transform);

	static RenderState s_renderState;
	RenderState m_renderState;

	Scene& m_scene;
};
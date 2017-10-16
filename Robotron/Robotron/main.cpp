//
// Bachelor of Software Engineering
// Media Design School
// Auckland
// New Zealand
//
// (c) 2017 Media Design School
//
// Description  : The main entry point for the program.
//                Handles executing the main loop.
// Author       : Lance Chaney
// Mail         : lance.cha7337@mediadesign.school.nz
//

#define _USE_MATH_DEFINES

#include "GLUtils.h"
#include "SceneUtils.h"
#include "InputSystem.h"
#include "PlayerControlSystem.h"
#include "RenderSystem.h"
#include "Scene.h"
#include "GameplayLogicSystem.h"
#include "NetworkSystem.h"

#include <GLFW\glfw3.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include <cmath>

int main()
{
	// Init combined Window and OpenGL context.
	GLFWwindow* window = GLUtils::initOpenGL();

	// Initialize the systems that act on components.
	Scene scene;

	RenderSystem renderSystem(window, scene);
	PlayerControlSystem movementSystem(scene);
	InputSystem inputSystem(window, scene);
	GameplayLogicSystem gameplayLogicSystem(scene, inputSystem);
	NetworkSystem networkSystem;

	// Create 3D entities.
	// Order matters right now, selection buttons are assigned to the first four entities created
	SceneUtils::createSphere(scene, glm::translate({}, glm::vec3{ -1.5f, 1.5f, 0 }));
	SceneUtils::createQuad(scene, 
		  glm::translate({}, glm::vec3{ 1.5f, 1.5f, 0})
		* glm::rotate(glm::mat4{}, static_cast<float>(-M_PI / 16), glm::vec3{ 1, 0, 0 }));
	SceneUtils::createCylinder(scene, 1.5, 1.5,
		  glm::translate(glm::mat4{}, glm::vec3{ -1.5f, -1.5f, 0 })
		* glm::rotate(glm::mat4{}, static_cast<float>(M_PI / 4), glm::vec3{ 0, 0, 1 }));
	SceneUtils::createPyramid(scene, glm::translate({}, glm::vec3{ 1.5f, -1.5f, 0 }));
	
	// Create the skybox
	size_t skybox = SceneUtils::createSkybox(scene, {
		"Assets/Textures/Skybox/right.jpg",
		"Assets/Textures/Skybox/left.jpg",
		"Assets/Textures/Skybox/top.jpg",
		"Assets/Textures/Skybox/bottom.jpg",
		"Assets/Textures/Skybox/back.jpg",
		"Assets/Textures/Skybox/front.jpg",
	});
	// Set skybox as environment map for reflections
	renderSystem.setEnvironmentMap(skybox);

	// Setup the camera
	size_t cameraEntity = SceneUtils::createCamera(scene, { 0, 0, 6 }, { 0, 0, 0 }, { 0, 1, 0 });
	renderSystem.setCamera(cameraEntity);

	while (!glfwWindowShouldClose(window)) {
		// Do any operations that should only happen once per frame.
		inputSystem.beginFrame();
		networkSystem.beginFrame();
		renderSystem.beginRender();

		// Update all the entities using all the systems.
		for (size_t entityID = 0; entityID < SceneUtils::getEntityCount(scene); ++entityID) {
			gameplayLogicSystem.update(entityID);
			inputSystem.update(entityID);
			movementSystem.update(entityID);
			renderSystem.update(entityID);
		}
		
		// Do operations that should happen at the end of the frame.
		renderSystem.endRender();
		
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
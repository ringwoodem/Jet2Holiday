#pragma once

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// project
#include "opengl.hpp"
#include "cgra/cgra_mesh.hpp"

#include "terrain.hpp"
#include "water.hpp"
#include "tree.hpp"

// Basic model that holds the shader, mesh and transform for drawing.
// Can be copied and modified for adding in extra information for drawing
// including textures for texture mapping etc.

struct basic_model {
	GLuint shader = 0;
	cgra::gl_mesh mesh;
	glm::vec3 color{0.7};
	glm::mat4 modelTransform{1.0};
	GLuint texture;

	void draw(const glm::mat4 &view, const glm::mat4 proj);
};


// Main application class
//
class Application {
private:
	// window
	glm::vec2 m_windowsize;
	GLFWwindow *m_window;

	GLuint m_shader;
	GLuint m_terrainShader;
	GLuint m_waterShader;
	GLuint m_skyboxShader;
	GLuint m_causticsShader;
	GLuint m_treeShader;

	Terrain m_terrain;
	Water m_water;
    
    // Tree things
    std::vector<Tree> m_trees;
    TreeParameters m_treeParams;
    bool m_showTrees = true;

	GLuint m_sandTexture;

	GLuint cubemap;
	GLuint skyboxVAO = 0, skyboxVBO = 0;

	float skyboxVertices[108] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	GLuint m_grassTexture;
	GLuint m_grassNormal;
	GLuint m_grassRoughness;

	GLuint m_trunkTexture;
	GLuint m_trunkNormal;
	GLuint m_trunkRoughness;

	// oribital camera
	float m_pitch = .86;
	float m_yaw = -.86;
	float m_distance = 20;

	// last input
	bool m_leftMouseDown = false;
	glm::vec2 m_mousePosition;

	// drawing flags
	bool m_show_axis = false;
	bool m_show_grid = false;
	bool m_showWireframe = false;

	// geometry
	basic_model m_model;
	cgra::gl_mesh m_sandMesh;

	float m_time = 0.0f; // seconds
	float sunOrbitRadius = 200.0f;
	float sunHeight = 100.0f;
	float sunSpeed = 0.2f;

public:
	// setup
	Application(GLFWwindow *);

	// disable copy constructors (for safety)
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	// rendering callbacks (every frame)
	void render();
	void renderGUI();

	// input callbacks
	void cursorPosCallback(double xpos, double ypos);
	void mouseButtonCallback(int button, int action, int mods);
	void scrollCallback(double xoffset, double yoffset);
	void keyCallback(int key, int scancode, int action, int mods);
	void charCallback(unsigned int c);
	GLuint loadTexture(const std::string& filepath);
	GLuint loadCubemap(const std::vector<std::string>& faces);
	void initSkybox();
	void renderSandPlane(const glm::mat4& view, const glm::mat4& proj, float time, const glm::vec3& sunPos, const glm::vec3& sunColour);
	void renderSkybox(GLuint skyboxShader, GLuint skyboxVAO, GLuint cubemap, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& sunPos, const glm::vec3& sunColour);
};

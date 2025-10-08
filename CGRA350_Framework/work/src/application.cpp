
// std
#include <iostream>
#include <string>
#include <chrono>

// glm
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// project
#include "application.hpp"
#include "cgra/cgra_geometry.hpp"
#include "cgra/cgra_gui.hpp"
#include "cgra/cgra_image.hpp"
#include "cgra/cgra_shader.hpp"
#include "cgra/cgra_wavefront.hpp"


using namespace std;
using namespace cgra;
using namespace glm;


void basic_model::draw(const glm::mat4 &view, const glm::mat4 proj) {
	mat4 modelview = view * modelTransform;
	
	glUseProgram(shader); // load shader and variables
	glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, value_ptr(modelview));
	glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	mesh.draw(); // draw
}


Application::Application(GLFWwindow *window) : m_window(window) {
	
	shader_builder sb;
    sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_vert.glsl"));
	sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_frag.glsl"));
	m_shader = sb.build();
    m_panel.init(m_shader);
    m_cam.yawDeg = 90.0f;
    //m_panel.setPanelZ(5.0f);
    m_panel.setPanelZ(0.2f);

	// terrain shader
	shader_builder terrain_sb;
	terrain_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//color_vert.glsl"));
	terrain_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//terrain_frag.glsl"));
	m_terrainShader = terrain_sb.build();

	m_model.shader = m_shader;
	m_model.mesh = load_wavefront_data(CGRA_SRCDIR + std::string("/res//assets//teapot.obj")).build();
	m_model.color = vec3(1, 0, 0);

	m_terrain = Terrain(128, 128, 20.0f);
}


void Application::render() {
	//temp
    int winW, winH;  glfwGetWindowSize(m_window, &winW, &winH);
    int fbW,  fbH;   glfwGetFramebufferSize(m_window, &fbW, &fbH);
    
    glViewport(0, 0, fbW, fbH);
    float aspect = (fbH > 0) ? float(fbW) / float(fbH) : 1.0f;
    
    
	// retrieve the window hieght
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height); 

	m_windowsize = vec2(width, height); // update window size
	//glViewport(0, 0, width, height); // set the viewport to draw to the entire window

	// clear the back-buffer
	glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

	// enable flags for normal/forward rendering
	glEnable(GL_DEPTH_TEST); 
	glDepthFunc(GL_LESS);

	/** projection matrix
	mat4 proj = perspective(1.f, float(width) / height, 0.1f, 1000.f);

	// view matrix
	mat4 view = translate(mat4(1), vec3(0, 0, -m_distance))
		* rotate(mat4(1), m_pitch, vec3(1, 0, 0))
		* rotate(mat4(1), m_yaw,   vec3(0, 1, 0));*/
    
    //camera
    
    //float aspect = (height > 0) ? float(width)/float(height) : 1.0f;
    m_cam.compute(aspect);
    mat4 proj = m_cam.proj;
    mat4 view = m_cam.view;

    bool leftDownScene = m_leftMouseDown && !ImGui::GetIO().WantCaptureMouse;
    m_panel.frame(winW, winH, m_mousePosition, leftDownScene, view, proj, m_cam);

    //bool leftDownScene = m_leftMouseDown && !ImGui::GetIO().WantCaptureMouse;
    //m_panel.frame(width, height, m_mousePosition, leftDownScene, view, proj, m_cam);
    
	m_time += 0.016f;
    


	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);


	// draw the model
	//m_model.draw(view, proj);

	float angle = m_time * sunSpeed;
	vec3 sunPos = vec3(
		sunOrbitRadius * cos(angle),
		sunHeight * sin(angle) + sunHeight,
		sunOrbitRadius * sin(angle)
	);

	float t = glm::clamp(sin(angle) * 0.5f + 0.5f, 0.0f, 1.0f);
	vec3 sunColour = mix(
		vec3(1.0f, 0.5f, 0.2f),
		vec3(1.0f, 1.0f, 1.0f),
		t
	);

	m_terrain.draw(view, proj, m_shader, vec3(0.2f, 0.8f, 0.2f), sunPos, sunColour);
}


void Application::renderGUI() {

	// setup window
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiSetCond_Once);
	ImGui::Begin("Options", 0);

	// display current camera parameters
	ImGui::Text("Application %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	//ImGui::SliderFloat("Pitch", &m_pitch, -pi<float>() / 2, pi<float>() / 2, "%.2f");
	//ImGui::SliderFloat("Yaw", &m_yaw, -pi<float>(), pi<float>(), "%.2f");
	//ImGui::SliderFloat("Distance", &m_distance, 0, 100, "%.2f", 2.0f);

	// helpful drawing options
	ImGui::Checkbox("Show axis", &m_show_axis);
	ImGui::SameLine();
	ImGui::Checkbox("Show grid", &m_show_grid);
	ImGui::Checkbox("Wireframe", &m_showWireframe);
	ImGui::SameLine();
	if (ImGui::Button("Screenshot")) rgba_image::screenshot(true);

	
	ImGui::Separator();
	ImGui::Text("Terrain Settings");

	static float amplitude = m_terrain.getAmplitude();
	static float frequency = m_terrain.getFrequency();
	static int octaves = m_terrain.getOctaves();
	static float persistence = m_terrain.getPersistence();
	static float lacunarity = m_terrain.getLacunarity();

	bool terrainChanged = false;

	if (ImGui::SliderFloat("Amplitude", &amplitude, 0.1f, 50.0f)) {
		m_terrain.setAmplitude(amplitude);
		terrainChanged = true;
	}

	if (ImGui::SliderFloat("Frequency", &frequency, 0.01f, 1.0f)) {
		m_terrain.setFrequency(frequency);
		terrainChanged = true;
	}

	if (ImGui::SliderInt("Octaves", &octaves, 1, 8)) {
		m_terrain.setOctaves(octaves);
		terrainChanged = true;
	}

	if (ImGui::SliderFloat("Persistence", &persistence, 0.1f, 1.0f)) {
		m_terrain.setPersistence(persistence);
		terrainChanged = true;
	}

	if (ImGui::SliderFloat("Lacunarity", &lacunarity, 1.5f, 4.0f)) {
		m_terrain.setLacunarity(lacunarity);
		terrainChanged = true;
	}

	if (terrainChanged) {
		m_terrain.update();
	}

	// finish creating window
	ImGui::End();
}


void Application::cursorPosCallback(double xpos, double ypos) {

    if (m_rightMouseDown) {
        if (m_firstMouse) { m_lastX = xpos; m_lastY = ypos; m_firstMouse = false; }
        double dx = xpos - m_lastX, dy = ypos - m_lastY;
        m_lastX = xpos; m_lastY = ypos;
        m_cam.mouseLook(float(dx), float(dy));
    }
    m_mousePosition = glm::vec2(xpos, ypos);
}


void Application::mouseButtonCallback(int button, int action, int mods) {
	/**(void)mods; // currently un-used

	// capture is left-mouse down
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		m_leftMouseDown = (action == GLFW_PRESS); // only other option is GLFW_RELEASE*/
    
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT){
        m_leftMouseDown = (action == GLFW_PRESS);
    }
    
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        m_rightMouseDown = (action == GLFW_PRESS);
        m_firstMouse = true;
        glfwSetInputMode(m_window, GLFW_CURSOR, m_rightMouseDown ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
}


void Application::scrollCallback(double xoffset, double yoffset) {
	//(void)xoffset; // currently un-used
	//m_distance *= pow(1.1f, -yoffset);
    
    m_cam.fovDeg = std::clamp(m_cam.fovDeg - float(yoffset) * 2.0f, 30.0f, 100.0f);
}


void Application::keyCallback(int key, int scancode, int action, int mods) {
	(void)key, (void)scancode, (void)action, (void)mods; // currently un-used
}


void Application::charCallback(unsigned int c) {
	(void)c; // currently un-used
}

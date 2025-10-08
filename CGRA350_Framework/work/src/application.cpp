
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
	float scene_size = 200.0f;

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

	// water shader
	shader_builder water_sb;
	water_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//water_vert.glsl"));
	water_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//water_frag.glsl"));
	m_waterShader = water_sb.build();

	// skybox shader
	shader_builder skybox_sb;
	skybox_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//skybox_vert.glsl"));
	skybox_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//skybox_frag.glsl"));
	m_skyboxShader = skybox_sb.build();

	shader_builder caustics_sb;
	caustics_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//caustics_vert.glsl"));
	caustics_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//caustics_frag.glsl"));
	m_causticsShader = caustics_sb.build();

	shader_builder tree_sb;
	tree_sb.set_shader(GL_VERTEX_SHADER, CGRA_SRCDIR + std::string("//res//shaders//tree_vert.glsl"));
	tree_sb.set_shader(GL_FRAGMENT_SHADER, CGRA_SRCDIR + std::string("//res//shaders//tree_frag.glsl"));
	m_treeShader = tree_sb.build();

	stbi_set_flip_vertically_on_load(false);
	std::vector<std::string> faces = {
		CGRA_SRCDIR + std::string("//res//textures//cubemap//px.png"),
		CGRA_SRCDIR + std::string("//res//textures//cubemap//nx.png"),
		CGRA_SRCDIR + std::string("//res//textures//cubemap//py.png"),
		CGRA_SRCDIR + std::string("//res//textures//cubemap//ny.png"),
		CGRA_SRCDIR + std::string("//res//textures//cubemap//pz.png"),
		CGRA_SRCDIR + std::string("//res//textures//cubemap//nz.png")
	};
	cubemap = loadCubemap(faces);

	m_model.shader = m_shader;
	m_model.mesh = load_wavefront_data(CGRA_SRCDIR + std::string("/res//assets//teapot.obj")).build();
	m_model.color = vec3(1, 0, 0);

	m_terrain = Terrain(512, 512, scene_size);
	m_water = Water(2048, scene_size);

    std::cout << "Creating trees..." << std::endl;
    for (int i = 0; i < 5; i++) {
        glm::vec3 pos(
            (i - 2) * 15.0f,
            0.0f,
            -20.0f
        );
        Tree tree(pos);
        m_trees.push_back(tree);
    }

	cgra::mesh_builder mb;
	float size = scene_size / 2;

	// Vertices (XZ plane at y = -1.0f)
	mb.push_vertex({ glm::vec3(-size, -1.0f, -size), glm::vec3(0,1,0), glm::vec2(0,0) });
	mb.push_vertex({ glm::vec3(size, -1.0f, -size), glm::vec3(0,1,0), glm::vec2(1,0) });
	mb.push_vertex({ glm::vec3(size, -1.0f,  size), glm::vec3(0,1,0), glm::vec2(1,1) });
	mb.push_vertex({ glm::vec3(-size, -1.0f,  size), glm::vec3(0,1,0), glm::vec2(0,1) });

	// Indices
	mb.push_index(0); mb.push_index(1); mb.push_index(2);
	mb.push_index(2); mb.push_index(3); mb.push_index(0);

	m_sandMesh = mb.build();

	m_grassTexture = loadTexture(CGRA_SRCDIR + std::string("/res/textures/grass.jpg"));
	m_grassNormal = loadTexture(CGRA_SRCDIR + std::string( "/res/textures/normal.jpg"));
	m_grassRoughness = loadTexture(CGRA_SRCDIR + std::string("/res/textures/roughness.jpg"));
	m_sandTexture = loadTexture(CGRA_SRCDIR + std::string("/res/textures/sand.png"));

	m_trunkTexture = loadTexture(CGRA_SRCDIR + std::string("/res/textures/bark_willow_diff_4k.jpg"));
	m_trunkNormal = loadTexture(CGRA_SRCDIR + std::string("/res/textures/bark_willow_nor_gl_4k.jpg"));
	m_trunkRoughness = loadTexture(CGRA_SRCDIR + std::string("/res/textures/bark_willow_rough_4k.jpg"));

	initSkybox();

	if (m_grassTexture == 0 || m_grassNormal == 0 || m_grassRoughness == 0) {
		std::cerr << "Warning: Some grass textures failed to load" << std::endl;
	}
}

void Application::initSkybox() {
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
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
		sunOrbitRadius * cos(angle),   // X: circle
		100.0f * sin(angle),           // Y: oscillates from -100 to 100
		sunOrbitRadius * sin(angle)    // Z: circle
	);

	float t = glm::clamp(sin(angle) * 0.5f + 0.5f, 0.0f, 1.0f);
	vec3 sunColour = mix(
		vec3(1.0f, 0.5f, 0.2f),
		vec3(1.0f, 1.0f, 1.0f),
		t
	);

	// helpful draw options
	if (m_show_grid) drawGrid(view, proj);
	if (m_show_axis) drawAxis(view, proj);
	glPolygonMode(GL_FRONT_AND_BACK, (m_showWireframe) ? GL_LINE : GL_FILL);

	glDepthFunc(GL_LEQUAL);
	renderSkybox(m_skyboxShader, skyboxVAO, cubemap, view, proj, sunPos, sunColour);
	glDepthFunc(GL_LESS);

	renderSandPlane(view, proj, m_time, sunPos, sunColour);

	// draw the model
	m_terrain.draw(view, proj, m_terrainShader, vec3(0.2f, 0.8f, 0.2f), sunPos, sunColour, m_grassTexture, m_grassNormal, m_grassRoughness);
  
	// Draw trees
	for (auto& tree : m_trees) {
		tree.draw(view, proj, m_treeShader, sunPos, sunColour, m_trunkTexture, m_trunkNormal, m_trunkRoughness);
	}

	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
	lastTime = currentTime;

	m_water.update(deltaTime);
	m_water.draw(view, proj, m_waterShader, cubemap, vec3(0.1f, 0.3f, 0.7f));

}

void Application::renderSandPlane(const glm::mat4& view, const glm::mat4& proj, float time, const glm::vec3& sunPos, const glm::vec3& sunColour) {
	glUseProgram(m_causticsShader);

	// Model matrix for sand plane (identity, or translate if needed)
	glm::mat4 modelview = view * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

	// Set transformation uniforms
	glUniformMatrix4fv(glGetUniformLocation(m_causticsShader, "uModelViewMatrix"), 1, GL_FALSE, glm::value_ptr(modelview));
	glUniformMatrix4fv(glGetUniformLocation(m_causticsShader, "uProjectionMatrix"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniform3fv(glGetUniformLocation(m_causticsShader, "uSunPos"), 1, glm::value_ptr(sunPos));
	glUniform3fv(glGetUniformLocation(m_causticsShader, "uSunColor"), 1, glm::value_ptr(sunColour));

	// Set caustics uniforms (tweak these as needed)
	glUniform1f(glGetUniformLocation(m_causticsShader, "uTime"), time);
	glUniform3fv(glGetUniformLocation(m_causticsShader, "uCausticsColor"), 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.8f))); // pale yellow caustics
	glUniform1f(glGetUniformLocation(m_causticsShader, "uCausticsIntensity"), 0.78f);
	glUniform1f(glGetUniformLocation(m_causticsShader, "uCausticsOffset"), 0.3f);
	glUniform1f(glGetUniformLocation(m_causticsShader, "uCausticsScale"), 8.0f);
	glUniform1f(glGetUniformLocation(m_causticsShader, "uCausticsSpeed"), 0.5f);
	glUniform1f(glGetUniformLocation(m_causticsShader, "uCausticsThickness"), 0.75f);

	// Bind sand texture to texture unit 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_sandTexture);
	glUniform1i(glGetUniformLocation(m_causticsShader, "uTexture"), 0);

	// Draw the sand mesh
	m_sandMesh.draw();
}

void Application::renderSkybox(GLuint skyboxShader, GLuint skyboxVAO, GLuint cubemap, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& sunPos, const glm::vec3& sunColour) {
	glDepthMask(GL_FALSE);
	glCullFace(GL_FRONT);

	glUseProgram(skyboxShader);

	glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glUniform3fv(glGetUniformLocation(skyboxShader, "uSunPos"), 1, glm::value_ptr(sunPos));
	glUniform3fv(glGetUniformLocation(skyboxShader, "uSunColor"), 1, glm::value_ptr(sunColour));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);

	glBindVertexArray(skyboxVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glCullFace(GL_BACK);
	glDepthMask(GL_TRUE);
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
	static float minHeight = m_terrain.getMinHeight();

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

	if (ImGui::SliderFloat("Min Height (Water Depth)", &minHeight, -10.0f, 0.0f)) {
		m_terrain.setMinHeight(minHeight);
		terrainChanged = true;
	}
    
    // tree stuff
    ImGui::Separator();
    ImGui::Text("Tree Settings");
    ImGui::Checkbox("Show Trees", &m_showTrees);

    if (m_showTrees && !m_trees.empty()) {
        TreeParameters& params = m_trees[0].getParameters();
        bool changed = false;
        
        ImGui::Text("Overall Shape");
        if (ImGui::SliderFloat("Scale (Height)", &params.scale, 5.0f, 30.0f)) changed = true;
        if (ImGui::SliderFloat("Base Size", &params.baseSize, 0.1f, 1.0f)) changed = true;
        if (ImGui::SliderFloat("Ratio", &params.ratio, 0.01f, 0.05f)) changed = true;
        if (ImGui::SliderFloat("Flare", &params.flare, 0.0f, 1.5f)) changed = true;
        
        ImGui::Separator();
        ImGui::Text("Trunk (Level 0)");
        if (ImGui::SliderInt("Segments##0", &params.level[0].nCurveRes, 3, 20)) changed = true;
        if (ImGui::SliderFloat("Curve##0", &params.level[0].nCurve, -50.0f, 50.0f)) changed = true;
        if (ImGui::SliderFloat("Curve Var##0", &params.level[0].nCurveV, 0.0f, 50.0f)) changed = true;
        if (ImGui::SliderInt("Branches##0", &params.level[0].nBranches, 0, 50)) changed = true;
        if (ImGui::SliderFloat("Branch Dist##0", &params.level[0].nBranchDist, -2.0f, 2.0f)) changed = true;
        
        if (params.levels > 1) {
            ImGui::Separator();
            ImGui::Text("Main Branches (Level 1)");
            if (ImGui::SliderFloat("Length##1", &params.level[1].nLength, 0.1f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("Length Var##1", &params.level[1].nLengthV, 0.0f, 0.2f)) changed = true;
            if (ImGui::SliderInt("Segments##1", &params.level[1].nCurveRes, 3, 15)) changed = true;
            if (ImGui::SliderFloat("Curve##1", &params.level[1].nCurve, -100.0f, 100.0f)) changed = true;
            if (ImGui::SliderFloat("Curve Var##1", &params.level[1].nCurveV, 0.0f, 100.0f)) changed = true;
            if (ImGui::SliderInt("Child Branches##1", &params.level[1].nBranches, 0, 30)) changed = true;
            if (ImGui::SliderFloat("Down Angle##1", &params.level[1].nDownAngle, 0.0f, 90.0f)) changed = true;
            if (ImGui::SliderFloat("Down Var##1", &params.level[1].nDownAngleV, 0.0f, 30.0f)) changed = true;
            if (ImGui::SliderFloat("Rotate##1", &params.level[1].nRotate, 0.0f, 180.0f)) changed = true;
        }
        
        if (params.levels > 2) {
            ImGui::Separator();
            ImGui::Text("Twigs (Level 2)");
            if (ImGui::SliderFloat("Length##2", &params.level[2].nLength, 0.1f, 1.0f)) changed = true;
            if (ImGui::SliderInt("Segments##2", &params.level[2].nCurveRes, 3, 10)) changed = true;
            if (ImGui::SliderFloat("Curve##2", &params.level[2].nCurve, -100.0f, 100.0f)) changed = true;
            if (ImGui::SliderFloat("Down Angle##2", &params.level[2].nDownAngle, 0.0f, 90.0f)) changed = true;
        }
        
        ImGui::Separator();
        ImGui::Text("Leaves");
        if (ImGui::Checkbox("Show Leaves", &params.hasLeaves)) changed = true;
        if (params.hasLeaves) {
            if (ImGui::SliderFloat("Leaf Scale", &params.leafScale, 0.05f, 0.5f)) changed = true;
            if (ImGui::SliderInt("Per Branch", &params.leavesPerBranch, 1, 15)) changed = true;
            
            if (ImGui::TreeNode("Leaf Shape")) {
                if (ImGui::SliderFloat("Width", &params.leafParams.lobeWidth, 0.1f, 1.0f)) changed = true;
                if (ImGui::SliderFloat("Height", &params.leafParams.lobeHeight, 0.3f, 2.0f)) changed = true;
                if (ImGui::SliderFloat("Offset", &params.leafParams.lobeOffset, 0.0f, 0.5f)) changed = true;
                if (ImGui::SliderFloat("Top Angle", &params.leafParams.topAngle, 10.0f, 80.0f)) changed = true;
                if (ImGui::SliderFloat("Bottom Angle", &params.leafParams.bottomAngle, 10.0f, 80.0f)) changed = true;
                if (ImGui::SliderInt("Lobes", &params.leafParams.lobeCount, 1, 5)) changed = true;
                
                if (params.leafParams.lobeCount > 1) {
                    if (ImGui::SliderFloat("Lobe Separation", &params.leafParams.lobeSeparation, 60.0f, 180.0f)) changed = true;
                    if (ImGui::SliderFloat("Lobe Scale", &params.leafParams.lobeScale, 0.5f, 1.0f)) changed = true;
                }
                
                ImGui::TreePop();
            }
        }
        
        if (changed) {
            for (auto& tree : m_trees) {
                tree.setParameters(params);
            }
        }
    }

	// finish creating window
	ImGui::End();
}

GLuint Application::loadCubemap(const std::vector<std::string>& faces) {
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (GLuint i = 0; i < faces.size(); i++) {
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data) {
			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
				GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else {
			std::cerr << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
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


GLuint Application::loadTexture(const std::string& filepath) {
	try {
		// Load image - constructor handles everything
		cgra::rgba_image img(filepath);

		// Set wrapping mode to repeat for tiling
		img.wrap = glm::vec<2, GLenum>(GL_REPEAT, GL_REPEAT);

		// Upload to GPU - uploadTexture() handles all OpenGL calls
		GLuint texture = img.uploadTexture();

		std::cout << "Loaded texture: " << filepath << " ("
			<< img.size.x << "x" << img.size.y << ")" << std::endl;

		return texture;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to load texture: " << filepath << std::endl;
		std::cerr << "Error: " << e.what() << std::endl;
		return 0;
	}
}

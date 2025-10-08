// std
#include <cmath>
#include <random>
#include <algorithm>
#include <numbers>

// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

// OpenGL
#include "opengl.hpp"

// project
#include "water.hpp"

Water::Water(int gridSize, float lengthScale)
    : m_gridSize(gridSize), m_lengthScale(lengthScale), m_time(0.0f),
    m_seaLevel(0.0f), m_meshGenerated(false) {

    initializeWaves();
    generateMesh();
}

void Water::initializeWaves() {
    m_waves.clear();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());

    // Create multiple Gerstner waves with varying parameters
    // Similar to your Three.js FBM approach but with Gerstner waves

    int numWaves = 8;
    float baseWavelength = 8.0f;
    float baseAmplitude = 0.15f;

    for (int i = 0; i < numWaves; i++) {
        GerstnerWave wave;

        // Exponentially decrease amplitude and wavelength
        wave.wavelength = baseWavelength * std::pow(1.8f, static_cast<float>(i));
        wave.amplitude = baseAmplitude * std::pow(0.6f, static_cast<float>(i));

        // Calculate speed from dispersion relation: speed = sqrt(g * wavelength / (2*pi))
        float gravity = 9.81f;
        wave.speed = std::sqrt(gravity * wave.wavelength / (2.0f * glm::pi<float>()));

        // Steepness controls how peaked the waves are (0 = sinusoidal, 1 = very peaked)
        wave.steepness = 0.3f / (static_cast<float>(numWaves) * wave.amplitude);
        wave.steepness = glm::clamp(wave.steepness, 0.0f, 1.0f);

        // Vary directions but keep them mostly aligned
        float angleVariation = (i % 2 == 0 ? 1.0f : -1.0f) * 0.3f * static_cast<float>(i) / numWaves;
        float angle = angleVariation;
        wave.direction = glm::normalize(glm::vec2(std::cos(angle), std::sin(angle)));

        m_waves.push_back(wave);
    }
}

glm::vec3 Water::calculateWaveDisplacement(const glm::vec2& position, float time) const {
    glm::vec3 result(position.x, 0.0f, position.y);

    for (const auto& wave : m_waves) {
        float k = 2.0f * glm::pi<float>() / wave.wavelength;
        float c = wave.speed;
        float a = wave.amplitude;
        float Q = wave.steepness;

        glm::vec2 d = wave.direction;

        float phase = k * glm::dot(d, position) - c * k * time;
        float sinPhase = std::sin(phase);
        float cosPhase = std::cos(phase);

        // Gerstner wave equations
        result.x += Q * a * d.x * cosPhase;
        result.y += a * sinPhase;
        result.z += Q * a * d.y * cosPhase;
    }

    return result;
}

glm::vec3 Water::calculateNormal(const glm::vec2& position, float time) const {
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    for (const auto& wave : m_waves) {
        float k = 2.0f * glm::pi<float>() / wave.wavelength;
        float c = wave.speed;
        float a = wave.amplitude;
        float Q = wave.steepness;

        glm::vec2 d = wave.direction;

        float phase = k * glm::dot(d, position) - c * k * time;
        float sinPhase = std::sin(phase);
        float cosPhase = std::cos(phase);

        float WA = k * a;

        // Partial derivatives for normal calculation
        normal.x -= d.x * WA * cosPhase;
        normal.y -= Q * WA * sinPhase;
        normal.z -= d.y * WA * cosPhase;
    }

    return glm::normalize(normal);
}

void Water::generateMesh() {
    cgra::mesh_builder mb;

    float cellSize = m_lengthScale / m_gridSize;

    // Generate vertices - FLAT GRID, no displacement
    for (int z = 0; z < m_gridSize; z++) {
        for (int x = 0; x < m_gridSize; x++) {
            float worldX = (x - m_gridSize / 2.0f) * cellSize;
            float worldZ = (z - m_gridSize / 2.0f) * cellSize;

            cgra::mesh_vertex vertex;
            // Simple flat position at sea level
            vertex.pos = glm::vec3(worldX, m_seaLevel, worldZ);
            // Default normal pointing up
            vertex.norm = glm::vec3(0.0f, 1.0f, 0.0f);
            // UV coordinates
            vertex.uv = glm::vec2(static_cast<float>(x) / (m_gridSize - 1),
                static_cast<float>(z) / (m_gridSize - 1));

            mb.push_vertex(vertex);
        }
    }

    // Generate indices (same as before)
    for (int z = 0; z < m_gridSize - 1; z++) {
        for (int x = 0; x < m_gridSize - 1; x++) {
            int topLeft = z * m_gridSize + x;
            int topRight = z * m_gridSize + x + 1;
            int bottomLeft = (z + 1) * m_gridSize + x;
            int bottomRight = (z + 1) * m_gridSize + x + 1;

            mb.push_index(topLeft);
            mb.push_index(bottomLeft);
            mb.push_index(topRight);

            mb.push_index(topRight);
            mb.push_index(bottomLeft);
            mb.push_index(bottomRight);
        }
    }

    m_mesh = mb.build();
    m_meshGenerated = true;
}

void Water::update(float deltaTime) {
    m_time += deltaTime * 0.5f; // Speed multiplier
}

void Water::draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, GLuint cubemap,
    const glm::vec3& color, const glm::vec3& sunPos, const glm::vec3& sunColour) {
    if (!m_meshGenerated) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

    glUseProgram(shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    glUniform1i(glGetUniformLocation(shader, "uEnvironmentMap"), 0); // 0 = GL_TEXTURE0

    glUniformMatrix4fv(glGetUniformLocation(shader, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shader, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(proj));

    glUniform3fv(glGetUniformLocation(shader, "cameraPosition"), 1, glm::value_ptr(cameraPos));

    glUniform3fv(glGetUniformLocation(shader, "uSunPos"), 1, glm::value_ptr(sunPos));
    glUniform3fv(glGetUniformLocation(shader, "uSunColor"), 1, glm::value_ptr(sunColour));

    glUniform1f(glGetUniformLocation(shader, "uOpacity"), 0.8f);

    glUniform3fv(glGetUniformLocation(shader, "uTroughColor"), 1, glm::value_ptr(glm::vec3(0.094f, 0.400f, 0.569f)));    // deep blue
    glUniform3fv(glGetUniformLocation(shader, "uSurfaceColor"), 1, glm::value_ptr(glm::vec3(0.608f, 0.847f, 0.753f)));    // surface blue
    glUniform3fv(glGetUniformLocation(shader, "uPeakColor"), 1, glm::value_ptr(glm::vec3(0.733f, 0.847f, 0.878f)));       // light/white for peaks

    glUniform1f(glGetUniformLocation(shader, "uPeakThreshold"), 0.08f);      // adjust as needed
    glUniform1f(glGetUniformLocation(shader, "uPeakTransition"), 0.05f);     // adjust as needed
    glUniform1f(glGetUniformLocation(shader, "uTroughThreshold"), -0.04f);   // adjust as needed
    glUniform1f(glGetUniformLocation(shader, "uTroughTransition"), 0.15f);   // adjust as needed

    glUniform1f(glGetUniformLocation(shader, "uFresnelScale"), 0.65f);       // adjust as needed
    glUniform1f(glGetUniformLocation(shader, "uFresnelPower"), 0.68f);       // adjust as needed

    glUniform1f(glGetUniformLocation(shader, "uTime"), m_time);
    glUniform1f(glGetUniformLocation(shader, "uWavesAmplitude"), 0.02f);
    glUniform1f(glGetUniformLocation(shader, "uWavesFrequency"), 1.5f);
    glUniform1f(glGetUniformLocation(shader, "uWavesSpeed"), 0.6f);
    glUniform1f(glGetUniformLocation(shader, "uWavesPersistence"), 0.330f);
    glUniform1f(glGetUniformLocation(shader, "uWavesLacunarity"), 1.5f);
    glUniform1f(glGetUniformLocation(shader, "uWavesIterations"), 7.0f);

    m_mesh.draw();
}

void Water::reset() {
    m_time = 0.0f;
    generateMesh();
}

float Water::getHeightAt(float x, float z, float time) const {
    glm::vec2 pos(x, z);
    glm::vec3 displacement = calculateWaveDisplacement(pos, time);
    return m_seaLevel + displacement.y;
}
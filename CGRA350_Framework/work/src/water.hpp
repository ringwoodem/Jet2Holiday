#pragma once

// std
#include <vector>
#include <complex>

// glm
#include <glm/glm.hpp>

// project
#include "cgra/cgra_mesh.hpp"

class Water {
private:
    struct GerstnerWave {
        float amplitude;
        float wavelength;
        float speed;
        float steepness;
        glm::vec2 direction;
    };

    int m_gridSize;
    float m_lengthScale;
    float m_time;

    std::vector<GerstnerWave> m_waves;
    cgra::gl_mesh m_mesh;
    bool m_meshGenerated;

    float m_seaLevel;

    void initializeWaves();
    void generateMesh();

    // Calculate wave displacement at a point
    glm::vec3 calculateWaveDisplacement(const glm::vec2& position, float time) const;
    glm::vec3 calculateNormal(const glm::vec2& position, float time) const;

public:
    Water(int gridSize = 256, float lengthScale = 100.0f);
    ~Water() = default;

    void update(float deltaTime);
    void draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, GLuint cubemap,
        const glm::vec3& color = glm::vec3(0.0f, 0.4f, 0.8f),
        const glm::vec3& sunPos = glm::vec3(0.0f, 100.0f, 0.0f),
        const glm::vec3& sunColour = glm::vec3(1.0f, 1.0f, 1.0f));

    void reset();
    float getHeightAt(float x, float z, float time) const;

    // Parameter setters
    void setSeaLevel(float level) { m_seaLevel = level; }
    float getSeaLevel() const { return m_seaLevel; }
};
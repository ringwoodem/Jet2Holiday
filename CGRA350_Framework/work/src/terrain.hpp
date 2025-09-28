#pragma once

// std
#include <vector>

// glm
#include <glm/glm.hpp>

// project
#include "cgra/cgra_mesh.hpp"

class Terrain {
private:
    // Terrain parameters
    int m_width;
    int m_height;
    float m_scale;
    float m_amplitude;
    float m_frequency;
    int m_octaves;
    float m_persistence;
    float m_lacunarity;

    // OpenGL data
    cgra::gl_mesh m_mesh;
    bool m_meshGenerated;

    // Height data
    std::vector<std::vector<float>> m_heightMap;

    // Perlin noise functions
    float fade(float t);
    float lerp(float t, float a, float b);
    float grad(int hash, float x, float y, float z);
    float noise(float x, float y, float z);
    float perlinNoise(float x, float y);

    // Mesh generation
    void generateHeightMap();
    void generateMesh();

    // Permutation table for noise
    static const int m_permutation[512];

public:
    Terrain(int width = 128, int height = 128, float scale = 100.0f);
    ~Terrain() = default;

    // Getters and setters
    void setWidth(int width) { m_width = width; m_meshGenerated = false; }
    void setHeight(int height) { m_height = height; m_meshGenerated = false; }
    void setScale(float scale) { m_scale = scale; m_meshGenerated = false; }
    void setAmplitude(float amplitude) { m_amplitude = amplitude; m_meshGenerated = false; }
    void setFrequency(float frequency) { m_frequency = frequency; m_meshGenerated = false; }
    void setOctaves(int octaves) { m_octaves = octaves; m_meshGenerated = false; }
    void setPersistence(float persistence) { m_persistence = persistence; m_meshGenerated = false; }
    void setLacunarity(float lacunarity) { m_lacunarity = lacunarity; m_meshGenerated = false; }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    float getScale() const { return m_scale; }
    float getAmplitude() const { return m_amplitude; }
    float getFrequency() const { return m_frequency; }
    int getOctaves() const { return m_octaves; }
    float getPersistence() const { return m_persistence; }
    float getLacunarity() const { return m_lacunarity; }

    // Height map access
    float getHeightAt(int x, int z) const;
    float getHeightAtWorld(float x, float z) const;

    // Rendering
    void draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, const glm::vec3& color = glm::vec3(0.2f, 0.8f, 0.2f));

    // Update terrain (regenerate if parameters changed)
    void update();

    // Force regeneration
    void regenerate();
};
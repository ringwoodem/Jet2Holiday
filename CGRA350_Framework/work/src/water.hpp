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
    // Grid parameters
    int m_gridSize;          // N x N grid (power of 2 for FFT)
    float m_lengthScale;     // Physical size of the simulation area
    float m_time;

    // Phillips spectrum parameters (Tessendorf)
    float m_windSpeed;       // Wind speed in m/s
    glm::vec2 m_windDirection;
    float m_amplitude;       // Wave amplitude scale (A in Phillips spectrum)
    float m_gravity;         // Gravity constant
    float m_dampingFactor;   // Small wave suppression
    float m_seaLevel;        // Height of water surface above origin
    float m_waveScale;       // Scale factor for wave visibility

    // Simulation data
    std::vector<std::complex<float>> m_h0;           // Initial height field (frequency domain)
    std::vector<std::complex<float>> m_h0Conj;       // Complex conjugate of h0
    std::vector<std::complex<float>> m_heightField;  // Current height field (frequency domain)
    std::vector<std::complex<float>> m_displacementX; // Horizontal displacement X
    std::vector<std::complex<float>> m_displacementZ; // Horizontal displacement Z

    std::vector<float> m_vertices;   // Spatial domain vertices (after FFT)
    std::vector<glm::vec3> m_normals;

    // Rendering
    cgra::gl_mesh m_mesh;
    bool m_meshGenerated;

    // Wave parameters for small-scale details (Muller)
    struct WaveParameters {
        float amplitude;
        float wavelength;
        float speed;
        glm::vec2 direction;
    };
    std::vector<WaveParameters> m_gerstnerWaves;

    // FFT helper functions
    void fft1D(std::vector<std::complex<float>>& data, int stride, int n);
    void fft2D(std::vector<std::complex<float>>& data);
    void ifft2D(std::vector<std::complex<float>>& data);

    // Phillips spectrum
    float phillipsSpectrum(const glm::vec2& k);

    // Complex number operations
    std::complex<float> gaussianRandom();

    // Height field generation (Tessendorf approach)
    void initializeHeightField();
    void updateHeightField(float deltaTime);

    // Mesh generation
    void generateMesh();
    void updateMeshVertices();

    // Normal calculation
    void calculateNormals();

    // Gerstner waves for detail (Muller approach)
    void initializeGerstnerWaves();
    glm::vec3 evaluateGerstnerWave(const glm::vec2& position, float time);

public:
    Water(int gridSize = 128, float lengthScale = 64.0f);
    ~Water() = default;

    // Parameter setters
    void setWindSpeed(float speed) { m_windSpeed = speed; }
    void setWindDirection(const glm::vec2& dir) { m_windDirection = glm::normalize(dir); }
    void setAmplitude(float amp) { m_amplitude = amp; }
    void setDampingFactor(float damping) { m_dampingFactor = damping; }
    void setChoppiness(float choppiness);
    void setSeaLevel(float level) { m_seaLevel = level; }

    // Parameter getters
    float getWindSpeed() const { return m_windSpeed; }
    glm::vec2 getWindDirection() const { return m_windDirection; }
    float getAmplitude() const { return m_amplitude; }
    float getDampingFactor() const { return m_dampingFactor; }
    float getSeaLevel() const { return m_seaLevel; }

    // Update and render
    void update(float deltaTime);
    void draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, const glm::vec3& color = glm::vec3(0.0f, 0.4f, 0.8f));

    // Reset simulation
    void reset();

    // Get height at world position (for boat interaction, etc.)
    float getHeightAt(float x, float z, float time) const;
};
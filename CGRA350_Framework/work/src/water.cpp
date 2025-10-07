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
    m_windSpeed(30.0f), m_windDirection(1.0f, 0.0f),
    m_amplitude(0.05f), m_gravity(9.81f), m_dampingFactor(0.0001f),
    m_seaLevel(0.2f), m_meshGenerated(false) {

    // Allocate frequency domain arrays
    int size = m_gridSize * m_gridSize;
    m_h0.resize(size);
    m_h0Conj.resize(size);
    m_heightField.resize(size);
    m_displacementX.resize(size);
    m_displacementZ.resize(size);
    m_vertices.resize(size);
    m_normals.resize(size);

    // Initialize the simulation
    initializeHeightField();
    initializeGerstnerWaves();
    generateMesh();
}

// Generate Gaussian random numbers (Box-Muller transform)
std::complex<float> Water::gaussianRandom() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float u1 = dist(gen);
    float u2 = dist(gen);

    if (u1 < 1e-6f) u1 = 1e-6f;

    float r = std::sqrt(-2.0f * std::log(u1));
    float theta = 2.0f * glm::pi<float>() * u2;

    return std::complex<float>(r * std::cos(theta), r * std::sin(theta));
}

// Phillips spectrum (Tessendorf)
float Water::phillipsSpectrum(const glm::vec2& k) {
    float k_length = glm::length(k);
    if (k_length < 0.000001f) return 0.0f;

    float k_length2 = k_length * k_length;
    float k_length4 = k_length2 * k_length2;

    // Wind speed
    float L = (m_windSpeed * m_windSpeed) / m_gravity;
    float L2 = L * L;

    // Direction term
    glm::vec2 k_norm = k / k_length;
    float k_dot_w = glm::dot(k_norm, m_windDirection);
    float k_dot_w2 = k_dot_w * k_dot_w;

    // Damping term for small waves
    float damping = std::exp(-k_length2 * m_dampingFactor * m_dampingFactor);

    // Phillips spectrum formula
    float phillips = m_amplitude * (std::exp(-1.0f / (k_length2 * L2)) / k_length4) * k_dot_w2 * damping;

    return phillips;
}

// Initialize height field with h0 (Tessendorf approach)
void Water::initializeHeightField() {
    for (int n = 0; n < m_gridSize; n++) {
        for (int m = 0; m < m_gridSize; m++) {
            int index = n * m_gridSize + m;

            // Wave vector k
            glm::vec2 k(
                (2.0f * glm::pi<float>() * (m - m_gridSize / 2.0f)) / m_lengthScale,
                (2.0f * glm::pi<float>() * (n - m_gridSize / 2.0f)) / m_lengthScale
            );

            // Phillips spectrum
            float P_k = phillipsSpectrum(k);

            // h0(k) = 1/sqrt(2) * (xi_r + i*xi_i) * sqrt(P(k))
            std::complex<float> gauss = gaussianRandom();
            m_h0[index] = gauss * std::sqrt(P_k) / std::sqrt(2.0f);

            // h0*(-k)
            glm::vec2 k_neg = -k;
            float P_k_neg = phillipsSpectrum(k_neg);
            std::complex<float> gauss_neg = gaussianRandom();
            m_h0Conj[index] = std::conj(gauss_neg * std::sqrt(P_k_neg) / std::sqrt(2.0f));
        }
    }
}

// Update height field based on dispersion relation
void Water::updateHeightField(float deltaTime) {
    m_time += deltaTime;

    for (int n = 0; n < m_gridSize; n++) {
        for (int m = 0; m < m_gridSize; m++) {
            int index = n * m_gridSize + m;

            // Wave vector k
            glm::vec2 k(
                (2.0f * glm::pi<float>() * (m - m_gridSize / 2.0f)) / m_lengthScale,
                (2.0f * glm::pi<float>() * (n - m_gridSize / 2.0f)) / m_lengthScale
            );

            float k_length = glm::length(k);

            // Dispersion relation: omega = sqrt(g * k)
            float omega = std::sqrt(m_gravity * k_length);

            // h(k,t) = h0(k)*e^(i*omega*t) + h0*(-k)*e^(-i*omega*t)
            std::complex<float> exp_pos(std::cos(omega * m_time), std::sin(omega * m_time));
            std::complex<float> exp_neg(std::cos(omega * m_time), -std::sin(omega * m_time));

            m_heightField[index] = m_h0[index] * exp_pos + m_h0Conj[index] * exp_neg;

            // Calculate displacements for horizontal movement (choppy waves)
            if (k_length > 0.00001f) {
                std::complex<float> i(0.0f, 1.0f);
                m_displacementX[index] = i * (k.x / k_length) * m_heightField[index];
                m_displacementZ[index] = i * (k.y / k_length) * m_heightField[index];
            }
            else {
                m_displacementX[index] = std::complex<float>(0.0f, 0.0f);
                m_displacementZ[index] = std::complex<float>(0.0f, 0.0f);
            }
        }
    }

    // Perform inverse FFT to get spatial domain
    ifft2D(m_heightField);
    ifft2D(m_displacementX);
    ifft2D(m_displacementZ);

    // Extract real parts and store in vertices array with scaling for visibility
    float heightScale = 2.0f; // Scale up the wave heights for visibility
    for (int i = 0; i < m_gridSize * m_gridSize; i++) {
        m_vertices[i] = m_heightField[i].real() * heightScale;
    }

    calculateNormals();
    updateMeshVertices();
}

// FFT implementation (Cooley-Tukey algorithm) - iterative version
void Water::fft1D(std::vector<std::complex<float>>& data, int stride, int n) {
    if (n <= 1) return;

    // Bit-reversal permutation
    for (int i = 0; i < n; i++) {
        int j = 0;
        int temp = i;
        for (int k = 0; k < static_cast<int>(std::log2(n)); k++) {
            j = (j << 1) | (temp & 1);
            temp >>= 1;
        }
        if (j > i) {
            std::swap(data[i * stride], data[j * stride]);
        }
    }

    // Cooley-Tukey FFT
    for (int s = 1; s <= static_cast<int>(std::log2(n)); s++) {
        int m = 1 << s;
        std::complex<float> wm = std::polar(1.0f, -2.0f * glm::pi<float>() / m);

        for (int k = 0; k < n; k += m) {
            std::complex<float> w(1.0f, 0.0f);

            for (int j = 0; j < m / 2; j++) {
                std::complex<float> t = w * data[(k + j + m / 2) * stride];
                std::complex<float> u = data[(k + j) * stride];

                data[(k + j) * stride] = u + t;
                data[(k + j + m / 2) * stride] = u - t;

                w *= wm;
            }
        }
    }
}

void Water::fft2D(std::vector<std::complex<float>>& data) {
    // Create temporary row buffer
    std::vector<std::complex<float>> row(m_gridSize);

    // FFT on rows
    for (int i = 0; i < m_gridSize; i++) {
        // Copy row to temporary buffer
        for (int j = 0; j < m_gridSize; j++) {
            row[j] = data[i * m_gridSize + j];
        }

        // Perform FFT on row
        fft1D(row, 1, m_gridSize);

        // Copy back
        for (int j = 0; j < m_gridSize; j++) {
            data[i * m_gridSize + j] = row[j];
        }
    }

    // FFT on columns
    std::vector<std::complex<float>> column(m_gridSize);
    for (int j = 0; j < m_gridSize; j++) {
        // Copy column to temporary buffer
        for (int i = 0; i < m_gridSize; i++) {
            column[i] = data[i * m_gridSize + j];
        }

        // Perform FFT on column
        fft1D(column, 1, m_gridSize);

        // Copy back
        for (int i = 0; i < m_gridSize; i++) {
            data[i * m_gridSize + j] = column[i];
        }
    }
}

void Water::ifft2D(std::vector<std::complex<float>>& data) {
    // Conjugate
    for (auto& val : data) {
        val = std::conj(val);
    }

    // Forward FFT
    fft2D(data);

    // Conjugate and normalize
    float norm = 1.0f / (m_gridSize * m_gridSize);
    for (auto& val : data) {
        val = std::conj(val) * norm;
    }
}

// Initialize Gerstner waves for small-scale detail (Muller)
void Water::initializeGerstnerWaves() {
    m_gerstnerWaves.clear();

    // Create several Gerstner waves with different parameters
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());

    int numWaves = 8;
    for (int i = 0; i < numWaves; i++) {
        WaveParameters wave;
        wave.amplitude = 0.05f / (i + 1);
        wave.wavelength = 2.0f + i * 0.5f;
        wave.speed = std::sqrt(m_gravity * 2.0f * glm::pi<float>() / wave.wavelength);

        float angle = angleDist(gen);
        wave.direction = glm::vec2(std::cos(angle), std::sin(angle));

        m_gerstnerWaves.push_back(wave);
    }
}

// Evaluate Gerstner waves at a position
glm::vec3 Water::evaluateGerstnerWave(const glm::vec2& position, float time) {
    glm::vec3 result(position.x, 0.0f, position.y);

    for (const auto& wave : m_gerstnerWaves) {
        float k = 2.0f * glm::pi<float>() / wave.wavelength;
        float phase = k * glm::dot(wave.direction, position) - wave.speed * time;

        float Q = 0.5f; // Steepness parameter

        result.x += Q * wave.amplitude * wave.direction.x * std::cos(phase);
        result.y += wave.amplitude * std::sin(phase);
        result.z += Q * wave.amplitude * wave.direction.y * std::cos(phase);
    }

    return result;
}

// Calculate normals from height field
void Water::calculateNormals() {
    for (int z = 0; z < m_gridSize; z++) {
        for (int x = 0; x < m_gridSize; x++) {
            int index = z * m_gridSize + x;

            // Get neighboring heights
            int left = z * m_gridSize + std::max(0, x - 1);
            int right = z * m_gridSize + std::min(m_gridSize - 1, x + 1);
            int down = std::max(0, z - 1) * m_gridSize + x;
            int up = std::min(m_gridSize - 1, z + 1) * m_gridSize + x;

            float hL = m_vertices[left];
            float hR = m_vertices[right];
            float hD = m_vertices[down];
            float hU = m_vertices[up];

            glm::vec3 normal;
            normal.x = hL - hR;
            normal.y = 2.0f;
            normal.z = hD - hU;

            m_normals[index] = glm::normalize(normal);
        }
    }
}

// Generate the mesh
void Water::generateMesh() {
    cgra::mesh_builder mb;

    float cellSize = m_lengthScale / m_gridSize;

    // Generate vertices
    for (int z = 0; z < m_gridSize; z++) {
        for (int x = 0; x < m_gridSize; x++) {
            int index = z * m_gridSize + x;

            float worldX = (x - m_gridSize / 2.0f) * cellSize;
            float worldZ = (z - m_gridSize / 2.0f) * cellSize;

            // Get wave height from simulation
            float waveHeight = (index < m_vertices.size()) ? m_vertices[index] : 0.0f;

            cgra::mesh_vertex vertex;
            vertex.pos = glm::vec3(worldX, m_seaLevel + waveHeight, worldZ);
            vertex.norm = (index < m_normals.size()) ? m_normals[index] : glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.uv = glm::vec2(static_cast<float>(x) / (m_gridSize - 1),
                static_cast<float>(z) / (m_gridSize - 1));

            mb.push_vertex(vertex);
        }
    }

    // Generate indices
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

// Update mesh vertices with current simulation data
void Water::updateMeshVertices() {
    cgra::mesh_builder mb;

    float cellSize = m_lengthScale / m_gridSize;

    // Generate vertices with updated wave heights
    for (int z = 0; z < m_gridSize; z++) {
        for (int x = 0; x < m_gridSize; x++) {
            int index = z * m_gridSize + x;

            float worldX = (x - m_gridSize / 2.0f) * cellSize;
            float worldZ = (z - m_gridSize / 2.0f) * cellSize;

            // Get wave height from simulation
            float waveHeight = m_vertices[index];

            cgra::mesh_vertex vertex;
            vertex.pos = glm::vec3(worldX, m_seaLevel + waveHeight, worldZ);
            vertex.norm = m_normals[index];
            vertex.uv = glm::vec2(static_cast<float>(x) / (m_gridSize - 1),
                static_cast<float>(z) / (m_gridSize - 1));

            mb.push_vertex(vertex);
        }
    }

    // Generate indices
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
}

void Water::update(float deltaTime) {
    updateHeightField(deltaTime);
}

void Water::draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, const glm::vec3& color) {
    if (!m_meshGenerated) return;

    glm::mat4 modelview = view;

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, glm::value_ptr(modelview));
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(color));

    m_mesh.draw();
}

void Water::reset() {
    m_time = 0.0f;
    initializeHeightField();
    generateMesh();
}

float Water::getHeightAt(float x, float z, float time) const {
    // Convert world position to grid coordinates
    float cellSize = m_lengthScale / m_gridSize;
    int gridX = static_cast<int>((x / cellSize) + m_gridSize / 2.0f);
    int gridZ = static_cast<int>((z / cellSize) + m_gridSize / 2.0f);

    if (gridX < 0 || gridX >= m_gridSize || gridZ < 0 || gridZ >= m_gridSize) {
        return 0.0f;
    }

    int index = gridZ * m_gridSize + gridX;
    return m_vertices[index];
}

void Water::setChoppiness(float choppiness) {
    // Choppiness affects horizontal displacement
    // This would be implemented by scaling m_displacementX and m_displacementZ
}
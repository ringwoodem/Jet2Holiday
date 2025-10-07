// std
#include <cmath>
#include <algorithm>

// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// OpenGL
#include "opengl.hpp"

// project
#include "terrain.hpp"

// Permutation table for Perlin noise (Ken Perlin's original)
const int Terrain::m_permutation[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,
    214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    // Duplicate the array to avoid buffer overflows
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,
    214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

Terrain::Terrain(int width, int height, float scale)
    : m_width(width), m_height(height), m_scale(scale),
    m_amplitude(7.544f), m_frequency(0.158f), m_octaves(7),
    m_persistence(0.453f), m_lacunarity(1.914f), m_islandFalloff(3.0f),
    m_minHeight(0.0f), m_meshGenerated(false) {

    m_heightMap.resize(m_height, std::vector<float>(m_width, 0.0f));
    generateHeightMap();
    generateMesh();
}

float Terrain::fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float Terrain::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float Terrain::grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float Terrain::noise(float x, float y, float z) {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    int A = m_permutation[X] + Y;
    int AA = m_permutation[A] + Z;
    int AB = m_permutation[A + 1] + Z;
    int B = m_permutation[X + 1] + Y;
    int BA = m_permutation[B] + Z;
    int BB = m_permutation[B + 1] + Z;

    return lerp(w, lerp(v, lerp(u, grad(m_permutation[AA], x, y, z),
        grad(m_permutation[BA], x - 1, y, z)),
        lerp(u, grad(m_permutation[AB], x, y - 1, z),
            grad(m_permutation[BB], x - 1, y - 1, z))),
        lerp(v, lerp(u, grad(m_permutation[AA + 1], x, y, z - 1),
            grad(m_permutation[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad(m_permutation[AB + 1], x, y - 1, z - 1),
                grad(m_permutation[BB + 1], x - 1, y - 1, z - 1))));
}

float Terrain::perlinNoise(float x, float y) {
    float value = 0.0f;
    float amplitude = m_amplitude;
    float frequency = m_frequency;

    for (int i = 0; i < m_octaves; i++) {
        value += amplitude * noise(x * frequency, y * frequency, 0.0f);
        amplitude *= m_persistence;
        frequency *= m_lacunarity;
    }

    return value;
}

void Terrain::generateHeightMap() {
    for (int z = 0; z < m_height; z++) {
        for (int x = 0; x < m_width; x++) {
            float worldX = static_cast<float>(x) / static_cast<float>(m_width - 1) * m_scale;
            float worldZ = static_cast<float>(z) / static_cast<float>(m_height - 1) * m_scale;

            float noiseValue = perlinNoise(worldX, worldZ);

            // Calculate radial falloff for island shape
            // Normalize coordinates to [-1, 1] range
            float normX = (static_cast<float>(x) / static_cast<float>(m_width - 1)) * 2.0f - 1.0f;
            float normZ = (static_cast<float>(z) / static_cast<float>(m_height - 1)) * 2.0f - 1.0f;

            // Calculate distance from center
            float distanceFromCenter = std::sqrt(normX * normX + normZ * normZ);

            // Create a smoother island falloff with an inner plateau
            float falloff;
            if (distanceFromCenter < 0.4f) {
                // Inner area - mostly flat with full height
                falloff = 1.0f;
            }
            else {
                // Outer area - smooth falloff to edges
                float normalizedDist = (distanceFromCenter - 0.4f) / 0.6f;
                falloff = std::max(0.0f, 1.0f - normalizedDist);
                falloff = std::pow(falloff, m_islandFalloff);
            }

            // Apply falloff to the noise value
            float finalHeight = noiseValue * falloff;

            // Redistribute terrain heights for more natural islands
            // This creates flatter beaches and steeper mountains
            if (finalHeight > 0.0f) {
                // Apply power curve to create more dramatic peaks
                finalHeight = std::pow(finalHeight / m_amplitude, 1.3f) * m_amplitude;
            }

            // Clamp the minimum height to prevent deep underwater terrain
            // This allows terrain to go high but limits how deep it can go
            m_heightMap[z][x] = std::max(finalHeight, m_minHeight);
        }
    }
}

void Terrain::generateMesh() {
    cgra::mesh_builder mb;

    // Generate vertices
    for (int z = 0; z < m_height; z++) {
        for (int x = 0; x < m_width; x++) {
            float worldX = static_cast<float>(x) / static_cast<float>(m_width - 1) * m_scale - m_scale * 0.5f;
            float worldZ = static_cast<float>(z) / static_cast<float>(m_height - 1) * m_scale - m_scale * 0.5f;
            float height = m_heightMap[z][x];

            cgra::mesh_vertex vertex;
            vertex.pos = glm::vec3(worldX, height, worldZ);

            // Calculate normal (using finite differences)
            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            if (x > 0 && x < m_width - 1 && z > 0 && z < m_height - 1) {
                float hL = m_heightMap[z][x - 1];     // height left
                float hR = m_heightMap[z][x + 1];     // height right
                float hD = m_heightMap[z - 1][x];     // height down
                float hU = m_heightMap[z + 1][x];     // height up

                normal.x = hL - hR;
                normal.z = hD - hU;
                normal.y = 2.0f;
                normal = glm::normalize(normal);
            }

            vertex.norm = normal;
            vertex.uv = glm::vec2(static_cast<float>(x) / (m_width - 1), static_cast<float>(z) / (m_height - 1));

            mb.push_vertex(vertex);
        }
    }

    // Generate indices for triangles
    for (int z = 0; z < m_height - 1; z++) {
        for (int x = 0; x < m_width - 1; x++) {
            int topLeft = z * m_width + x;
            int topRight = z * m_width + x + 1;
            int bottomLeft = (z + 1) * m_width + x;
            int bottomRight = (z + 1) * m_width + x + 1;

            // First triangle
            mb.push_index(topLeft);
            mb.push_index(bottomLeft);
            mb.push_index(topRight);

            // Second triangle
            mb.push_index(topRight);
            mb.push_index(bottomLeft);
            mb.push_index(bottomRight);
        }
    }

    m_mesh = mb.build();
    m_meshGenerated = true;
}

float Terrain::getHeightAt(int x, int z) const {
    if (x < 0 || x >= m_width || z < 0 || z >= m_height) {
        return 0.0f;
    }
    return m_heightMap[z][x];
}

float Terrain::getHeightAtWorld(float x, float z) const {
    // Convert world coordinates to heightmap coordinates
    float normX = (x + m_scale * 0.5f) / m_scale;
    float normZ = (z + m_scale * 0.5f) / m_scale;

    int mapX = static_cast<int>(normX * (m_width - 1));
    int mapZ = static_cast<int>(normZ * (m_height - 1));

    return getHeightAt(mapX, mapZ);
}

void Terrain::draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader, const glm::vec3& color, const glm::vec3& sunPos, const glm::vec3& sunColour,
    GLuint grassDiff, GLuint grassNorm, GLuint grassRough) {
    if (!m_meshGenerated) {
        generateMesh();
    }

    glm::mat4 modelview = view * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.5f, 0.0f));

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, glm::value_ptr(modelview));
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

	// Example values for terrain shader uniforms
	// ideally these would be parameters of the Terrain class or passed into this function
    // - Tyler
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    float sunRadius = 10.0f;
    glm::vec3 terrainAlbedo = color;
    float terrainRoughness = 0.7f;
    float terrainMetallic = 0.0f;
    float terrainWaterDepth = 2.0f;
    float windIntensity = 1.0f;

    glUniform3fv(glGetUniformLocation(shader, "uCameraPos"), 1, glm::value_ptr(cameraPos));
    glUniform3fv(glGetUniformLocation(shader, "uSunPos"), 1, glm::value_ptr(sunPos));
    glUniform3fv(glGetUniformLocation(shader, "uSunColor"), 1, glm::value_ptr(sunColour));
    glUniform1f(glGetUniformLocation(shader, "uSunRadius"), sunRadius);
    glUniform3fv(glGetUniformLocation(shader, "uAlbedo"), 1, glm::value_ptr(terrainAlbedo));
    glUniform1f(glGetUniformLocation(shader, "uRoughness"), terrainRoughness);
    glUniform1f(glGetUniformLocation(shader, "uMetallic"), terrainMetallic);
    glUniform1f(glGetUniformLocation(shader, "uWaterDepth"), terrainWaterDepth);
    glUniform1f(glGetUniformLocation(shader, "uWindIntensity"), windIntensity);


    if (grassDiff != 0) {
        // Bind grass texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassDiff);
        glUniform1i(glGetUniformLocation(shader, "uGrassTexture"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, grassNorm);
        glUniform1i(glGetUniformLocation(shader, "uGrassNormal"), 1);


        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, grassRough);
        glUniform1i(glGetUniformLocation(shader, "uGrassRoughness"), 2);

        // Enable texture mode
        glUniform1i(glGetUniformLocation(shader, "uUseTextures"), 1);

        // Height-based blending parameters
        glUniform1f(glGetUniformLocation(shader, "uGrassHeight"), m_grassHeight);
    }
    else {
        glUniform1i(glGetUniformLocation(shader, "uUseTextures"), 0);
    }

    m_mesh.draw();

    for (int i = 0; i < 3; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Terrain::update() {
    if (!m_meshGenerated) {
        generateHeightMap();
        generateMesh();
    }
}

void Terrain::regenerate() {
    m_meshGenerated = false;
    generateHeightMap();
    generateMesh();
}
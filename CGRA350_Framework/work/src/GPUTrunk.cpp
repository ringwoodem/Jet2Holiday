#include "GPUTrunk.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

GPUTrunk::GPUTrunk()
    : m_segments(10)
    , m_radialSegments(16)
    , m_scale(10.0f)
    , m_baseSize(0.4f)
    , m_ratio(0.015f)
    , m_ratioPower(1.2f)
    , m_flare(0.6f)
{
    compileComputeShader();
    setupBuffers();
}

GPUTrunk::~GPUTrunk() {
    glDeleteProgram(m_computeProgram);
    glDeleteBuffers(1, &m_paramsUBO);
    glDeleteBuffers(1, &m_vertexSSBO);
    glDeleteBuffers(1, &m_indexSSBO);
    glDeleteVertexArrays(1, &m_vao);
}

std::string GPUTrunk::loadShaderSource(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void GPUTrunk::compileComputeShader() {
    // Load compute shader source
    std::string computeSource = loadShaderSource("res/shaders/trunk_gen.comp");
    
    if (computeSource.empty()) {
        // Fallback: embedded shader source
        computeSource = R"(
#version 430 core
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std140, binding = 0) uniform TreeParams {
    float scale;
    float baseSize;
    float ratio;
    float ratioPower;
    float flare;
    int segments;
    int radialSegments;
};

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(std430, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

layout(std430, binding = 2) buffer IndexBuffer {
    uint indices[];
};

float getTrunkRadius(float offset, float length) {
    float unitTaper = offset / length;
    float radius = scale * ratio * pow(1.0 - unitTaper, ratioPower);
    
    if (offset < flare * length) {
        float flareAmount = 1.0 - offset / (flare * length);
        radius += flareAmount * flareAmount * flare * baseSize;
    }
    
    return max(radius, 0.005);
}

void main() {
    uint segmentIdx = gl_GlobalInvocationID.x;
    
    if (segmentIdx > segments) return;
    
    float trunkLength = scale;
    float segmentLength = trunkLength / float(segments);
    float yPos = float(segmentIdx) * segmentLength;
    float radius = getTrunkRadius(yPos, trunkLength);
    
    uint baseVertexIdx = segmentIdx * radialSegments;
    
    for (int i = 0; i < radialSegments; i++) {
        float angle = float(i) / float(radialSegments) * 2.0 * 3.14159265359;
        
        Vertex v;
        v.position = vec3(cos(angle) * radius, yPos, sin(angle) * radius);
        v.normal = normalize(vec3(cos(angle), 0.0, sin(angle)));
        v.uv = vec2(float(i) / float(radialSegments), float(segmentIdx) / float(segments));
        
        vertices[baseVertexIdx + i] = v;
    }
    
    if (segmentIdx < segments) {
        uint nextRingBase = (segmentIdx + 1) * radialSegments;
        uint indexBase = segmentIdx * radialSegments * 6;
        
        for (int i = 0; i < radialSegments; i++) {
            int nextI = (i + 1) % radialSegments;
            uint idx = indexBase + i * 6;
            
            indices[idx + 0] = baseVertexIdx + i;
            indices[idx + 1] = baseVertexIdx + nextI;
            indices[idx + 2] = nextRingBase + i;
            
            indices[idx + 3] = baseVertexIdx + nextI;
            indices[idx + 4] = nextRingBase + nextI;
            indices[idx + 5] = nextRingBase + i;
        }
    }
}
        )";
    }
    
    const char* source = computeSource.c_str();
    
    m_computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(m_computeShader, 1, &source, nullptr);
    glCompileShader(m_computeShader);
    
    // Check compilation
    GLint success;
    glGetShaderiv(m_computeShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(m_computeShader, 1024, nullptr, infoLog);
        std::cerr << "ERROR: Compute shader compilation failed:\n" << infoLog << std::endl;
        return;
    }
    
    m_computeProgram = glCreateProgram();
    glAttachShader(m_computeProgram, m_computeShader);
    glLinkProgram(m_computeProgram);
    
    // Check linking
    glGetProgramiv(m_computeProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_computeProgram, 1024, nullptr, infoLog);
        std::cerr << "ERROR: Compute program linking failed:\n" << infoLog << std::endl;
        return;
    }
    
    std::cout << "Compute shader compiled and linked successfully!" << std::endl;
}

void GPUTrunk::setupBuffers() {
    // Calculate buffer sizes
    uint32_t vertexCount = (m_segments + 1) * m_radialSegments;
    uint32_t indexCount = m_segments * m_radialSegments * 6;
    
    std::cout << "Setting up buffers:" << std::endl;
    std::cout << "  Vertices: " << vertexCount << std::endl;
    std::cout << "  Indices: " << indexCount << std::endl;
    
    // Create Uniform Buffer for parameters
    glGenBuffers(1, &m_paramsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_paramsUBO);
    glBufferData(GL_UNIFORM_BUFFER, 7 * sizeof(float) + 2 * sizeof(int), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_paramsUBO);
    
    // Create SSBO for vertices (vec3 pos, vec3 normal, vec2 uv = 8 floats)
    glGenBuffers(1, &m_vertexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertexCount * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertexSSBO);
    
    // Create SSBO for indices
    glGenBuffers(1, &m_indexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_indexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, indexCount * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_indexSSBO);
    
    // Setup VAO for rendering
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexSSBO);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // UV attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexSSBO);
    
    glBindVertexArray(0);
    
    std::cout << "Buffers setup complete!" << std::endl;
}

void GPUTrunk::generate() {
    std::cout << "\n=== Generating Trunk on GPU ===" << std::endl;
    
    // Upload parameters to uniform buffer
    struct Params {
        float scale;
        float baseSize;
        float ratio;
        float ratioPower;
        float flare;
        int segments;
        int radialSegments;
        int padding; // For alignment
    } params;
    
    params.scale = m_scale;
    params.baseSize = m_baseSize;
    params.ratio = m_ratio;
    params.ratioPower = m_ratioPower;
    params.flare = m_flare;
    params.segments = m_segments;
    params.radialSegments = m_radialSegments;
    
    glBindBuffer(GL_UNIFORM_BUFFER, m_paramsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Params), &params);
    
    std::cout << "Parameters uploaded to GPU" << std::endl;
    std::cout << "  Scale: " << m_scale << std::endl;
    std::cout << "  Segments: " << m_segments << std::endl;
    std::cout << "  Radial Segments: " << m_radialSegments << std::endl;
    
    // Dispatch compute shader
    glUseProgram(m_computeProgram);
    
    // We need (segments + 1) invocations, one per ring
    glDispatchCompute(m_segments + 1, 1, 1);
    
    // Wait for compute to finish
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    
    std::cout << "Compute shader dispatched and completed!" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void GPUTrunk::draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 modelview = view * model;
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, glm::value_ptr(modelview));
    
    // Set color
    glm::vec3 trunkColor(0.4f, 0.25f, 0.15f);
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(trunkColor));
    
    // Draw
    glBindVertexArray(m_vao);
    uint32_t indexCount = m_segments * m_radialSegments * 6;
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void GPUTrunk::setParameters(float scale, float baseSize, float ratio, float ratioPower,
                             float flare, int segments, int radialSegments) {
    bool needsRebuffer = (segments != m_segments || radialSegments != m_radialSegments);
    
    m_scale = scale;
    m_baseSize = baseSize;
    m_ratio = ratio;
    m_ratioPower = ratioPower;
    m_flare = flare;
    m_segments = segments;
    m_radialSegments = radialSegments;
    
    if (needsRebuffer) {
        // Recreate buffers with new sizes
        glDeleteBuffers(1, &m_vertexSSBO);
        glDeleteBuffers(1, &m_indexSSBO);
        glDeleteVertexArrays(1, &m_vao);
        setupBuffers();
    }
}

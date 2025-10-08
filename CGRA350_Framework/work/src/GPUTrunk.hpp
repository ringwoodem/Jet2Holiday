#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

class GPUTrunk {
public:
    GPUTrunk();
    ~GPUTrunk();
    
    // Generate trunk geometry on GPU
    void generate();
    
    // Draw the trunk
    void draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader);
    
    // Update parameters
    void setParameters(float scale, float baseSize, float ratio, float ratioPower,
                      float flare, int segments, int radialSegments);
    
    // Getters
    float getScale() const { return m_scale; }
    float getBaseSize() const { return m_baseSize; }
    float getRatio() const { return m_ratio; }
    float getRatioPower() const { return m_ratioPower; }
    float getFlare() const { return m_flare; }
    int getSegments() const { return m_segments; }
    int getRadialSegments() const { return m_radialSegments; }
    
private:
    void setupBuffers();
    void compileComputeShader();
    std::string loadShaderSource(const std::string& filepath);
    
    // OpenGL objects
    GLuint m_computeShader;
    GLuint m_computeProgram;
    
    GLuint m_paramsUBO;      // Uniform buffer for parameters
    GLuint m_vertexSSBO;     // Shader storage buffer for vertices
    GLuint m_indexSSBO;      // Shader storage buffer for indices
    GLuint m_vao;            // Vertex array object for rendering
    
    // Parameters
    float m_scale;
    float m_baseSize;
    float m_ratio;
    float m_ratioPower;
    float m_flare;
    int m_segments;
    int m_radialSegments;
};

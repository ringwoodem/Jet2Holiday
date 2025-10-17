#include "cloud_renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CloudRenderer::CloudRenderer() : m_cloudShader(0), m_quadVAO(0), m_quadVBO(0) {}

CloudRenderer::~CloudRenderer() {
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void CloudRenderer::init(GLuint shader) {
    m_cloudShader = shader;
    setupQuad();
}

void CloudRenderer::setupQuad() {
    // Fullscreen quad in NDC
    float quadVertices[] = {
        // positions
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
         
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
}

void CloudRenderer::render(const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& cameraPos, float time,
                          const glm::vec3& sunPos, const glm::vec3& sunColor) {
    glUseProgram(m_cloudShader);
    
    // Calculate inverse view-projection
    glm::mat4 viewProj = proj * view;
    glm::mat4 invViewProj = glm::inverse(viewProj);
    
    // Set uniforms
    glUniformMatrix4fv(glGetUniformLocation(m_cloudShader, "uInvViewProj"),
                       1, GL_FALSE, glm::value_ptr(invViewProj));
    glUniform3fv(glGetUniformLocation(m_cloudShader, "uCameraPos"),
                 1, glm::value_ptr(cameraPos));
    glUniform1f(glGetUniformLocation(m_cloudShader, "uTime"), time);
    glUniform3fv(glGetUniformLocation(m_cloudShader, "uSunPos"),
                 1, glm::value_ptr(sunPos));
    glUniform3fv(glGetUniformLocation(m_cloudShader, "uSunColor"),
                 1, glm::value_ptr(sunColor));
    
    // Enable blending for clouds
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth buffer
    
    // Render fullscreen quad
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

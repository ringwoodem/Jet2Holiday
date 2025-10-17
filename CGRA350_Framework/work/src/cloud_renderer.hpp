#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

class CloudRenderer {
private:
    GLuint m_cloudShader;
    GLuint m_quadVAO, m_quadVBO;
    
public:
    CloudRenderer();
    ~CloudRenderer();
    
    void init(GLuint shader);
    void render(const glm::mat4& view, const glm::mat4& proj,
                const glm::vec3& cameraPos, float time,
                const glm::vec3& sunPos, const glm::vec3& sunColor);
    
private:
    void setupQuad();
};

//
//  camerapov.hpp
//  CGRA_PROJECT_base
//
//

#pragma once
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct PovCamera {
    //Pose (head position fixed to 'seat')
    glm::vec3 position{0.0f, 10.40f, 0.0f}; //1.4 10
    float yawDeg = 0.0f;   //left/right
    float pitchDeg = 0.0f;   //up/down

    //lens
    float fovDeg = 60.0f;
    float nearP = 0.1f;
    float farP = 1000.0f;

    //mouse-look
    float sensYaw = 0.12f; //deg/pixel
    float sensPitch = 0.10f; //deg/pixel
    float pitchMin = -80.0f;
    float pitchMax = +80.0f;

    //matrices
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};

    void reset() { yawDeg = 90.0f; pitchDeg = 0.0f; fovDeg = 60.0f; }

    void mouseLook(float dxPx, float dyPx) {
        yawDeg += dxPx * sensYaw;
        pitchDeg -= dyPx * sensPitch; //up looks up
        pitchDeg = std::clamp(pitchDeg, pitchMin, pitchMax);
    }

    void compute(float aspect) {
        const float yaw = glm::radians(yawDeg);
        const float pitch = glm::radians(pitchDeg);
        glm::vec3 fwd{
            std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw)
        };
        view = glm::lookAt(position, position + glm::normalize(fwd), {0,1,0});
        proj = glm::perspective(glm::radians(fovDeg), aspect, nearP, farP);
    }
};

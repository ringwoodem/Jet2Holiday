#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;

// Output world-space position and normal
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;

void main() {
    vUv = aTexCoord;
    vWorldPos = aPosition;      // world-space position
    vNormal = normalize(aNormal); // world-space normal
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
}
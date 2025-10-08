#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
uniform vec3 uColor;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vTexCoord;

void main() {
    vWorldPos = aPosition;
    vNormal = normalize(aNormal);
    vColor = uColor;
    vTexCoord = aTexCoord;
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
}
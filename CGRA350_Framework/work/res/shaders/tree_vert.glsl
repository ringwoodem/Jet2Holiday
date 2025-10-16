#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 colour; // Add color attribute

uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uModelMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColour; // Pass color to the fragment shader
out vec2 vTexCoord;

void main() {
    vWorldPos = vec3(uModelMatrix * vec4(position, 1.0));
    vNormal = mat3(transpose(inverse(uModelMatrix))) * normal;
    vTexCoord = texCoord;
    vColour = colour; // Pass the color attribute

    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(position, 1.0);
}
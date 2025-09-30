#version 330 core

// Uniforms
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;

// Attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// Outputs to fragment shader
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    // Transform normal to view space
    vNormal = normalize((uModelViewMatrix * vec4(aNormal, 0.0)).xyz);
    vTexCoord = aTexCoord;

    // Standard position transform
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
}
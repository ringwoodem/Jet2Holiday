#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform mat4 uLightSpacematrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec3 vSunPos;
out vec3 vSunColor;
out vec4 vFragPosLightSpace;

void main() {
    vUv = aTexCoord;
    vWorldPos = aPosition;  

    
    // Transform normal to world space
    vNormal = normalize(aNormal);
    vSunPos = uSunPos;
    vSunColor = uSunColor;
    
    // Calculate light space position for shadow mapping
    vFragPosLightSpace = uLightSpacematrix * vec4(vWorldPos, 1.0);
    
    // Final position
    gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
}
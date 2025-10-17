#version 330 core

layout (location = 0) in vec2 aPos;

out vec3 fragRayDir;
out vec2 texCoord;

uniform mat4 uInvViewProj;

void main() {
    texCoord = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.999, 1.0); // Almost at far plane
    
    // Compute world space position for ray direction
    vec4 worldPos = uInvViewProj * vec4(aPos, 1.0, 1.0);
    fragRayDir = worldPos.xyz / worldPos.w;
}

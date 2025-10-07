#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec3 vSunPos;
out vec3 vSunColor;

void main() {
	vUv = aTexCoord;
	vWorldPos = aPosition;
    vNormal = normalize(aNormal);
	vSunPos = uSunPos;
    vSunColor = uSunColor;
	gl_Position = uProjectionMatrix * uModelViewMatrix * vec4(aPosition, 1.0);
}
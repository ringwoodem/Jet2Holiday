#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uCameraPos;
uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform vec3 uAlbedo;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Simple diffuse lighting
    float diffuse = max(dot(N, L), 0.0);
    vec3 color = uAlbedo * uSunColor * diffuse;

    // For debugging: output normals as color
    // vec3 color = N * 0.5 + 0.5;

    FragColor = vec4(color, 1.0);
}
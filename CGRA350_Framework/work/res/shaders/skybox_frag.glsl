#version 330 core
in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

void main()
{
    vec3 envColor = texture(skybox, TexCoords).rgb;

    // Simple sun glow: increase brightness near sun direction
    float sunGlow = pow(max(dot(normalize(TexCoords), normalize(uSunPos)), 0.0), 128.0);
    vec3 sunEffect = uSunColor * sunGlow;

    FragColor = vec4(envColor + sunEffect, 1.0);
}
#version 330 core
in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube skybox;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

void main()
{
    vec3 envColor = texture(skybox, TexCoords).rgb;
    
    // Check if sun is above horizon
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
    
    // Darken skybox at night
    envColor *= mix(0.01, 1.0, dayFactor);
    
    // Simple sun glow: increase brightness near sun direction
    // Only show sun when above horizon
    float sunGlow = pow(max(dot(normalize(TexCoords), normalize(uSunPos)), 0.0), 128.0);
    vec3 sunEffect = dayFactor * uSunColor * sunGlow;
    
    FragColor = vec4(envColor + sunEffect, 1.0);
}
#version 330 core
in vec3 TexCoords;
out vec4 FragColor;

uniform samplerCube uDayCubemap;
uniform samplerCube uNightCubemap;
uniform float uDayFactor;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

void main()
{
    // Sample both cubemaps
    vec3 dayColor = texture(uDayCubemap, TexCoords).rgb;
    vec3 nightColor = texture(uNightCubemap, TexCoords).rgb;

    // Blend between day and night cubemaps
    vec3 envColor = mix(nightColor, dayColor, uDayFactor);

    // Sun glow effect (only when above horizon)
    float sunHeight = uSunPos.y;
    float dayGlow = smoothstep(-50.0, 50.0, sunHeight);
    float sunGlow = pow(max(dot(normalize(TexCoords), normalize(uSunPos)), 0.0), 128.0);
    vec3 sunEffect = dayGlow * uSunColor * sunGlow;

    FragColor = vec4(envColor + sunEffect, 1.0);
}
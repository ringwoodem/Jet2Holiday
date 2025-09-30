#version 330 core

in vec3 vNormal;
in vec2 vTexCoord;
out vec4 FragColor;

// Textures
uniform sampler2D cloudColor;
uniform sampler2D cloudDepth;

// Lighting uniforms
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float sssRadius; // softness

void main() {
    vec3 color = texture(cloudColor, vTexCoord).rgb;
    float depth = texture(cloudDepth, vTexCoord).r;
    vec3 sss = vec3(0.0);

    // Screen-space diffusion (soft scattering)
    for (int i = -2; i <= 2; ++i)
    for (int j = -2; j <= 2; ++j) {
        vec2 offset = vec2(i, j) * sssRadius;
        float neighborDepth = texture(cloudDepth, vTexCoord + offset).r;
        float weight = exp(-abs(depth - neighborDepth) * 10.0);
        sss += texture(cloudColor, vTexCoord + offset).rgb * weight;
    }
    sss /= 25.0;
    vec3 softColor = mix(color, sss, 0.5);

    // Directional lighting (Lambertian)
    float diffuse = max(dot(normalize(vNormal), normalize(uSunDir)), 0.0);
    vec3 litColor = softColor * uSunColor * diffuse;

    // Optional: add ambient term for cloud softness
    vec3 ambient = softColor * 0.2;

    FragColor = vec4(litColor + ambient, 1.0);
}
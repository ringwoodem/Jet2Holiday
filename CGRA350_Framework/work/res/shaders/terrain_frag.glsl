#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in float vHeight;

uniform vec3 uCameraPos;
uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform float uSunRadius;
uniform vec3 uAlbedo;
uniform float uRoughness;
uniform float uMetallic;
uniform float uWaterDepth;
uniform float uWindIntensity;

uniform sampler2D uGrassTexture;
uniform sampler2D uRockTexture;
uniform int uUseTextures;
uniform float uGrassHeight;
uniform float uRockHeight;
uniform float uBlendRange;


out vec4 FragColor;

// --- Schlick Fresnel ---
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// --- GGX Normal Distribution Function ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (3.14159265 * denom * denom);
}

// --- Geometry Smith ---
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// --- Cook-Torrance BRDF ---
vec3 cookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 F0, float roughness) {
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float denom = 4.0 * NdotL * NdotV + 0.001;
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    return (NDF * G * F) / denom;
}

// --- Subsurface scattering (simple) ---
vec3 computeSSS(float depth, vec3 color) {
    float scatter = exp(-depth * 0.1);
    return color * scatter;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunPos - vWorldPos);

    vec3 albedo = uAlbedo;

     if (uUseTextures == 1) {
        // Tile the texture (adjust multiplier to control texture scale)
        vec2 tiledUV = vTexCoord * 10.0;
        
        // Sample grass texture only
        albedo = texture(uGrassTexture, tiledUV).rgb;
    }

    // Shadow (placeholder, not true PCSS)
    float shadow = 1.0; // No shadow map

    float NdotL = max(dot(N, L), 0.0);

    // BRDF
    vec3 F0 = mix(vec3(0.04), uAlbedo, uMetallic);
    vec3 brdf = cookTorranceBRDF(N, V, L, F0, uRoughness);

    // Subsurface scattering (for water)
    vec3 sss = computeSSS(uWaterDepth, uAlbedo);

    // Final color composition
    vec3 ambient = 0.3 * albedo;  // Ambient term
    vec3 diffuse = NdotL * albedo * uSunColor;  // Diffuse term
    vec3 specular = brdf * uSunColor;  // Specular term
    
    vec3 finalColor = ambient + shadow * (diffuse + specular) + (sss * 0.2);
    
    FragColor = vec4(finalColor, 1.0);
}
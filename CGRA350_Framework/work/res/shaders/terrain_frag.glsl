#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;
in float vHeight;
in vec4 vFragPosLightSpace;
uniform vec3 uCameraPos;
uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform float uSunRadius;
uniform vec3 uAlbedo;
uniform float uMetallic;
uniform float uWaterDepth;
uniform float uWindIntensity;
uniform sampler2D uGrassTexture;
uniform sampler2D uRockTexture;
uniform float uGrassHeight;
uniform float uRockHeight;
uniform float uBlendRange;
uniform sampler2D uGrassNormal;
uniform sampler2D uGrassRoughness;
uniform sampler2D uShadowMap;

// PCSS parameters
uniform float uLightSize;        // Light source size (larger = softer shadows)
uniform float uNearPlane;        // Shadow map near plane
uniform int uBlockerSearchSamples; // Samples for blocker search 
uniform int uPCFSamples;         // Samples for PCF filtering

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

// --- Poisson disk sampling pattern ---
const vec2 poissonDisk[64] = vec2[](
    vec2(-0.613392, 0.617481),
    vec2(0.170019, -0.040254),
    vec2(-0.299417, 0.791925),
    vec2(0.645680, 0.493210),
    vec2(-0.651784, 0.717887),
    vec2(0.421003, 0.027070),
    vec2(-0.817194, -0.271096),
    vec2(-0.705374, -0.668203),
    vec2(0.977050, -0.108615),
    vec2(0.063326, 0.142369),
    vec2(0.203528, 0.214331),
    vec2(-0.667531, 0.326090),
    vec2(-0.098422, -0.295755),
    vec2(-0.885922, 0.215369),
    vec2(0.566637, 0.605213),
    vec2(0.039766, -0.396100),
    vec2(0.751946, 0.453352),
    vec2(0.078707, -0.715323),
    vec2(-0.075838, -0.529344),
    vec2(0.724479, -0.580798),
    vec2(0.222999, -0.215125),
    vec2(-0.467574, -0.405438),
    vec2(-0.248268, -0.814753),
    vec2(0.354411, -0.887570),
    vec2(0.175817, 0.382366),
    vec2(0.487472, -0.063082),
    vec2(-0.084078, 0.898312),
    vec2(0.488876, -0.783441),
    vec2(0.470016, 0.217933),
    vec2(-0.696890, -0.549791),
    vec2(-0.149693, 0.605762),
    vec2(0.034211, 0.979980),
    vec2(0.503098, -0.308878),
    vec2(-0.016205, -0.872921),
    vec2(0.385784, -0.393902),
    vec2(-0.146886, -0.859249),
    vec2(0.643361, 0.164098),
    vec2(0.634388, -0.049471),
    vec2(-0.688894, 0.007843),
    vec2(0.464034, -0.188818),
    vec2(-0.440840, 0.137486),
    vec2(0.364483, 0.511704),
    vec2(0.034028, 0.325968),
    vec2(0.099094, -0.308023),
    vec2(0.693960, -0.366253),
    vec2(0.678884, -0.204688),
    vec2(0.001801, 0.780328),
    vec2(0.145177, -0.898984),
    vec2(0.062655, -0.611866),
    vec2(0.315226, -0.604297),
    vec2(-0.780145, 0.486251),
    vec2(-0.371868, 0.882138),
    vec2(0.200476, 0.494430),
    vec2(-0.494552, -0.711051),
    vec2(0.612476, 0.705252),
    vec2(-0.578845, -0.768792),
    vec2(-0.772454, -0.090976),
    vec2(0.504440, 0.372295),
    vec2(0.155736, 0.065157),
    vec2(0.391522, 0.849605),
    vec2(-0.620106, -0.328104),
    vec2(0.789239, -0.419965),
    vec2(-0.545396, 0.538133),
    vec2(-0.178564, -0.596057)
);

// Step 1: Find average blocker depth
float findBlockerDistance(vec2 uv, float zReceiver, float searchWidth) {
    float blockerSum = 0.0;
    int blockerCount = 0;
    
    for (int i = 0; i < uBlockerSearchSamples; i++) {
        vec2 offset = poissonDisk[i] * searchWidth;
        float shadowMapDepth = texture(uShadowMap, uv + offset).r;
        
        if (shadowMapDepth < zReceiver) {
            blockerSum += shadowMapDepth;
            blockerCount++;
        }
    }
    
    if (blockerCount == 0) {
        return -1.0; // No blockers found
    }
    
    return blockerSum / float(blockerCount);
}

// Step 2: Estimate penumbra size
float estimatePenumbraSize(float zReceiver, float zBlocker) {
    return (zReceiver - zBlocker) / zBlocker;
}

// Step 3: PCF with variable filter size
float PCF(vec2 uv, float zReceiver, float filterRadius, vec3 normal, vec3 lightDir) {
    float sum = 0.0;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    for (int i = 0; i < uPCFSamples; i++) {
        vec2 offset = poissonDisk[i % 64] * filterRadius;
        float shadowMapDepth = texture(uShadowMap, uv + offset).r;
        sum += (zReceiver - bias > shadowMapDepth) ? 1.0 : 0.0;
    }
    
    return sum / float(uPCFSamples);
}

// PCSS Shadow calculation
float calculatePCSS(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    // Early exit if outside shadow map
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }
    
    float zReceiver = projCoords.z;
    
    // Step 1: Blocker search
    float searchWidth = uLightSize * (zReceiver - uNearPlane) / zReceiver;
    float avgBlockerDepth = findBlockerDistance(projCoords.xy, zReceiver, searchWidth);
    
    // No blockers means no shadow
    if (avgBlockerDepth < 0.0) {
        return 0.0;
    }
    
    // Step 2: Penumbra estimation
    float penumbraWidth = estimatePenumbraSize(zReceiver, avgBlockerDepth);
    float filterRadius = penumbraWidth * uLightSize * uNearPlane / zReceiver;
    
    // Step 3: PCF filtering
    return PCF(projCoords.xy, zReceiver, filterRadius, normal, lightDir);
}

void main() {
    vec2 tiledUV = vUv * 10.0;
    vec3 grassColor = texture(uGrassTexture, tiledUV).rgb;
    vec3 albedo = grassColor;
    
    vec3 N = normalize(vNormal);
    vec3 grassNormal = texture(uGrassNormal, tiledUV).rgb * 2.0 - 1.0;
    N = normalize(mix(N, grassNormal, 0.5));
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunPos - vWorldPos);
    float grassRoughness = texture(uGrassRoughness, tiledUV).r;
    float roughness = grassRoughness;
    
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
    
    // PCSS Shadow calculation
    float shadow = calculatePCSS(vFragPosLightSpace, N, L);
    
    float NdotL = max(dot(N, L), 0.0);
    
    // BRDF
    vec3 F0 = mix(vec3(0.04), uAlbedo, uMetallic);
    vec3 brdf = cookTorranceBRDF(N, V, L, F0, roughness);
    
    // Final color composition
    vec3 ambient = mix(
        vec3(0.01) * albedo, 
        vec3(0.3) * albedo, 
        dayFactor
    );
    vec3 diffuse = dayFactor * NdotL * albedo * uSunColor;
    vec3 specular = dayFactor * brdf * uSunColor;
    
    vec3 finalColor = ambient + (1.0 - shadow) * (diffuse + specular);
    
    FragColor = vec4(finalColor, 1.0);
}
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColour;
in vec2 vTexCoord;
in vec4 vFragPosLightSpace;

uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform vec3 uCameraPos;
uniform sampler2D uTrunkDiffuse;
uniform sampler2D uTrunkNormal;
uniform sampler2D uTrunkRoughness;
uniform sampler2D uShadowMap;

// PCSS uniforms
uniform float uLightSize;
uniform float uNearPlane;
uniform int uBlockerSearchSamples;
uniform int uPCFSamples;

out vec4 FragColor;

// Poisson disk for PCSS sampling
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
        return -1.0;
    }
    
    return blockerSum / float(blockerCount);
}

float estimatePenumbraSize(float zReceiver, float zBlocker) {
    return (zReceiver - zBlocker) / zBlocker;
}

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

float calculatePCSS(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float zReceiver = projCoords.z;

    float searchWidth = uLightSize * (zReceiver - uNearPlane) / zReceiver;
    float avgBlockerDepth = findBlockerDistance(projCoords.xy, zReceiver, searchWidth);

    if (avgBlockerDepth < 0.0) {
        return 0.0;
    }

    float penumbraWidth = estimatePenumbraSize(zReceiver, avgBlockerDepth);
    float filterRadius = penumbraWidth * uLightSize * uNearPlane / zReceiver;

    return PCF(projCoords.xy, zReceiver, filterRadius, normal, lightDir);
}

void main() {
    // Determine if this is a leaf or trunk/branch based on vertex color
    bool isLeaf = (vColour.g > 0.5 && vColour.r < 0.6);
    
    vec3 albedo;
    if (isLeaf) {
        // Leaf color - rich green
        albedo = vColour;
    } else {
        // Sample bark texture for trunk and branches
        vec3 trunkColor = texture(uTrunkDiffuse, vTexCoord).rgb;
        
        // If texture is black/empty, use a default brown color
        if (length(trunkColor) < 0.1) {
            trunkColor = vec3(0.4, 0.25, 0.15);
        }
        albedo = trunkColor;
    }
    
    // Use geometry normal
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunPos - vWorldPos);
    vec3 V = normalize(uCameraPos - vWorldPos);
    
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
    
    // Calculate PCSS shadow
    float shadow = calculatePCSS(vFragPosLightSpace, N, L);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 ambient = mix(
        vec3(0.01) * albedo,
        vec3(0.3) * albedo,
        dayFactor
    );
    
    vec3 diffuse = dayFactor * NdotL * albedo * uSunColor;
    
    // Simple specular
    vec3 H = normalize(V + L);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = dayFactor * spec * uSunColor * 0.3;
    
    vec3 finalColor = ambient + (1.0 - shadow) * (diffuse + specular);
    
    // Ensure we never output pure black
    finalColor = max(finalColor, vec3(0.01));
    
    FragColor = vec4(finalColor, 1.0);
}
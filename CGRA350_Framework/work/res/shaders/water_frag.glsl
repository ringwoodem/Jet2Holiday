#version 330 core

precision highp float;

uniform float uOpacity;

uniform vec3 uTroughColor;
uniform vec3 uSurfaceColor;
uniform vec3 uPeakColor;

uniform float uPeakThreshold;
uniform float uPeakTransition;
uniform float uTroughThreshold;
uniform float uTroughTransition;

uniform vec3 uSunPos;
uniform vec3 uSunColor;

uniform float uFresnelScale;
uniform float uFresnelPower;

uniform sampler2D uShadowMap;
uniform float uLightSize;
uniform float uNearPlane;
uniform int uBlockerSearchSamples;
uniform int uPCFSamples;

in vec3 vNormal;
in vec3 vWorldPosition;
in vec4 vFragPosLightSpace; 

uniform vec3 cameraPosition;
uniform samplerCube uEnvironmentMap;

out vec4 fragColor;

// --- Poisson disk for PCSS sampling ---
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
        return -1.0;
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
    float bias = max(0.02 * (1.0 - dot(normal, lightDir)), 0.005);
    
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
    
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }
    
    float zReceiver = projCoords.z;
    
    // Step 1: Blocker search
    float searchWidth = uLightSize * (zReceiver - uNearPlane) / zReceiver;
    float avgBlockerDepth = findBlockerDistance(projCoords.xy, zReceiver, searchWidth);
    
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
  vec3 viewDir = normalize(cameraPosition - vWorldPosition);
  vec3 reflectedDir = reflect(-viewDir, normalize(vNormal));
  vec3 sunDir = normalize(uSunPos - vWorldPosition);
  float diffuse = max(dot(normalize(vNormal), sunDir), 0.0);
  
  float sunHeight = uSunPos.y;
  float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
  
  vec4 reflectionColor = texture(uEnvironmentMap, reflectedDir);
  reflectionColor.rgb *= mix(0.001, 1.0, dayFactor);
  
  float fresnel = uFresnelScale * pow(1.0 - clamp(dot(viewDir, vNormal), 0.0, 1.0), uFresnelPower);
  fresnel *= mix(0.1, 1.0, dayFactor);
  
  float elevation = vWorldPosition.y;
  float peakFactor = smoothstep(uPeakThreshold - uPeakTransition, uPeakThreshold + uPeakTransition, elevation);
  float troughFactor = smoothstep(uTroughThreshold - uTroughTransition, uTroughThreshold + uTroughTransition, elevation);
  
  vec3 mixedColor1 = mix(uTroughColor, uSurfaceColor, troughFactor);
  vec3 mixedColor2 = mix(mixedColor1, uPeakColor, peakFactor);
  vec3 finalColor = mix(mixedColor2, reflectionColor.rgb, fresnel);
  
  // NEW: Calculate shadow
  float shadow = calculatePCSS(vFragPosLightSpace, normalize(vNormal), sunDir);
  
  vec3 ambient = mix(
    vec3(0.005) * finalColor,
    vec3(0.2) * finalColor,
    dayFactor
  );
  
  // Apply shadow to direct lighting only
  vec3 directLight = dayFactor * 0.8 * diffuse * finalColor * uSunColor * (1.0 - shadow);
  
  vec3 litColor = ambient + directLight;
  fragColor = vec4(litColor, uOpacity);
}
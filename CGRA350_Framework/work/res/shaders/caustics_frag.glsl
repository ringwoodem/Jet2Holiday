#version 330 core
uniform float uTime;
uniform sampler2D uTexture;
uniform vec3 uCausticsColor;
uniform float uCausticsIntensity;
uniform float uCausticsOffset;
uniform float uCausticsScale;
uniform float uCausticsSpeed;
uniform float uCausticsThickness;

uniform sampler2D uShadowMap;
uniform float uLightSize;
uniform float uNearPlane;
uniform int uBlockerSearchSamples;
uniform int uPCFSamples;

in vec2 vUv;
in vec3 vSunPos;
in vec3 vSunColor;
in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vFragPosLightSpace;

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

//	Simplex 3D Noise 
vec4 permute(vec4 x) {
  return mod(((x * 34.0) + 1.0) * x, 289.0);
}

vec4 taylorInvSqrt(vec4 r) {
  return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
  const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
  const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
  vec3 i = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);
  vec3 x1 = x0 - i1 + 1.0 * C.xxx;
  vec3 x2 = x0 - i2 + 2.0 * C.xxx;
  vec3 x3 = x0 - 1.0 + 3.0 * C.xxx;
  i = mod(i, 289.0);
  vec4 p = permute(permute(permute(
    i.z + vec4(0.0, i1.z, i2.z, 1.0))
    + i.y + vec4(0.0, i1.y, i2.y, 1.0))
    + i.x + vec4(0.0, i1.x, i2.x, 1.0));
  float n_ = 1.0 / 7.0;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_);
  vec4 x = x_ * ns.x + ns.yyyy;
  vec4 y = y_ * ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0) * 2.0 + 1.0;
  vec4 s1 = floor(b1) * 2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));
  vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
  vec3 p0 = vec3(a0.xy, h.x);
  vec3 p1 = vec3(a0.zw, h.y);
  vec3 p2 = vec3(a1.xy, h.z);
  vec3 p3 = vec3(a1.zw, h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
  m = m * m;
  return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

// --- PCSS Functions ---
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
    vec4 texColor = texture(uTexture, vUv);
    vec3 normal = normalize(vNormal);
    vec3 sunDir = normalize(vSunPos - vWorldPos);
    float sunIntensity = max(dot(normal, sunDir), 0.0);
    float sunHeight = vSunPos.y;
    float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
    
    // Calculate shadow
    float shadow = calculatePCSS(vFragPosLightSpace, normal, sunDir);
    
    vec3 ambient = mix(
        vec3(0.01) * texColor.rgb,
        vec3(0.3) * texColor.rgb,
        dayFactor
    );
    
    // Apply shadow to diffuse lighting
    float shadowFactor = 1.0 - shadow;  // Now 0 = shadow, 1 = lit
    vec3 diffuse = dayFactor * sunIntensity * texColor.rgb * vSunColor * shadowFactor;
    
    // Only calculate caustics when sun is above horizon and not in shadow
    float caustics = 0.0;
    if (dayFactor > 0.01) {
        caustics += uCausticsIntensity * (uCausticsOffset - abs(snoise(vec3(vUv.xy * uCausticsScale, uTime * uCausticsSpeed))));
        caustics += uCausticsIntensity * (uCausticsOffset - abs(snoise(vec3(vUv.yx * uCausticsScale, -uTime * uCausticsSpeed))));
        caustics = smoothstep(0.5 - uCausticsThickness, 0.5 + uCausticsThickness, caustics);
        
        // Scale caustics intensity based on sun height
        float sunHeightFactor = smoothstep(0.1, 1.0, sunHeight); // Scale from 0 to 50
        caustics *= sunHeightFactor * sunIntensity * dayFactor * (1.0 - shadow * 0.8);  // Reduce caustics in shadow but don't eliminate completely
    }
    
    vec3 finalColor = ambient + diffuse + caustics * uCausticsColor * vSunColor;
    fragColor = vec4(finalColor, 1.0);
}
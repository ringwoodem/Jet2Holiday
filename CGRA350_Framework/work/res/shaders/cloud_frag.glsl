#version 330 core

in vec3 fragRayDir;
in vec2 texCoord;

out vec4 fragColor;

uniform vec3 uCameraPos;
uniform float uTime;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

// ============================================
// IMPROVED HASH FUNCTIONS
// ============================================

vec3 hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// ============================================
// PERLIN NOISE
// ============================================

float perlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    
    // Quintic interpolation (smoother than cubic)
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    
    return mix(mix(mix(dot(hash3(i + vec3(0,0,0)), f - vec3(0,0,0)),
                       dot(hash3(i + vec3(1,0,0)), f - vec3(1,0,0)), u.x),
                   mix(dot(hash3(i + vec3(0,1,0)), f - vec3(0,1,0)),
                       dot(hash3(i + vec3(1,1,0)), f - vec3(1,1,0)), u.x), u.y),
               mix(mix(dot(hash3(i + vec3(0,0,1)), f - vec3(0,0,1)),
                       dot(hash3(i + vec3(1,0,1)), f - vec3(1,0,1)), u.x),
                   mix(dot(hash3(i + vec3(0,1,1)), f - vec3(0,1,1)),
                       dot(hash3(i + vec3(1,1,1)), f - vec3(1,1,1)), u.x), u.y), u.z);
}

// Lower octave FBM for smoother results
float perlinFbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for(int i = 0; i < octaves; i++) {
        value += amplitude * perlinNoise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// ============================================
// WORLEY NOISE
// ============================================

vec3 worleyHash(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.xxy + p.yxx) * p.zyx);
}

vec3 worleyNoise(vec3 p) {
    vec3 id = floor(p);
    vec3 f = fract(p);
    
    float minDist1 = 1.0;
    float minDist2 = 1.0;
    float minDist3 = 1.0;
    
    for(int k = -1; k <= 1; k++) {
        for(int j = -1; j <= 1; j++) {
            for(int i = -1; i <= 1; i++) {
                vec3 neighbor = vec3(float(i), float(j), float(k));
                vec3 point = worleyHash(id + neighbor);
                point = 0.5 + 0.5 * sin(point * 6.2831);
                
                vec3 diff = neighbor + point - f;
                float dist = length(diff);
                
                if(dist < minDist1) {
                    minDist3 = minDist2;
                    minDist2 = minDist1;
                    minDist1 = dist;
                } else if(dist < minDist2) {
                    minDist3 = minDist2;
                    minDist2 = dist;
                } else if(dist < minDist3) {
                    minDist3 = dist;
                }
            }
        }
    }
    
    return vec3(minDist1, minDist2, minDist3);
}

// ============================================
// IMPROVED CLOUD DENSITY
// ============================================

float cloudDensity(vec3 pos, float time) {
    // Very slow wind drift
    vec3 windOffset = vec3(time * 0.02, 0.0, time * 0.01);
    vec3 p = pos + windOffset;
    
    // === STEP 1: BASE SHAPE (Large fluffy clouds) ===
    // Use VERY LOW frequency Perlin for big puffy shapes
    float baseShape = perlinFbm(p * 0.08, 3); // Much lower frequency!
    baseShape = baseShape * 0.5 + 0.5; // Remap to 0-1
    
    // === STEP 2: CLOUD COVERAGE (Where clouds exist) ===
    // Another low-frequency Perlin to define cloud coverage
    float coverage = perlinNoise(p * 0.05) * 0.5 + 0.5;
    coverage = smoothstep(0.3, 0.7, coverage); // Creates distinct cloud regions
    
    baseShape *= coverage;
    
    // === STEP 3: WORLEY FOR EDGES (Wispy boundaries) ===
    // Low-frequency Worley for cloud edges
    vec3 worley1 = worleyNoise(p * 0.3);
    float edges = (worley1.y - worley1.x); // F2-F1 creates cell boundaries
    edges = smoothstep(0.0, 0.3, edges); // Soften the edges
    
    // Blend base with edges
    float density = baseShape * (1.0 - edges * 0.4);
    
    // === STEP 4: MEDIUM DETAIL ===
    // Medium frequency for some variation
    float mediumDetail = perlinNoise(p * 0.6) * 0.5 + 0.5;
    density = mix(density, density * mediumDetail, 0.3);
    
    // === STEP 5: FINE DETAIL (Subtle texture) ===
    // High frequency but LOW IMPACT for subtle texture
    vec3 worley2 = worleyNoise(p * 2.0);
    float fineDetail = worley2.x;
    density -= fineDetail * 0.15; // Very subtle erosion
    
    // === HEIGHT GRADIENT ===
    float cloudBottom = 25.0;
    float cloudTop = 45.0;
    float heightFactor = smoothstep(cloudBottom - 3.0, cloudBottom + 3.0, pos.y) *
                        (1.0 - smoothstep(cloudTop - 8.0, cloudTop + 2.0, pos.y));
    density *= heightFactor;
    
    // === FINAL THRESHOLD ===
    // Sharper threshold for more defined clouds
    density = smoothstep(0.4, 0.65, density);
    
    return clamp(density, 0.0, 1.0);
}

// ============================================
// IMPROVED LIGHTING
// ============================================

float cloudLighting(vec3 pos, vec3 sunDir, float time) {
    // Take fewer, larger samples for lighting
    float shadowSample1 = cloudDensity(pos + sunDir * 2.0, time);
    float shadowSample2 = cloudDensity(pos + sunDir * 4.0, time);
    
    // Beer's law with less aggressive shadowing
    float shadow = exp(-shadowSample1 * 1.5 - shadowSample2 * 0.8);
    
    // Add powder effect (bright edges on lit side)
    float powder = 1.0 - exp(-shadowSample1 * 2.0);
    shadow = mix(shadow, 1.0, powder * 0.3);
    
    return clamp(shadow, 0.1, 1.0); // Never fully black
}

// ============================================
// OPTIMIZED RAY MARCHING
// ============================================

vec4 renderClouds(vec3 rayOrigin, vec3 rayDir, float time) {
    if (rayDir.y < 0.05) {
        return vec4(0.0);
    }
    
    float cloudBottom = 25.0;
    float cloudTop = 45.0;
    
    float tBottom = (cloudBottom - rayOrigin.y) / rayDir.y;
    float tTop = (cloudTop - rayOrigin.y) / rayDir.y;
    
    if (tBottom < 0.0 && tTop < 0.0) {
        return vec4(0.0);
    }
    
    float tStart = max(0.0, min(tBottom, tTop));
    float tEnd = max(tBottom, tTop);
    
    // Fewer steps but larger step size for performance
    const int MAX_STEPS = 48;
    float stepSize = (tEnd - tStart) / float(MAX_STEPS);
    
    vec3 pos = rayOrigin + rayDir * tStart;
    float transmittance = 1.0;
    vec3 lightAccum = vec3(0.0);
    
    vec3 sunDir = normalize(uSunPos);
    
    // Richer ambient color
    vec3 skyAmbient = vec3(0.4, 0.5, 0.7) * 0.4;
    vec3 sunAmbient = uSunColor * 0.2;
    vec3 ambient = skyAmbient + sunAmbient;
    
    for(int i = 0; i < MAX_STEPS; i++) {
        if(transmittance < 0.01) break;
        
        float density = cloudDensity(pos, time);
        
        if(density > 0.02) {
            // Lighting calculation
            float sunVisibility = cloudLighting(pos, sunDir, time);
            
            // Henyey-Greenstein phase function approximation
            float cosAngle = dot(rayDir, sunDir);
            float g = 0.3; // Forward scattering
            float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5));
            phase = mix(0.7, 1.0, phase * 2.0); // Simplified
            
            // Combine lighting
            vec3 sunLight = uSunColor * sunVisibility * phase;
            vec3 lighting = sunLight + ambient;
            
            // Energy conserving integration
            float dt = density * stepSize * 1.5;
            float sampleTransmittance = exp(-dt);
            
            lightAccum += transmittance * (1.0 - sampleTransmittance) * lighting;
            transmittance *= sampleTransmittance;
        }
        
        pos += rayDir * stepSize;
    }
    
    float alpha = 1.0 - transmittance;
    
    // Silver lining (bright edges toward sun)
    float edgeFactor = pow(1.0 - transmittance, 3.0);
    float sunDot = max(0.0, dot(rayDir, sunDir));
    float silverLining = edgeFactor * sunDot * 0.4;
    lightAccum += uSunColor * silverLining;
    
    return vec4(lightAccum, alpha);
}

// ============================================
// MAIN
// ============================================

void main() {
    vec3 rayDir = normalize(fragRayDir - uCameraPos);
    
    if (rayDir.y < -0.1) {
        discard;
    }
    
    vec4 clouds = renderClouds(uCameraPos, rayDir, uTime);
    
    // Horizon fade
    float horizonFade = smoothstep(0.0, 0.25, rayDir.y);
    clouds.a *= horizonFade;
    
    // Atmospheric scattering (distant clouds fade to sky color)
    float distance = length(fragRayDir - uCameraPos);
    float atmosphericFade = exp(-distance * 0.0005);
    vec3 skyColor = vec3(0.5, 0.65, 0.85);
    clouds.rgb = mix(skyColor, clouds.rgb, atmosphericFade);
    
    fragColor = clouds;
}

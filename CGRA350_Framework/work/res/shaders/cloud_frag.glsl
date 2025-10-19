#version 330 core

in vec3 fragRayDir;
in vec2 texCoord;

out vec4 fragColor;

uniform vec3 uCameraPos;
uniform float uTime;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

// Hash Function
vec3 hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

//  Perlin Noise

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

// Worley Noise

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

// Cloud Density

float cloudDensity(vec3 pos, float time) {
    vec3 windOffset = vec3(time * 0.01, 0.0, time * 0.005);
    vec3 p = pos + windOffset;
    
    // BASE SHAPE - Lower frequency for bigger clouds
    float baseShape = perlinFbm(p * 0.05, 4);
    baseShape = baseShape * 0.5 + 0.5;
    
    // COVERAGE - More coverage
    float coverage = perlinNoise(p * 0.03) * 0.5 + 0.5;
    coverage = smoothstep(0.2, 0.8, coverage);
    
    baseShape *= coverage;
    
    // WORLEY - Less aggressive
    vec3 worley1 = worleyNoise(p * 0.2);
    float edges = (worley1.y - worley1.x);
    edges = smoothstep(0.0, 0.5, edges);
    
    float density = baseShape * (1.0 - edges * 0.2);
    
    // MEDIUM DETAIL - Subtle
    float mediumDetail = perlinNoise(p * 0.5) * 0.5 + 0.5;
    density = mix(density, density * mediumDetail, 0.2);
    
    // FINE DETAIL
    vec3 worley2 = worleyNoise(p * 1.5);
    float fineDetail = 1.0 - worley2.x * 0.3;
    density *= mix(1.0, fineDetail, 0.15);
    
    // HEIGHT GRADIENT - Taller clouds
    float cloudBase = 25.0;
    float cloudTop = 50.0;
    float heightFactor = smoothstep(cloudBase - 2.0, cloudBase + 5.0, pos.y) *
                        (1.0 - smoothstep(cloudTop - 10.0, cloudTop + 3.0, pos.y));
    density *= heightFactor;
    
    // THRESHOLD - Softer for puffier clouds
    density = smoothstep(0.3, 0.7, density);
    
    return clamp(density, 0.0, 1.0);
}

// Lighting
float cloudLighting(vec3 pos, vec3 sunDir, float time, float dayFactor) {
    // During night, skip expensive shadow calculations
    if (dayFactor < 0.01) {
        return 0.1; // Minimal lighting at night
    }
    
    // Larger steps for softer shadows
    float shadowSample1 = cloudDensity(pos + sunDir * 3.0, time);
    float shadowSample2 = cloudDensity(pos + sunDir * 6.0, time);
    
    // Less aggressive shadowing for brighter clouds
    float shadow = exp(-shadowSample1 * 1.0 - shadowSample2 * 0.5);
    
    // More powder effect for fluffy appearance
    float powder = 1.0 - exp(-shadowSample1 * 2.5);
    shadow = mix(shadow, 1.0, powder * 0.5);
    
    return clamp(shadow, 0.3, 1.0);
}

// Rendering clouds

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
    
    // Calculate day factor based on sun height
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-10.0, 10.0, sunHeight); // Transition zone
    
    // Adaptive step count - fewer steps at night for performance
    int maxSteps = int(mix(24.0, 48.0, dayFactor));
    float stepSize = (tEnd - tStart) / float(maxSteps);
    
    vec3 pos = rayOrigin + rayDir * tStart;
    float transmittance = 1.0;
    vec3 lightAccum = vec3(0.0);
    
    vec3 sunDir = normalize(uSunPos);
    
    // Day/Night ambient lighting
    vec3 dayAmbient = vec3(0.6, 0.7, 0.9) * 0.6 + uSunColor * 0.3;
    vec3 nightAmbient = vec3(0.02, 0.03, 0.05); // Very dark blue-grey at night
    vec3 ambient = mix(nightAmbient, dayAmbient, dayFactor);
    
    for(int i = 0; i < maxSteps; i++) {
        if(transmittance < 0.01) break;
        
        float density = cloudDensity(pos, time);
        
        if(density > 0.01) {
            float sunVisibility = cloudLighting(pos, sunDir, time, dayFactor);
            
            // Phase function (less important at night)
            float cosAngle = dot(rayDir, sunDir);
            float g = 0.3;
            float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5));
            phase = mix(0.8, 1.0, phase * 2.0);
            
            // Scale sun contribution by day factor
            vec3 sunLight = uSunColor * sunVisibility * phase * 1.2 * dayFactor;
            vec3 lighting = sunLight + ambient * mix(0.5, 1.5, dayFactor);
            
            // Integration
            float dt = density * stepSize * 1.2;
            float sampleTransmittance = exp(-dt);
            
            lightAccum += transmittance * (1.0 - sampleTransmittance) * lighting;
            transmittance *= sampleTransmittance;
        }
        
        pos += rayDir * stepSize;
    }
    
    float alpha = 1.0 - transmittance;
    
    // Silver lining (only during day)
    if (dayFactor > 0.1) {
        float edgeFactor = pow(1.0 - transmittance, 3.0);
        float sunDot = max(0.0, dot(rayDir, sunDir));
        float silverLining = edgeFactor * sunDot * 0.4 * dayFactor;
        lightAccum += uSunColor * silverLining;
    }
    
    return vec4(lightAccum, alpha);
}

// Main
void main() {
    vec3 rayDir = normalize(fragRayDir - uCameraPos);
    
    if (rayDir.y < -0.1) {
        discard;
    }
    
    vec4 clouds = renderClouds(uCameraPos, rayDir, uTime);
    
    // Horizon fade
    float horizonFade = smoothstep(0.0, 0.25, rayDir.y);
    clouds.a *= horizonFade;

    // Day/night brightness adjustment
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-10.0, 10.0, sunHeight);
    float brightnessFactor = mix(0.8, 1.5, dayFactor); // Darker at night, brighter during day
    clouds.rgb *= brightnessFactor;
    
    // Atmospheric scattering - different colors for day/night
    float distance = length(fragRayDir - uCameraPos);
    float atmosphericFade = exp(-distance * 0.0005);
    vec3 daySkyColor = vec3(0.5, 0.65, 0.85);
    vec3 nightSkyColor = vec3(0.01, 0.02, 0.04); // Very dark at night
    vec3 skyColor = mix(nightSkyColor, daySkyColor, dayFactor);
    clouds.rgb = mix(skyColor, clouds.rgb, atmosphericFade);
    
    fragColor = clouds;
}
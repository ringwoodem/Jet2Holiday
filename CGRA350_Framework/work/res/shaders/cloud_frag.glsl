#version 330 core

in vec3 fragRayDir;
in vec2 texCoord;

out vec4 fragColor;

uniform vec3 uCameraPos;
uniform float uTime;
uniform vec3 uSunPos;
uniform vec3 uSunColor;

// Cloud control parameters
uniform float uCloudCoverage;
uniform float uCloudDensity;
uniform float uCloudScale;
uniform float uEvolutionSpeed;
uniform float uCloudHeight;
uniform float uCloudThickness;
uniform float uCloudFuzziness;

// Hash Function
vec3 hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// Single float hash for jittering
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Improved Perlin Noise with better interpolation
float perlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    
    // Quintic interpolation for smoother results
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

// FBM with rotation for more organic look
float perlinFbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    // Rotation matrix for breaking up patterns
    mat3 rot = mat3(0.00, 0.80, 0.60,
                    -0.80, 0.36, -0.48,
                    -0.60, -0.48, 0.64);
    
    for(int i = 0; i < octaves; i++) {
        value += amplitude * perlinNoise(p * frequency);
        p = rot * p; // Rotate for each octave
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Improved Worley with better distribution
float worleyNoise(vec3 p) {
    vec3 id = floor(p);
    vec3 f = fract(p);
    
    float minDist = 1.0;
    
    // Full 3x3x3 search for better quality
    for(int k = -1; k <= 1; k++) {
        for(int j = -1; j <= 1; j++) {
            for(int i = -1; i <= 1; i++) {
                vec3 neighbor = vec3(float(i), float(j), float(k));
                
                // Better hash for point distribution
                vec3 cellId = id + neighbor;
                vec3 point = fract(sin(cellId * vec3(12.9898, 78.233, 45.164)) * 43758.5453);
                point = 0.5 + 0.25 * sin(point * 6.2831 + uTime * 0.1); // Slight animation
                
                vec3 diff = neighbor + point - f;
                float dist = length(diff);
                
                minDist = min(minDist, dist);
            }
        }
    }
    
    return minDist;
}

// Optimized but fluffy cloud density with dynamic evolution and user controls
float cloudDensity(vec3 pos, float time) {
    // WIND - clouds drift horizontally
    vec3 windOffset = vec3(time * 0.01, 0.0, time * 0.005);
    
    // EVOLUTION - clouds morph over time (controlled by uEvolutionSpeed)
    float evolutionTime = time * uEvolutionSpeed;
    vec3 evolution = vec3(
        sin(evolutionTime) * 5.0,
        cos(evolutionTime * 0.7) * 3.0,
        sin(evolutionTime * 0.5) * 4.0
    );
    
    vec3 p = (pos + windOffset + evolution) * uCloudScale;
    
    // BASE SHAPE - animated in 4D (3D space + time)
    float timeOffset = time * uEvolutionSpeed * 1.5;
    vec3 p1 = p + vec3(timeOffset * 10.0);
    vec3 p2 = p + vec3(timeOffset * -8.0);
    
    float baseShape1 = perlinFbm(p1 * 0.04, 3);
    float baseShape2 = perlinFbm(p2 * 0.04, 3);
    
    // Blend between two noise fields for smooth transitions
    float blendFactor = sin(time * uEvolutionSpeed * 0.5) * 0.5 + 0.5;
    float baseShape = mix(baseShape1, baseShape2, blendFactor);
    baseShape = baseShape * 0.5 + 0.5;
    
    // COVERAGE with user control
    float coverage = perlinNoise(p * 0.025 + vec3(time * uEvolutionSpeed)) * 0.5 + 0.5;
    float coverageMin = mix(0.15, 0.5, 1.0 - uCloudCoverage);
    float coverageMax = mix(0.5, 0.95, uCloudCoverage);
    coverage = smoothstep(coverageMin, coverageMax, coverage);
    
    baseShape *= coverage;
    
    // WORLEY for cloud structure - animated for changing shapes
    float worleyFreq = mix(0.15, 0.25, uCloudFuzziness);
    float worley = worleyNoise(p * worleyFreq + vec3(0.0, time * uEvolutionSpeed * 0.5, 0.0));
    float edgeMin = mix(0.1, 0.3, uCloudFuzziness);
    float edgeMax = mix(0.6, 0.8, uCloudFuzziness);
    float edges = smoothstep(edgeMin, edgeMax, worley);
    
    float erosion = mix(0.15, 0.3, uCloudFuzziness);
    float density = baseShape * (1.0 - edges * erosion);
    
    // Add back subtle detail for fluffiness - also animated
    float detail = perlinNoise(p * 0.8 + vec3(sin(time * uEvolutionSpeed), 0.0, cos(time * uEvolutionSpeed))) * 0.5 + 0.5;
    density = mix(density, density * detail, 0.15);
    
    // Apply density control
    density *= uCloudDensity;
    
    // HEIGHT GRADIENT with user-controlled altitude
    float cloudBase = uCloudHeight - uCloudThickness * 0.5;
    float cloudTop = uCloudHeight + uCloudThickness * 0.5;
    float heightFactor = smoothstep(cloudBase - 3.0, cloudBase + 8.0, pos.y) *
                        (1.0 - smoothstep(cloudTop - 12.0, cloudTop + 2.0, pos.y));
    density *= heightFactor;
    
    // SOFTER THRESHOLD for puffier appearance
    density = smoothstep(0.25, 0.75, density);
    
    return clamp(density, 0.0, 1.0);
}

// Lighting with day/night optimization
float cloudLighting(vec3 pos, vec3 sunDir, float time, float dayFactor) {
    // During night, skip expensive shadow calculations
    if (dayFactor < 0.01) {
        return 0.1; // Minimal lighting at night
    }
    
    float shadowSample = cloudDensity(pos + sunDir * 4.0, time);
    
    float shadow = exp(-shadowSample * 1.5);
    
    float powder = 1.0 - exp(-shadowSample * 2.5);
    shadow = mix(shadow, 1.0, powder * 0.5);
    
    return clamp(shadow, 0.3, 1.0);
}

// Optimized Cloud Rendering with jittering, user controls, and day/night cycle
vec4 renderClouds(vec3 rayOrigin, vec3 rayDir, float time) {
    if (rayDir.y < 0.05) {
        return vec4(0.0);
    }
    
    float cloudBottom = uCloudHeight - uCloudThickness * 0.5;
    float cloudTop = uCloudHeight + uCloudThickness * 0.5;
    
    float tBottom = (cloudBottom - rayOrigin.y) / rayDir.y;
    float tTop = (cloudTop - rayOrigin.y) / rayDir.y;
    
    if (tBottom < 0.0 && tTop < 0.0) {
        return vec4(0.0);
    }
    
    float tStart = max(0.0, min(tBottom, tTop));
    float tEnd = max(tBottom, tTop);
    
    // Calculate day factor based on sun height
    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-10.0, 10.0, sunHeight);
    
    // Adaptive step count - fewer steps at night for performance
    int maxSteps = int(mix(20.0, 26.0, dayFactor));
    float stepSize = (tEnd - tStart) / float(maxSteps);
    
    // CRITICAL: Add jitter to starting position to reduce banding/streaking
    float jitter = hash(gl_FragCoord.xy + time * 0.1) * stepSize;
    
    vec3 pos = rayOrigin + rayDir * (tStart + jitter);
    float transmittance = 1.0;
    vec3 lightAccum = vec3(0.0);
    
    vec3 sunDir = normalize(uSunPos);
    
    // Day/Night ambient lighting
    vec3 dayAmbient = vec3(0.6, 0.7, 0.9) * 0.6 + uSunColor * 0.3;
    vec3 nightAmbient = vec3(0.02, 0.03, 0.05); // Very dark blue-grey at night
    vec3 ambient = mix(nightAmbient, dayAmbient, dayFactor);
    
    float g = 0.3;
    float phaseConstant = (1.0 - g * g) / (4.0 * 3.14159);
    
    for(int i = 0; i < maxSteps; i++) {
        if(transmittance < 0.01) break;
        
        float density = cloudDensity(pos, time);
        
        if(density > 0.01) {
            float sunVisibility = cloudLighting(pos, sunDir, time, dayFactor);
            
            // Phase function (less important at night)
            float cosAngle = dot(rayDir, sunDir);
            float phase = phaseConstant / pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5);
            phase = mix(0.8, 1.0, phase * 2.0);
            
            // Scale sun contribution by day factor
            vec3 sunLight = uSunColor * sunVisibility * phase * 1.2 * dayFactor;
            vec3 lighting = sunLight + ambient * mix(0.5, 1.5, dayFactor);
            
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

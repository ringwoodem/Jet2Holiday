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

in vec3 vNormal;
in vec3 vWorldPosition;

uniform vec3 cameraPosition;
uniform samplerCube uEnvironmentMap;

out vec4 fragColor;

void main() {
  // Calculate vector from camera to the vertex
  vec3 viewDir = normalize(cameraPosition - vWorldPosition);
  vec3 reflectedDir = reflect(-viewDir, normalize(vNormal));
  vec3 sunDir = normalize(uSunPos - vWorldPosition);
  float diffuse = max(dot(normalize(vNormal), sunDir), 0.0);
  
  // Check if sun is above horizon
  float sunHeight = uSunPos.y;
  float dayFactor = smoothstep(-50.0, 50.0, sunHeight);
  
  // Sample environment map and darken it at night
  vec4 reflectionColor = texture(uEnvironmentMap, reflectedDir);
  reflectionColor.rgb *= mix(0.001, 1.0, dayFactor); // Darken reflections at night
  
  // Calculate fresnel effect
  float fresnel = uFresnelScale * pow(1.0 - clamp(dot(viewDir, vNormal), 0.0, 1.0), uFresnelPower);
  fresnel *= mix(0.1, 1.0, dayFactor);

  // Calculate elevation-based color
  float elevation = vWorldPosition.y;
  // Calculate transition factors using smoothstep
  float peakFactor = smoothstep(uPeakThreshold - uPeakTransition, uPeakThreshold + uPeakTransition, elevation);
  float troughFactor = smoothstep(uTroughThreshold - uTroughTransition, uTroughThreshold + uTroughTransition, elevation);
  // Mix between trough and surface colors based on trough transition
  vec3 mixedColor1 = mix(uTroughColor, uSurfaceColor, troughFactor);
  // Mix between surface and peak colors based on peak transition 
  vec3 mixedColor2 = mix(mixedColor1, uPeakColor, peakFactor);
  // Mix the final color with the reflection color
  vec3 finalColor = mix(mixedColor2, reflectionColor.rgb, fresnel);
  
  // Apply day/night lighting with much darker night
  vec3 ambient = mix(
    vec3(0.005) * finalColor,  // Almost black at night
    vec3(0.2) * finalColor,    // Reduced ambient during day
    dayFactor
  );
  
  vec3 directLight = dayFactor * 0.8 * diffuse * finalColor * uSunColor;
  
  vec3 litColor = ambient + directLight;
  fragColor = vec4(litColor, uOpacity);
}
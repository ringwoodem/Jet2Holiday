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

  // Sample environment map to get the reflected color
  vec4 reflectionColor = texture(uEnvironmentMap, reflectedDir);

  // Calculate fresnel effect
  float fresnel = uFresnelScale * pow(1.0 - clamp(dot(viewDir, vNormal), 0.0, 1.0), uFresnelPower);

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
  vec3 litColor = finalColor * (0.3 + 0.7 * diffuse * uSunColor);

  fragColor = vec4(litColor, uOpacity);
}
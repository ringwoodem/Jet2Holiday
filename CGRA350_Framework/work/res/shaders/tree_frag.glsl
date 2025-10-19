#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColour; // Color passed from the vertex shader
in vec2 vTexCoord;
in vec4 vFragPosLightSpace;

uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform vec3 uCameraPos;

uniform sampler2D uTrunkDiffuse;
uniform sampler2D uTrunkNormal;
uniform sampler2D uTrunkRoughness;
uniform sampler2D uShadowMap;

out vec4 FragColor;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // Transform to [0,1] range
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);  
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}


void main() {
    // Determine if this is a leaf or trunk/branch based on vertex color
    bool isLeaf = (vColour.g > 0.5 && vColour.r < 0.5);
    
    vec3 albedo;
    if (isLeaf) {
        // Leaf color - rich green
        albedo = vec3(0.2, 0.6, 0.2);
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
    
    float shadow = calculateShadow(vFragPosLightSpace, N, L);
    
    float NdotL = max(dot(N, L), 0.0);
    
    vec3 ambient = mix(
        vec3(0.05) * albedo,
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
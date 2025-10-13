#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;

uniform vec3 uSunPos;
uniform vec3 uSunColor;

uniform sampler2D uTrunkDiffuse;
uniform sampler2D uTrunkNormal;
uniform sampler2D uTrunkRoughness;

out vec4 FragColor;

void main() {
    vec3 trunkColor = texture(uTrunkDiffuse, vTexCoord).rgb;
    vec3 trunkNormal = texture(uTrunkNormal, vTexCoord).rgb * 2.0 - 1.0; // Convert to [-1,1]
    float trunkRoughness = texture(uTrunkRoughness, vTexCoord).r;

    vec3 N = normalize(trunkNormal);
    vec3 L = normalize(uSunPos - vWorldPos);

    float sunHeight = uSunPos.y;
    float dayFactor = smoothstep(-50.0, 50.0, sunHeight); // 0 at night, 1 at day

    float NdotL = max(dot(N, L), 0.0);
    vec3 albedo = trunkColor * vColor; // Tint texture by vertex color if desired

    vec3 ambient = mix(
        vec3(0.01) * albedo, 
        vec3(0.3) * albedo, 
        dayFactor
    ); // Ambient term

    vec3 diffuse = dayFactor * NdotL * albedo * uSunColor;

    vec3 finalColor = ambient + diffuse;

    FragColor = vec4(finalColor, 1.0);
}
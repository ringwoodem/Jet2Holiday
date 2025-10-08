#version 430 core

// Use a work group size of 64 threads
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Tree parameters
layout(std140, binding = 0) uniform TreeParams {
    float scale;           // Tree height
    float baseSize;        // Base radius multiplier
    float ratio;           // Radius to height ratio
    float ratioPower;      // Taper exponent
    float flare;           // Base flare amount
    int segments;          // Number of segments along trunk
    int radialSegments;    // Number of vertices around trunk
};

// Output vertex structure
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

// Output buffers
layout(std430, binding = 1) buffer VertexBuffer {
    Vertex vertices[];
};

layout(std430, binding = 2) buffer IndexBuffer {
    uint indices[];
};

// Calculate trunk radius at a given height
float getTrunkRadius(float offset, float length) {
    float unitTaper = offset / length;
    float radius = scale * ratio * pow(1.0 - unitTaper, ratioPower);
    
    // Add flare at base
    if (offset < flare * length) {
        float flareAmount = 1.0 - offset / (flare * length);
        radius += flareAmount * flareAmount * flare * baseSize;
    }
    
    return max(radius, 0.005);
}

void main() {
    // Each invocation generates one segment ring
    uint segmentIdx = gl_GlobalInvocationID.x;
    
    if (segmentIdx > segments) return;
    
    float trunkLength = scale;
    float segmentLength = trunkLength / float(segments);
    float yPos = float(segmentIdx) * segmentLength;
    float radius = getTrunkRadius(yPos, trunkLength);
    
    // Generate ring of vertices at this height
    uint baseVertexIdx = segmentIdx * radialSegments;
    
    for (int i = 0; i < radialSegments; i++) {
        float angle = float(i) / float(radialSegments) * 2.0 * 3.14159265359;
        
        Vertex v;
        v.position = vec3(
            cos(angle) * radius,
            yPos,
            sin(angle) * radius
        );
        v.normal = normalize(vec3(cos(angle), 0.0, sin(angle)));
        v.uv = vec2(
            float(i) / float(radialSegments),
            float(segmentIdx) / float(segments)
        );
        
        vertices[baseVertexIdx + i] = v;
    }
    
    // Generate indices to connect this ring to the next
    if (segmentIdx < segments) {
        uint nextRingBase = (segmentIdx + 1) * radialSegments;
        uint indexBase = segmentIdx * radialSegments * 6;
        
        for (int i = 0; i < radialSegments; i++) {
            int nextI = (i + 1) % radialSegments;
            
            uint idx = indexBase + i * 6;
            
            // Triangle 1
            indices[idx + 0] = baseVertexIdx + i;
            indices[idx + 1] = baseVertexIdx + nextI;
            indices[idx + 2] = nextRingBase + i;
            
            // Triangle 2
            indices[idx + 3] = baseVertexIdx + nextI;
            indices[idx + 4] = nextRingBase + nextI;
            indices[idx + 5] = nextRingBase + i;
        }
    }
}

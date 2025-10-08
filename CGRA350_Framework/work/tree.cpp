#include "tree.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <cmath>
#include <random>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

// Random number generator
static std::mt19937 rng(12345); // Fixed seed for consistency

Tree::Tree(const glm::vec3& position)
    : m_position(position)
    , m_meshGenerated(false)
    , m_rotation(0.0f)
{
    m_params.scale = 10.0f;
    m_params.baseSize = 0.15f;
    m_params.ratio = 0.015f;
    m_params.ratioPower = 1.2f;
    m_params.flare = 0.6f;
    m_params.radialSegments = 16;
    m_params.levels = 3;
    
    // Level 0: Trunk
    m_params.level[0].nLength = 1.0f;
    m_params.level[0].nCurveRes = 10;
    m_params.level[0].nCurve = 0.0f;
    m_params.level[0].nBranches = 20;
    
    // Level 1: Main branches
    m_params.level[1].nLength = 0.3f;
    m_params.level[1].nCurveRes = 8;
    m_params.level[1].nCurve = 0.0f;
    m_params.level[1].nDownAngle = 45.0f;
    m_params.level[1].nRotate = 72.0f;
    m_params.level[1].nBranches = 16;  // More sub-branches
    
    // Level 2: Twigs for leaves
    m_params.level[2].nLength = 0.25f;  // Longer twigs
    m_params.level[2].nCurveRes = 6;
    m_params.level[2].nCurve = 0.0f;
    m_params.level[2].nDownAngle = 35.0f;
    m_params.level[2].nRotate = 120.0f;
    m_params.level[2].nBranches = 0;
    
    m_params.hasLeaves = true;
    m_params.leafScale = 0.25f;  // Larger leaves
    m_params.leavesPerBranch = 12;  // More leaves per branch
}

void Tree::setParameters(const TreeParameters& params) {
    m_params = params;
    m_meshGenerated = false;
}

void Tree::regenerate() {
    m_meshGenerated = false;
    m_stems.clear();
}

float Tree::randomVariance(float variance) {
    std::uniform_real_distribution<float> dist(-variance, variance);
    return dist(rng);
}

float Tree::stemRadius(int level, float offset, float length) {
    float radius;
    
    if (level == 0) {
        float unitTaper = offset / length;
        radius = m_params.scale * m_params.ratio * pow(1.0f - unitTaper, m_params.ratioPower);
        
        if (offset < m_params.flare * length) {
            float flareAmount = (1.0f - offset / (m_params.flare * length));
            radius += flareAmount * flareAmount * m_params.flare * m_params.baseSize;
        }
    } else {
        float unitTaper = offset / length;
        radius = m_params.scale * m_params.ratio * pow(length, m_params.ratioPower);
        radius *= pow(1.0f - unitTaper, m_params.ratioPower);
    }
    
    return std::max(radius, 0.005f);
}

int Tree::branchesAtSegment(int level, int segmentIndex, int totalSegments) {
    const BranchLevel& params = m_params.level[level];
    
    if (params.nBranches == 0) return 0;
    if (level >= m_params.levels - 1) return 0;
    
    if (segmentIndex <= 0 || segmentIndex >= totalSegments) return 0;
    
    // Calculate how many segments we have available for branching
    int availableSegments = totalSegments - 2;  // Exclude first and last
    
    // If we want more branches than segments, put multiple branches per segment
    if (params.nBranches <= availableSegments) {
        // Original behavior: distribute branches evenly, one per segment
        float segmentsPerBranch = (float)availableSegments / (float)params.nBranches;
        
        for (int b = 0; b < params.nBranches; b++) {
            int targetSegment = 1 + (int)(b * segmentsPerBranch + segmentsPerBranch * 0.5f);
            if (segmentIndex == targetSegment) {
                return 1;
            }
        }
    } else {
        // New behavior: multiple branches per segment
        int branchesPerSegment = (params.nBranches + availableSegments - 1) / availableSegments;  // Ceiling division
        
        // Check if this segment should have branches
        int segmentOffset = segmentIndex - 1;  // 0-indexed from first valid segment
        if (segmentOffset >= 0 && segmentOffset < availableSegments) {
            // Calculate how many branches this segment should have
            int startBranch = segmentOffset * branchesPerSegment;
            int endBranch = std::min(startBranch + branchesPerSegment, params.nBranches);
            
            return endBranch - startBranch;  // Return number of branches for this segment
        }
    }
    
    return 0;
}

void Tree::generateStem(int level, const glm::vec3& startPos, const glm::vec3& startDir,
                       float length, float baseRadius, int parentSegment) {
    if (level >= m_params.levels) return;
    
    const BranchLevel& params = m_params.level[level];
    
    Stem stem;
    stem.level = level;
    stem.position = startPos;
    stem.direction = glm::normalize(startDir);
    stem.length = length;
    stem.radius = baseRadius;
    
    glm::vec3 currentPos = startPos;
    glm::vec3 currentDir = stem.direction;
    float segmentLength = length / params.nCurveRes;
    
    for (int i = 0; i <= params.nCurveRes; i++) {
        float offset = i * segmentLength;
        float radius = stemRadius(level, offset, length);
        
        if (level > 0 && baseRadius > 0.0f) {
            radius = std::min(radius, baseRadius * 0.5f);
        }
        
        StemSegment seg;
        seg.position = currentPos;
        seg.direction = currentDir;
        seg.rotation = glm::rotation(glm::vec3(0, 1, 0), currentDir);
        seg.radius = radius;
        seg.length = segmentLength;
        seg.level = level;
        seg.segmentIndex = i;
        seg.totalSegments = params.nCurveRes;
        
        stem.segments.push_back(seg);
        
        int branchCount = branchesAtSegment(level, i, params.nCurveRes);
        
        if (branchCount > 0 && level + 1 < m_params.levels) {
            const BranchLevel& childParams = m_params.level[level + 1];
            
            // Create multiple branches at this segment
            for (int branchIdx = 0; branchIdx < branchCount; branchIdx++) {
                float downAngle = glm::radians(childParams.nDownAngle);
                
                // Distribute branches around the stem
                float baseRotation = glm::radians(childParams.nRotate * i);
                float branchSpacing = glm::two_pi<float>() / std::max(branchCount, 1);
                float rotateAngle = baseRotation + branchIdx * branchSpacing;
                
                glm::vec3 perpendicular;
                if (glm::abs(glm::dot(currentDir, glm::vec3(0, 0, 1))) < 0.99f) {
                    perpendicular = glm::normalize(glm::cross(currentDir, glm::vec3(0, 0, 1)));
                } else {
                    perpendicular = glm::normalize(glm::cross(currentDir, glm::vec3(1, 0, 0)));
                }
                
                glm::quat rotAroundStem = glm::angleAxis(rotateAngle, currentDir);
                glm::vec3 outward = rotAroundStem * perpendicular;
                
                glm::quat downRotation = glm::angleAxis(downAngle, glm::cross(currentDir, outward));
                glm::vec3 branchDir = downRotation * currentDir;
                
                float childLength = length * childParams.nLength;
                generateStem(level + 1, currentPos, branchDir, childLength, radius);
            }
        }
        
        if (i < params.nCurveRes) {
            currentPos += currentDir * segmentLength;
        }
    }
    
    m_stems.push_back(stem);
}

void Tree::generateMeshFromSegments() {
    cgra::mesh_builder trunkBuilder;
    cgra::mesh_builder branchBuilder;
    
    const int radSegs = m_params.radialSegments;
    
    for (const Stem& stem : m_stems) {
        cgra::mesh_builder& mb = (stem.level == 0) ? trunkBuilder : branchBuilder;
        size_t vertexStart = mb.vertices.size();
        
        for (const StemSegment& seg : stem.segments) {
            for (int j = 0; j < radSegs; j++) {
                float angle = (float)j / radSegs * glm::two_pi<float>();
                glm::vec3 offset(cos(angle) * seg.radius, 0, sin(angle) * seg.radius);
                glm::vec3 worldOffset = seg.rotation * offset;
                glm::vec3 normal = glm::normalize(worldOffset);
                
                cgra::mesh_vertex vertex;
                vertex.pos = seg.position + worldOffset;
                vertex.norm = normal;
                vertex.uv = glm::vec2((float)j / radSegs, (float)seg.segmentIndex / seg.totalSegments);
                
                mb.push_vertex(vertex);
            }
        }
        
        size_t ringCount = stem.segments.size();
        for (size_t r = 0; r < ringCount - 1; r++) {
            size_t baseIdx = vertexStart + r * radSegs;
            for (int j = 0; j < radSegs; j++) {
                int j_next = (j + 1) % radSegs;
                
                mb.push_index(static_cast<GLuint>(baseIdx + j));
                mb.push_index(static_cast<GLuint>(baseIdx + j_next));
                mb.push_index(static_cast<GLuint>(baseIdx + radSegs + j));
                
                mb.push_index(static_cast<GLuint>(baseIdx + j_next));
                mb.push_index(static_cast<GLuint>(baseIdx + radSegs + j_next));
                mb.push_index(static_cast<GLuint>(baseIdx + radSegs + j));
            }
        }
    }
    
    std::cout << "Trunk - vertices: " << trunkBuilder.vertices.size()
              << ", indices: " << trunkBuilder.indices.size() << std::endl;
    std::cout << "Branches - vertices: " << branchBuilder.vertices.size()
              << ", indices: " << branchBuilder.indices.size() << std::endl;
    
    m_trunkMesh = trunkBuilder.build();
    m_branchesMesh = branchBuilder.build();
}

void Tree::generateLeavesMesh() {
    if (!m_params.hasLeaves) return;
    
    cgra::mesh_builder leafBuilder;
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    
    for (const Stem& stem : m_stems) {
        if (stem.level != m_params.levels - 1) continue;
        
        int numLeaves = m_params.leavesPerBranch;
        
        // Place leaves more densely along the branch
        for (int i = 0; i < numLeaves; i++) {
            // Start from 20% along the branch to 100%
            float baseT = 0.2f + (float)i / (numLeaves - 1) * 0.8f;
            
            // Add some randomness to position
            float t = baseT + randomVariance(0.05f);
            t = glm::clamp(t, 0.0f, 1.0f);
            
            int segmentIdx = (int)(t * (stem.segments.size() - 1));
            segmentIdx = glm::clamp(segmentIdx, 0, (int)stem.segments.size() - 1);
            
            const StemSegment& seg = stem.segments[segmentIdx];
            
            // Multiple leaves around each point
            int leavesAround = 2;  // 2 leaves at each position
            for (int j = 0; j < leavesAround; j++) {
                float rotAngle = (float)i / numLeaves * glm::two_pi<float>() * 3.0f +
                                (float)j / leavesAround * glm::two_pi<float>();
                rotAngle += randomVariance(0.3f);
                
                // Slightly offset position for variety
                glm::vec3 leafPos = seg.position + seg.direction * randomVariance(0.02f);
                
                createLeaf(leafBuilder, leafPos, seg.direction, seg.rotation, rotAngle);
            }
        }
    }
    
    std::cout << "Leaves - vertices: " << leafBuilder.vertices.size()
              << ", indices: " << leafBuilder.indices.size() << std::endl;
    
    m_leavesMesh = leafBuilder.build();
}

void Tree::createLeaf(cgra::mesh_builder& mb, const glm::vec3& position,
                     const glm::vec3& stemDir, const glm::quat& stemRot, float rotAngle) {
    const LeafParameters& lp = m_params.leafParams;
    float scale = m_params.leafScale;
    
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    
    // Add scale variation
    scale *= (0.8f + dist01(rng) * 0.4f);
    
    glm::vec3 perpendicular;
    if (glm::abs(glm::dot(stemDir, glm::vec3(0, 0, 1))) < 0.99f) {
        perpendicular = glm::normalize(glm::cross(stemDir, glm::vec3(0, 0, 1)));
    } else {
        perpendicular = glm::normalize(glm::cross(stemDir, glm::vec3(1, 0, 0)));
    }
    
    glm::quat rot = glm::angleAxis(rotAngle, stemDir);
    glm::vec3 leafUp = rot * perpendicular;
    glm::vec3 leafRight = glm::cross(stemDir, leafUp);
    
    // Random tilt angle for variety
    float tiltAngle = glm::radians(25.0f + dist01(rng) * 20.0f);
    glm::quat tilt = glm::angleAxis(tiltAngle, leafRight);
    glm::vec3 leafNormal = tilt * stemDir;
    leafUp = tilt * leafUp;
    
    if (lp.lobeCount == 1) {
        createSimpleLeaf(mb, position, leafRight, leafUp, leafNormal, scale, lp);
    } else {
        createLobedLeaf(mb, position, leafRight, leafUp, leafNormal, scale, lp);
    }
}

void Tree::createSimpleLeaf(cgra::mesh_builder& mb, const glm::vec3& center,
                           const glm::vec3& right, const glm::vec3& up,
                           const glm::vec3& normal, float scale,
                           const LeafParameters& lp) {
    size_t baseIdx = mb.vertices.size();
    
    int segments = 10;  // More segments for smoother leaves
    float width = lp.lobeWidth * scale;
    float height = lp.lobeHeight * scale;
    
    cgra::mesh_vertex centerVert;
    centerVert.pos = center;
    centerVert.norm = normal;
    centerVert.uv = glm::vec2(0.5f, 0.5f);
    mb.push_vertex(centerVert);
    
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * glm::two_pi<float>();
        float x = cos(angle) * width;
        float y = sin(angle) * height;
        
        // Better leaf shape - pointed at ends
        float angleFactor = sin(angle);
        float taper = 1.0f - abs(angleFactor) * 0.5f;
        x *= taper;
        
        // Make tip more pointed
        if (angleFactor > 0.7f) {
            float tipFactor = (angleFactor - 0.7f) / 0.3f;
            x *= (1.0f - tipFactor * 0.5f);
            y *= (1.0f + tipFactor * 0.2f);
        }
        
        cgra::mesh_vertex v;
        v.pos = center + right * x + up * y;
        v.norm = normal;
        v.uv = glm::vec2(0.5f + x / width * 0.5f, 0.5f + y / height * 0.5f);
        mb.push_vertex(v);
    }
    
    for (int i = 0; i < segments; i++) {
        mb.push_index(baseIdx);
        mb.push_index(baseIdx + 1 + i);
        mb.push_index(baseIdx + 1 + ((i + 1) % (segments + 1)));
    }
    
    // Back face
    size_t backBaseIdx = mb.vertices.size();
    centerVert.norm = -normal;
    mb.push_vertex(centerVert);
    
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * glm::two_pi<float>();
        float x = cos(angle) * width;
        float y = sin(angle) * height;
        
        float angleFactor = sin(angle);
        float taper = 1.0f - abs(angleFactor) * 0.5f;
        x *= taper;
        
        if (angleFactor > 0.7f) {
            float tipFactor = (angleFactor - 0.7f) / 0.3f;
            x *= (1.0f - tipFactor * 0.5f);
            y *= (1.0f + tipFactor * 0.2f);
        }
        
        cgra::mesh_vertex v;
        v.pos = center + right * x + up * y;
        v.norm = -normal;
        v.uv = glm::vec2(0.5f + x / width * 0.5f, 0.5f + y / height * 0.5f);
        mb.push_vertex(v);
    }
    
    for (int i = 0; i < segments; i++) {
        mb.push_index(backBaseIdx);
        mb.push_index(backBaseIdx + 1 + ((i + 1) % (segments + 1)));
        mb.push_index(backBaseIdx + 1 + i);
    }
}

void Tree::createLobedLeaf(cgra::mesh_builder& mb, const glm::vec3& center,
                          const glm::vec3& right, const glm::vec3& up,
                          const glm::vec3& normal, float scale,
                          const LeafParameters& lp) {
    size_t baseIdx = mb.vertices.size();
    
    cgra::mesh_vertex centerVert;
    centerVert.pos = center + up * lp.lobeOffset * scale;
    centerVert.norm = normal;
    centerVert.uv = glm::vec2(0.5f, 0.5f);
    mb.push_vertex(centerVert);
    
    std::vector<glm::vec3> lobePoints;
    
    for (int lobe = 0; lobe < lp.lobeCount; lobe++) {
        float lobeAngle = (float)lobe / lp.lobeCount * glm::two_pi<float>();
        float lobeDist = lp.lobeHeight * scale * (lobe == 0 ? 1.0f : lp.lobeScale);
        
        glm::vec3 lobeDir = glm::cos(lobeAngle) * right + glm::sin(lobeAngle) * up;
        glm::vec3 lobeTip = centerVert.pos + lobeDir * lobeDist;
        
        int pointsPerLobe = 5;
        for (int p = 0; p <= pointsPerLobe; p++) {
            float t = (float)p / pointsPerLobe;
            glm::vec3 pos = glm::mix(centerVert.pos, lobeTip, t);
            lobePoints.push_back(pos);
        }
    }
    
    for (size_t i = 0; i < lobePoints.size(); i++) {
        cgra::mesh_vertex v;
        v.pos = lobePoints[i];
        v.norm = normal;
        v.uv = glm::vec2(0.5f, 0.5f);
        mb.push_vertex(v);
    }
    
    for (size_t i = 0; i < lobePoints.size(); i++) {
        mb.push_index(baseIdx);
        mb.push_index(baseIdx + 1 + i);
        mb.push_index(baseIdx + 1 + ((i + 1) % lobePoints.size()));
    }
    
    // Back face
    size_t backBaseIdx = mb.vertices.size();
    centerVert.norm = -normal;
    mb.push_vertex(centerVert);
    
    for (size_t i = 0; i < lobePoints.size(); i++) {
        cgra::mesh_vertex v;
        v.pos = lobePoints[i];
        v.norm = -normal;
        v.uv = glm::vec2(0.5f, 0.5f);
        mb.push_vertex(v);
    }
    
    for (size_t i = 0; i < lobePoints.size(); i++) {
        mb.push_index(backBaseIdx);
        mb.push_index(backBaseIdx + 1 + ((i + 1) % lobePoints.size()));
        mb.push_index(backBaseIdx + 1 + i);
    }
}

void Tree::draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    if (!m_meshGenerated) {
        std::cout << "=== GENERATING TREE WITH BRANCHES ===" << std::endl;
        std::cout << "Position: (" << m_position.x << ", " << m_position.y << ", " << m_position.z << ")" << std::endl;
        
        m_stems.clear();
        float trunkLength = m_params.scale * m_params.level[0].nLength;
        
        generateStem(0, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), trunkLength, m_params.baseSize);
        std::cout << "Total stems: " << m_stems.size() << std::endl;
        
        generateMeshFromSegments();
        generateLeavesMesh();
        m_meshGenerated = true;
        
        std::cout << "Trunk mesh VBO: " << m_trunkMesh.vbo << ", indices: " << m_trunkMesh.index_count << std::endl;
        std::cout << "Branch mesh VBO: " << m_branchesMesh.vbo << ", indices: " << m_branchesMesh.index_count << std::endl;
    }
    
    // Create model matrix with position and rotation
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_position);
    
    // Apply rotation to align with terrain
    if (glm::length(m_rotation) > 0.001f) {
        model = glm::rotate(model, m_rotation.x, glm::vec3(1, 0, 0)); // Pitch
        model = glm::rotate(model, m_rotation.y, glm::vec3(0, 1, 0)); // Yaw
        model = glm::rotate(model, m_rotation.z, glm::vec3(0, 0, 1)); // Roll
    }
    
    glm::mat4 modelview = view * model;
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, false, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, false, glm::value_ptr(modelview));
    
    // Draw trunk
    glm::vec3 trunkColor(0.4f, 0.25f, 0.15f);
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(trunkColor));
    if (m_trunkMesh.vbo != 0 && m_trunkMesh.index_count > 0) {
        m_trunkMesh.draw();
    }
    
    // Draw branches
    glm::vec3 branchColor(0.35f, 0.22f, 0.12f);
    glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(branchColor));
    if (m_branchesMesh.vbo != 0 && m_branchesMesh.index_count > 0) {
        m_branchesMesh.draw();
    }
    
    // Draw leaves
    if (m_params.hasLeaves && m_leavesMesh.vbo != 0 && m_leavesMesh.index_count > 0) {
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(m_params.leafParams.color));
        m_leavesMesh.draw();
    }
}



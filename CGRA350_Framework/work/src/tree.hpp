#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include "cgra/cgra_mesh.hpp"

struct BranchLevel {
    // Length and shape
    float nLength = 1.0f;          // Length relative to parent
    float nLengthV = 0.0f;         // Length variance
    float nTaper = 1.0f;           // Taper rate (1 = no taper, 0 = cone)
    
    // Curvature
    int nCurveRes = 5;             // Segments in this branch level
    float nCurve = 0.0f;           // Overall curvature angle
    float nCurveV = 0.0f;          // Curvature variance
    float nCurveBack = 0.0f;       // Bend back toward parent
    
    // Branching
    int nBranches = 0;             // Number of child branches
    float nBranchDist = 0.0f;      // Distribution along stem (0=even, negative=base, positive=tip)
    float nDownAngle = 45.0f;      // Angle down from parent direction
    float nDownAngleV = 0.0f;      // Down angle variance
    float nRotate = 140.0f;        // Rotation around parent (137.5 = golden angle)
    float nRotateV = 0.0f;         // Rotation variance
};

struct LeafParameters {
    float lobeWidth = 0.4f;
    float lobeHeight = 0.8f;
    float lobeOffset = 0.1f;
    float topAngle = 45.0f;
    float bottomAngle = 30.0f;
    
    int lobeCount = 1;
    float lobeSeparation = 120.0f;
    float lobeScale = 0.8f;
    
    glm::vec3 color = glm::vec3(0.2f, 0.6f, 0.2f);
};

struct TreeParameters {
    // Overall shape
    float shape = 70.0f;            // Overall tree shape (conical=0, spherical=7, etc)
    float baseSize = 0.10f;         // Radius of trunk base
    float scale = 5.0f;           // Overall tree height
    float scaleV = 30.0f;           // Height variance
    
    // Trunk specific
    int levels = 3;                // Number of branch levels (0=trunk, 1=branches, 2=twigs)
    float ratio = 0.015f;          // Ratio of trunk radius to height
    float ratioPower = 10.2f;       // How taper changes with height
    float flare = 5.6f;            // Base flare (trunk widening at base)
    
    // Branch levels (0 = trunk)
    BranchLevel level[4];          // Support trunk + 3 branch levels
    
    // Leaves
    bool hasLeaves = true;
    float leafScale = 2.17f;
    int leavesPerBranch = 10;
    LeafParameters leafParams;
    
    // Quality settings
    int radialSegments = 16;       // Smoothness of cylinders
};

class Tree {
private:
    TreeParameters m_params;
    glm::vec3 m_position;
    glm::vec3 m_rotation;  // ADD THIS - rotation for terrain alignment
    cgra::gl_mesh m_trunkMesh;
    cgra::gl_mesh m_branchesMesh;
    cgra::gl_mesh m_leavesMesh;
    bool m_meshGenerated;
    
    struct StemSegment {
        glm::vec3 position;
        glm::vec3 direction;       // Unit vector pointing forward
        glm::quat rotation;
        float radius;
        float length;              // Length of this segment
        int level;                 // 0=trunk, 1=branch, 2=twig, etc
        int segmentIndex;          // Index within this stem
        int totalSegments;         // Total segments in this stem
    };
    
    std::vector<StemSegment> m_segments;
    
    struct Stem {
        glm::vec3 position;
        glm::vec3 direction;       // Unit vector pointing forward
        glm::quat rotation;
        float radius;
        float length;              // Length of this segment
        int level;                 // 0=trunk, 1=branch, 2=twig, etc
        int segmentIndex;          // Index within this stem
        int totalSegments;
        std::vector<StemSegment> segments;
    };
    std::vector<Stem> m_stems;
    
    // Helper functions
    float randomVariance(float variance);
    float stemRadius(int level, float offset, float length);
    int branchesAtSegment(int level, int segmentIndex, int totalSegments);
    
    // Generation functions
    void generateStem(int level, const glm::vec3& startPos, const glm::vec3& startDir,
                     float length, float baseRadius, int parentSegment = -1);
    void generateMeshFromSegments();
    void generateLeavesMesh();
    
    void createLeaf(cgra::mesh_builder& mb, const glm::vec3& position,
                   const glm::vec3& stemDir, const glm::quat& stemRot, float rotAngle);
    void createSimpleLeaf(cgra::mesh_builder& mb, const glm::vec3& center,
                         const glm::vec3& right, const glm::vec3& up,
                         const glm::vec3& normal, float scale,
                         const LeafParameters& lp);
    void createLobedLeaf(cgra::mesh_builder& mb, const glm::vec3& center,
                        const glm::vec3& right, const glm::vec3& up,
                        const glm::vec3& normal, float scale,
                        const LeafParameters& lp);
    
public:
    Tree(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 0.0f));
    
    void setParameters(const TreeParameters& params);
    void regenerate();
    void draw(const glm::mat4& view, const glm::mat4& proj, GLuint shader,
        const glm::vec3& sunPos = glm::vec3(0.0f, 100.0f, 0.0f), const glm::vec3& sunColour = glm::vec3(1.0f, 1.0f, 1.0f),
        GLuint trunkDiffuse = 0, GLuint trunkNormal = 0, GLuint trunkRoughness = 0, const glm::vec3& cameraPos = glm::vec3(0.0f, 1.0f, 0.0f), 
        const glm::mat4& lightSpaceMatrix = glm::mat4(1.0f), GLuint shadowMap = 0);

    void drawShadows(GLuint shader);
    
    TreeParameters& getParameters() { return m_params; }
    
    void setRotation(const glm::vec3& rotation) {
        m_rotation = rotation;
        m_meshGenerated = false;
    }
    glm::vec3 getRotation() const { return m_rotation; }
};

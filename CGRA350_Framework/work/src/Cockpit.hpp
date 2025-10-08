#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <GL/glew.h>

struct PovCamera;

struct AABB {
    glm::vec3 min, max;
};

inline AABB makeAABB(const glm::vec3& c, const glm::vec3& half){
    return {c-half, c+half};
}
inline bool aabbContainsXY(const AABB& b, const glm::vec3& p){
    return (p.x>=b.min.x && p.x<=b.max.x && p.y>=b.min.y && p.y<=b.max.y);
}

struct Slider1D {
    glm::vec3 base{0};  //track center
    glm::vec3 axis{1,0,0};  //movement axis (unit)
    float halfLen = 0.2f;   //track half length
    glm::vec3 handlePos{0};
    glm::vec3 handleHalf{0.02f,0.01f,0.02f};
    float minVal = 30.f, maxVal = 100.f;
    float value01 = 0.5f;
    bool grabbed = false;

    inline void setFrom01(float t){
        value01 = std::clamp(t, 0.f, 1.f);
        float u = -halfLen + (2.f*halfLen)*value01;
        handlePos = base + axis * u;
    }
    inline float mapped() const {
        return minVal + (maxVal-minVal)*value01;
    }
    inline void placeFromPoint(const glm::vec3& p){
        float u = glm::dot(p - base, axis);
        u = std::clamp(u, -halfLen, +halfLen);
        handlePos = base + axis * u;
        value01 = (u + halfLen) / (2.f*halfLen);
    }
    inline AABB aabb() const {
        return makeAABB(handlePos, handleHalf);
    }
};

struct Button {
    glm::vec3 topCenter{0};
    glm::vec3 half{0.025f,0.012f,0.025f};
    float depth=0.f, vel=0.f, maxDepth=0.012f, k=900.f, c=0.9f;
    bool  isDown=false, pressedEdge=false;
    inline AABB aabb() const { return makeAABB(topCenter - glm::vec3(0,depth,0), half); }
    inline void step(bool pushing, float dt){
        float target = pushing ? maxDepth : 0.f;
        float force = -k * (depth - target) - c * vel;
        vel += force * dt;
        depth += vel * dt;
        depth = std::clamp(depth, 0.f, maxDepth);
        if ((depth==0.f && vel<0) || (depth==maxDepth && vel>0)){
            vel = 0;
        }
        bool now = (depth/maxDepth) > 0.9f;
        pressedEdge = (!isDown && now);
        isDown = now;
    }
};

class Cockpit {
public:
    inline void init(GLuint colorShader){
        m_shader = colorShader;
        layout();
        ensureWireCube();
    }

    //Per-frame hook from Application::render()
    inline void frame(int width, int height,
               const glm::vec2& mousePos, bool leftDown,
               const glm::mat4& view, const glm::mat4& proj,
               PovCamera& camera)
    {
        m_mousePos = mousePos;
        m_leftDown = leftDown;

        //
        glm::vec2 ndc = mouseToNDC(mousePos, width, height);
        Ray ray = makeRay(proj, view, ndc);
        glm::vec3 hit;
        bool hitPanel = rayPlaneZ(ray, m_panelZ, hit);
        AABB dot = makeAABB(hit, {0.01f, 0.01f, 0.01f});
        drawAABB(dot, view, proj, {1,0,1});
        

        //slider interaction
        bool overHandle = aabbContainsXY(m_sliderFOV.aabb(), hit);
        bool overTrack  = hit.x >= m_sliderFOV.base.x - m_sliderFOV.halfLen && hit.x <= m_sliderFOV.base.x + m_sliderFOV.halfLen && std::abs(hit.y - m_sliderFOV.base.y) <= 0.03f;
        
        if (hitPanel) {
            //printf("lever");
            if (m_leftDown) {
                //printf("leverdown");
                if (!m_sliderFOV.grabbed && (overHandle || overTrack)){
                    //if (aabbContainsXY(m_sliderFOV.aabb(), hit)){
                    m_sliderFOV.grabbed = true;
                    //printf("yippie");
                    //}
                }
                if (m_sliderFOV.grabbed){
                    m_sliderFOV.placeFromPoint(hit);
                }
            } else {
                m_sliderFOV.grabbed = false;
            }
        }
        camera.fovDeg = m_sliderFOV.mapped();

        //button interaction
        bool pushing = (hitPanel && aabbContainsXY(m_buttonReset.aabb(), hit) && m_leftDown);
        m_buttonReset.step(pushing, 1.f/60.f);
        if (m_buttonReset.pressedEdge) {
            //AABB dot = makeAABB(hit, {0.01f, 0.01f, 0.01f});
            //drawAABB(dot, view, proj, {1,0,1});
            //printf("button");
            camera.reset();
            float t = (camera.fovDeg - m_sliderFOV.minVal) / (m_sliderFOV.maxVal - m_sliderFOV.minVal);
            m_sliderFOV.setFrom01(std::clamp(t,0.f,1.f));
        }

        //draw
        drawAABB(m_table,view, proj, {0.35f,0.35f,0.4f});
        drawAABB(m_sliderFOV.aabb(),view, proj, {0.2f,0.8f,0.9f});
        drawAABB(m_buttonReset.aabb(),view, proj, {0.9f,0.7f,0.2f});
    }

    inline void setPanelZ(float z) {
        m_panelZ = z; layout();
    }

private:
    //state
    float m_panelZ = -5.0f;
    AABB m_table;
    Slider1D m_sliderFOV;
    Button m_buttonReset;

    //inputs
    glm::vec2 m_mousePos{0};
    bool m_leftDown = false;

    //drawing
    GLuint m_shader = 0;
    GLuint m_unitWireVAO = 0, m_unitWireVBO = 0; //wireframe unit cube

    inline void layout(){
        glm::vec3 center = {0.0f, 0.80f, m_panelZ};
        glm::vec3 half = {0.60f, 0.03f, 0.30f};
        m_table = makeAABB(center, half);

        m_sliderFOV.base = { -0.25f, 0.85f, m_panelZ };
        m_sliderFOV.axis = glm::normalize(glm::vec3{1,0,0});
        m_sliderFOV.halfLen = 0.22f;
        m_sliderFOV.minVal = 30.f;
        m_sliderFOV.maxVal = 100.f;
        m_sliderFOV.setFrom01(0.5f);

        m_buttonReset.topCenter = { 0.25f, 0.85f, m_panelZ };
        m_buttonReset.half = { 0.03f, 0.012f, 0.03f };
        m_buttonReset.maxDepth = 0.012f;
    }

    inline void ensureWireCube(){
        if (m_unitWireVAO) return;
        const glm::vec3 c[8] = {
            {-1,-1,-1},{+1,-1,-1},{+1,+1,-1},{-1,+1,-1},
            {-1,-1,+1},{+1,-1,+1},{+1,+1,+1},{-1,+1,+1}
        };
        std::vector<glm::vec3> lines = {
            c[0],c[1], c[1],c[2], c[2],c[3], c[3],c[0],
            c[4],c[5], c[5],c[6], c[6],c[7], c[7],c[4],
            c[0],c[4], c[1],c[5], c[2],c[6], c[3],c[7]
        };
        glGenVertexArrays(1, &m_unitWireVAO);
        glGenBuffers(1, &m_unitWireVBO);
        glBindVertexArray(m_unitWireVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_unitWireVBO);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(lines.size()*sizeof(glm::vec3)), lines.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(glm::vec3),(void*)0);
        glBindVertexArray(0);
    }

    inline void drawAABB(const AABB& b, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& color){
        glm::vec3 c = (b.min + b.max) * 0.5f;
        glm::vec3 s = (b.max - b.min) * 0.5f;     //half-extents
        glm::mat4 M = glm::translate(glm::mat4(1), c) * glm::scale(glm::mat4(1), s);
        glm::mat4 MV = view * M;
        glUseProgram(m_shader);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProjectionMatrix"),1,GL_FALSE,glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uModelViewMatrix"),1,GL_FALSE,glm::value_ptr(MV));
        glUniform3fv(glGetUniformLocation(m_shader,"uColor"),1,glm::value_ptr(color));
        glBindVertexArray(m_unitWireVAO);
        glDrawArrays(GL_LINES, 0, 24);
        glBindVertexArray(0);
    }

    struct Ray {glm::vec3 o, d; };
    static inline glm::vec2 mouseToNDC(const glm::vec2& mouse, int w, int h){
        return {(mouse.x/float(w))*2.f - 1.f, 1.f - (mouse.y/float(h))*2.f };
    }
    static inline Ray makeRay(const glm::mat4& proj, const glm::mat4& view, const glm::vec2& ndc){
        glm::mat4 invVP = glm::inverse(proj * view);
        glm::vec4 np = invVP * glm::vec4(ndc.x, ndc.y, -1.f, 1.f);
        glm::vec4 fp = invVP * glm::vec4(ndc.x, ndc.y,  1.f, 1.f);
        np /= np.w; fp /= fp.w;
        return { glm::vec3(np), glm::normalize(glm::vec3(fp-np)) };
    }
    static inline bool rayPlaneZ(const Ray& r, float z, glm::vec3& hit){
        if (std::abs(r.d.z) < 1e-5f){
            return false;
        }
        float t = (z - r.o.z) / r.d.z;
        if (t < 0){
            return false;
        }
        hit = r.o + r.d * t;
        return true;
    }
};

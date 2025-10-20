#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <GL/glew.h>
#include "cgra/cgra_gui.hpp"
#include <imgui.h>

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


struct Links {
    float *amp = nullptr; // [0.1, 50]
    float *freq = nullptr;// [0.01, 1.0]
    int *octaves = nullptr;// [1, 8]
    float *persistence = nullptr; // [0.1, 1.0]
    float *lacunarity = nullptr; // [1.5, 4.0]
    float *minHeight = nullptr; // [-10, 0]
    bool *showClouds = nullptr; // toggle
    bool *showTrees  = nullptr; // toggle
};

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

inline void raiseZ(Slider1D& s, float dz){
    s.base.z += dz;
    s.setFrom01(s.value01);
}

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
    void bind(const Links &lnk){
        m_links = lnk;
        //initialize sliders from current values if pointersvalid
        if (m_links.amp){
            m_sAmp.minVal=0.1f, m_sAmp.maxVal=50.f, m_sAmp.setFrom01((*m_links.amp -m_sAmp.minVal)/(m_sAmp.maxVal-m_sAmp.minVal));
        }
        if (m_links.freq){
            m_sFreq.minVal=0.01f,m_sFreq.maxVal=1.0f, m_sFreq.setFrom01((*m_links.freq-m_sFreq.minVal)/(m_sFreq.maxVal-m_sFreq.minVal));
        }
        if (m_links.octaves){
            m_sOct.minVal=1.f,m_sOct.maxVal=8.f,m_sOct.setFrom01((float(*m_links.octaves)-m_sOct.minVal)/(m_sOct.maxVal-m_sOct.minVal));
        }
        if (m_links.persistence){
            m_sPers.minVal=0.1f, m_sPers.maxVal=1.0f,m_sPers.setFrom01((*m_links.persistence-m_sPers.minVal)/(m_sPers.maxVal-m_sPers.minVal));
        }
        if (m_links.lacunarity){
            m_sLac.minVal=1.5f, m_sLac.maxVal=4.0f,m_sLac.setFrom01((*m_links.lacunarity-m_sLac.minVal)/(m_sLac.maxVal-m_sLac.minVal));
        }
        if (m_links.minHeight){
            m_sMin.minVal=-10.f, m_sMin.maxVal=0.0f,m_sMin.setFrom01((*m_links.minHeight-m_sMin.minVal)/(m_sMin.maxVal-m_sMin.minVal));
        }
    }
    
    const std::string& hoverText() const {
        return m_hover;
    }
    
    inline void init(GLuint colorShader){
        m_shader = colorShader;
        layout();
        ensureWireCube();
    }
    
    inline void resetAll(PovCamera& camera) {
        //Camera
        camera.reset();

        //sliders
        m_sliderFOV.setFrom01(m_def.fov_t01);

        m_sAmp.setFrom01(to01(m_def.amp,m_sAmp.minVal, m_sAmp.maxVal));
        m_sFreq.setFrom01(to01(m_def.freq, m_sFreq.minVal, m_sFreq.maxVal));
        m_sOct.setFrom01( to01(float(m_def.octaves), m_sOct.minVal, m_sOct.maxVal));
        m_sPers.setFrom01(to01(m_def.persistence,m_sPers.minVal, m_sPers.maxVal));
        m_sLac.setFrom01( to01(m_def.lacunarity, m_sLac.minVal, m_sLac.maxVal));
        m_sMin.setFrom01( to01(m_def.minHeight, m_sMin.minVal, m_sMin.maxVal));

        //values back to bound pointers
        auto w = [&](float* p, const Slider1D& s){ if (p) *p = s.mapped(); };
        w(m_links.amp, m_sAmp);
        w(m_links.freq,m_sFreq);
        w(m_links.persistence, m_sPers);
        w(m_links.lacunarity, m_sLac);
        w(m_links.minHeight, m_sMin);
        if (m_links.octaves){
            *m_links.octaves = int(std::round(m_sOct.mapped()));
        }

        if (m_links.showClouds){
            *m_links.showClouds = m_def.showClouds;
        }
        if (m_links.showTrees){
            *m_links.showTrees  = m_def.showTrees;
        }

        //no slider is stuck
        releaseAll();
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
            if (m_leftDown) {
                trySliderAtOwnZ(m_sliderFOV,ray);
                trySliderAtOwnZ(m_sAmp,ray);
                trySliderAtOwnZ(m_sFreq,ray);
                trySliderAtOwnZ(m_sOct, ray);
                trySliderAtOwnZ(m_sPers, ray);
                trySliderAtOwnZ(m_sLac, ray);
                trySliderAtOwnZ(m_sMin, ray);

            } else {
                releaseAll();
            }
        }
        
        auto pushingBtn = [&](const Button& b){
            glm::vec3 hitB;
            return pickOnPlane(ray, b.topCenter.z, hitB) && aabbContainsXY(b.aabb(), hitB) && m_leftDown;
        };

        m_buttonReset.step(pushingBtn(m_buttonReset), 1.f/60.f);
        m_bClouds.step(pushingBtn(m_bClouds), 1.f/60.f);
        m_bTrees.step(pushingBtn(m_bTrees),1.f/60.f);

        if (m_buttonReset.pressedEdge) {
            resetAll(camera);
            camera.reset();
            float t = (camera.fovDeg - m_sliderFOV.minVal) / (m_sliderFOV.maxVal - m_sliderFOV.minVal);
            m_sliderFOV.setFrom01(std::clamp(t, 0.f, 1.f));
        }

        
        camera.fovDeg = m_sliderFOV.mapped();
        
            
            
        //toggle buttons:

        if (m_bClouds.pressedEdge && m_links.showClouds){
            *m_links.showClouds = !*m_links.showClouds;
        }
 
        if (m_bTrees.pressedEdge && m_links.showTrees){
            *m_links.showTrees = !*m_links.showTrees;
        }
            
        //write slider values back
        auto w = [&](float* p, const Slider1D& s){ if (p) *p = s.mapped(); };
        w(m_links.amp,m_sAmp);
        w(m_links.freq,m_sFreq);
        w(m_links.persistence,m_sPers);
        w(m_links.lacunarity, m_sLac);
        w(m_links.minHeight, m_sMin);
        if (m_links.octaves){
            *m_links.octaves = int(std::round(m_sOct.mapped())); } // round to int
        
        //labels
        m_hover.clear(); 

        if (hoverSlider(m_sliderFOV, ray)) {
            m_hover = "Camera FOV";
        }
        else if (hoverSlider(m_sAmp, ray)) {
            m_hover = "Terrain Amplitude";
        }
        else if (hoverSlider(m_sFreq, ray)) {
            m_hover = "Terrain Frequency";
        }
        else if (hoverSlider(m_sOct, ray)) {
            m_hover = "Octaves";
        }
        else if (hoverSlider(m_sPers, ray)) {
            m_hover = "Persistence";
        }
        else if (hoverSlider(m_sLac, ray)) {
            m_hover = "Lacunarity";
        }
        else if (hoverSlider(m_sMin, ray)) {
            m_hover = "Min Height (Water Depth)";
        }
        else if (hoverButton(m_bClouds, ray)) {
            if (m_links.showClouds){
                m_hover = *m_links.showClouds ? "Toggle Clouds (ON)" : "Toggle Clouds (OFF)";
            }else{
                m_hover = "Toggle Clouds";
            }
        }
        else if (hoverButton(m_bTrees, ray)) {
            if (m_links.showTrees){
                m_hover = *m_links.showTrees ? "Toggle Trees (ON)" : "Toggle Trees (OFF)";
            }else{
                m_hover = "Toggle Trees";
            }
        }
        else if (hoverButton(m_buttonReset, ray)) {
            m_hover = "Reset (camera + controls)";
        }
        
        //draw
        drawAABB(m_table,view, proj, {0.35f,0.35f,0.4f});
        drawAABB(m_sliderFOV.aabb(),view, proj, {0.2f,0.8f,0.9f});
        drawAABB(m_buttonReset.aabb(),view, proj, {0.9f,0.7f,0.2f});
        
        //new
        drawAABB(m_sAmp.aabb(), view, proj, {0.10f,0.70f,0.95f});
        drawAABB(m_sFreq.aabb(), view, proj, {0.10f,0.85f,0.30f});
        drawAABB(m_sOct.aabb(), view, proj, {0.95f,0.85f,0.10f});
        drawAABB(m_sPers.aabb(),view, proj, {0.80f,0.40f,0.95f});
        drawAABB(m_sLac.aabb(),view, proj, {0.95f,0.55f,0.25f});
        drawAABB(m_sMin.aabb(),view, proj, {0.55f,0.65f,0.95f});
        
        // toggle buttons
        drawAABB(m_bClouds.aabb(), view, proj, (m_links.showClouds && *m_links.showClouds)? glm::vec3{0.1f,0.8f,0.3f}:glm::vec3{0.4f,0.4f,0.4f});
        drawAABB(m_bTrees.aabb(),  view, proj, (m_links.showTrees  && *m_links.showTrees )? glm::vec3{0.1f,0.8f,0.3f}:glm::vec3{0.4f,0.4f,0.4f});
        
    }

    inline void setPanelZ(float z) {
        m_panelZ = z;
        layout();
    }

private:
    //state
    Links m_links{};
    float m_panelZ = -5.0f;
    AABB m_table;
    //Default sliders
    Slider1D m_sliderFOV;
    Button m_buttonReset;
    std::string m_hover;
        
    //Terrain sliders
    Slider1D m_sAmp, m_sFreq, m_sOct, m_sPers, m_sLac, m_sMin;
        
    //additional toggles
    Button m_bClouds, m_bTrees;

    //inputs
    glm::vec2 m_mousePos{0};
    bool m_leftDown = false;

    //drawing
    GLuint m_shader = 0;
    GLuint m_unitWireVAO = 0, m_unitWireVBO = 0; //wireframe unit cube
    
    //ray
    struct Ray {glm::vec3 o, d; };
    
    struct Defaults {
        float fov_t01   = 0.5f;  // FOV slider default
        float amp       = 10.0f;
        float freq      = 0.20f;
        int   octaves   = 4;
        float persistence = 0.5f;
        float lacunarity  = 2.5f;
        float minHeight   = -2.0f;
        bool  showClouds  = false;
        bool  showTrees   = true;
    } m_def;
    
    inline bool hoverSlider(const Slider1D& s, const Ray& ray) const {
        glm::vec3 hit;
        if (!pickOnPlane(ray, s.base.z, hit)){
            return false;
        }
        bool overHandle = aabbContainsXY(s.aabb(), hit);
        bool overTrack  = (hit.x >= s.base.x - s.halfLen && hit.x <= s.base.x + s.halfLen && std::abs(hit.y - s.base.y) <= 0.03f);
        return overHandle || overTrack;
    }

    inline bool hoverButton(const Button& b, const Ray& ray) const {
        glm::vec3 hit;
        return pickOnPlane(ray, b.topCenter.z, hit) && aabbContainsXY(b.aabb(), hit);
    }
    
    inline static float to01(float v, float minV, float maxV) {
        return (v - minV) / (maxV - minV);
    }
    
    inline bool anyGrabbed() const {
        return m_sliderFOV.grabbed || m_sAmp.grabbed || m_sFreq.grabbed || m_sOct.grabbed || m_sPers.grabbed || m_sLac.grabbed || m_sMin.grabbed;
    }

    inline bool pickOnPlane(const Ray& ray, float z, glm::vec3& hit) const {
        if (std::abs(ray.d.z) < 1e-6f){
            return false;
        }
        float t = (z - ray.o.z) / ray.d.z;
        if (t < 0){
            return false;
        }
        hit = ray.o + ray.d * t;
        return true;
    }
    
    inline void trySliderAtOwnZ(Slider1D &s, const Ray& ray){
        glm::vec3 hit;
        if (!pickOnPlane(ray, s.base.z, hit)) {
            s.grabbed = false; return;
        }

        bool overHandle = aabbContainsXY(s.aabb(), hit);
        bool overTrack  = (hit.x >= s.base.x - s.halfLen && hit.x <= s.base.x + s.halfLen && std::abs(hit.y - s.base.y) <= 0.03f);

        if (!m_leftDown) {
            s.grabbed = false; return;
        }

        // start grab only if none already grabbed (prevents 2 sliders moving together)
        if (!anyGrabbed() && (overHandle || overTrack)){
            s.grabbed = true;
        }

        if (s.grabbed){
            s.placeFromPoint(hit);
        }
    }
    
    inline void releaseAll() {
        m_sliderFOV.grabbed =false;
        m_sAmp.grabbed = false;
        m_sFreq.grabbed = false;
        m_sOct.grabbed = false;
        m_sPers.grabbed = false;
        m_sLac.grabbed =false;
        m_sMin.grabbed = false;
    }
    
    inline void placeSlider(Slider1D& s, float x, float y, float z, float minV, float maxV, float t01){
        s.base = {x, y, z};
        s.axis = glm::normalize(glm::vec3{1,0,0});
        s.halfLen= 0.22f;
        s.minVal = minV;
        s.maxVal = maxV;
        s.setFrom01(t01);
    }

        
    inline bool handleSlider(Slider1D &s, const glm::vec3 &hit){
        bool overHandle = aabbContainsXY(s.aabb(),hit);
        bool overTrack = (hit.x >= s.base.x - s.halfLen && hit.x <= s.base.x + s.halfLen && std::abs(hit.y - s.base.y) <= 0.03f);
        if (!m_leftDown){
            s.grabbed = false; return false;
        }
        if (!s.grabbed && (overHandle || overTrack)){
            s.grabbed = true;
        }
        if (s.grabbed){
            s.placeFromPoint(hit);
        }
        return s.grabbed;
    }
    
    inline void placeButton(Button& b, float x, float y, float z, glm::vec3 half = {0.02f, 0.012f, 0.02f}, float maxDepth = 0.012f){
        b.topCenter = {x, y, z};
        b.half= half;
        b.maxDepth = maxDepth;
        b.depth = 0.f;
        b.vel = 0.f;
        b.isDown = false;
        b.pressedEdge = false;
    }

    inline void layout(){
        glm::vec3 center = {0.0f, 9.80f, m_panelZ};
        glm::vec3 half = {0.60f, 0.03f, 0.30f};
        m_table = makeAABB(center, half);

        m_sliderFOV.base = { -0.25f, 9.85f, m_panelZ };
        m_sliderFOV.axis = glm::normalize(glm::vec3{1,0,0});
        m_sliderFOV.halfLen = 0.22f;
        m_sliderFOV.minVal = 30.f;
        m_sliderFOV.maxVal = 100.f;
        m_sliderFOV.setFrom01(0.5f);
        
        
        //Additional

        //Row 1: Amp, Freq, Octaves
        float x = -0.25f, y = 9.85f;
        float z0 = m_panelZ + 0.05f;   // starting plane
        float z02 = m_panelZ - 0.05f;
        float dz = 0.05f;              // equal step in Z

        placeSlider(m_sAmp, x, y, z0 + 0*dz, 0.1f, 50.f, 0.2f);
        placeSlider(m_sFreq,x, y, z0 + 1*dz, 0.01f, 1.0f, 0.1f);
        placeSlider(m_sOct,x, y, z0 + 2*dz, 1.f, 8.f, 0.5f);


        //Row 2: Persistence, Lacunarity, MinHeight
        placeSlider(m_sPers, x, y, z02 - 0*dz, 0.1f, 1.0f, 0.5f);
        placeSlider(m_sLac,x, y, z02 - 1*dz, 1.5f, 4.0f, 0.5f);
        placeSlider(m_sMin,x, y, z02 - 2*dz, -10.f, 0.0f, 0.5f);
    

        //buttons clouds, trees
        float by = 9.85f;         // same Y row as FOV (or below if you want)
        float bz = m_panelZ;      // same plane as panel (keeps picking simple)
        float startX = 0.05f;    // leftmost button x
        float dx = 0.12f;         // horizontal spacing
        
        
        placeButton(m_bClouds, startX + 0*dx, by, bz);
        placeButton(m_bTrees,  startX + 1*dx, by, bz);
        placeButton(m_buttonReset, startX + 2*dx, by, bz, {0.03f,0.012f,0.03f}); // big button
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

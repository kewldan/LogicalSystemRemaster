#pragma once

#include "Shader.h"
#include "Camera2D.h"
#include <functional>

const float screenVertices[] = {
        // positions        // texture Coords
        -1.0f, 1.0f, 0.f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.f, 1.0f, 1.0f,
        1.0f, -1.0f, 0.f, 1.0f, 0.0f,
};

const float quadVertices[] = {
        0.0f, 1.0f, 0.f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.f, 1.0f, 0.0f,
};

class RenderPipeline {
private:
    unsigned int pingpongFBO[2]{};
    unsigned int pingpongBuffer[2]{};
    Engine::Shader *gShader, *blurShader, *finalShader, *backgroundShader, *selectionShader, *glowShader;
    unsigned int FBO{}, gAlbedo{}, screenVAO{}, screenVBO{}, quadVAO{}, quadVBO{}, intermediateFBO{}, screenTexture{};
public:
    int w, h;
    glm::vec3 blockDefaultColor{}, blockGlowColor{};
    bool bloom;

    RenderPipeline(Engine::Shader *blockShader, Engine::Shader *blurShader, Engine::Shader *finalShader,
                   Engine::Shader *backgroundShader, Engine::Shader *selectionShader, Engine::Shader *glowShader, int width, int height);

    void resize(int nw, int nh);

    void beginPass(Engine::Camera2D *camera, unsigned int atlas, unsigned int blockVao,
                   const std::function<void()> &drawFunction);

    void drawSelection(Engine::Camera2D *camera, glm::vec2 position, glm::vec2 size) const;

    void drawScreenQuad() const;

    void drawRectQuad() const;
};
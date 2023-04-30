#include "RenderPipeline.h"

RenderPipeline::RenderPipeline(Engine::Shader *blockShader, Engine::Shader *blurShader, Engine::Shader *finalShader,
                               Engine::Shader *backgroundShader, Engine::Shader *selectionShader,
                               Engine::Shader *glowShader, int width, int height) : gShader(blockShader),
                                                                                    blurShader(blurShader),
                                                                                    finalShader(finalShader),
                                                                                    backgroundShader(backgroundShader),
                                                                                    selectionShader(selectionShader),
                                                                                    glowShader(glowShader) {
    w = width;
    h = height;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, gAlbedo);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 16, GL_RGBA16F, w, h, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, gAlbedo, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        PLOGE << "Framebuffer not complete!";
    }

    glGenFramebuffers(1, &intermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture,
                           0);    // we only need a color buffer

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        PLOGE << "Intermediate framebuffer is not complete!";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    blockDefaultColor = glm::vec3(0.92, 0.31, 0.2);
    blockGlowColor = glm::vec3(0.33, 0.9, 0.27);

    glGenVertexArrays(1, &screenVAO);
    glGenBuffers(1, &screenVBO);
    glBindVertexArray(screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenVertices), &screenVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));


    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongBuffer);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffer[i]);
        glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongBuffer[i], 0
        );
    }
}

void RenderPipeline::resize(int nw, int nh) {
    w = nw;
    h = nh;

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, gAlbedo);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 16, GL_RGBA16F, w, h, GL_TRUE);

    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);

    for (unsigned int i: pingpongBuffer) {
        glBindTexture(GL_TEXTURE_2D, i);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::beginPass(Engine::Camera *camera, bool bloom, unsigned int atlas, unsigned int blockVao,
                               const std::function<void(Engine::Shader *)> &useFunction,
                               const std::function<void(Engine::Shader *)> &glowFunction) {
    glViewport(0, 0, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    backgroundShader->bind();
    static const glm::mat4 backgroundMvp = glm::translate(glm::mat4(1), glm::vec3(0.f, 0.f, -0.3f));
    backgroundShader->upload("mvp", backgroundMvp);
    backgroundShader->upload("horizontal", glm::vec2(camera->left, camera->right));
    backgroundShader->upload("vertical", glm::vec2(camera->bottom, camera->top));
    backgroundShader->upload("screen", glm::vec2(camera->window->width, camera->window->height));
    backgroundShader->upload("offset", glm::vec2(camera->position));
    drawScreenQuad();
    gShader->bind();
    gShader->upload("proj", camera->getOrthographic());
    gShader->upload("view", camera->getView());
    gShader->upload("tex", 0);
    static const glm::vec3 selectionColor = glm::vec3(1.3f, 1.2f, 1.8f);
    glm::vec3 color = selectionColor * ((std::sinf((float) glfwGetTime() * 6.f) + 1) * 0.2f + 0.8f);
    gShader->upload("selectionColor", color);
    gShader->upload("ON", blockGlowColor);
    gShader->upload("OFF", blockDefaultColor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);

    glBindVertexArray(blockVao);
    useFunction(gShader);

    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!bloom) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        bool horizontal = true, first_iteration = true;
        glowShader->bind();
        glowShader->upload("proj", camera->getOrthographic());
        glowShader->upload("view", camera->getView());
        glowShader->upload("tex", 0);
        glowShader->upload("ON", blockGlowColor);
        glowShader->upload("OFF", blockDefaultColor);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);

        glBindVertexArray(blockVao);
        glowFunction(glowShader);
        blurShader->bind();
        blurShader->upload("image", 0);
        for (int i = 0; i < 12; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader->upload("horizontal", horizontal);
            glBindTexture(
                    GL_TEXTURE_2D, pingpongBuffer[!horizontal]
            );
            drawScreenQuad();

            horizontal ^= 1;
            if (first_iteration)
                first_iteration = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        finalShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffer[!horizontal]);
        finalShader->upload("scene", 0);
        finalShader->upload("bloomBlur", 1);
        drawScreenQuad();
    }
}

void RenderPipeline::drawSelection(Engine::Camera *camera, glm::vec2 position, glm::vec2 size) const {
    selectionShader->bind();
    selectionShader->upload("proj", camera->getOrthographic());
    glm::mat4 mvp = glm::translate(glm::mat4(1), glm::vec3(position, -0.1f));
    selectionShader->upload("mvp", glm::scale(mvp, glm::vec3(size, 1.f)));
    drawRectQuad();
}

void RenderPipeline::drawScreenQuad() const {
    glBindVertexArray(screenVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void RenderPipeline::drawRectQuad() const {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


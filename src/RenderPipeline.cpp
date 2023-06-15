#include "RenderPipeline.h"

RenderPipeline::RenderPipeline(Engine::Shader *blockShader, Engine::Shader *blurShader, Engine::Shader *finalShader,
                               Engine::Shader *backgroundShader, Engine::Shader *selectionShader,
                               int width, int height) : gShader(blockShader),
                                                        blurShader(blurShader),
                                                        finalShader(finalShader),
                                                        backgroundShader(backgroundShader),
                                                        selectionShader(selectionShader) {
    w = width;
    h = height;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedo, 0);

    glGenTextures(1, &gAlbedoHDR);
    glBindTexture(GL_TEXTURE_2D, gAlbedoHDR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gAlbedoHDR, 0);


    unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        PLOGE << "Framebuffer not complete!";
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

    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, gAlbedoHDR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);

    for (unsigned int i: pingpongBuffer) {
        glBindTexture(GL_TEXTURE_2D, i);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::beginPass(Engine::Camera2D *camera, unsigned int atlas, unsigned int blockVao,
                               const std::function<void()> &drawFunction) {
    glViewport(0, 0, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    backgroundShader->bind();
    backgroundShader->upload("horizontal", glm::vec2(camera->left, camera->right));
    backgroundShader->upload("vertical", glm::vec2(camera->bottom, camera->top));
    backgroundShader->upload("offset", glm::vec2(camera->position) + 16.f);
    drawScreenQuad();

    gShader->bind();
    gShader->upload("proj", camera->getProjection());
    gShader->upload("view", camera->getView());
    gShader->upload("tex", 0);
    static const glm::vec3 selectionColor = glm::vec3(1.3f, 1.2f, 1.8f);
    glm::vec3 color = selectionColor * ((sinf((float) glfwGetTime() * 6.f) + 1) * 0.2f + 0.8f);
    gShader->upload("selectionColor", color);
    gShader->upload("ON", blockGlowColor);
    gShader->upload("OFF", blockDefaultColor);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);
    glBindVertexArray(blockVao);
    drawFunction();

    bool horizontal = true, first_iteration = true;
    if (bloom) {
        blurShader->bind();
        blurShader->upload("image", 0);
        for (int i = 0; i < 8; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader->upload("horizontal", horizontal);
            glBindTexture(
                    GL_TEXTURE_2D, first_iteration ? gAlbedoHDR : pingpongBuffer[!horizontal]
            );
            drawScreenQuad();

            horizontal ^= 1;
            if (first_iteration)
                first_iteration = false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    finalShader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpongBuffer[!horizontal]);
    finalShader->upload("scene", 0);
    finalShader->upload("bloomBlur", 1);
    finalShader->upload("bloom", bloom ? 1.0f : 0.0f);
    drawScreenQuad();
}

void RenderPipeline::drawSelection(Engine::Camera2D *camera, glm::vec2 position, glm::vec2 size) const {
    selectionShader->bind();
    selectionShader->upload("proj", camera->getProjection());
    glm::mat4 mvp = glm::translate(glm::mat4(1), glm::vec3(position, -0.1f));
    selectionShader->upload("mvp", glm::scale(mvp, glm::vec3(size, 1.f)));
    selectionShader->upload("size", size);
    selectionShader->upload("width", (camera->right - camera->left) / 640.f);
    selectionShader->upload("color", glm::vec4(0.01, 0.6, 1, 0.1));
    selectionShader->upload("borderColor", glm::vec4(0.015, 0.6, 1, 1));
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


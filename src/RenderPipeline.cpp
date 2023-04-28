#include "RenderPipeline.h"

RenderPipeline::RenderPipeline(const char *gShaderPath, const char *blurShaderPath, const char *finalShaderPath,
                               const char *backgroundShaderPath, const char *selectionShaderPath, int width,
                               int height) {
    w = width;
    h = height;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedo, 0);

    glGenTextures(1, &gAlbedoHDR);
    glBindTexture(GL_TEXTURE_2D, gAlbedoHDR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
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
    gShader = new Engine::Shader(gShaderPath);
    blurShader = new Engine::Shader(blurShaderPath);
    finalShader = new Engine::Shader(finalShaderPath);
    backgroundShader = new Engine::Shader(backgroundShaderPath);
    selectionShader = new Engine::Shader(selectionShaderPath);

    glGenVertexArrays(1, &screenVAO);
    glGenBuffers(1, &screenVBO);
    glBindVertexArray(screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenVertices), &screenVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));


    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongBuffer);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffer[i]);
        glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);

    glBindTexture(GL_TEXTURE_2D, gAlbedoHDR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);

    for (unsigned int i : pingpongBuffer) {
        glBindTexture(GL_TEXTURE_2D, i);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::beginPass(Engine::Camera *camera, bool bloom, unsigned int atlas, unsigned int blockVao,
                               const std::function<void(Engine::Shader *)>& useFunction) {
    glViewport(0, 0, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    backgroundShader->bind();
    static const glm::mat4 backgroundMvp = glm::translate(glm::mat4(1), glm::vec3(0.f, 0.f, -0.3f));
    backgroundShader->upload("mvp", backgroundMvp);
    backgroundShader->upload("offset", glm::vec2(camera->position));
    drawScreenQuad();
    gShader->bind();
    gShader->upload("proj", camera->getOrthographic());
    gShader->upload("view", camera->getView());
    gShader->upload("tex", 0);
    gShader->upload("selectionColor", glm::vec3(1.3, 1.2, 1.8));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);

    glBindVertexArray(blockVao);
    useFunction(gShader);
    bool horizontal = true, first_iteration = true;
    if (bloom) {
        blurShader->bind();
        blurShader->upload("image", 0);
        for (int i = 0; i < 12; i++) {
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
    finalShader->upload("bloom", bloom);
    finalShader->upload("scene", 0);
    finalShader->upload("bloomBlur", 1);
    static const glm::mat4 finalMvp = glm::translate(glm::mat4(1), glm::vec3(0.f, 0.f, -0.5f));
    finalShader->upload("mvp", finalMvp);
    drawScreenQuad();
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

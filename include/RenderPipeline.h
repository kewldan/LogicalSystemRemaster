#pragma once

#include "Shader.h"
#include <functional>

const float screenVertices[] = {
	// positions        // texture Coords
	-1.0f,  1.0f, 0.f, 0.0f, 1.0f,
	-1.0f, -1.0f, 0.f, 0.0f, 0.0f,
	 1.0f,  1.0f, 0.f, 1.0f, 1.0f,
	 1.0f, -1.0f, 0.f, 1.0f, 0.0f,
};

const float quadVertices[] = {
	0.0f, 1.0f, 0.f, 0.0f, 1.0f,
	0.0f, 0.0f, 0.f, 0.0f, 0.0f,
	1.0f, 1.0f, 0.f, 1.0f, 1.0f,
	1.0f, 0.0f, 0.f, 1.0f, 0.0f,
};

class RenderPipeline {
public:
	int w, h;
	unsigned int pingpongFBO[2];
	unsigned int pingpongBuffer[2];
	Engine::Shader* gShader, *blurShader, *finalShader, * backgroundShader, *selectionShader;
	unsigned int FBO, gAlbedo, gAlbedoHDR, screenVAO, screenVBO, quadVAO, quadVBO;
	RenderPipeline(const char* gShaderPath, const char* blurShaderPath, const char* finalShaderPath, const char* backgroundShaderPath, const char* selectionShaderPath, int width, int height);
	void resize(int nw, int nh);
	void beginPass(Engine::Camera* camera, bool bloom, std::function<void(Engine::Shader*)> useFunction);
	void drawSelection(Engine::Camera* camera, glm::vec2 position, glm::vec2 size);
	void drawScreenQuad();
	void drawRectQuad();
};
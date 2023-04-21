#pragma once

#include "Shader.h"

const float screenVertices[] = {
	// positions        // texture Coords
	-1.0f,  1.0f, 0.f, 0.0f, 1.0f,
	-1.0f, -1.0f, 0.f, 0.0f, 0.0f,
	 1.0f,  1.0f, 0.f, 1.0f, 1.0f,
	 1.0f, -1.0f, 0.f, 1.0f, 0.0f,
};

class RenderPipeline {
public:
	int w, h;
	unsigned int pingpongFBO[2];
	unsigned int pingpongBuffer[2];
	Shader* gShader, *blurShader, *finalShader, * backgroundShader;
	unsigned int FBO, gAlbedo, gAlbedoHDR, VAO, VBO;
	RenderPipeline(const char* gShaderPath, const char* blurShaderPath, const char* finalShaderPath, const char* backgroundShaderPath, int width, int height);
	void resize(int nw, int nh);
	Shader* beginPass(Camera* camera);
	void endPass(int amount, bool bloom);
	void drawScreenQuad();
};
#pragma once

#include "glad/glad.h"
#include <string>
#include "plog/Log.h"

#include "stb_image.h"

class Texture {
	unsigned int texture;
public:
	int width, height, nrChannels;
	Texture(const char* filename);
	~Texture();

	void nearest();
	void bind() const;
};

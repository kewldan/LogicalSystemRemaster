#include "Texture.h"

Engine::Texture::Texture(const char* filename) {
	glGenTextures(1, &texture);

	bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	char* f = new char[256];
	strcpy_s(f, 256, "./data/textures/");
	strcat_s(f, 256, filename);

	unsigned char* data = stbi_load(f, &width, &height, &nrChannels, 0);
	if (data) {
		assert((void("Image channels must be 3 or 4!"), nrChannels == 3 || nrChannels == 4));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, nrChannels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE,
			data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glObjectLabelBuild(GL_TEXTURE, texture, "Texture", filename);

		PLOGI << "Texture [" << filename << "] loaded (" << width << "x" << height << ")";
	}
	else {
		PLOGE << "Failed to load texture [" << filename << "]";
		PLOGE << stbi_failure_reason();
	}
	stbi_image_free(data);
}

Engine::Texture::~Texture()
{
	glDeleteTextures(1, &texture);
}

void Engine::Texture::nearest()
{
	bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Engine::Texture::bind() const {
	glBindTexture(GL_TEXTURE_2D, texture);
}

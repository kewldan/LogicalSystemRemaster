#pragma once

#include "glad/glad.h"
#include <string>
#include "plog/Log.h"
#include "File.h"
#include "stb_image.h"

namespace Engine {
    class Texture {
        unsigned int texture{};
    public:
        int width{}, height{}, nrChannels{};

        explicit Texture(const char *filename);

        ~Texture();

        void nearest() const;

        void bind() const;

        static unsigned char *loadImage(const char *path, int *w, int *h);
    };
}

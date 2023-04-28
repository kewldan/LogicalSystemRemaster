#pragma once

#include "glad/glad.h"
#include "Camera.h"
#include <map>
#include <plog/Log.h>
#include <unordered_map>
#include "File.h"

namespace Engine {
    class UniformBlock {
        unsigned int offset;
    public:
        unsigned int block{};

        explicit UniformBlock(unsigned int size);

        void add(unsigned int size, void *value);
    };

    typedef std::unordered_map<const char *, int> Uniforms;

    class Shader {
        unsigned int vertex, fragment, geometry, program, blockIndex;
        Uniforms *uniforms;
        int8_t shaderParts;
        const char *filename;

        int getUniformLocation(const char *name) const;

        int getAttribLocation(const char *name) const;

        int loadShader(const char *path, int type, char bitshift);

    public:
        explicit Shader(const char *filename);

        ~Shader();

        unsigned int getProgramId() const;

        void bind() const;

        void upload(const char *name, int value) const;

        void upload(const char *name, float value) const;

        void upload(const char *name, glm::vec2 value) const;

        void upload(const char *name, glm::vec3 value) const;

        void upload(const char *name, glm::vec4 value) const;

        void upload(const char *name, glm::mat4 value) const;

        static char *getElementName(const char *name, int index);

        void uploadMat4(const char *name, float *value) const;

        void bindUniformBlock(const char *name);
    };
}
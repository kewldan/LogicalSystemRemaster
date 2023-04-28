#pragma once

#include "Texture.h"
#include "glm/glm.hpp"
#include <glm/gtx/transform.hpp>
#include <functional>

#define BlockRotation unsigned char

const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1), glm::vec3(32, 32, 1));
const glm::mat4 rotationMatrices[] = {
        glm::translate(glm::rotate(scaleMatrix, glm::radians(0.f), glm::vec3(0.f, 0.f, 1.f)), glm::vec3(-0.5, -0.5, 0)),
        glm::translate(glm::rotate(scaleMatrix, glm::radians(-90.f), glm::vec3(0.f, 0.f, 1.f)),
                       glm::vec3(-0.5, -0.5, 0)),
        glm::translate(glm::rotate(scaleMatrix, glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f)),
                       glm::vec3(-0.5, -0.5, 0)),
        glm::translate(glm::rotate(scaleMatrix, glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f)), glm::vec3(-0.5, -0.5, 0))
};

class BlockType {
public:
    int id{};
    std::function<bool(int)> isActive;

    BlockType();

    BlockType(int id, const std::function<bool(int)> &func);
};

BlockRotation rotateBlock(BlockRotation r, int k);

#define Block_X(l) static_cast<int>(l >> 32)
#define Block_Y(l) static_cast<int>(l & 0xFFFFFFFFL)

long long Block_TO_LONG(int x, int y);

class Block {
public:
    int connections{};
    BlockRotation rotation{};
    bool active{}, selected{};
    BlockType *type;
    glm::mat4 mvp{};

    [[nodiscard]] glm::mat4 getMVP() const;

    Block(int x, int y, BlockType *type, BlockRotation rotation);

    Block(const char *buffer, BlockType *types, long long *pos);

    void write(char *buffer, long long pos);

    void updateMvp(int x, int y);
};
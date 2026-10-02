#pragma once

#include <3ds.h>
#include <citro3d.h>

namespace luckee {

class Level;
class Player;

class Renderer {
public:
    bool initialize();
    void shutdown();
    void render(const Level& level, const Player& player);

private:
    void renderLayer(const Level& level, int layer, const Player& player);
    void renderBlock(const Level& level, int x, int y, int z, int layer);
    void sendFace(int x, int y, int z, int face, float brightness);

    C3D_RenderTarget* target_ = nullptr;
    DVLB_s* shaderDvlb_ = nullptr;
    shaderProgram_s program_{};
    C3D_Mtx projection_{};
    int projectionLocation_ = -1;
    int modelViewLocation_ = -1;
    bool initialized_ = false;
};

} // namespace luckee

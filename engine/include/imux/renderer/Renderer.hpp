#pragma once
namespace imux {
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool initialize(int width, int height) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
};
}

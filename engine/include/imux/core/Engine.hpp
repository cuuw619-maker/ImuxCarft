#pragma once
#include <memory>
namespace imux {
struct EngineConfig { const char* title="ImuxCarft"; int width=1280; int height=720; bool vsync=true; };
class Engine { public: explicit Engine(EngineConfig config); ~Engine(); Engine(const Engine&)=delete; Engine& operator=(const Engine&)=delete; bool initialize(); void run(); void shutdown(); private: struct Impl; std::unique_ptr<Impl> impl_; };
}
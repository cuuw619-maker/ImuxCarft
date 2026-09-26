#include "imux/core/Engine.hpp"
#include "imux/renderer/Renderer.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>

namespace imux {
class OpenGLRenderer final : public Renderer {
public:
    bool initialize(int w,int h) override { resize(w,h); return true; }
    void resize(int w,int h) override {
        width_=std::max(w,1); height_=std::max(h,1); glViewport(0,0,width_,height_);
    }
    void beginFrame() override { glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); }
    void endFrame() override {}
private: int width_=1,height_=1;
};

struct Engine::Impl {
    explicit Impl(EngineConfig c):config(c){}
    EngineConfig config;
    SDL_Window* window=nullptr;
    SDL_GLContext context=nullptr;
    std::unique_ptr<Renderer> renderer;
    bool running=false;
};

Engine::Engine(EngineConfig c):impl_(std::make_unique<Impl>(c)){}
Engine::~Engine(){shutdown();}

bool Engine::initialize() {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){
        std::cerr<<"SDL_Init failed: "<<SDL_GetError()<<"\n"; return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    impl_->window=SDL_CreateWindow(impl_->config.title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        impl_->config.width,impl_->config.height,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!impl_->window){std::cerr<<"SDL_CreateWindow failed: "<<SDL_GetError()<<"\n"; SDL_Quit(); return false;}
    impl_->context=SDL_GL_CreateContext(impl_->window);
    if(!impl_->context){SDL_DestroyWindow(impl_->window); impl_->window=nullptr; SDL_Quit(); return false;}
    SDL_GL_SetSwapInterval(impl_->config.vsync?1:0);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f,0.10f,0.14f,1.0f);
    impl_->renderer=std::make_unique<OpenGLRenderer>();
    impl_->renderer->initialize(impl_->config.width,impl_->config.height);
    impl_->running=true;
    return true;
}

void Engine::run() {
    auto previous=std::chrono::steady_clock::now();
    while(impl_->running){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT) impl_->running=false;
            if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)
                impl_->renderer->resize(event.window.data1,event.window.data2);
        }
        auto now=std::chrono::steady_clock::now();
        [[maybe_unused]] float dt=std::chrono::duration<float>(now-previous).count();
        previous=now;
        impl_->renderer->beginFrame();
        impl_->renderer->endFrame();
        SDL_GL_SwapWindow(impl_->window);
    }
}

void Engine::shutdown(){
    if(!impl_) return;
    impl_->renderer.reset();
    if(impl_->context){SDL_GL_DeleteContext(impl_->context); impl_->context=nullptr;}
    if(impl_->window){SDL_DestroyWindow(impl_->window); impl_->window=nullptr;}
    SDL_Quit();
}
}

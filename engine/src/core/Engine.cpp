#include "imux/core/Engine.hpp"
#include "imux/renderer/Renderer.hpp"
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace imux {
namespace {
GLuint makeProgram(const char*vs,const char*fs){auto compile=[](GLenum t,const char*s){GLuint x=glCreateShader(t);glShaderSource(x,1,&s,nullptr);glCompileShader(x);return x;};GLuint a=compile(GL_VERTEX_SHADER,vs),b=compile(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();glAttachShader(p,a);glAttachShader(p,b);glLinkProgram(p);glDeleteShader(a);glDeleteShader(b);return p;}
GLuint loadPng(const std::filesystem::path& path){SDL_Surface*s=IMG_Load(path.string().c_str());if(!s){std::cerr<<IMG_GetError()<<"\n";return 0;}SDL_Surface*r=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(s);if(!r)return 0;GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r->w,r->h,0,GL_RGBA,GL_UNSIGNED_BYTE,r->pixels);SDL_FreeSurface(r);return t;}

class Menu{
 GLuint p_=0,vao_=0,vbo_=0,t_[6]{};
public:
 bool init(const std::filesystem::path&root){const char*v="#version 330 core\nlayout(location=0)in vec2 p;layout(location=1)in vec2 u;out vec2 uv;void main(){uv=u;gl_Position=vec4(p,0,1);}";const char*f="#version 330 core\nin vec2 uv;out vec4 c;uniform sampler2D tex;uniform float alpha;void main(){vec4 q=texture(tex,uv);c=vec4(q.rgb,alpha);}";p_=makeProgram(v,f);float q[]={-1,-1,0,1,1,-1,1,1,1,1,1,0,-1,1,0,0};glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);glVertexAttribPointer(0,2,GL_FLOAT,0,16,(void*)0);glEnableVertexAttribArray(0);glVertexAttribPointer(1,2,GL_FLOAT,0,16,(void*)8);glEnableVertexAttribArray(1);for(int i=0;i<6;i++)t_[i]=loadPng(root/"background"/("panorama_"+std::to_string(i)+".png"));return p_&&t_[0];}
 void draw(float time){int i=int(time*.12f)%6,n=(i+1)%6;float a=std::fmod(time*.12f,1.f);glDisable(GL_DEPTH_TEST);glUseProgram(p_);glBindVertexArray(vao_);glUniform1i(glGetUniformLocation(p_,"tex"),0);glUniform1f(glGetUniformLocation(p_,"alpha"),1);glBindTexture(GL_TEXTURE_2D,t_[i]);glDrawArrays(GL_TRIANGLE_FAN,0,4);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUniform1f(glGetUniformLocation(p_,"alpha"),a);glBindTexture(GL_TEXTURE_2D,t_[n]);glDrawArrays(GL_TRIANGLE_FAN,0,4);glDisable(GL_BLEND);glBindVertexArray(0);glUseProgram(0);}
 void quit(){for(auto&t:t_)if(t)glDeleteTextures(1,&t);if(vbo_)glDeleteBuffers(1,&vbo_);if(vao_)glDeleteVertexArrays(1,&vao_);if(p_)glDeleteProgram(p_);}
};

class World{
 GLuint p_=0;
public:
 bool init(){const char*v="#version 330 core\nlayout(location=0)in vec3 p;uniform mat4 m;void main(){gl_Position=m*vec4(p,1);}";const char*f="#version 330 core\nout vec4 c;uniform vec3 col;void main(){c=vec4(col,1);}";p_=makeProgram(v,f);return p_;}
 void draw(int w,int h,float time){glEnable(GL_DEPTH_TEST);glClearColor(.48f,.68f,.88f,1);float asp=float(w)/std::max(h,1);glUseProgram(p_);float m[16]={1/asp,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(p_,"m"),1,0,m);GLint c=glGetUniformLocation(p_,"col");for(int y=0;y<6;y++)for(int x=0;x<12;x++){float sx=.10f,sy=.10f,px=(x-5.5f)*sx*1.8f,py=-.45f+y*sy*1.5f+std::sin(time*.5f+x)*.002f;float q[]={px-sx,py-sy,0,px+sx,py-sy,0,px+sx,py+sy,0,px-sx,py-sy,0,px+sx,py+sy,0,px-sx,py+sy,0};GLuint a,b;glGenVertexArrays(1,&a);glGenBuffers(1,&b);glBindVertexArray(a);glBindBuffer(GL_ARRAY_BUFFER,b);glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STREAM_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,0,12,(void*)0);glEnableVertexAttribArray(0);glUniform3f(c,(y==5)?.20f:.34f,(y==5)?.62f:.25f,(y==5)?.18f:.12f);glDrawArrays(GL_TRIANGLES,0,6);glDeleteBuffers(1,&b);glDeleteVertexArrays(1,&a);}glUseProgram(0);}
 void quit(){if(p_)glDeleteProgram(p_);}
};
}
class OpenGLRenderer final:public Renderer{int w_=1,h_=1;public:bool initialize(int w,int h)override{resize(w,h);return true;}void resize(int w,int h)override{w_=std::max(w,1);h_=std::max(h,1);glViewport(0,0,w_,h_);}void beginFrame()override{glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}void endFrame()override{}int w()const{return w_;}int h()const{return h_;}};
struct Engine::Impl{explicit Impl(EngineConfig c):config(c){}EngineConfig config;SDL_Window*window=nullptr;SDL_GLContext context=nullptr;std::unique_ptr<Renderer>renderer;Menu menu;World world;bool running=false,game=false;float time=0;std::filesystem::path root;};
Engine::Engine(EngineConfig c):impl_(std::make_unique<Impl>(c)){}Engine::~Engine(){shutdown();}
bool Engine::initialize(){if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0)return false;if(!(IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG)){SDL_Quit();return false;}SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);impl_->window=SDL_CreateWindow(impl_->config.title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,impl_->config.width,impl_->config.height,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);if(!impl_->window)return false;impl_->context=SDL_GL_CreateContext(impl_->window);if(!impl_->context)return false;glewExperimental=GL_TRUE;if(glewInit()!=GLEW_OK)return false;SDL_GL_SetSwapInterval(impl_->config.vsync?1:0);impl_->renderer=std::make_unique<OpenGLRenderer>();impl_->renderer->initialize(impl_->config.width,impl_->config.height);impl_->root=std::filesystem::current_path();if(!impl_->menu.init(impl_->root)||!impl_->world.init())return false;impl_->running=true;return true;}
void Engine::run(){auto prev=std::chrono::steady_clock::now();while(impl_->running){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)impl_->running=false;if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)impl_->renderer->resize(e.window.data1,e.window.data2);if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE){if(impl_->game)impl_->game=false;else impl_->running=false;}if(e.type==SDL_KEYDOWN&&!impl_->game&&(e.key.keysym.sym==SDLK_RETURN||e.key.keysym.sym==SDLK_SPACE))impl_->game=true;if(e.type==SDL_MOUSEBUTTONDOWN&&!impl_->game&&e.button.button==SDL_BUTTON_LEFT)impl_->game=true;}auto now=std::chrono::steady_clock::now();impl_->time+=std::chrono::duration<float>(now-prev).count();prev=now;impl_->renderer->beginFrame();auto*r=static_cast<OpenGLRenderer*>(impl_->renderer.get());if(impl_->game)impl_->world.draw(r->w(),r->h(),impl_->time);else impl_->menu.draw(impl_->time);impl_->renderer->endFrame();SDL_GL_SwapWindow(impl_->window);}}
void Engine::shutdown(){if(!impl_)return;impl_->menu.quit();impl_->world.quit();impl_->renderer.reset();if(impl_->context){SDL_GL_DeleteContext(impl_->context);impl_->context=nullptr;}if(impl_->window){SDL_DestroyWindow(impl_->window);impl_->window=nullptr;}IMG_Quit();SDL_Quit();}
}
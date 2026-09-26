#include "imux/core/Engine.hpp"
#include "imux/renderer/Renderer.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace imux {
namespace {

struct Vec3 { float x,y,z; };
static Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec3 norm(Vec3 a){float l=std::sqrt(dot(a,a));return l>0?Vec3{a.x/l,a.y/l,a.z/l}:Vec3{0,0,1};}

static std::array<float,16> perspective(float fov,float aspect,float zn,float zf){
    float f=1.f/std::tan(fov*.5f), q=1.f/(zn-zf);
    return {f/aspect,0,0,0, 0,f,0,0, 0,0,(zf+zn)*q,-1, 0,0,(2*zf*zn)*q,0};
}
static std::array<float,16> lookAt(Vec3 eye,Vec3 center,Vec3 up){
    Vec3 f=norm(sub(center,eye)), s=norm(cross(f,up)), u=cross(s,f);
    return {s.x,u.x,-f.x,0, s.y,u.y,-f.y,0, s.z,u.z,-f.z,0,
            -dot(s,eye),-dot(u,eye),dot(f,eye),1};
}
static std::array<float,16> mul(const std::array<float,16>&a,const std::array<float,16>&b){
    std::array<float,16> r{};
    for(int c=0;c<4;c++)for(int row=0;row<4;row++)
        for(int k=0;k<4;k++)r[c*4+row]+=a[k*4+row]*b[c*4+k];
    return r;
}

static GLuint makeProgram(const char* vs,const char* fs){
    auto compile=[](GLenum type,const char* src){
        GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);
        GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){char log[2048]{};glGetShaderInfoLog(s,sizeof(log),nullptr,log);std::cerr<<log<<"\n";}
        return s;
    };
    GLuint a=compile(GL_VERTEX_SHADER,vs),b=compile(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();
    glAttachShader(p,a);glAttachShader(p,b);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::cerr<<log<<"\n";}
    glDeleteShader(a);glDeleteShader(b);return p;
}

static GLuint loadPng(const std::filesystem::path& path){
    SDL_Surface* s=IMG_Load(path.string().c_str());
    if(!s){std::cerr<<"Texture "<<path<<" : "<<IMG_GetError()<<"\n";return 0;}
    SDL_Surface* r=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(s);
    if(!r)return 0;
    GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r->w,r->h,0,GL_RGBA,GL_UNSIGNED_BYTE,r->pixels);
    glBindTexture(GL_TEXTURE_2D,0);SDL_FreeSurface(r);return t;
}

class GameLog {
    std::ofstream file_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
public:
    void open(const std::filesystem::path& root){
        std::error_code ec;
        std::filesystem::create_directories(root / "logs", ec);
        file_.open(root / "logs" / "imuxcarft.log", std::ios::out | std::ios::trunc);
        write("=== ImuxCarft runtime log ===");
    }
    void write(const std::string& message){
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start_).count();
        const std::string line="["+std::to_string(ms)+" ms] "+message;
        if(file_){file_<<line<<'\\n';file_.flush();}
        std::cerr<<line<<'\\n';
    }
    ~GameLog(){write("=== ImuxCarft shutdown ===");}
};

class Menu {
    GLuint skyP_=0,skyVao_=0,skyVbo_=0,uiP_=0,uiVao_=0,uiVbo_=0;
    std::array<GLuint,6> sky_{};
    int w_=1280,h_=720;
    bool hoverPlay_=false,hoverQuit_=false;

    static void addQuad(std::vector<float>&v,float x,float y,float z,float sx,float sy){
        v.insert(v.end(),{x,y,z,x+sx,y,z,x+sx,y+sy,z,x,y,z,x+sx,y+sy,z,x,y+sy,z});
    }
    void rect(float x,float y,float sx,float sy,float r,float g,float b,float a){
        std::vector<float> v;addQuad(v,x,y,0,sx,sy);
        glUseProgram(uiP_);glBindVertexArray(uiVao_);glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);
        glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(float)),v.data(),GL_STREAM_DRAW);
        glUniform4f(glGetUniformLocation(uiP_,"color"),r,g,b,a);glDrawArrays(GL_TRIANGLES,0,6);
    }
    static const char* glyph(char c){
        switch(c){
            case 'A':return "01110100011000111111100011000110001";
            case 'C':return "01110100011000010000100001000101110";
            case 'E':return "11111100001000011110100001000011111";
            case 'F':return "11111100001000011110100001000010000";
            case 'I':return "11111001000010000100001000011111";
            case 'L':return "10000100001000010000100001000011111";
            case 'M':return "10001110111010110101100011000110001";
            case 'N':return "10001110011010110011100011000110001";
            case 'P':return "11110100011000111110100001000010000";
            case 'R':return "11110100011000111110101001001010001";
            case 'T':return "11111001000010000100001000001000010";
            case 'X':return "10001010001010000010001001010001001";
            case 'Y':return "10001010001010000010000100001000010";
            case 'U':return "10001100011000110001100011000101110";
            case 'G':return "01110100011000010111100011000101110";
            case 'S':return "01111100001000001110000010000111110";
            default:return nullptr;
        }
    }
    void text(const std::string&s,float cx,float y,float scale,float r,float g,float b,float a){
        std::vector<float> v;float cw=6*scale,total=(float)s.size()*cw;float x=cx-total*.5f;
        for(char ch:s){
            const char* p=glyph(ch);
            if(p){
                for(int py=0;py<7;py++){
                    for(int px=0;px<5;px++){
                        if(p[py*5+px]=='1'){
                            addQuad(v,(x+px*scale)/float(w_), (y+py*scale)/float(h_),0,scale/float(w_),scale/float(h_));
                        }
                    }
                }
            }
            x+=cw;
        }
        glUseProgram(uiP_);glBindVertexArray(uiVao_);glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);
        glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(float)),v.data(),GL_STREAM_DRAW);
        glUniform4f(glGetUniformLocation(uiP_,"color"),r,g,b,a);glDrawArrays(GL_TRIANGLES,0,(GLsizei)(v.size()/3));
    }
public:
    bool init(const std::filesystem::path&root,int w,int h){
        w_=w;h_=h;
        const char* sv=R"(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec2 uv;out vec2 U;uniform mat4 vp;void main(){U=uv;gl_Position=vp*vec4(p,1);})";
        const char* sf=R"(#version 330 core
in vec2 U;out vec4 c;uniform sampler2D tex;void main(){c=texture(tex,U);})";
        skyP_=makeProgram(sv,sf);
        float q[]={
        -1,-1,-1,0,0, 1,-1,-1,1,0, 1,1,-1,1,1, -1,-1,-1,0,0, 1,1,-1,1,1, -1,1,-1,0,1,
         1,-1,-1,0,0, 1,-1,1,1,0, 1,1,1,1,1, 1,-1,-1,0,0, 1,1,1,1,1, 1,1,-1,0,1,
         1,-1,1,0,0, -1,-1,1,1,0, -1,1,1,1,1, 1,-1,1,0,0, -1,1,1,1,1, 1,1,1,0,1,
        -1,-1,1,0,0, -1,-1,-1,1,0, -1,1,-1,1,1, -1,-1,1,0,0, -1,1,-1,1,1, -1,1,1,0,1,
        -1,1,-1,0,0, 1,1,-1,1,0, 1,1,1,1,1, -1,1,-1,0,0, 1,1,1,1,1, -1,1,1,0,1,
        -1,-1,1,0,0, 1,-1,1,1,0, 1,-1,-1,1,1, -1,-1,1,0,0, 1,-1,-1,1,1, -1,-1,-1,0,1};
        glGenVertexArrays(1,&skyVao_);glGenBuffers(1,&skyVbo_);glBindVertexArray(skyVao_);
        glBindBuffer(GL_ARRAY_BUFFER,skyVbo_);glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
        int loadedSky=0;
        for(int i=0;i<6;i++){
            const auto path=root/"background"/("panorama_"+std::to_string(i)+".png");
            sky_[i]=loadPng(path);
            if(sky_[i]) ++loadedSky;
        }
        const char* uv=R"(#version 330 core
layout(location=0)in vec3 p;void main(){gl_Position=vec4(p.x*2.0-1.0,1.0-p.y*2.0,0,1);})";
        const char* uf=R"(#version 330 core
out vec4 c;uniform vec4 color;void main(){c=color;})";
        uiP_=makeProgram(uv,uf);glGenVertexArrays(1,&uiVao_);glGenBuffers(1,&uiVbo_);glBindVertexArray(uiVao_);
        glBindBuffer(GL_ARRAY_BUFFER,uiVbo_);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
        if(!uiP_ || !skyVao_ || !uiVao_){
            std::cerr<<"Menu GPU objects are incomplete\\n";
            return false;
        }
        if(loadedSky!=6) std::cerr<<"Menu panorama textures loaded: "<<loadedSky<<"/6\\n";
        return true;
    }
    void resize(int w,int h){w_=w;h_=h;}
    bool hitPlay(int x,int y)const{return x>w_*.33f&&x<w_*.67f&&y>h_*.57f&&y<h_*.66f;}
    bool hitQuit(int x,int y)const{return x>w_*.33f&&x<w_*.67f&&y>h_*.68f&&y<h_*.77f;}
    void mouse(int x,int y){hoverPlay_=hitPlay(x,y);hoverQuit_=hitQuit(x,y);}
    void draw(float time){
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glUseProgram(skyP_);
        float yaw=time*.035f,px=std::sin(yaw)*.01f,pz=std::cos(yaw)*.01f;
        auto p=perspective(1.18f,float(w_)/std::max(h_,1),.05f,10.f);
        auto v=lookAt({px,0,pz},{0,0,0},{0,1,0});auto vp=mul(p,v);
        glUniformMatrix4fv(glGetUniformLocation(skyP_,"vp"),1,GL_FALSE,vp.data());glBindVertexArray(skyVao_);
        glUniform1i(glGetUniformLocation(skyP_,"tex"),0);
        for(int i=0;i<6;i++){glBindTexture(GL_TEXTURE_2D,sky_[i]);glDrawArrays(GL_TRIANGLES,i*6,6);}
        glBindTexture(GL_TEXTURE_2D,0);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUniform4f(glGetUniformLocation(uiP_,"color"),0,0,0,.34f);
        rect(0,0,1,1,.0f,.0f,.0f,.20f);
        text("IMUXCRAFT",w_*.5f/1.f,h_*.18f,8,.95f,.95f,.95f,1);
        rect(.33f,.57f,.34f,.09f,hoverPlay_?.30f:.08f,hoverPlay_?.70f:.08f,hoverPlay_?.30f:.08f,.88f);
        rect(.33f,.68f,.34f,.09f,hoverQuit_?.70f:.08f,hoverQuit_?.18f:.08f,hoverQuit_?.18f:.08f,.88f);
        text("PLAY",w_*.5f,h_*.595f,5,1,1,1,1);
        text("QUIT",w_*.5f,h_*.705f,5,1,1,1,1);
        glDisable(GL_BLEND);glUseProgram(0);
    }
    void quit(){for(auto&t:sky_)if(t)glDeleteTextures(1,&t);if(skyVbo_)glDeleteBuffers(1,&skyVbo_);if(skyVao_)glDeleteVertexArrays(1,&skyVao_);if(uiVbo_)glDeleteBuffers(1,&uiVbo_);if(uiVao_)glDeleteVertexArrays(1,&uiVao_);if(skyP_)glDeleteProgram(skyP_);if(uiP_)glDeleteProgram(uiP_);}
};

class World {
    struct V{float x,y,z,u,v;};
    GLuint p_=0,vao_=0;std::array<GLuint,4> tex_{};std::array<GLuint,4> vbo_{};std::array<GLsizei,4> count_{};
    static void face(std::vector<V>&v,int side,float x,float y,float z){
        static const float uv[6][10]={
          {0,0,1,0,1,1,0,0,1,1},
          {0,0,1,0,1,1,0,0,1,1},
          {0,0,1,0,1,1,0,0,1,1},
          {0,0,1,0,1,1,0,0,1,1},
          {0,0,1,0,1,1,0,0,1,1},
          {0,0,1,0,1,1,0,0,1,1}};
        const float X=x,Y=y,Z=z;
        const float f[6][18]={
        {X,Y,Z+1,X+1,Y,Z+1,X+1,Y+1,Z+1,X,Y,Z+1,X+1,Y+1,Z+1,X,Y+1,Z+1},
        {X+1,Y,Z,X,Y,Z,X,Y+1,Z,X+1,Y,Z,X,Y+1,Z,X+1,Y+1,Z},
        {X,Y,Z,X,Y,Z+1,X,Y+1,Z+1,X,Y,Z,X,Y+1,Z+1,X,Y+1,Z},
        {X+1,Y,Z+1,X+1,Y,Z,X+1,Y+1,Z,X+1,Y,Z+1,X+1,Y+1,Z,X+1,Y+1,Z+1},
        {X,Y+1,Z+1,X+1,Y+1,Z+1,X+1,Y+1,Z,X,Y+1,Z+1,X+1,Y+1,Z,X,Y+1,Z},
        {X,Y,Z,X+1,Y,Z,X+1,Y,Z+1,X,Y,Z,X+1,Y,Z+1,X,Y,Z+1}};
        for(int i=0;i<6;i++)v.push_back({f[side][i*3],f[side][i*3+1],f[side][i*3+2],uv[side][(i%6)*2],uv[side][(i%6)*2+1]});
    }
    int idx(int x,int y,int z)const{return x+16*(z+16*y);}
    static int mat(int type,int side){if(type==1)return side==4?0:(side==5?1:1);if(type==2)return 2;return 3;}
public:
    bool init(const std::filesystem::path&root){
        const char* v=R"(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec2 uv;out vec2 U;uniform mat4 vp;void main(){U=uv;gl_Position=vp*vec4(p,1);})";
        const char* f=R"(#version 330 core
in vec2 U;out vec4 c;uniform sampler2D tex;void main(){c=texture(tex,U);})";
        p_=makeProgram(v,f);
        tex_[0]=loadPng(root/"assets/textures/blocks/grass_block_top.png");
        tex_[1]=loadPng(root/"assets/textures/blocks/grass_block_side.png");
        tex_[2]=loadPng(root/"assets/textures/blocks/dirt.png");
        tex_[3]=loadPng(root/"assets/textures/blocks/stone.png");
        std::array<std::vector<V>,4> mesh;
        for(int z=0;z<16;z++)for(int x=0;x<16;x++){
            int h=5+(int)(3*std::sin(x*.45f)+2*std::cos(z*.38f));h=std::clamp(h,2,9);
            for(int y=0;y<h;y++){
                int type=(y==h-1)?1:(y>=h-3?2:3);
                static const int dx[6]={0,0,-1,1,0,0},dy[6]={0,0,0,0,1,-1},dz[6]={1,-1,0,0,0,0};
                for(int s=0;s<6;s++){int nx=x+dx[s],ny=y+dy[s],nz=z+dz[s];bool solid=nx>=0&&nx<16&&ny>=0&&ny<h&&nz>=0&&nz<16;if(!solid)mesh[mat(type,s)].reserve(mesh[mat(type,s)].size()+6),face(mesh[mat(type,s)],s,x,y,z);}
            }
        }
        for(int i=0;i<4;i++) count_[i]=(GLsizei)mesh[i].size();
        std::cerr<<"World textures: grass_top="<<(tex_[0]!=0)
                 <<" grass_side="<<(tex_[1]!=0)
                 <<" dirt="<<(tex_[2]!=0)
                 <<" stone="<<(tex_[3]!=0)<<"\\n";
        glGenVertexArrays(1,&vao_);
        glGenBuffers(4,vbo_.data());
        glBindVertexArray(vao_);
        for(int i=0;i<4;i++){
            glBindBuffer(GL_ARRAY_BUFFER,vbo_[i]);glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(mesh[i].size()*sizeof(V)),mesh[i].data(),GL_STATIC_DRAW);
        }
        glBindBuffer(GL_ARRAY_BUFFER,vbo_[0]);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        if(!p_){
            std::cerr<<"World shader program creation failed\\n";
            return false;
        }
        if(!tex_[0] || !tex_[1] || !tex_[2] || !tex_[3]){
            std::cerr<<"World texture initialization failed\\n";
            return false;
        }
        return true;
    }
    void draw(int w,int h,float time){
        glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
        glClearColor(.48f,.68f,.88f,1);
        float a=float(w)/std::max(h,1),yaw=time*.10f,dist=25;
        Vec3 eye{8+std::sin(yaw)*dist,10+std::sin(time*.23f)*1.5f,8+std::cos(yaw)*dist};
        auto vp=mul(perspective(1.05f,a,.1f,150),lookAt(eye,{8,3,8},{0,1,0}));
        glUseProgram(p_);glUniformMatrix4fv(glGetUniformLocation(p_,"vp"),1,GL_FALSE,vp.data());glUniform1i(glGetUniformLocation(p_,"tex"),0);
        glBindVertexArray(vao_);
        for(int i=0;i<4;i++){if(!count_[i])continue;glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex_[i]);glBindBuffer(GL_ARRAY_BUFFER,vbo_[i]);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)(3*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,count_[i]);}
        glBindVertexArray(0);glUseProgram(0);
    }
    void quit(){for(auto&t:tex_)if(t)glDeleteTextures(1,&t);glDeleteBuffers(4,vbo_.data());if(vao_)glDeleteVertexArrays(1,&vao_);if(p_)glDeleteProgram(p_);}
};

}
class OpenGLRenderer final:public Renderer{int w_=1,h_=1;public:bool initialize(int w,int h)override{resize(w,h);return true;}void resize(int w,int h)override{w_=std::max(w,1);h_=std::max(h,1);glViewport(0,0,w_,h_);}void beginFrame()override{glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}void endFrame()override{}int w()const{return w_;}int h()const{return h_;}};
struct Engine::Impl{explicit Impl(EngineConfig c):config(c){}EngineConfig config;SDL_Window*window=nullptr;SDL_GLContext context=nullptr;std::unique_ptr<Renderer>renderer;Menu menu;World world;GameLog log;bool running=false,game=false;float time=0;std::filesystem::path root;};

Engine::Engine(EngineConfig c):impl_(std::make_unique<Impl>(c)){}
Engine::~Engine(){shutdown();}

bool Engine::initialize(){
    const char* base=SDL_GetBasePath();
    if(base){impl_->root=std::filesystem::path(base);SDL_free((void*)base);}else{impl_->root=std::filesystem::current_path();}
    std::error_code rootEc;
    impl_->root=std::filesystem::weakly_canonical(impl_->root,rootEc);
    impl_->log.open(impl_->root);
    impl_->log.write("Starting engine");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0){impl_->log.write(std::string("SDL_Init failed: ")+SDL_GetError());return false;}
    if(!(IMG_Init(IMG_INIT_PNG)&IMG_INIT_PNG)){impl_->log.write(std::string("IMG_Init failed: ")+IMG_GetError());SDL_Quit();return false;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    impl_->window=SDL_CreateWindow(impl_->config.title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,impl_->config.width,impl_->config.height,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!impl_->window)return false;
    impl_->context=SDL_GL_CreateContext(impl_->window);
    if(!impl_->context)return false;
    glewExperimental=GL_TRUE;if(glewInit()!=GLEW_OK)return false;SDL_GL_SetSwapInterval(impl_->config.vsync?1:0);
    impl_->renderer=std::make_unique<OpenGLRenderer>();impl_->renderer->initialize(impl_->config.width,impl_->config.height);
    impl_->log.write("OpenGL context created");
    impl_->log.write(std::string("OpenGL vendor: ")+(const char*)glGetString(GL_VENDOR));
    impl_->log.write(std::string("OpenGL renderer: ")+(const char*)glGetString(GL_RENDERER));
    impl_->log.write(std::string("OpenGL version: ")+(const char*)glGetString(GL_VERSION));
    const bool menuOk=impl_->menu.init(impl_->root,impl_->config.width,impl_->config.height);
    impl_->log.write(std::string("Menu initialization: ")+(menuOk?"OK":"FAILED"));
    const bool worldOk=impl_->world.init(impl_->root);
    impl_->log.write(std::string("World initialization: ")+(worldOk?"OK":"FAILED"));
    if(!menuOk || !worldOk){
        impl_->log.write("Initialization failure details were printed above");
        return false;
    }
    impl_->running=true;impl_->log.write("Engine initialized successfully");return true;
}
void Engine::run(){
    auto prev=std::chrono::steady_clock::now();
    while(impl_->running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT){impl_->log.write("Window close event");impl_->running=false;}
            if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){impl_->renderer->resize(e.window.data1,e.window.data2);impl_->menu.resize(e.window.data1,e.window.data2);}
            if(e.type==SDL_MOUSEMOTION&&!impl_->game)impl_->menu.mouse(e.motion.x,e.motion.y);
            if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE){if(impl_->game){impl_->log.write("Returned to menu");impl_->game=false;}else{impl_->log.write("ESC pressed in menu");impl_->running=false;}}
            if(e.type==SDL_KEYDOWN&&!impl_->game&&(e.key.keysym.sym==SDLK_RETURN||e.key.keysym.sym==SDLK_SPACE)){impl_->log.write("Entered game from keyboard");impl_->game=true;}
            if(e.type==SDL_MOUSEBUTTONDOWN&&!impl_->game&&e.button.button==SDL_BUTTON_LEFT){if(impl_->menu.hitPlay(e.button.x,e.button.y)){impl_->log.write("Entered game from PLAY");impl_->game=true;}else if(impl_->menu.hitQuit(e.button.x,e.button.y)){impl_->log.write("QUIT selected");impl_->running=false;}}
        }
        auto now=std::chrono::steady_clock::now();impl_->time+=std::chrono::duration<float>(now-prev).count();prev=now;
        impl_->renderer->beginFrame();auto*r=static_cast<OpenGLRenderer*>(impl_->renderer.get());
        if(impl_->game)impl_->world.draw(r->w(),r->h(),impl_->time);else impl_->menu.draw(impl_->time);
        impl_->renderer->endFrame();SDL_GL_SwapWindow(impl_->window);
    }
}
void Engine::shutdown(){
    if(!impl_)return;
    impl_->menu.quit();
    impl_->world.quit();
    impl_->renderer.reset();
    if(impl_->context){SDL_GL_DeleteContext(impl_->context);impl_->context=nullptr;}if(impl_->window){SDL_DestroyWindow(impl_->window);impl_->window=nullptr;}
    IMG_Quit();SDL_Quit();
}
}
#pragma once
#include "Dependencies/glew.h"
#include <iostream>

struct PostProcessSettings {
    bool enabled = true;
    float exposure = 1.15f;
    float bloomStrength = .35f;
    float bloomThreshold = 1.f;
    float vignetteStrength = .38f;
    float edgeBlurStrength = .65f;
};

// Linear HDR scene -> half-resolution blur/bloom -> tone mapping -> display-space UI.
class PostProcessing {
    GLuint filter=0, composite=0, vao=0;
    GLuint textures[5]={}, framebuffers[5]={};
    int width=0,height=0,halfWidth=0,halfHeight=0;
    bool ready=false;
    static GLuint Program(const char* fragment) {
        const char* vertex=R"GLSL(#version 330
out vec2 uv;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    uv=p; gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
)GLSL";
        GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
        const char* sources[2]={vertex,fragment};
        bool valid=true;
        for(int i=0;i<2;++i) {
            glShaderSource(shaders[i],1,&sources[i],nullptr);glCompileShader(shaders[i]);
            GLint ok=0;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);
            if(!ok) {
                char log[2048]={};glGetShaderInfoLog(shaders[i],sizeof(log),nullptr,log);
                std::cerr<<"후처리 셰이더 오류: "<<log<<std::endl;valid=false;
            }
        }
        GLuint program=0;
        if(valid) {
            program=glCreateProgram();
            for(GLuint shader:shaders) glAttachShader(program,shader);
            glLinkProgram(program);
            GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
            if(!ok) {
                char log[2048]={};glGetProgramInfoLog(program,sizeof(log),nullptr,log);
                std::cerr<<"후처리 연결 오류: "<<log<<std::endl;
                glDeleteProgram(program);program=0;
            }
        }
        for(GLuint shader:shaders) glDeleteShader(shader);
        return program;
    }
    void ReleaseTargets() {
        glDeleteTextures(5,textures);glDeleteFramebuffers(5,framebuffers);
        for(int i=0;i<5;++i) {textures[i]=0;framebuffers[i]=0;}
        ready=false;
    }
    void Pass(int source,int destination,bool horizontal,bool extract,float threshold) {
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[destination]);
        glViewport(0,0,halfWidth,halfHeight);
        glUseProgram(filter);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[source]);
        glUniform1i(glGetUniformLocation(filter,"sourceImage"),0);
        glUniform2f(glGetUniformLocation(filter,"direction"),horizontal?1.f:0.f,horizontal?0.f:1.f);
        glUniform1i(glGetUniformLocation(filter,"extractBright"),extract?1:0);
        glUniform1f(glGetUniformLocation(filter,"threshold"),threshold);
        glDrawArrays(GL_TRIANGLES,0,3);
    }
public:
    PostProcessing() {
        filter=Program(R"GLSL(#version 330
in vec2 uv; out vec4 result;
uniform sampler2D sourceImage;
uniform vec2 direction;
uniform bool extractBright;
uniform float threshold;
vec3 sampleColor(vec2 p) {
    vec3 c=texture(sourceImage,p).rgb;
    if(extractBright) {
        float brightness=max(c.r,max(c.g,c.b));
        float knee=max(threshold*.5,.001);
        float soft=clamp(brightness-threshold+knee,0.0,2.0*knee);
        soft=soft*soft/(4.0*knee);
        c*=max(brightness-threshold,soft)/max(brightness,.0001);
    }
    return c;
}
void main() {
    vec2 stepUV=direction/vec2(textureSize(sourceImage,0));
    vec3 color=sampleColor(uv)*.227027;
    color+=(sampleColor(uv+stepUV*1.384615)+sampleColor(uv-stepUV*1.384615))*.316216;
    color+=(sampleColor(uv+stepUV*3.230769)+sampleColor(uv-stepUV*3.230769))*.070270;
    result=vec4(color,1.0);
}
)GLSL");
        composite=Program(R"GLSL(#version 330
in vec2 uv; out vec4 result;
uniform sampler2D sceneImage,blurImage,bloomImage;
uniform float exposure,bloomStrength,vignetteStrength,edgeBlurStrength;
vec3 toneMap(vec3 c) {
    return clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.0,1.0);
}
void main() {
    // Elliptical screen-space falloff: clear center, gradually softer/darker edges.
    float radius=length((uv-.5)*2.0);
    float edge=smoothstep(.45,1.25,radius);
    vec3 color=mix(texture(sceneImage,uv).rgb,texture(blurImage,uv).rgb,
                   clamp(edge*edgeBlurStrength,0.0,1.0));
    color+=texture(bloomImage,uv).rgb*bloomStrength;
    color*=1.0-clamp(vignetteStrength,0.0,.9)*smoothstep(.35,1.35,radius);
    color=toneMap(color*max(exposure,.01));
    result=vec4(pow(color,vec3(1.0/2.2)),1.0);
}
)GLSL");
        glGenVertexArrays(1,&vao);
    }
    ~PostProcessing() {
        ReleaseTargets();
        if(filter) glDeleteProgram(filter);
        if(composite) glDeleteProgram(composite);
        if(vao) glDeleteVertexArrays(1,&vao);
    }
    void Resize(int w,int h) {
        if(ready&&width==w&&height==h) return;
        ReleaseTargets();width=w;height=h;
        halfWidth=w/2>0?w/2:1;halfHeight=h/2>0?h/2:1;
        if(!filter||!composite||!vao) return;
        glGenTextures(5,textures);glGenFramebuffers(5,framebuffers);
        ready=true;
        for(int i=0;i<5;++i) {
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,i==0?w:halfWidth,i==0?h:halfHeight,
                         0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[i],0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) ready=false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
        if(!ready) {
            std::cerr<<"후처리 버퍼 생성 실패: 기본 렌더링으로 전환합니다."<<std::endl;
            ReleaseTargets();
        }
    }
    bool Begin(bool enabled) {
        bool active=enabled&&ready;
        glBindFramebuffer(GL_FRAMEBUFFER,active?framebuffers[0]:0);
        glViewport(0,0,width,height);
        glDisable(GL_FRAMEBUFFER_SRGB);
        return active;
    }
    void Finish(const PostProcessSettings& settings) {
        glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glBindVertexArray(vao);
        Pass(0,1,true,false,settings.bloomThreshold);
        Pass(1,2,false,false,settings.bloomThreshold);
        Pass(0,3,true,true,settings.bloomThreshold);
        Pass(3,4,false,false,settings.bloomThreshold);
        // A second bloom pass spreads the small emissive shapes beyond their edges.
        Pass(4,3,true,false,settings.bloomThreshold);
        Pass(3,4,false,false,settings.bloomThreshold);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);
        glUseProgram(composite);
        const int indices[3]={0,2,4};
        const char* names[3]={"sceneImage","blurImage","bloomImage"};
        for(int i=0;i<3;++i) {
            glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[indices[i]]);
            glUniform1i(glGetUniformLocation(composite,names[i]),i);
        }
        glUniform1f(glGetUniformLocation(composite,"exposure"),settings.exposure);
        glUniform1f(glGetUniformLocation(composite,"bloomStrength"),settings.bloomStrength);
        glUniform1f(glGetUniformLocation(composite,"vignetteStrength"),settings.vignetteStrength);
        glUniform1f(glGetUniformLocation(composite,"edgeBlurStrength"),settings.edgeBlurStrength);
        glDrawArrays(GL_TRIANGLES,0,3);
        for(int i=2;i>=0;--i) {glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,0);}
        glBindVertexArray(0);glUseProgram(0);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    }
};

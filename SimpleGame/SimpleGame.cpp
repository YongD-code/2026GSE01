/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/
#include "stdafx.h"
#include "Dependencies/glew.h"
#include "Dependencies/freeglut.h"
#include "Renderer.h"
#include "Prototype.h"
#include <iostream>
#include <memory>
#include <windows.h>

// Korean text uses GDI rasterization; scene geometry uses OpenGL 3.3 shaders.
#pragma comment(lib, "opengl32.lib")
namespace {
std::unique_ptr<Renderer> renderer;
std::unique_ptr<Prototype> game;
int previousTime=0;
void Display() { if(game) {game->Draw();glutSwapBuffers();} }
void Timer(int) {
    if(!game) return;
    int now=glutGet(GLUT_ELAPSED_TIME);
    float dt=(std::min)(.05f,(std::max)(0.f,(now-previousTime)*.001f));
    // Poll Shift every frame so pressing/releasing it while moving takes effect immediately.
    const bool focused = GetForegroundWindow() == WindowFromDC(wglGetCurrentDC());
    const bool sprint = focused && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (!focused) game->ClearInput();
    previousTime=now;game->Update(dt, sprint);glutPostRedisplay();glutTimerFunc(16,Timer,0);
}
void Reshape(int w,int h) {if(game) game->Resize((std::max)(1,w),(std::max)(1,h));}
void KeyDown(unsigned char key,int,int) {if(game) game->Key(key,true);}
void KeyUp(unsigned char key,int,int) {if(game) game->Key(key,false);}
int ArrowIndex(int key) {
    switch(key) {case GLUT_KEY_LEFT:return 0;case GLUT_KEY_UP:return 1;
    case GLUT_KEY_RIGHT:return 2;case GLUT_KEY_DOWN:return 3;default:return -1;}
}
void SpecialDown(int key,int,int) {if(game) game->Arrow(ArrowIndex(key),true);}
void SpecialUp(int key,int,int) {if(game) game->Arrow(ArrowIndex(key),false);}
void Entry(int state) {if(game&&state==GLUT_LEFT) game->ClearInput();}
void Close() {game.reset();renderer.reset();} // Release GPU objects while context is alive.
}
int main(int argc,char** argv) {
    glutInit(&argc,argv);
    glutInitContextVersion(3,3);
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(1280,800);
    glutCreateWindow("2026GSE01");
    SetWindowTextW(WindowFromDC(wglGetCurrentDC()), L"마지막 불씨 | 2026GSE01 시제품");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental=GL_TRUE;
    GLenum result=glewInit();
    if(result!=GLEW_OK || !GLEW_VERSION_3_3) {
        std::cerr<<"OpenGL 3.3 호환 프로필을 지원하는 그래픽 환경이 필요합니다."<<std::endl;
        return 1;
    }
    renderer.reset(new Renderer(1280,800));
    if(!renderer->IsInitialized()) {renderer.reset();return 1;}
    game.reset(new Prototype(*renderer));
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyDown);glutKeyboardUpFunc(KeyUp);
    glutSpecialFunc(SpecialDown);glutSpecialUpFunc(SpecialUp);
    glutEntryFunc(Entry);glutCloseFunc(Close);
    previousTime=glutGet(GLUT_ELAPSED_TIME);glutTimerFunc(16,Timer,0);
    glutMainLoop();
    return 0;
}


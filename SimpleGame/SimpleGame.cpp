/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/
#include "stdafx.h"
#include "Dependencies/glew.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "Renderer.h"
#include "Prototype.h"
#include "FrameProfiler.h"
#include "ModelIntegrationReview.h"
#include <iostream>
#include <memory>
#include <cstring>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>

// Korean text and the asset pipeline still use Windows APIs.
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
std::unique_ptr<Renderer> renderer;
std::unique_ptr<Prototype> game;
int windowWidth = 1280, windowHeight = 800;
int framebufferWidth = 1280, framebufferHeight = 800;
bool resizePending = false;

void Close(bool saveProgress)
{
    if (game && saveProgress)
    {
        game->SaveBeforeClose();
    }
    game.reset();
    renderer.reset();
    FrameProfiler::Close();
}

struct WindowLifetime
{
    GLFWwindow* window = nullptr;
    bool fineTimer = false;
    bool saveProgress = true;

    ~WindowLifetime()
    {
        // Save and release every GPU object before destroying the context.
        if (window)
        {
            glfwMakeContextCurrent(window);
        }
        Close(saveProgress);
        if (window)
        {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
        if (fineTimer)
        {
            timeEndPeriod(1);
        }
    }
};

void Error(int code, const char* description)
{
    std::cerr << "GLFW 오류 " << code << ": " << (description ? description : "") << '\n';
}

void ApplyResize()
{
    if (resizePending && game && framebufferWidth > 0 && framebufferHeight > 0)
    {
        game->Resize(framebufferWidth, framebufferHeight);
        resizePending = false;
    }
}

void FramebufferSize(GLFWwindow*, int width, int height)
{
    framebufferWidth = width;
    framebufferHeight = height;
    resizePending = true;
}

void WindowSize(GLFWwindow*, int width, int height)
{
    windowWidth = width;
    windowHeight = height;
}

Point Pointer(GLFWwindow* window)
{
    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    // GLFW cursor coordinates use window units; game UI and picking use framebuffer pixels.
    return {float(x * framebufferWidth / (std::max)(1, windowWidth)),
            float(y * framebufferHeight / (std::max)(1, windowHeight))};
}

void PointerMotion(GLFWwindow* window, double, double)
{
    if (game && framebufferWidth > 0 && framebufferHeight > 0)
    {
        const auto at = Pointer(window);
        game->PointerMove(int(std::floor(at.x)), int(std::floor(at.y)));
    }
}

void Mouse(GLFWwindow* window, int button, int action, int)
{
    if (!game || framebufferWidth <= 0 || framebufferHeight <= 0 ||
        (action == GLFW_PRESS && !glfwGetWindowAttrib(window, GLFW_FOCUSED)))
    {
        return;
    }
    // Preserve the game's existing button contract: left 0, middle 1, right 2.
    const int mapped = button == GLFW_MOUSE_BUTTON_LEFT     ? 0
                       : button == GLFW_MOUSE_BUTTON_RIGHT  ? 2
                       : button == GLFW_MOUSE_BUTTON_MIDDLE ? 1
                                                            : -1;
    if (mapped >= 0)
    {
        ApplyResize();
        const auto at = Pointer(window);
        game->MouseButton(
            mapped, action == GLFW_PRESS, int(std::floor(at.x)), int(std::floor(at.y)));
    }
}

int ArrowIndex(int key)
{
    switch (key)
    {
    case GLFW_KEY_LEFT:
        return 0;
    case GLFW_KEY_UP:
        return 1;
    case GLFW_KEY_RIGHT:
        return 2;
    case GLFW_KEY_DOWN:
        return 3;
    default:
        return -1;
    }
}

int GameKey(int key)
{
    // These are physical game bindings, not text input. Key-up uses the same mapping.
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
    {
        return 'a' + key - GLFW_KEY_A;
    }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
    {
        return '0' + key - GLFW_KEY_0;
    }
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    {
        return '0' + key - GLFW_KEY_KP_0;
    }
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        return 27;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        return 13;
    case GLFW_KEY_TAB:
        return '\t';
    case GLFW_KEY_SPACE:
        return ' ';
    case GLFW_KEY_BACKSPACE:
        return 8;
    case GLFW_KEY_LEFT_BRACKET:
        return '[';
    case GLFW_KEY_RIGHT_BRACKET:
        return ']';
    default:
        return -1;
    }
}

void Key(GLFWwindow* window, int key, int, int action, int)
{
    if (!game || action == GLFW_REPEAT ||
        (action == GLFW_PRESS && !glfwGetWindowAttrib(window, GLFW_FOCUSED)))
    {
        return;
    }
    const bool down = action == GLFW_PRESS;
    if (key == GLFW_KEY_F5 || key == GLFW_KEY_F9)
    {
        if (down)
        {
            game->ProgressKey(key == GLFW_KEY_F9);
        }
        return;
    }
    const int arrow = ArrowIndex(key);
    if (arrow >= 0)
    {
        game->Arrow(arrow, down);
        return;
    }
    const int mapped = GameKey(key);
    if (mapped >= 0)
    {
        game->Key(static_cast<unsigned char>(mapped), down);
    }
}

void Focus(GLFWwindow*, int focused)
{
    if (game)
    {
        game->SetWindowFocused(focused == GLFW_TRUE);
    }
}

void Entry(GLFWwindow*, int entered)
{
    if (game && !entered)
    {
        game->ClearInput();
    }
}

void Run(GLFWwindow* window)
{
    auto previous = FrameProfiler::Clock::now();
    auto nextFrame = previous;
    bool presentationSuspended = false;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(window) || game->WantsExit())
        {
            break;
        }
        const bool drawable = framebufferWidth > 0 && framebufferHeight > 0 &&
                              !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
        const bool suspended = !drawable;
        if (presentationSuspended != suspended)
        {
            presentationSuspended = suspended;
            FrameProfiler::Event("presentation_state", drawable ? "visible" : "suspended", 0);
        }
        const auto now = FrameProfiler::Clock::now();
        if (now < nextFrame)
        {
            // Input wakes the wait, but does not start an extra simulation/render tick.
            glfwWaitEventsTimeout(std::chrono::duration<double>(nextFrame - now).count());
            continue;
        }
        ApplyResize();
        FrameProfiler::TimerSample(
            (std::max)(0., std::chrono::duration<double, std::milli>(now - nextFrame).count()));
        const float dt =
            (std::max)(0.f, (std::min)(.05f, std::chrono::duration<float>(now - previous).count()));
        previous = now;
        nextFrame = now + std::chrono::milliseconds(16);
        const bool focused = drawable && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        game->SetWindowFocused(focused);
        const bool sprint = focused && (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        const auto updateStart = FrameProfiler::Clock::now();
        game->Update(dt, sprint);
        FrameProfiler::UpdateTime(FrameProfiler::Milliseconds(updateStart));
        if (game->WantsExit())
        {
            break;
        }
        if (!drawable)
        {
            // Keep NPC life and monitoring running while avoiding minimized GPU work.
            continue;
        }
        FrameProfiler::BeginFrame();
        game->Draw();
        FrameProfiler::BeforeSwap();
        glfwSwapBuffers(window);
        FrameProfiler::EndFrame();
    }
}
} // namespace

int main(int argc, char** argv)
{
    const bool verifyModels = argc > 1 && std::strcmp(argv[1], "--verify-models") == 0;
    SetConsoleOutputCP(CP_UTF8);
    glfwSetErrorCallback(Error);
    if (!glfwInit())
    {
        return 1;
    }
    WindowLifetime lifetime;
    lifetime.saveProgress = !verifyModels;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // glDrawPixels and other compatibility-profile paths remain in the renderer.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_VISIBLE, verifyModels ? GLFW_FALSE : GLFW_TRUE);
    lifetime.window =
        glfwCreateWindow(1280, 800, "주흔 — 잊힌 이름 | 2026GSE01 시제품", nullptr, nullptr);
    if (!lifetime.window)
    {
        return 1;
    }
    GLFWwindow* window = lifetime.window;
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 호환 프로필을 지원하는 그래픽 환경이 필요합니다.\n";
        return 1;
    }
    // Opt in explicitly; the default keeps presentation from imposing a second frame limiter.
    wchar_t vsyncValue[8] = {};
    const bool requestedVsync =
        GetEnvironmentVariableW(L"GSE_VSYNC", vsyncValue, 8) == 1 && vsyncValue[0] == L'1';
    glfwSwapInterval(requestedVsync ? 1 : 0);
    FrameProfiler::Event("window_backend", glfwGetVersionString(), 0);
    FrameProfiler::Event("swap_interval_requested", requestedVsync ? "1" : "0", 0);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    renderer.reset(new Renderer((std::max)(1, framebufferWidth), (std::max)(1, framebufferHeight)));
    if (!renderer->IsInitialized())
    {
        return 1;
    }
    game.reset(new Prototype(*renderer));
    game->SetWindowFocused(glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE);
    if (verifyModels)
    {
        return ModelIntegrationReview::Run(*game);
    }
    glfwSetFramebufferSizeCallback(window, FramebufferSize);
    glfwSetWindowSizeCallback(window, WindowSize);
    glfwSetCursorPosCallback(window, PointerMotion);
    glfwSetMouseButtonCallback(window, Mouse);
    glfwSetKeyCallback(window, Key);
    glfwSetWindowFocusCallback(window, Focus);
    glfwSetCursorEnterCallback(window, Entry);
    PointerMotion(window, 0, 0);
    lifetime.fineTimer = timeBeginPeriod(1) == TIMERR_NOERROR;
    FrameProfiler::Event("timer_resolution_request", lifetime.fineTimer ? "1ms" : "unavailable", 0);
    Run(window);
    return 0;
}

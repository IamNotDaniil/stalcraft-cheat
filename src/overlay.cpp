#define GLEW_NO_GLU
#include <GL/glew.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "overlay.h"
#include "aimbot.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <thread>

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

namespace overlay {
    void run(std::atomic<bool>& running) {
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit()) return;

        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow* window = glfwCreateWindow(1920, 1080, "Overlay", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            return;
        }

        Display* disp = XOpenDisplay(nullptr);
        if (disp) {
            Window win = glfwGetX11Window(window);
            XSetWindowAttributes attr;
            attr.override_redirect = True;
            XChangeWindowAttributes(disp, win, CWOverrideRedirect, &attr);
            Atom wm_state = XInternAtom(disp, "_NET_WM_STATE", False);
            Atom above = XInternAtom(disp, "_NET_WM_STATE_ABOVE", False);
            XChangeProperty(disp, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&above, 1);
            XFlush(disp);
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        while (running) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Stalcraft X Cheat", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Checkbox("Aimbot Enabled", &aimbot::g_config.enabled);
            ImGui::SliderFloat("FOV", &aimbot::g_config.fov, 0.0f, 180.0f);
            ImGui::Checkbox("Draw FOV", &aimbot::g_config.draw_fov);
            ImGui::ColorEdit3("FOV Color", aimbot::g_config.fov_color);
            ImGui::SliderFloat("Smooth", &aimbot::g_config.smooth, 1.0f, 20.0f);
            const char* bones[] = {"Head", "Body", "Legs"};
            ImGui::Combo("Bone", &aimbot::g_config.bone, bones, IM_ARRAYSIZE(bones));
            ImGui::Checkbox("Triggerbot", &aimbot::g_config.triggerbot);
            ImGui::Checkbox("Silent Aim", &aimbot::g_config.silent);
            ImGui::Checkbox("No Recoil", &aimbot::g_config.no_recoil);
            ImGui::Text("Hotkey: F1 (toggle menu)");
            ImGui::End();

            if (aimbot::g_config.draw_fov && aimbot::g_config.enabled) {
                ImDrawList* draw = ImGui::GetBackgroundDrawList();
                ImVec2 center = ImGui::GetIO().DisplaySize;
                center.x *= 0.5f; center.y *= 0.5f;
                float radius = aimbot::g_config.fov * 5.0f;
                draw->AddCircle(center, radius, ImColor(aimbot::g_config.fov_color[0], aimbot::g_config.fov_color[1], aimbot::g_config.fov_color[2]), 64, 2.0f);
            }

            ImGui::Render();
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

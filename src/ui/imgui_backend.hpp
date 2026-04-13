#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <functional>

namespace cs2 {

class ImGuiBackend {
public:
    static bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    static void Shutdown();
    static void NewFrame();
    static void EndFrame();
    static void Render();

    static void SetRenderCallback(std::function<void()> callback);
    static std::function<void()> GetRenderCallback();

    static ID3D11Device* GetDevice() { return s_device; }
    static ID3D11DeviceContext* GetContext() { return s_context; }
    static IDXGISwapChain* GetSwapChain() { return s_swap_chain; }

private:
    static ID3D11Device* s_device;
    static ID3D11DeviceContext* s_context;
    static IDXGISwapChain* s_swap_chain;
    static ID3D11RenderTargetView* s_render_target_view;
    static std::function<void()> s_render_callback;
};

}

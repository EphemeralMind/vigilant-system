#include <renderer.h>
#include <rasterizer.h>
#include <s1516.h>

#define FLYTHROUGH_CAMERA_IMPLEMENTATION
#include <flythrough_camera.h>

#include <Windows.h>
#include <GL/gl.h>

#pragma comment(lib, "OpenGL32.lib")

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
        ExitProcess(0);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

HDC g_hDC;

void init_window(int32_t width, int32_t height)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = MyWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    wc.lpszClassName = TEXT("WindowClass");
    RegisterClassEx(&wc);

    RECT wr = { 0, 0, width, height };
    AdjustWindowRect(&wr, 0, FALSE);
    HWND hWnd = CreateWindowEx(
        0, TEXT("WindowClass"),
        TEXT("BasicGL"),
        WS_OVERLAPPEDWINDOW,
        0, 0, wr.right - wr.left, wr.bottom - wr.top,
        0, 0, GetModuleHandle(NULL), 0);

    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    g_hDC = GetDC(hWnd);

    int chosenPixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, chosenPixelFormat, &pfd);

    HGLRC hGLRC = wglCreateContext(g_hDC);
    wglMakeCurrent(g_hDC, hGLRC);

    ShowWindow(hWnd, SW_SHOWNORMAL);
}

int main()
{
    int fbwidth = 1024;
    int fbheight = 768;

    init_window(fbwidth, fbheight);

    renderer_t* rd = new_renderer(fbwidth, fbheight);

    scene_t* sc = new_scene();
    uint32_t first_model_id, num_added_models;
    scene_add_models(sc, "assets/cube/cube.obj", "assets/cube/", &first_model_id, &num_added_models);
    for (uint32_t model_id = first_model_id; model_id < first_model_id + num_added_models; model_id++)
    {
        uint32_t instance_id;
        scene_add_instance(sc, model_id, &instance_id);
    }

    scene_set_camera_lookat(s1516_int(5), s1516_int(5), s1516_int(5), 0, 0, 0, 0, s1516_int(1), 0);
    scene_set_camera_perspective(s1516_flt(70.0f * 3.14f / 180.0f), s1516_flt((float)fbwidth / fbheight), s1516_flt(0.01f), s1516_flt(10.0f));

    float pos[3] = { 0.0f, 0.0f, 0.0f };
    float look[3] = { 0.0f, 0.0f, 1.0f };
    const float up[3] = { 0.0f, 1.0f, 0.0f };

    LARGE_INTEGER then, now, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&then);

    POINT oldcursor;
    GetCursorPos(&oldcursor);

    uint8_t* rgba8_pixels = (uint8_t*)malloc(fbwidth * fbheight * 4);
    assert(rgba8_pixels);

    // readback framebuffer contents
    framebuffer_t* fb = renderer_get_framebuffer(rd);


    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        QueryPerformanceCounter(&now);
        float delta_time_sec = (float)(now.QuadPart - then.QuadPart) / freq.QuadPart;

        POINT cursor;
        GetCursorPos(&cursor);

        // only move and rotate camera when right mouse button is pressed
        float activated = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1.0f : 0.0f;

        float view[16];
        flythrough_camera_update(
            pos, look, up, view,
            delta_time_sec,
            10.0f * ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) ? 2.0f : 1.0f) * activated,
            0.5f * activated,
            80.0f,
            cursor.x - oldcursor.x, cursor.y - oldcursor.y,
            GetAsyncKeyState('W') & 0x8000, GetAsyncKeyState('A') & 0x8000, GetAsyncKeyState('S') & 0x8000, GetAsyncKeyState('D') & 0x8000,
            GetAsyncKeyState(VK_SPACE) & 0x8000, GetAsyncKeyState(VK_LCONTROL) & 0x8000,
            0);

        renderer_render_scene(rd, sc);

        framebuffer_t* fb = renderer_get_framebuffer(rd);
        framebuffer_pack_row_major(fb, 0, 0, fbwidth, fbheight, pixelformat_r8g8b8a8_unorm, rgba8_pixels);
        glDrawPixels(fbwidth, fbheight, GL_RGBA, GL_UNSIGNED_BYTE, rgba8_pixels);

        SwapBuffers(g_hDC);

        then = now;
        oldcursor = cursor;
    }

    free(rgba8_pixels);

    delete_scene(sc);
    delete_renderer(rd);

    return 0;
}
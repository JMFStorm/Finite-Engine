// ----------
// Includes

#include <windows.h>
#include <commdlg.h>
#include <limits.h>

#include <combaseapi.h>
#include <xaudio2.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// ---------
// Defines

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STBI_ONLY_PNG

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

const int MAX_TEXT_UI_VERTEX_COUNT = 500 * 6;
const int STR_BUFFER_COUNT = 256;
const int WINDOW_DEFAULT_WIDTH = 1600;
const int WINDOW_DEFAULT_HEIGHT = 1200;

typedef unsigned char byte;

static_assert(sizeof(unsigned char) * CHAR_BIT == 8, "unsigned char is not 8 bits");

typedef int i32;
typedef long long i64;

static_assert(sizeof(int) * CHAR_BIT == 32, "int is not 32 bits");
static_assert(sizeof(long long) * CHAR_BIT == 64, "long is not 64 bits");

typedef unsigned int u32;
typedef unsigned long long u64;

static_assert(sizeof(unsigned int) * CHAR_BIT == 32, "int is not 32 bits");
static_assert(sizeof(unsigned long long) * CHAR_BIT == 64, "long is not 64 bits");

typedef float f32;
typedef double f64;

static_assert(sizeof(float) * CHAR_BIT == 32, "float is not 32 bits");
static_assert(sizeof(double) * CHAR_BIT == 64, "double is not 64 bits");

// ---------
// Structs

struct Vec2i {
    i32 x;
    i32 y;
};

struct Vec2f {
    f32 x;
    f32 y;
};

struct Vec3i {
    i32 x;
    i32 y;
    i32 z;
};

struct Vec3f {
    f32 x;
    f32 y;
    f32 z;
};

struct Buffer {
    i32 size_bytes;
    byte* data;
};

struct Window {
    Vec2i size_px;
    Vec2i new_size_px;
    RECT original_rect;
    LARGE_INTEGER frequency;
    LARGE_INTEGER last_frame_time;
    HWND handle;
    DWORD original_style;
    u64 frame_counter = 0;
    f32 frame_delta = 0.0f;
    i32 mousewheel_delta = 0;
    i32 frame_draw_calls = 0;
    bool is_resizing;

    Vec2f ScreenPxToNDC(Vec2i px) {
        f32 ndcX = (2.0f * px.x) / (size_px.x - 1) - 1.0f;
        f32 ndcY = 1.0f - (2.0f * px.y) / (size_px.y - 1);
        Vec2f result = {
            .x = ndcX,
            .y = ndcY
        };
        return result;
    }

    Vec2i NDCToScreenPx(Vec2f ndc) {
        int x = (size_px.x * (ndc.x + 1.0f)) * 0.5f;
        int y = (size_px.y * (ndc.y + 1.0f)) * 0.5f;
        Vec2i result = {
            .x = x,
            .y = y
        };
        return result;
    }

    Vec2f GetWindowMousePosition() {
        POINT mousePos;
        if (GetCursorPos(&mousePos) && ScreenToClient(handle, &mousePos)) {
            return Vec2f{(f32)mousePos.x, (f32)mousePos.y};
        }
        return Vec2f{-1.0f, -1.0f};
    }

    bool IsWindowFocused() {
        HWND foregroundWindow = GetForegroundWindow();
        return (foregroundWindow == handle);
    }

    f32 GetVWInPx(f32 vw) {
        return (vw / 100.0f) * (f32)size_px.x;
    }

    f32 GetVHInPx(f32 vh) {
        return (vh / 100.0f) * (f32)size_px.y;
    }

    f32 GetPxInVW(f32 px) {
        return px / (f32)size_px.x;
    }

    f32 GetPxInVH(f32 px) {
        return px / (f32)size_px.y;
    }
};

struct KeyInputState {
    int keycode;
    bool pressed;
    bool is_down;
};

struct GameKeys {
    KeyInputState arrow_up = { VK_UP };
    KeyInputState arrow_down { VK_DOWN };
    KeyInputState arrow_left = { VK_LEFT };
    KeyInputState arrow_right = { VK_RIGHT };
    KeyInputState a = { (int)'A' };
    KeyInputState s = { (int)'S' };
    KeyInputState d = { (int)'D' };
    KeyInputState _1 = { (int)'1' };
    KeyInputState _2 = { (int)'2' };
    KeyInputState _3 = { (int)'3' };
};

struct FrameInput {
    f32 mouse_x = 0.0f;
    f32 mouse_y = 0.0f;
    i32 mouse_tilemap_x = 0;
    i32 mouse_tilemap_y = 0;
    bool mousewheel_up = false;
    bool mousewheel_down = false;
    GameKeys keys = {};
};

struct Camera2D {
    DirectX::XMFLOAT2 position;
    f32 zoom;
};

struct ViewProjectionMatrixBufferType {
    DirectX::XMMATRIX view_projection_matrix;
};

enum class ImageFileType {
    png
};

struct Texture {
    char filename[128];
    i32 x;
    i32 y;
    i32 channels;
    ImageFileType filetype;
    ID3D11ShaderResourceView* resource_view;
};

struct TilemapTileVertex {
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv;
};

struct TextUiVertex {
    DirectX::XMFLOAT4 Pos;
    DirectX::XMFLOAT2 TexCoord;
};

struct RectangleVertex {
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 color;
};

struct FontGlyphInfo {
    i32 bitmap_width = 0;
    i32 bitmap_height = 0;
    i32 x_offset = 0;
    i32 y_offset = 0;
    f32 advance = 0.0f;
    f32 uv_x0 = 0.0f;
    f32 uv_y0 = 0.0f;
    f32 uv_x1 = 0.0f;
    f32 uv_y1 = 0.0f;
    char character;
};

struct FontAtlasInfo {
    ID3D11ShaderResourceView* texture = nullptr;
    i32 font_size_px = 0;
    i32 font_atlas_width = 0;
    i32 font_atlas_height = 0;
    f32 font_ascent = 0.0f;
    f32 font_descent = 0.0f;
    f32 font_linegap = 0.0f;
    FontGlyphInfo glyphs[96] = {};
};

struct Tile {
    i32 type;
};

struct Tilemap {
    i32 width = 0;
    i32 height = 0;
    Tile* tiles = nullptr;
};

struct CStrBuffer {
    char* buffer = nullptr;
    i32 size = 0;

    char* GetCStrBuffer() {
        memset(buffer, 0, size);
        return buffer;
    }

    void MemsetBuffer(char value) {
        memset(buffer, value, size);
    }
};

struct WStrBuffer {
    wchar_t* buffer = nullptr;
    i32 size = 0;

    wchar_t* GetWStrBuffer() {
        memset(buffer, 0, size);
        return buffer;
    }

    void MemsetBuffer(wchar_t value) {
        memset(buffer, value, size);
    }
};

#pragma pack(1)
typedef struct {
    char riffHeader[4];        // "RIFF"
    DWORD fileSize;            // Size of the entire file minus 8 bytes
    char waveHeader[4];        // "WAVE"
    char fmtHeader[4];         // "fmt "
    DWORD fmtChunkSize;        // Size of the fmt chunk
    WORD audioFormat;          // Audio format (1 for PCM)
    WORD numChannels;          // Number of channels
    DWORD sampleRate;          // Sampling frequency
    DWORD byteRate;            // (Sample Rate * BitsPerSample * Channels) / 8
    WORD blockAlign;           // Block align (Channels * BitsPerSample) / 8
    WORD bitsPerSample;        // Bits per sample
    char dataHeader[4];        // "data"
    DWORD dataSize;            // Size of the data section
} WAVHeader;
#pragma pack()

// -----------------------
// Function declarations

/**
 * @brief Stop application execution and display a message to the user.
 *
 * Apply debugging breakpoint if DEBUG -flag is set.
 */
void ErrorMessageAndBreak(wchar_t* message);
void ErrorMessageAndBreak(char* message);

void DebugMessage(wchar_t* message);
void DebugMessage(char* message);

bool IsKeyPressed(int key);

void WindowResizeEvent();

bool CursorOverTilemap();

void BufferRectangle2d(Vec2f offset);
void DrawBufferedRectangle2ds(ID3D11ShaderResourceView* texture);

void StrToWideStr(char* str, wchar_t* wresult, int str_count);

Vec2f TilemapCoordsToIsometricScreenSpace(Vec2f tilemap_coord);

unsigned char* LoadFileToPtr(wchar_t* filename, size_t* get_file_size);

FontAtlasInfo LoadFontAtlas(char* filepath, float pixel_height);

f32 GetTextWidthPx(char* text, FontAtlasInfo* font_info);

void LoadTextureFromFilepath(Texture* texture, char* filepath);

Vec2f ScreenSpaceToTilemapCoords(Vec2f tilemap_coord);

void LoadGlobalFonts();

Tile* GetCursorTilePtr();

void SetDefaultViewportDimensions();

void DrawDotOnScreen(Vec2f ndc, f32 size_px, Vec3f color);
Vec2f DrawTextToScreen(char* text, Vec2f screen_pos, FontAtlasInfo* font_info);
void DrawRectangleToScreen(Vec2f top_left, Vec2f top_right, Vec2f bot_left, Vec2f bot_right, Vec3f color);
// void DrawTilemapTile(ID3D11ShaderResourceView* texture, Vec2f coordinate);
void DrawLineOnScreen(Vec2f ndc_start, Vec2f ndc_end, f32 size_px, Vec3f color);

DirectX::XMMATRIX GetViewportProjectionMatrix();
DirectX::XMMATRIX GetViewportViewMatrix();

BYTE* LoadWAWFile(wchar_t* filePath, WAVHeader* wavHeader);

void PlayMonoSound(Buffer audio_buffer);

// ---------
// Globals

const int tilemap_width = 40;
const int tilemap_height = 40;

Tile tilemap_data[tilemap_width * tilemap_height] = {0};

Tilemap g_tilemap = {
    .width = tilemap_width,
    .height = tilemap_height,
    .tiles = tilemap_data
};

Buffer sound_buffer_1 = {};
Buffer sound_buffer_2 = {};
Buffer sound_buffer_3 = {};

const int MAX_VOICES = 20;
IXAudio2SourceVoice* mono_source_voice_pool[MAX_VOICES] = { nullptr };
IXAudio2* pXAudio2 = NULL;
IXAudio2MasteringVoice* pMasterVoice = NULL;

const f32 debug_font_vh_size = 1.5f;
FontAtlasInfo g_debug_font;

Window g_window = {};
FrameInput frame_input = {};

const int BUFFER_SLOT_VIEW_PROJECTION = 0;
const int BUFFER_SLOT_MODEL = 1;
const int BUFFER_SLOT_VIEW = 2;

ID3D11Buffer* cbuffer_view_projection = nullptr;
ID3D11Buffer* cbuffer_model = nullptr;
ID3D11Buffer* cbuffer_view = nullptr;

IDXGISwapChain *swapChain;
ID3D11Device* id3d11_device;
ID3D11DeviceContext *deviceContext;
ID3D11RenderTargetView *renderTargetView;
D3D11_VIEWPORT render_viewport;

ID3D11SamplerState* g_sampler;
Texture tile_atlas_01 = {};
Texture dude_01 = {};
FLOAT clear_color[] = { 1.0f, 0.0f, 1.0f, 1.0f };

ID3D11VertexShader* text_ui_vertex_shader = nullptr;
ID3D11PixelShader* text_ui_pixel_shader = nullptr;
ID3D11Buffer* text_ui_vertex_buffer = nullptr;
ID3D11InputLayout* text_ui_input_layout = nullptr;

ID3D11VertexShader* rectangle_vertex_shader = nullptr;
ID3D11PixelShader* rectangle_pixel_shader = nullptr;
ID3D11Buffer* rectangle_vertex_buffer = nullptr;
ID3D11InputLayout* rectangle_input_layout = nullptr;

ID3D11VertexShader* rectangle_2d_vertex_shader = nullptr;
ID3D11PixelShader* rectangle_2d_pixel_shader = nullptr;
ID3D11Buffer* rectangle_2d_vertex_buffer = nullptr;
ID3D11InputLayout* rectangle_2d_input_layout = nullptr;
u64 buffered_rectangle_2d_vertex_count = 0;

char cstr_buffer_256[STR_BUFFER_COUNT] = {};
CStrBuffer temp_cstr = {
    .buffer = cstr_buffer_256,
    .size = STR_BUFFER_COUNT
};

wchar_t wstr_buffer_256[STR_BUFFER_COUNT] = {};
WStrBuffer temp_wstr {
    .buffer = wstr_buffer_256,
    .size = STR_BUFFER_COUNT
};

const wchar_t* viewport_window_class_name = L"ViewportWindowClass";
const wchar_t* window_class_name = L"MyWindowClass";
const wchar_t* window_title = L"Finite Engine";

Camera2D viewport_camera = {
    .position = {0, 0},
    .zoom = 10.0f,
};

// --------------------------
// Function implementations

void LoadGlobalFonts() {
    float debug_font_size = g_window.GetVHInPx(debug_font_vh_size);
    g_debug_font = LoadFontAtlas((char*)"G:\\projects\\game\\finite-engine-dev\\resources\\fonts\\Roboto-Light.ttf", debug_font_size);
}

void LoadTextureFromFilepath(Texture* texture, char* filepath) {
    HRESULT hr;
    int image_x = 0;
    int image_y = 0;
    int image_channels = 0;
    int bytes_per_pixel = 4;

    auto image = stbi_load(filepath, &image_x, &image_y, &image_channels, bytes_per_pixel);

    if (!image) {
       ErrorMessageAndBreak((wchar_t*)L"stbi_load failed");
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = image_x;
    textureDesc.Height = image_y;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA img_subresource_data = {};
    img_subresource_data.pSysMem = image;
    img_subresource_data.SysMemPitch = static_cast<UINT>(bytes_per_pixel * image_x);

    ID3D11Texture2D* texture_2d;
    hr = id3d11_device->CreateTexture2D(&textureDesc, &img_subresource_data, &texture_2d);

    if (FAILED(hr)) {
        ErrorMessageAndBreak((wchar_t*)L"Failed to create texture");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = id3d11_device->CreateShaderResourceView(texture_2d, &srvDesc, &texture->resource_view);

    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Failed to create shader resource view");
    }

    texture->x = image_x;
    texture->y = image_y;
    texture->channels = image_channels;

    stbi_image_free(image); // Free filedata
}

void _ErrorMessageAndBreak(wchar_t* message) {
    MessageBoxW(NULL, message, L"Error", MB_ICONERROR | MB_OK);
#ifdef DEBUG
    __debugbreak();
#else
    exit(1);
#endif
}

void ErrorMessageAndBreak(wchar_t* message) {
    _ErrorMessageAndBreak(message);
}

void ErrorMessageAndBreak(char* message) {
    wchar_t* wide_message = temp_wstr.GetWStrBuffer();
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, STR_BUFFER_COUNT);
    _ErrorMessageAndBreak(wide_message);
}

void DebugMessage(char* message) {
#ifdef DEBUG
    wchar_t* wide_message = temp_wstr.GetWStrBuffer();
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, STR_BUFFER_COUNT);
    OutputDebugStringW(wide_message);
#endif
}

void DebugMessage(wchar_t* message) {
#ifdef DEBUG
    OutputDebugStringW(message);
#endif
}

void StrToWideStr(char* str, wchar_t* wresult, int str_count) {
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wresult, str_count);
}

/**
 * @brief Check last DirectX  Shader compilation error from HRESULT and recieves error message from ID3DBlob pointer.
 * If error was found, breaks execution and prints error message.
 */
void CheckShaderCompileError(HRESULT hr, ID3DBlob* error_blob) {
    if (SUCCEEDED(hr)) {
        error_blob = nullptr;
        return;
    }

    if (error_blob) {
        OutputDebugStringA((char*)error_blob->GetBufferPointer());
        ErrorMessageAndBreak((char*)error_blob->GetBufferPointer());
    }

    ErrorMessageAndBreak((char*)"ERROR from: CheckDirecXError()");
}

bool IsKeyPressed(int key) {
    SHORT state = GetAsyncKeyState(key);
    return (state & 0x8000) != 0;
}

void ResizeViewport(int width, int height) {
    HRESULT hr;
    renderTargetView->Release();
    renderTargetView = nullptr;

    hr = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Resize ResizeBuffers() failed.");
    }

    ID3D11Texture2D* backBuffer = nullptr;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Resize GetBuffer() failed.");
    }

    hr = id3d11_device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    if (FAILED(hr)) {
         ErrorMessageAndBreak((char*)"Resize CreateRenderTargetView failed.");
    }

    backBuffer->Release();
    deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

    auto buffer = temp_wstr.GetWStrBuffer();
    wprintf(buffer, "Viewport resize event, x: %d, y: %d\n", width, height);
    DebugMessage(buffer);
}

void WindowResizeEvent() {
    ResizeViewport(g_window.size_px.x, g_window.size_px.y);
    LoadGlobalFonts();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_window.original_style = GetWindowLong(hwnd, GWL_STYLE);
            GetWindowRect(hwnd, &g_window.original_rect);
            break;
        }

        case WM_COMMAND: 
            break;

        case WM_KEYDOWN: {
            if (wParam == VK_F11) {
                DWORD style = GetWindowLong(hwnd, GWL_STYLE);
                if (style & WS_POPUP) {
                    // Restore original style and position
                    SetWindowLong(hwnd, GWL_STYLE, g_window.original_style);
                    SetWindowPos(hwnd, HWND_TOP, g_window.original_rect.left, g_window.original_rect.top, 
                                 g_window.original_rect.right - g_window.original_rect.left, 
                                 g_window.original_rect.bottom - g_window.original_rect.top,
                                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    g_window.size_px.x = g_window.original_rect.right - g_window.original_rect.left;
                    g_window.size_px.y = g_window.original_rect.bottom - g_window.original_rect.top;
                }
                else {
                    // Set to fullscreen
                    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
                    SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), 
                                 GetSystemMetrics(SM_CYSCREEN), 
                                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    g_window.size_px.x =  GetSystemMetrics(SM_CXSCREEN);
                    g_window.size_px.y = GetSystemMetrics(SM_CYSCREEN);
                }

                WindowResizeEvent();
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_window.mousewheel_delta = delta;
            break;
        }

        case WM_ENTERSIZEMOVE: {
            g_window.is_resizing = true;
            break;
        }

        case WM_EXITSIZEMOVE: {
            g_window.is_resizing = false;
            if (id3d11_device && swapChain) {
                if (g_window.size_px.x != g_window.new_size_px.x || g_window.size_px.y != g_window.new_size_px.y) {
                    g_window.size_px.x = g_window.new_size_px.x;
                    g_window.size_px.y = g_window.new_size_px.y;
                    WindowResizeEvent();
                }
            }
            break;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                g_window.new_size_px.x = (i32)width;
                g_window.new_size_px.y = (i32)height;
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

bool isFullscreen = false;

/**
 * @brief Program main entry.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ---------------------------------
    // Register and create window class
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = window_class_name;
        wc.style = 0;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszMenuName = NULL;

        if (!RegisterClassW(&wc)) {
            MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }

        DWORD style, exStyle;
        int xPos, yPos;

        if (isFullscreen) {
            style = WS_POPUP;
            exStyle = WS_EX_APPWINDOW;

            g_window.size_px.x = GetSystemMetrics(SM_CXSCREEN);
            g_window.size_px.y = GetSystemMetrics(SM_CYSCREEN);
            xPos = 0;
            yPos = 0;
        }
        else {
            style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
            exStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

            g_window.size_px.x = WINDOW_DEFAULT_WIDTH;
            g_window.size_px.y = WINDOW_DEFAULT_HEIGHT;

            // Center the window on the screen
            xPos = (GetSystemMetrics(SM_CXSCREEN) - g_window.size_px.x) / 2;
            yPos = (GetSystemMetrics(SM_CYSCREEN) - g_window.size_px.y) / 2;
        }

        RECT windowRect = {0, 0, g_window.size_px.x, g_window.size_px.y};
        AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

        g_window.handle = CreateWindowExW(
            exStyle, window_class_name, window_title, style,
            xPos, yPos, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
            NULL, NULL, hInstance, NULL);

        if (g_window.handle == NULL) {
            ErrorMessageAndBreak((char*)"Window Creation Failed!");
        }
    }

    // -------------
    // Init XAudio
    {
        HRESULT hr;
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
             ErrorMessageAndBreak((char*)"Failed to initialize CoInitializeEx.");
        }

        hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"Failed to initialize XAudio2.");
        }

        hr = pXAudio2->CreateMasteringVoice(&pMasterVoice);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"Failed to create mastering voice.");
        }
    }

    // ------------
    // Init timer

    QueryPerformanceFrequency(&g_window.frequency);
    QueryPerformanceCounter(&g_window.last_frame_time);

    // ----------------
    // Init DirectX 11
    {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = g_window.size_px.x;
        scd.BufferDesc.Height = g_window.size_px.y;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        scd.OutputWindow = g_window.handle;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
            nullptr, 0, D3D11_SDK_VERSION, &scd,
            &swapChain, &id3d11_device, nullptr, &deviceContext);

        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"D3D11CreateDeviceAndSwapChain Failed!");
        }

        ID3D11Texture2D* backBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
        id3d11_device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
        backBuffer->Release();
        
        deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
    }

    // -----------------------
    // Configure blend state
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        D3D11_RENDER_TARGET_BLEND_DESC& renderTargetBlendDesc = blendDesc.RenderTarget[0];
        renderTargetBlendDesc.BlendEnable = TRUE;
        renderTargetBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        renderTargetBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        renderTargetBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
        renderTargetBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
        renderTargetBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
        renderTargetBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        renderTargetBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ID3D11BlendState* blendState = nullptr;
        HRESULT hr = id3d11_device->CreateBlendState(&blendDesc, &blendState);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateBlendState failed!");
        }

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        UINT sampleMask = 0xffffffff;
        deviceContext->OMSetBlendState(blendState, blendFactor, sampleMask);
    }

    // -----------------------
    // Create global sampler
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        
        HRESULT hr = id3d11_device->CreateSamplerState(&samplerDesc, &g_sampler);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"Failed to create sampler state.");
        }
    }

    // -------------------------
    // Create constant buffers
    {
        HRESULT hresult;

        // View projection matrix b0
        D3D11_BUFFER_DESC viewProjectionMatrixBufferDesc = {};
        viewProjectionMatrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        viewProjectionMatrixBufferDesc.ByteWidth = sizeof(ViewProjectionMatrixBufferType);
        viewProjectionMatrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        viewProjectionMatrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hresult = id3d11_device->CreateBuffer(&viewProjectionMatrixBufferDesc, nullptr, &cbuffer_view_projection);
        if (FAILED(hresult)) {
            ErrorMessageAndBreak((char*)"CreateBuffer ViewProjectionMatrixBufferType failed!");
        }
    }

    // --------------------------
    // Create rectangle shader
    {
        HRESULT hr;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* error_blob = nullptr;

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\rectangle.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain", "vs_5_0", 0, 0, &vsBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\rectangle.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain", "ps_5_0", 0, 0, &psBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = id3d11_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &rectangle_vertex_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateVertexShader failed!");
        }

        hr = id3d11_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &rectangle_pixel_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreatePixelShader failed!");
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0,    DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = id3d11_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &rectangle_input_layout);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateInputLayout failed!");
        }

        vsBlob->Release();
        psBlob->Release();

        // -----------------------
        // Create dynamic buffer
        {
            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexBufferDesc.ByteWidth = sizeof(RectangleVertex) * MAX_TEXT_UI_VERTEX_COUNT;
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            hr = id3d11_device->CreateBuffer(&vertexBufferDesc, nullptr, &rectangle_vertex_buffer);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"CreateBuffer for rectangle_vertex_buffer dynamic vertex buffer failed!");
            }
        }
    }

    // -----------------------------------------
    // Create rectangle 2D shader (with batch)
    {
        HRESULT hr;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* error_blob = nullptr;

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\rectangle_2d.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain", "vs_5_0", 0, 0, &vsBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\rectangle_2d.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain", "ps_5_0", 0, 0, &psBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = id3d11_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &rectangle_2d_vertex_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateVertexShader failed!");
        }

        hr = id3d11_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &rectangle_2d_pixel_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreatePixelShader failed!");
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = id3d11_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &rectangle_2d_input_layout);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateInputLayout failed!");
        }

        vsBlob->Release();
        psBlob->Release();

        // -----------------------
        // Create dynamic buffer
        {
            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexBufferDesc.ByteWidth = sizeof(RectangleVertex) * MAX_TEXT_UI_VERTEX_COUNT;
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            hr = id3d11_device->CreateBuffer(&vertexBufferDesc, nullptr, &rectangle_2d_vertex_buffer);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"CreateBuffer for rectangle_vertex_buffer dynamic vertex buffer failed!");
            }
        }
    }

    // -----------------------
    // Create font_ui shader
    {
        HRESULT hr;
        ID3DBlob* error_blob = nullptr;
        ID3DBlob* pVSBlob = nullptr;
        ID3DBlob* pPSBlob = nullptr;

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\text_ui.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", 0, 0, &pVSBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\text_ui.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", 0, 0, &pPSBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = id3d11_device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &text_ui_vertex_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateVertexShader font_ui failed!");
        }

        hr = id3d11_device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &text_ui_pixel_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreatePixelShader font_ui failed!");
        }

        // Define the input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        // Create the input layout
        hr = id3d11_device->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &text_ui_input_layout);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateInputLayout font_ui failed!");
        }

        pVSBlob->Release();
        pPSBlob->Release();

        // -----------------------
        // Create dynamix text_ui buffer
        {
            D3D11_BUFFER_DESC vertexBufferDesc = {};
            vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexBufferDesc.ByteWidth = sizeof(TextUiVertex) * MAX_TEXT_UI_VERTEX_COUNT;
            vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            hr = id3d11_device->CreateBuffer(&vertexBufferDesc, nullptr, &text_ui_vertex_buffer);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"CreateBuffer for text_ui dynamic vertex buffer failed!");
            }
        }
    }

    // -------------------------------
    // Audio source voice (channels)
    {
        WAVHeader wav_sound_header = {};
        BYTE* wav_audio_data1 = LoadWAWFile((LPWSTR)L"G:\\projects\\game\\finite-engine-dev\\resources\\sounds\\Jump.wav", &wav_sound_header);
        sound_buffer_1.data = wav_audio_data1;
        sound_buffer_1.size_bytes = wav_sound_header.dataSize;

        BYTE* wav_audio_data2 = LoadWAWFile((LPWSTR)L"G:\\projects\\game\\finite-engine-dev\\resources\\sounds\\Laser_Shoot.wav", &wav_sound_header);
        sound_buffer_2.data = wav_audio_data2;
        sound_buffer_2.size_bytes = wav_sound_header.dataSize;

        BYTE* wav_audio_data3 = LoadWAWFile((LPWSTR)L"G:\\projects\\game\\finite-engine-dev\\resources\\sounds\\Pickup_Coin.wav", &wav_sound_header);
        sound_buffer_3.data = wav_audio_data3;
        sound_buffer_3.size_bytes = wav_sound_header.dataSize;

        WAVEFORMATEX wfx = { 0 };
        wfx.wFormatTag = wav_sound_header.audioFormat;
        wfx.nChannels = wav_sound_header.numChannels;
        wfx.nSamplesPerSec = wav_sound_header.sampleRate;
        wfx.nAvgBytesPerSec = wav_sound_header.byteRate;
        wfx.nBlockAlign = wav_sound_header.blockAlign;
        wfx.wBitsPerSample = wav_sound_header.bitsPerSample;
        wfx.cbSize = 0;

        for (int i = 0; i < MAX_VOICES; ++i) {
            HRESULT hr = pXAudio2->CreateSourceVoice(&mono_source_voice_pool[i], &wfx);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"Failed to create source voice.");
            }
        }
    }

    LoadTextureFromFilepath(&tile_atlas_01, (char*)"G:\\projects\\game\\finite-engine-dev\\resources\\images\\tiles_01.png");
    LoadTextureFromFilepath(&dude_01, (char*)"G:\\projects\\game\\finite-engine-dev\\resources\\images\\dude_01.png");

    ShowWindow(g_window.handle, nCmdShow);
    UpdateWindow(g_window.handle);

    LoadGlobalFonts();

    // -----------
    // Game loop
    MSG window_message = {};
    while (window_message.message != WM_QUIT) {
        // --------------
        // Handle input
        {
            g_window.mousewheel_delta = 0;

            if (PeekMessage(&window_message, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&window_message);
                DispatchMessageW(&window_message);
            }

            Vec2f mouse_pos = g_window.GetWindowMousePosition();
            frame_input.mouse_x = mouse_pos.x;
            frame_input.mouse_y = mouse_pos.y;

            if (g_window.IsWindowFocused()) {
                int keys_struct_size = sizeof(GameKeys);
                int keys_size = sizeof(KeyInputState);
                int keys_count = keys_struct_size / keys_size;

                auto keys_ptr = (KeyInputState*)&frame_input.keys;

                for (int i = 0; i < keys_count; i++) {
                    KeyInputState* key = &keys_ptr[i];
                    if (IsKeyPressed(key->keycode)) {
                        key->pressed = key->is_down ? false : true;
                        key->is_down = true;
                    }
                    else {
                        key->is_down = false;
                        key->pressed = false;
                    }
                }   

                frame_input.mousewheel_up = false;
                frame_input.mousewheel_down = false;

                if (g_window.mousewheel_delta > 0) {
                    frame_input.mousewheel_up = true;
                }   
                else if (g_window.mousewheel_delta < 0) {
                    frame_input.mousewheel_down = true;
                }

                if (IsKeyPressed(VK_ESCAPE)) {
                    PostQuitMessage(0);
                }
            }
        }

        // -------------
        // Query timer
        {
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);

            LONGLONG elapsedTicks = currentTime.QuadPart - g_window.last_frame_time.QuadPart;
            g_window.frame_delta = static_cast<f32>(elapsedTicks) / g_window.frequency.QuadPart;
            g_window.last_frame_time = currentTime;
        }

        // -------------
        // Scene logic
        {
            if (frame_input.mousewheel_down) {
                viewport_camera.zoom += 1.0f;
            }
            else if (frame_input.mousewheel_up) {
                viewport_camera.zoom -= 1.0f;
                if (viewport_camera.zoom <= 1.0f) {
                    viewport_camera.zoom = 1.0f;
                }
            }

            f32 camera_speed = 15.0f;

            if (frame_input.keys.arrow_down.is_down) {
                viewport_camera.position.y -= camera_speed * g_window.frame_delta;
            }
            if (frame_input.keys.arrow_up.is_down) {
                viewport_camera.position.y += camera_speed * g_window.frame_delta;
            }
            if (frame_input.keys.arrow_left.is_down) {
                viewport_camera.position.x -= camera_speed * g_window.frame_delta;
            }
            if (frame_input.keys.arrow_right.is_down) {
                viewport_camera.position.x += camera_speed * g_window.frame_delta;
            }

            if (frame_input.keys._1.is_down) {
                if (CursorOverTilemap()) {
                    Tile* cursor_tile = GetCursorTilePtr();
                    cursor_tile->type = 0;
                }
            }
            if (frame_input.keys._2.is_down) {
                if (CursorOverTilemap()) {
                    Tile* cursor_tile = GetCursorTilePtr();
                    cursor_tile->type = 1;
                }
            }
            if (frame_input.keys._3.is_down) {
                if (CursorOverTilemap()) {
                    Tile* cursor_tile = GetCursorTilePtr();
                    cursor_tile->type = 2;
                }
            }

            // ----------------------------
            // Get mouse in tilemap coords
            {
                DirectX::XMMATRIX viewMatrix = GetViewportViewMatrix();
                DirectX::XMMATRIX projection = GetViewportProjectionMatrix();
                DirectX::XMMATRIX viewProjInverse = DirectX::XMMatrixInverse(nullptr, XMMatrixMultiply(viewMatrix, projection));

                float normalizedX = (2.0f * frame_input.mouse_x) / g_window.new_size_px.x - 1.0f;
                float normalizedY = 1.0f - (2.0f * frame_input.mouse_y) / g_window.new_size_px.y;

                DirectX::XMVECTOR mouseNDC = DirectX::XMVectorSet(normalizedX, normalizedY, 1.0f, 1.0f);
                DirectX::XMVECTOR worldPosition = XMVector3TransformCoord(mouseNDC, viewProjInverse);

                f32 mx = DirectX::XMVectorGetX(worldPosition);
                f32 my = DirectX::XMVectorGetY(worldPosition);

                Vec2f tileCoord = ScreenSpaceToTilemapCoords(Vec2f{mx, my});
                frame_input.mouse_tilemap_x = (int)tileCoord.x;
                frame_input.mouse_tilemap_y = (int)tileCoord.y;
            }
        }

        if (frame_input.keys.a.pressed) {
            PlayMonoSound(sound_buffer_1);
        }
        if (frame_input.keys.s.pressed) {
            PlayMonoSound(sound_buffer_2);
        }
        if (frame_input.keys.d.pressed) {
            PlayMonoSound(sound_buffer_3);
        }

        // -----------------------
        // Render viewport frame
        {
            deviceContext->ClearRenderTargetView(renderTargetView, clear_color);
            SetDefaultViewportDimensions();
            deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

            // -----------------
            // Update cbuffers
            {
                DirectX::XMMATRIX viewMatrix = GetViewportViewMatrix();
                DirectX::XMMATRIX projectionMatrix = GetViewportProjectionMatrix();
                DirectX::XMMATRIX view_projection_matrix = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);

                ViewProjectionMatrixBufferType viewProjection = {
                    .view_projection_matrix = DirectX::XMMatrixTranspose(view_projection_matrix),
                };

                // -------------------------------
                // Update view projection matrix
                {
                    D3D11_MAPPED_SUBRESOURCE mappedResource;
                    HRESULT hr = deviceContext->Map(cbuffer_view_projection, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                    if (FAILED(hr)) {
                        ErrorMessageAndBreak((char*)"deviceContext->Map() ViewProjectionMatrixBufferType failed!");
                    }

                    ViewProjectionMatrixBufferType* view_projection_data_ptr = (ViewProjectionMatrixBufferType*)mappedResource.pData;
                    view_projection_data_ptr->view_projection_matrix = viewProjection.view_projection_matrix;
                    deviceContext->Unmap(cbuffer_view_projection, 0);
                    deviceContext->VSSetConstantBuffers(BUFFER_SLOT_VIEW_PROJECTION, 1, &cbuffer_view_projection);
                }
            }

            // -----------------
            // Draw background
            DrawRectangleToScreen({-1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 0.0f, 0.0f});

            // ---------------
            // Draw tilemaps
            {
                for (int y = 0; y < g_tilemap.height; y++) {
                    for (int x = 0; x < g_tilemap.width; x++) {
                        // Vec2f coordinate = {(f32)x, f32(y)};
                        // Tile tile = tilemap_data[x + (y * g_tilemap.width)];
                        // DrawTilemapTile(texture.resource_view, coordinate);
                    }
                }
            }

            if (CursorOverTilemap()) {
                // DrawTilemapTile(selector_tile.resource_view, {(f32)frame_input.mouse_tilemap_x, (f32)frame_input.mouse_tilemap_y});
            }

            BufferRectangle2d({0.5f, 0.5f});
            BufferRectangle2d({0.25f, -0.25f});
            DrawBufferedRectangle2ds(tile_atlas_01.resource_view);

            DrawLineOnScreen({-0.025f, 0.0f}, {0.025f, 0.0f}, 1.0f, {1.0f, 1.0f, 1.0f});
            DrawLineOnScreen({0.0f, -0.025f}, {0.0f, 0.025f}, 1.0f, {1.0f, 1.0f, 1.0f});

            // ---------------
            // Display debug
            {
                SetDefaultViewportDimensions();

                DrawRectangleToScreen({-1.0f, 1.0f}, {-0.5f, 1.0f}, {-1.0f, 0.75}, {-0.5f, 0.75f}, {0.2f, 0.2f, 0.2f});

                Vec2f cursor01 = {0.0f, 0.0f};
                char* d_str = temp_cstr.GetCStrBuffer();
                sprintf(d_str, "Frames: %llu\n", g_window.frame_counter);
                cursor01 = { 5.0f, g_window.GetVHInPx(debug_font_vh_size) };
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);

                temp_cstr.MemsetBuffer(0);
                sprintf(d_str, "Window width: %d, Window height: %d\n", (int)g_window.size_px.x, (int)g_window.size_px.y);
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);

                temp_cstr.MemsetBuffer(0);
                sprintf(d_str, "Mouse x: %d, Mouse y: %d\n", (int)frame_input.mouse_x, (int)frame_input.mouse_y);
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);

                temp_cstr.MemsetBuffer(0);
                sprintf(d_str, "Mouse tilemap x: %d, Mouse tilemap y: %d\n", frame_input.mouse_tilemap_x, frame_input.mouse_tilemap_y);
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);

                temp_cstr.MemsetBuffer(0);
                sprintf(d_str, "Camera x: %.1f, Camera y: %.1f, Camera zoom: %.2f\n", viewport_camera.position.x, viewport_camera.position.y, viewport_camera.zoom);
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);

                temp_cstr.MemsetBuffer(0);
                sprintf(d_str, "Draw calls: %d\n", g_window.frame_draw_calls);
                cursor01 = DrawTextToScreen((char*)d_str, cursor01, &g_debug_font);
            }

            swapChain->Present(1, 0);
        }

        g_window.frame_counter++;
        g_window.frame_draw_calls = 0;
    }

    return window_message.wParam;
}

FontAtlasInfo LoadFontAtlas(char* filepath, float pixel_height) {
    FontAtlasInfo result = {};

    stbtt_fontinfo font;
    size_t file_size;
    auto wide_buffer = temp_wstr.GetWStrBuffer();
    StrToWideStr(filepath, wide_buffer, STR_BUFFER_COUNT);
    unsigned char *fontBuffer = LoadFileToPtr(wide_buffer, &file_size);

    stbtt_InitFont(&font, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

    int used_height = (int)pixel_height;
    float scale = stbtt_ScaleForPixelHeight(&font, (float)used_height);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        
    result.font_size_px = used_height;
    result.font_ascent = ascent * scale;
    result.font_descent = descent * scale;
    result.font_linegap = lineGap * scale;

    int fontAtlasWidth = 0;
    int fontAtlasHeight = 0;

    for (int c = 32; c < 128; c++) {
        int codepoint = c;
        int width, height, xoffset, yoffset;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, codepoint, &width, &height, &xoffset, &yoffset);

        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advanceWidth, &leftSideBearing);
        float advanceScaled = ((f32)advanceWidth * scale);      

        int glyph_index = c - 32;
        result.glyphs[glyph_index].advance = advanceScaled;
        result.glyphs[glyph_index].bitmap_height = height;
        result.glyphs[glyph_index].bitmap_width = width;
        result.glyphs[glyph_index].character = c;
        result.glyphs[glyph_index].x_offset = xoffset;
        result.glyphs[glyph_index].y_offset = -1 * yoffset;

        fontAtlasWidth += width;
        if (fontAtlasHeight < height) {
            fontAtlasHeight = height;
        }

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    result.font_atlas_width = fontAtlasWidth;
    result.font_atlas_height = fontAtlasHeight;

    int atlasX = 0;
    unsigned char* font_atlas_buffer = (unsigned char*)calloc(fontAtlasHeight * fontAtlasWidth, sizeof(unsigned char));

    for (int c = 32; c < 128; c++) {
        int codepoint = c;
        int width, height, xoffset, yoffset;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, codepoint, &width, &height, &xoffset, &yoffset);

        for (int row = 0; row < height; row++) {
            int dest_index = (result.font_atlas_width * row) + atlasX;
            unsigned char *dest = &font_atlas_buffer[dest_index];
            int src_index = width * row;
            unsigned char *src = &bitmap[src_index];
            memcpy(dest, src, width);
        }

        f32 uv_x0 = (f32)atlasX / (f32)result.font_atlas_width;
        f32 uv_y0 = 0.0f;
        f32 uv_x1 = uv_x0 + (f32)width / (f32)result.font_atlas_width;
        f32 uv_y1 = (f32)height / (f32)result.font_atlas_height;

        int glyph_index = c - 32;
        result.glyphs[glyph_index].uv_x0 = uv_x0;
        result.glyphs[glyph_index].uv_y0 = uv_y0;
        result.glyphs[glyph_index].uv_x1 = uv_x1;
        result.glyphs[glyph_index].uv_y1 = uv_y1;

        atlasX += width;
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    result.glyphs[32].advance = result.glyphs['M' - 32].bitmap_width / 2.0f; // Spacebar

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = result.font_atlas_width;
    desc.Height = result.font_atlas_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = font_atlas_buffer;
    initData.SysMemPitch = result.font_atlas_width; // The distance in bytes between the start of each line of the texture
        
    ID3D11Texture2D* font_texture = nullptr;
    HRESULT hr = id3d11_device->CreateTexture2D(&desc, &initData, &font_texture);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"CreateTexture2D font atlas failed!");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = id3d11_device->CreateShaderResourceView(font_texture, &srvDesc, &result.texture);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"CreateShaderResourceView font atlas failed!");
    }

    font_texture->Release();

    auto buffer = temp_wstr.GetWStrBuffer();
    wprintf(buffer, "Font loaded with texture atlas => width: %d, height: %d\n", result.font_atlas_width, result.font_atlas_height);
    DebugMessage(buffer);

    return result;
}

DirectX::XMMATRIX GetViewportViewMatrix() {
    // Give camera position in negative values for translation to keep logical data for game
    DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(-viewport_camera.position.x, -viewport_camera.position.y, 0.0f);
    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
    viewMatrix = XMMatrixMultiply(viewMatrix, translationMatrix);
    return viewMatrix;
}

DirectX::XMMATRIX GetViewportProjectionMatrix() {
    float nearPlane = 0.0f;
    float farPlane = 10.0f;
    f32 aspect_ratio = (f32)g_window.size_px.x / (f32)g_window.size_px.y;
    float viewWidth = 2.0f * viewport_camera.zoom;
    float viewHeight = viewWidth / aspect_ratio;

    DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicLH(viewWidth, viewHeight, nearPlane, farPlane);
    return projectionMatrix;
}

void PlayMonoSound(Buffer audio_buffer) {
    IXAudio2SourceVoice* pVoice = nullptr;
    for (int i = 0; i < MAX_VOICES; ++i) {
        XAUDIO2_VOICE_STATE state;
        mono_source_voice_pool[i]->GetState(&state);
        if (state.BuffersQueued == 0) {
            pVoice = mono_source_voice_pool[i];
            break;
        }
    }

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = audio_buffer.size_bytes;
    buffer.pAudioData = audio_buffer.data;
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (pVoice != nullptr) {
        HRESULT hr = pVoice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"Failed to submit source buffer.");
        }

        hr = pVoice->Start(0);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"Failed to start the source voice.");
        }
    }
}

f32 GetTextWidthPx(char* text, FontAtlasInfo* font_info) {
    f32 longest = 0.0f;
    f32 width = 0.0f;

    for (char* p = (char*)text; *p != '\0'; p++) {
        char c = *p;
        if (c == '\n') {
            if (longest < width) {
                longest = width;
                width = 0.0f;
            }
        }

        FontGlyphInfo glyph = font_info->glyphs[c - 32];
        width += glyph.advance;
    }

    if (longest < width) {
        longest = width;
    }
    return longest;
}

Vec2f DrawTextToScreen(char* text, Vec2f screen_pos, FontAtlasInfo* font_info) {
    Vec2f cursor = {
        .x = screen_pos.x,
        .y = screen_pos.y
    };

    Vec2f cursor_original = {
        .x = screen_pos.x,
        .y = screen_pos.y
    };
    
    for (char* p = (char*)text; *p != '\0'; p++) {
        char c = *p;

        if (c == '\n') {
            cursor.x = cursor_original.x;
            cursor.y += font_info->font_size_px;
            continue;
        }

        FontGlyphInfo glyph = font_info->glyphs[c - 32];

        i32 px_x0 = cursor.x + glyph.x_offset;
        i32 px_x1 = px_x0 + glyph.bitmap_width;
        i32 px_y0 = cursor.y - glyph.y_offset;
        i32 px_y1 = px_y0 + glyph.bitmap_height;

        auto top_left = g_window.ScreenPxToNDC({px_x0, px_y0});
        auto top_right = g_window.ScreenPxToNDC({px_x1, px_y0});
        auto bot_left = g_window.ScreenPxToNDC({px_x0, px_y1});
        auto bot_right = g_window.ScreenPxToNDC({px_x1, px_y1});

        TextUiVertex vertices[] = {
            { DirectX::XMFLOAT4(top_left.x, top_left.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x0, glyph.uv_y0) },  // Top-left
            { DirectX::XMFLOAT4(top_right.x, top_right.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x1, glyph.uv_y0) },   // Top-right
            { DirectX::XMFLOAT4(bot_left.x, bot_left.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x0, glyph.uv_y1) }, // Bottom-lef   
            { DirectX::XMFLOAT4(bot_left.x, bot_left.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x0, glyph.uv_y1) }, // Bottom-left
            { DirectX::XMFLOAT4(top_right.x, top_right.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x1, glyph.uv_y0) },   // Top-right
            { DirectX::XMFLOAT4(bot_right.x, bot_right.y, 1.0f, 1.0f), DirectX::XMFLOAT2(glyph.uv_x1, glyph.uv_y1) }   // Bottom-right
        };

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = deviceContext->Map(text_ui_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (FAILED(hr)) {
             ErrorMessageAndBreak((char*)"Map for text_ui dynamic vertex buffer failed!");
        }

        memcpy(mappedResource.pData, vertices, sizeof(TextUiVertex) * 6);
        deviceContext->Unmap(text_ui_vertex_buffer, 0);

        deviceContext->IASetInputLayout(text_ui_input_layout);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        deviceContext->VSSetShader(text_ui_vertex_shader, nullptr, 0);
        deviceContext->PSSetShader(text_ui_pixel_shader, nullptr, 0);

        deviceContext->PSSetShaderResources(0, 1, &font_info->texture);
        deviceContext->PSSetSamplers(0, 1, &g_sampler);

        UINT stride = sizeof(TextUiVertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &text_ui_vertex_buffer, &stride, &offset);
        deviceContext->Draw(6, 0);
        g_window.frame_draw_calls++;

        cursor.x += glyph.advance;
    }

    if (cursor.x < 0) {
        ErrorMessageAndBreak((char*)"Cursor x less than 0");
    }

    if (cursor.y < 0) {
        ErrorMessageAndBreak((char*)"Cursor y less than 0");
    }

    return cursor;
}

BYTE* LoadWAWFile(wchar_t* filePath, WAVHeader* wavHeader) {
    HANDLE hFile;
    DWORD bytesRead;

    hFile = CreateFileW(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ErrorMessageAndBreak((char*)"Error opening file.");
    }

    if (!ReadFile(hFile, wavHeader, sizeof(WAVHeader), &bytesRead, NULL)) {
        ErrorMessageAndBreak((char*)"Error reading file.\n");
    }

    if (memcmp(wavHeader->riffHeader, "RIFF", 4) != 0 || memcmp(wavHeader->waveHeader, "WAVE", 4) != 0) {
        ErrorMessageAndBreak((char*)"Invalid WAV file.");
    }

    BYTE* audioData = (BYTE*)GlobalAlloc(GMEM_FIXED, wavHeader->dataSize);
    if (audioData == NULL) {
        ErrorMessageAndBreak((char*)"Memory allocation failed.");
    }

    if (!ReadFile(hFile, audioData, wavHeader->dataSize, &bytesRead, NULL)) {
        ErrorMessageAndBreak((char*)"Error reading audio data.");
    }

    CloseHandle(hFile);
    return audioData;
}

Vec2f ScreenSpaceToTilemapCoords(Vec2f screen_coord) {
    float x_tile = (screen_coord.x + 2.0f * screen_coord.y) / 2.0f;
    float y_tile = (2.0f * screen_coord.y - screen_coord.x) / 2.0f;
    Vec2f result = {x_tile, y_tile};
    return result;
}

Vec2f TilemapCoordsToIsometricScreenSpace(Vec2f tilemap_coord) {
    const float y_offset = 0.5f; // So that 0,0 coord is the bottom corner of tile
    float x = (tilemap_coord.y * (-1.0f)) + (tilemap_coord.x * (1.0f));
    float y = (tilemap_coord.y * (0.5f))  + (tilemap_coord.x * (0.5f)) + y_offset;
    Vec2f result = {x, y};
    return result;
}

void DrawBufferedRectangle2ds(ID3D11ShaderResourceView* texture) {
    UINT stride = sizeof(TilemapTileVertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &rectangle_2d_vertex_buffer, &stride, &offset);
    deviceContext->IASetInputLayout(rectangle_2d_input_layout);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->VSSetShader(rectangle_2d_vertex_shader, nullptr, 0);
    deviceContext->PSSetShader(rectangle_2d_pixel_shader, nullptr, 0);

    deviceContext->PSSetShaderResources(0, 1, &texture);
    deviceContext->PSSetSamplers(0, 1, &g_sampler);

    deviceContext->Draw(buffered_rectangle_2d_vertex_count, 0);
    g_window.frame_draw_calls++;
    buffered_rectangle_2d_vertex_count = 0;
}

void BufferRectangle2d(Vec2f offset) {
    TilemapTileVertex vertices[] = {
        // Top Right
        { DirectX::XMFLOAT4( 0.5f + offset.x,  0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        // Bottom Left
        { DirectX::XMFLOAT4(-0.5f + offset.x, -0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
        // Top Left
        { DirectX::XMFLOAT4(-0.5f + offset.x,  0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },

        // Bottom Right
        { DirectX::XMFLOAT4( 0.5f + offset.x, -0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
        // Bottom Left
        { DirectX::XMFLOAT4(-0.5f + offset.x, -0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
        // Top Right
        { DirectX::XMFLOAT4( 0.5f + offset.x,  0.5f + offset.y, 0.0f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) }
    };

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = deviceContext->Map(rectangle_2d_vertex_buffer, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Failed to map vertex buffer!");
    }

    size_t offsetInBytes = buffered_rectangle_2d_vertex_count * sizeof(TilemapTileVertex);
    memcpy((byte*)mappedResource.pData + offsetInBytes, vertices, sizeof(vertices));

    deviceContext->Unmap(rectangle_2d_vertex_buffer, 0);
    buffered_rectangle_2d_vertex_count += 6;
}

unsigned char* LoadFileToPtr(wchar_t* filename, size_t* get_file_size) {
    HANDLE file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        ErrorMessageAndBreak((char*)"CreateFileW failed!");
    }

    DWORD fileSize = GetFileSize(file, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        ErrorMessageAndBreak((char*)"CreateFileW failed!");
    }

    unsigned char* buffer = (unsigned char*)malloc(fileSize);
    if (!buffer) {
        ErrorMessageAndBreak((char*)"CreateFileW failed!");
    }

    DWORD bytesRead;
    BOOL success = ReadFile(file, buffer, fileSize, &bytesRead, NULL);
    if (!success || bytesRead != fileSize) {
        ErrorMessageAndBreak((char*)"CreateFileW failed!");
    }

    CloseHandle(file);

    if (get_file_size) {
        *get_file_size = fileSize;
    }

    return buffer;
}

bool CursorOverTilemap() {
    if (0 <= frame_input.mouse_tilemap_x && 0 <= frame_input.mouse_tilemap_y) {
        if (frame_input.mouse_tilemap_x < g_tilemap.width && frame_input.mouse_tilemap_y < g_tilemap.height) {
            return true;
        }
    }
    return false;
}

void DrawRectangleToScreen(Vec2f top_left, Vec2f top_right, Vec2f bot_left, Vec2f bot_right, Vec3f color) {
    RectangleVertex vertices[] = {
        { DirectX::XMFLOAT4(top_left.x, top_left.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) },  // Top-left
        { DirectX::XMFLOAT4(top_right.x, top_right.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) },   // Top-right
        { DirectX::XMFLOAT4(bot_left.x, bot_left.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) }, // Bottom-left

        { DirectX::XMFLOAT4(bot_left.x, bot_left.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) }, // Bottom-left
        { DirectX::XMFLOAT4(top_right.x, top_right.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) },   // Top-right
        { DirectX::XMFLOAT4(bot_right.x, bot_right.y, 1.0f, 1.0f), DirectX::XMFLOAT4(color.x, color.y, color.z, 1.0f) }   // Bottom-right
    };

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = deviceContext->Map(rectangle_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Map for rectangle dynamic vertex buffer failed!");
    }

    memcpy(mappedResource.pData, vertices, sizeof(RectangleVertex) * 6);
    deviceContext->Unmap(rectangle_vertex_buffer, 0);
    deviceContext->IASetInputLayout(rectangle_input_layout);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->VSSetShader(rectangle_vertex_shader, nullptr, 0);
    deviceContext->PSSetShader(rectangle_pixel_shader, nullptr, 0);

    UINT stride = sizeof(RectangleVertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &rectangle_vertex_buffer, &stride, &offset);
    deviceContext->Draw(6, 0);
    g_window.frame_draw_calls++;
}

void DrawDotOnScreen(Vec2f ndc, f32 size_px, Vec3f color) {
    f32 dot_w = g_window.GetPxInVW(size_px / 2);
    f32 dot_h = g_window.GetPxInVH(size_px / 2);
    DrawRectangleToScreen({-dot_w, dot_h}, {dot_w, dot_h}, {-dot_w, -dot_h}, {dot_w, -dot_h}, {color.x, color.y, color.z});
}

void DrawLineOnScreen(Vec2f ndc_start, Vec2f ndc_end, f32 size_px, Vec3f color) {
    Vec2i start_px = g_window.NDCToScreenPx(ndc_start);
    Vec2i end_px = g_window.NDCToScreenPx(ndc_end);

    Vec2i line_vec = {start_px.x - end_px.x, start_px.y - end_px.y};
    Vec2i perpendicular = {-line_vec.y, line_vec.x};

    DirectX::XMVECTOR perpVec = DirectX::XMVectorSet(perpendicular.x, perpendicular.y, 0.0f, 0.0f);
    DirectX::XMVECTOR unit_perp_dx = DirectX::XMVector2Normalize(perpVec);
    unit_perp_dx = DirectX::XMVectorScale(unit_perp_dx, size_px);

    Vec2i perp_vec = {(i32)DirectX::XMVectorGetX(unit_perp_dx), (i32)DirectX::XMVectorGetY(unit_perp_dx) };

    Vec2i line_start_1 = {start_px.x - perp_vec.x, start_px.y - perp_vec.y};
    Vec2i line_start_2 = {start_px.x + perp_vec.x, start_px.y + perp_vec.y};
    Vec2i line_end_1 = {end_px.x - perp_vec.x, end_px.y - perp_vec.y};
    Vec2i line_end_2 = {end_px.x + perp_vec.x, end_px.y + perp_vec.y};

    auto s1 = g_window.ScreenPxToNDC(line_start_1);
    auto s2 = g_window.ScreenPxToNDC(line_start_2);
    auto e1 = g_window.ScreenPxToNDC(line_end_1);
    auto e2 = g_window.ScreenPxToNDC(line_end_2);

    DrawRectangleToScreen(s1, s2, e1, e2, {color.x, color.y, color.z});
}

Tile* GetCursorTilePtr() {
    int index = frame_input.mouse_tilemap_x + (g_tilemap.width * frame_input.mouse_tilemap_y); 
    Tile* tile = &tilemap_data[index];
    return tile;
}

void SetDefaultViewportDimensions() {
    render_viewport.Width = (float)g_window.size_px.x;
    render_viewport.Height = (float)g_window.size_px.y;
    render_viewport.TopLeftX = 0.0f;
    render_viewport.TopLeftY = 0.0f;
    render_viewport.MinDepth = 0.0f;
    render_viewport.MaxDepth = 1.0f;
    deviceContext->RSSetViewports(1, &render_viewport);
}

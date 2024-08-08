// ----------
// Includes

#include <windows.h>
#include <commdlg.h>
#include <limits.h>

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

const int MAX_TEXT_UI_VERTEX_COUNT = 1000;

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

struct Window {
    i32 width_px;
    i32 height_px;
};

struct FrameInput {
    bool mousewheel_up;
    bool mousewheel_down;
    bool arrow_up;
    bool arrow_down;
    bool arrow_left;
    bool arrow_right;
};

struct Camera2D {
    DirectX::XMFLOAT2 position;
    f32 zoom; // how many tiles can see vertically
};

struct ViewProjectionMatrixBufferType {
    DirectX::XMMATRIX view_projection_matrix;
};

struct AspectProjectionMatrixBufferType {
    DirectX::XMMATRIX aspect_projection_matrix;
};

struct ModelBufferType {
    DirectX::XMMATRIX modelMatrix;
};

struct Vec2 {
    f32 x;
    f32 y;
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
    DirectX::XMFLOAT3 position;
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

struct TilemapTile {
    Texture* texture;
};

struct Tilemap {
    i32 width;
    i32 height;
    TilemapTile* tiles;
};

struct FontGlyphInfo {
    i32 bitmap_width;
    i32 bitmap_height;
    i32 x_offset;
    i32 y_offset;
    f32 advance;
    f32 uv_x0;
    f32 uv_y0;
    f32 uv_x1;
    f32 uv_y1;
    char character;
};

struct FontAtlasInfo {
    ID3D11ShaderResourceView* texture_view = nullptr;
    i32 font_size_px;
    f32 font_ascent;
    f32 font_descent;
    f32 font_linegap;
    i32 font_atlas_width;
    i32 font_atlas_height;
    FontGlyphInfo glyphs[96] = {};
};

// -----------------------
// Function declarations

/**
 * @brief Stop application execution and display a message to the user.
 *
 * Apply debugging breakpoint if DEBUG -flag is set.
 */
void ErrorMessageAndBreak(char* message);

void DebugMessage(char* message);

void LoadTextureFromFilepath(Texture* texture, char* filepath);

bool IsKeyPressed(int key);

bool IsWindowFocused(HWND window_handle);

void DrawTilemapTile(Texture* texture, Vec2 coordinate);

void StrToWideStr(char* str, wchar_t* wresult, int str_count);

Vec2 TilemapCoordsToIsometricScreenSpace(Vec2 tilemap_coord);

unsigned char* LoadFileToPtr(wchar_t* filename, size_t* get_file_size);

Vec2 ScreenPxToNDC(int x, int y);

Vec2 DrawTextToScreen(char* text, Vec2 screen_pos, FontAtlasInfo* font_info);

FontAtlasInfo LoadFontAtlas(char* filepath, float pixel_height);

f32 GetVWInPx(f32 vw);

f32 GetVHInPx(f32 vh);

// ---------
// Globals

FontAtlasInfo g_debug_font;

Window g_main_window = {
    .width_px = 1600,
    .height_px = 1200,
};

LARGE_INTEGER g_frequency;
LARGE_INTEGER g_lastTime;

Vec2 viewport_mouse = {};

u64 frame_counter = 0;
f32 frame_delta = 0.0f;
int mousewheel_delta = 0;
FrameInput frame_input = {};

ID3D11Buffer* cbuffer_view_projection = nullptr;
ID3D11Buffer* cbuffer_aspect_projection_matrix = nullptr;
ID3D11Buffer* cbuffer_model = nullptr;

HWND main_window_handle;
MSG main_window_message;

IDXGISwapChain *swapChain;
ID3D11Device* id3d11_device;
ID3D11DeviceContext *deviceContext;
ID3D11RenderTargetView *renderTargetView;
D3D11_VIEWPORT render_viewport;

ID3D11SamplerState* g_sampler;
Texture test_texture_01 = {};
FLOAT clear_color[] = { 1.0f, 0.0f, 1.0f, 1.0f };

ID3D11VertexShader* tilemap_tile_vertex_shader;
ID3D11PixelShader* tilemap_tile_pixel_shader;
ID3D11Buffer* tilemap_tile_vertex_buffer;
ID3D11InputLayout* tilemap_tile_input_layout;

ID3D11VertexShader* text_ui_vertex_shader;
ID3D11PixelShader* text_ui_pixel_shader;
ID3D11Buffer* text_ui_vertex_buffer;
ID3D11InputLayout* text_ui_input_layout;

ID3D11VertexShader* rectangle_vertex_shader;
ID3D11PixelShader* rectangle_pixel_shader;
ID3D11Buffer* rectangle_vertex_buffer;
ID3D11InputLayout* rectangle_input_layout;

wchar_t open_filename_path[260] = {};
OPENFILENAMEW open_filename_struct;

const wchar_t* viewport_window_class_name = L"ViewportWindowClass";
const wchar_t* window_class_name = L"MyWindowClass";
const wchar_t* window_title = L"Finite Engine";

Camera2D viewport_camera = {
    .position = {0, 0},
    .zoom = 10.0f,
};

// --------------------------
// Function implementations

void LoadTextureFromFilepath(Texture* texture, char* filepath) {
    HRESULT hr;
    int image_x = 0;
    int image_y = 0;
    int image_channels = 0;
    int bytes_per_pixel = 4;

    auto image = stbi_load(filepath, &image_x, &image_y, &image_channels, bytes_per_pixel);

    if (!image) {
       ErrorMessageAndBreak((char*)"stbi_load failed");
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
        ErrorMessageAndBreak((char*)"Failed to create texture");
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

void ErrorMessageAndBreak(char* message) {
    const int size = 258;
    wchar_t wide_message[size];

    MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, size);
    MessageBoxW(NULL, wide_message, L"Error", MB_ICONERROR | MB_OK);

#ifdef DEBUG
    __debugbreak();
#else
    exit(1);
#endif
}

bool IsWindowFocused(HWND window_handle) {
    HWND foregroundWindow = GetForegroundWindow();
    return (foregroundWindow == window_handle);
}

void DebugMessage(char* message) {
#ifdef DEBUG
    const int size = 258;
    wchar_t wide_message[size];
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, size);
    OutputDebugStringW(wide_message);
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
        error_blob->Release();
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

    render_viewport.TopLeftX = 0;
    render_viewport.TopLeftY = 0;
    render_viewport.Width = width;
    render_viewport.Height = height;
    render_viewport.MinDepth = 0.0f;
    render_viewport.MaxDepth = 1.0f;

    char buffer[128] = {};
    sprintf(buffer, "Viewport resize event, x: %.2f, y: %.2f\n", width, height);
    DebugMessage(buffer);
}

static bool g_isResizing = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            break;
        }

        case WM_COMMAND: {
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            mousewheel_delta = delta;
            break;
        }

        case WM_ENTERSIZEMOVE: {
            g_isResizing = true;
            DebugMessage((char*)"ENTER RESIZE\n");
            break;
        }

        case WM_EXITSIZEMOVE: {
            g_isResizing = false;
            char buffer[128] = {};
            sprintf(buffer, "Window resize event, x: %d, y: %d\n", g_main_window.width_px, g_main_window.height_px);
            DebugMessage(buffer);
            if (id3d11_device && swapChain) {
                ResizeViewport(g_main_window.width_px, g_main_window.height_px);
            }
            DebugMessage((char*)"EXIT RESIZE\n");
            break;
        }

        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                g_main_window.width_px = (i32)width;
                g_main_window.height_px = (i32)height;
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

        main_window_handle = CreateWindowW(
            window_class_name, window_title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, g_main_window.width_px, g_main_window.height_px,
            NULL, NULL, hInstance, NULL
        );

        if (main_window_handle == NULL) {
            ErrorMessageAndBreak((char*)"Window Creation Failed!");
        }
    }

    // ------------
    // Init timer

    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_lastTime);

    // ----------------
    // Init DirectX 11
    {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = g_main_window.width_px;
        scd.BufferDesc.Height = g_main_window.height_px;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        scd.OutputWindow = main_window_handle;

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

        render_viewport.Width = (float)g_main_window.width_px;
        render_viewport.Height = (float)g_main_window.height_px;
        render_viewport.MinDepth = 0.0f;
        render_viewport.MaxDepth = 1.0f;
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

        // View projection matrix
        D3D11_BUFFER_DESC matrixBufferDesc = {};
        matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        matrixBufferDesc.ByteWidth = sizeof(ViewProjectionMatrixBufferType);
        matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hresult = id3d11_device->CreateBuffer(&matrixBufferDesc, nullptr, &cbuffer_view_projection);
        if (FAILED(hresult)) {
            ErrorMessageAndBreak((char*)"CreateBuffer ViewProjectionMatrixBufferType failed!");
        }
        
        // Model matrix
        D3D11_BUFFER_DESC modelBufferDesc = {};
        modelBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        modelBufferDesc.ByteWidth = sizeof(ModelBufferType);
        modelBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        modelBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hresult = id3d11_device->CreateBuffer(&modelBufferDesc, nullptr, &cbuffer_model);
        if (FAILED(hresult)) {
            ErrorMessageAndBreak((char*)"CreateBuffer ModelBufferType failed!");
        }

        // Aspect projection matrix
        D3D11_BUFFER_DESC projectionBufferDesc = {};
        projectionBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        projectionBufferDesc.ByteWidth = sizeof(AspectProjectionMatrixBufferType);
        projectionBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        projectionBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hresult = id3d11_device->CreateBuffer(&projectionBufferDesc, nullptr, &cbuffer_aspect_projection_matrix);
        if (FAILED(hresult)) {
            ErrorMessageAndBreak((char*)"CreateBuffer ProjectionMatrixBufferType failed!");
        }

        // Update aspect projection matrix
        {
            DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicLH((float)g_main_window.width_px, (float)g_main_window.height_px, 0.0f, 10.0f);
            AspectProjectionMatrixBufferType projection = {
                .aspect_projection_matrix = DirectX::XMMatrixTranspose(projectionMatrix),
            };

            D3D11_MAPPED_SUBRESOURCE mappedResource1;
            HRESULT hr = deviceContext->Map(cbuffer_aspect_projection_matrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource1);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"deviceContext->Map() ProjectionMatrixBufferType failed!");
            }

            AspectProjectionMatrixBufferType* projection_data_ptr = (AspectProjectionMatrixBufferType*)mappedResource1.pData;
            projection_data_ptr->aspect_projection_matrix = projection.aspect_projection_matrix;
            deviceContext->Unmap(cbuffer_aspect_projection_matrix, 0);
            deviceContext->VSSetConstantBuffers(2, 1, &cbuffer_aspect_projection_matrix);
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

    // --------------------------
    // Create tilemap shader
    {
        HRESULT hr;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* error_blob = nullptr;

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\tile.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain", "vs_5_0", 0, 0, &vsBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\tile.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain", "ps_5_0", 0, 0, &psBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = id3d11_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &tilemap_tile_vertex_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateVertexShader failed!");
        }

        hr = id3d11_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &tilemap_tile_pixel_shader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreatePixelShader failed!");
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = id3d11_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &tilemap_tile_input_layout);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateInputLayout failed!");
        }

        vsBlob->Release();
        psBlob->Release();

        // ------------------------------
        // Create tilemap shader buffer
        {
            TilemapTileVertex vertices[] = {
                { DirectX::XMFLOAT3(-1.0f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },  // Top-left
                { DirectX::XMFLOAT3(1.0f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },  // Bottom-right
                { DirectX::XMFLOAT3(-1.0f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }, // Bottom-left

                { DirectX::XMFLOAT3(-1.0f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },  // Top-left
                { DirectX::XMFLOAT3(1.0f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },   // Top-right
                { DirectX::XMFLOAT3(1.0f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }   // Bottom-right
            };

            D3D11_BUFFER_DESC bufferDesc = {};
            bufferDesc.Usage = D3D11_USAGE_DEFAULT;
            bufferDesc.ByteWidth = sizeof(vertices);
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = vertices;

            hr = id3d11_device->CreateBuffer(&bufferDesc, &initData, &tilemap_tile_vertex_buffer);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"CreateBuffer (vertex) failed!");
            }
        }
    }

    LoadTextureFromFilepath(&test_texture_01, (char*)"G:\\projects\\game\\finite-engine-dev\\resources\\images\\tiles\\grassland_tile_01.png");
    
    float debug_font_size = GetVHInPx(1.5f);
    g_debug_font = LoadFontAtlas((char*)"G:\\projects\\game\\finite-engine-dev\\resources\\fonts\\Roboto-Light.ttf", debug_font_size);

    ShowWindow(main_window_handle, nCmdShow);
    UpdateWindow(main_window_handle);
    ResizeViewport(g_main_window.width_px, g_main_window.height_px);

    // -----------
    // Game loop
    
    while (main_window_message.message != WM_QUIT) {
        // --------------
        // Handle input
        {
            mousewheel_delta = 0;
            frame_input = {0};

            if (PeekMessage(&main_window_message, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&main_window_message);
                DispatchMessageW(&main_window_message);
            }

            if (IsWindowFocused(main_window_handle)) {
                if (mousewheel_delta > 0) {
                    frame_input.mousewheel_up = true;
                }   
                else if (mousewheel_delta < 0) {
                    frame_input.mousewheel_down = true;
                }

                if (IsKeyPressed(VK_LEFT)) {
                    frame_input.arrow_left = true;
                }

                if (IsKeyPressed(VK_RIGHT)) {
                    frame_input.arrow_right = true;
                }

                if (IsKeyPressed(VK_UP)) {
                    frame_input.arrow_up = true;
                }

                if (IsKeyPressed(VK_DOWN)) {
                    frame_input.arrow_down = true;
                }
            }
        }

        // -------------
        // Query timer
        {
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);

            LONGLONG elapsedTicks = currentTime.QuadPart - g_lastTime.QuadPart;
            frame_delta = static_cast<f32>(elapsedTicks) / g_frequency.QuadPart;
            g_lastTime = currentTime;

            // char buffer[32];
            // f32 frame_delta_ms = frame_delta * 1000.0f;
            // sprintf(buffer, "Delta in ms: %.6f\n", frame_delta_ms);
            // DebugMessage(buffer);
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

            if (frame_input.arrow_down) {
                viewport_camera.position.y += 10.0f * frame_delta;
            }
            if (frame_input.arrow_up) {
                viewport_camera.position.y -= 10.0f * frame_delta;
            }
            if (frame_input.arrow_left) {
                viewport_camera.position.x += 10.0f * frame_delta;
            }
            if (frame_input.arrow_right) {
                viewport_camera.position.x -= 10.0f * frame_delta;
            }
        }

        // -----------------------
        // Render viewport frame
        {
            deviceContext->ClearRenderTargetView(renderTargetView, clear_color);
            deviceContext->RSSetViewports(1, &render_viewport);
            deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);

            // --------------------------------
            // Update viewport camera cbuffer
            {
                float nearPlane = 0.0f;
                float farPlane = 10.0f;
                f32 aspect_ratio = (f32)g_main_window.width_px / (f32)g_main_window.height_px;
                float viewWidth = 2.0f * viewport_camera.zoom;
                float viewHeight = viewWidth / aspect_ratio;

                char buffer[255] = {};
                sprintf(buffer, "Aspect ratio: %.2f, Width: %.2f, Height: %.2f\n", aspect_ratio, viewWidth, viewHeight);
                DebugMessage(buffer);

                DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(viewport_camera.position.x, viewport_camera.position.y, 0.0f);
                DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
                viewMatrix = XMMatrixMultiply(viewMatrix, translationMatrix);

                DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicLH(viewWidth, viewHeight, nearPlane, farPlane);
                DirectX::XMMATRIX view_projection_matrix = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);

                ViewProjectionMatrixBufferType viewProjection = {
                    .view_projection_matrix = DirectX::XMMatrixTranspose(view_projection_matrix),
                };

                D3D11_MAPPED_SUBRESOURCE mappedResource;
                HRESULT hr = deviceContext->Map(cbuffer_view_projection, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                if (FAILED(hr)) {
                    ErrorMessageAndBreak((char*)"deviceContext->Map() ViewProjectionMatrixBufferType failed!");
                }

                ViewProjectionMatrixBufferType* view_projection_data_ptr = (ViewProjectionMatrixBufferType*)mappedResource.pData;
                view_projection_data_ptr->view_projection_matrix = viewProjection.view_projection_matrix;
                deviceContext->Unmap(cbuffer_view_projection, 0);
                deviceContext->VSSetConstantBuffers(0, 1, &cbuffer_view_projection);
            }

            // -----------------
            // Draw background
            {
                RectangleVertex vertices[] = {
                    { DirectX::XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },  // Top-left
                    { DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },   // Top-right
                    { DirectX::XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }, // Bottom-left

                    { DirectX::XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }, // Bottom-left
                    { DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },   // Top-right
                    { DirectX::XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }   // Bottom-right
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
            }

            // ---------------
            // Draw tilemaps
            {
                Tilemap map {
                    .width = 8,
                    .height = 7,
                };

                for (int y = 0; y < map.height; y++) {
                    for (int x = 0; x < map.width; x++) {
                        Vec2 coordinate = {(f32)x, f32(y)};
                        DrawTilemapTile(&test_texture_01, coordinate);
                    }
                }
            }

            const char* text01 = "Janne\nnew line vali lyonti testi.\n!kakaka jajaja";
            Vec2 cursor01 = { 50.0f, 50.0f };
            DrawTextToScreen((char*)text01, cursor01, &g_debug_font);

            swapChain->Present(1, 0);
        }

        frame_counter++;
    }

    return main_window_message.wParam;
}

FontAtlasInfo LoadFontAtlas(char* filepath, float pixel_height) {
    FontAtlasInfo result = {};

    int used_height = (int)pixel_height;

    stbtt_fontinfo font;
    size_t file_size;
    wchar_t wide_buffer[256] = {};
    StrToWideStr(filepath, wide_buffer, 256);
    unsigned char *fontBuffer = LoadFileToPtr(wide_buffer, &file_size);

    stbtt_InitFont(&font, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

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
        
    ID3D11Texture2D* g_font_texture = nullptr;
    HRESULT hr = id3d11_device->CreateTexture2D(&desc, &initData, &g_font_texture);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"CreateTexture2D font atlas failed!");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = id3d11_device->CreateShaderResourceView(g_font_texture, &srvDesc, &result.texture_view);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"CreateShaderResourceView font atlas failed!");
    }

    g_font_texture->Release();

    char buffer[256] = {};
    sprintf(buffer, "Font loaded with texture atlas => width: %d, height: %d\n", result.font_atlas_width, result.font_atlas_height);
    DebugMessage(buffer);

    return result;
}

Vec2 DrawTextToScreen(char* text, Vec2 screen_pos, FontAtlasInfo* font_info) {
    Vec2 cursor = {
        .x = screen_pos.x,
        .y = screen_pos.y
    };

    Vec2 cursor_original = {
        .x = screen_pos.x,
        .y = screen_pos.y
    };
    
    for (char* p = (char*)text; *p != '\0'; p++) {
        char c = *p;

        if (c == '\n') {
            cursor.x = cursor_original.x;
            cursor.y += font_info->font_size_px;
        }

        FontGlyphInfo glyph = font_info->glyphs[c - 32];

        i32 px_x0 = cursor.x + glyph.x_offset;
        i32 px_x1 = px_x0 + glyph.bitmap_width;
        i32 px_y0 = cursor.y - glyph.y_offset;
        i32 px_y1 = px_y0 + glyph.bitmap_height;

        auto top_left = ScreenPxToNDC(px_x0, px_y0);
        auto top_right = ScreenPxToNDC(px_x1, px_y0);
        auto bot_left = ScreenPxToNDC(px_x0, px_y1);
        auto bot_right = ScreenPxToNDC(px_x1, px_y1);

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

        deviceContext->PSSetShaderResources(0, 1, &font_info->texture_view);
        deviceContext->PSSetSamplers(0, 1, &g_sampler);

        UINT stride = sizeof(TextUiVertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &text_ui_vertex_buffer, &stride, &offset);
        deviceContext->Draw(6, 0);

        cursor.x += glyph.advance;
    }

    return cursor;
}

Vec2 ScreenPxToNDC(int x, int y) {
    f32 ndcX = (2.0f * x) / (g_main_window.width_px - 1) - 1.0f;
    f32 ndcY = 1.0f - (2.0f * y) / (g_main_window.height_px - 1);
    Vec2 result = {
        .x = ndcX,
        .y = ndcY
    };
    return result;
}

Vec2 TilemapCoordsToIsometricScreenSpace(Vec2 tilemap_coord) {
    float x = (tilemap_coord.x * (-1.0f)) + (tilemap_coord.y * (1.0f));
    float y = (tilemap_coord.x * (0.5f)) + (tilemap_coord.y * (0.5f));
    Vec2 result = {x, y};
    return result;
}

void DrawTilemapTile(Texture* texture, Vec2 coordinate) {
    // Constant buffer part
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    HRESULT hr = deviceContext->Map(cbuffer_model, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"deviceContext->Map() ModelBufferType failed!");
    }

    Vec2 screen_coord = TilemapCoordsToIsometricScreenSpace(coordinate);
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(screen_coord.x, screen_coord.y, 0.0f);

    auto model_matrix = DirectX::XMMatrixIdentity();
    model_matrix = XMMatrixMultiply(model_matrix, translation);

    ModelBufferType modelBuffer = {
        .modelMatrix = DirectX::XMMatrixTranspose(model_matrix),
    };

    ModelBufferType* model_data_ptr = (ModelBufferType*)mappedResource.pData;
    model_data_ptr->modelMatrix = modelBuffer.modelMatrix;
    deviceContext->Unmap(cbuffer_model, 0);
    deviceContext->VSSetConstantBuffers(1, 1, &cbuffer_model);

    // Shader part
    deviceContext->IASetInputLayout(tilemap_tile_input_layout);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->VSSetShader(tilemap_tile_vertex_shader, nullptr, 0);
    deviceContext->PSSetShader(tilemap_tile_pixel_shader, nullptr, 0);

    deviceContext->PSSetShaderResources(0, 1, &texture->resource_view);
    deviceContext->PSSetSamplers(0, 1, &g_sampler);

    UINT stride = sizeof(TilemapTileVertex);
    UINT offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &tilemap_tile_vertex_buffer, &stride, &offset);
    deviceContext->Draw(6, 0);
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

f32 GetVWInPx(f32 vw) {
    return (vw / 100.0f) * (f32)g_main_window.width_px;
}

f32 GetVHInPx(f32 vh) {
    return (vh / 100.0f) * (f32)g_main_window.height_px;
}

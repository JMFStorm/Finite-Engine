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

#define ID_FILE_QUIT 9001
#define ID_FILE_SAVE 9002
#define ID_FILE_OPEN 9003
#define ID_EDIT_COPY 9004
#define ID_EDIT_PASTE 9005
#define ID_HELP_ABOUT 9006

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

struct ProjectionMatrixBufferType {
    DirectX::XMMATRIX projection_matrix;
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

struct TilemapTile {
    Texture* texture;
};

struct Tilemap {
    i32 width;
    i32 height;
    TilemapTile* tiles;
};

struct FontGlyphInfo {
    char character;
    i32 bitmap_width;
    i32 bitmap_height;
    i32 x_offset;
    i32 y_offset;
    f32 advance;
};

struct FontAtlasInfo {
    f32 font_size_px;
    f32 font_ascent;
    f32 font_descent;
    f32 font_linegap;
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

void RegisterViewportWindowClass(HINSTANCE hInstance);

bool IsKeyPressed(int key);

bool IsWindowFocused(HWND window_handle);

void DrawTilemapTile(Texture* texture, Vec2 coordinate);

void StrToWideStr(char* str, wchar_t* wresult, int str_count);

Vec2 TilemapCoordsToIsometricScreenSpace(Vec2 tilemap_coord);

unsigned char* LoadFileToPtr(wchar_t* filename, size_t* get_file_size);

// ---------
// Globals

const DirectX::XMMATRIX isometric_transformation_matrix = DirectX::XMMatrixSet(
    -0.5f, 0.5f, 0.0f, 0.0f,  // Row 1
     0.5f, 0.5f, 0.0f, 0.0f,  // Row 2
     0.0f, 0.0f, 1.0f, 0.0f,  // Row 3
     0.0f, 0.0f, 0.0f, 1.0f   // Row 4
);

// DirectX::XMVECTOR determinant;
// const DirectX::XMMATRIX inverse_isometric_transformation_matrix = DirectX::XMMatrixInverse(&determinant, isometric_transformation_matrix);

const DirectX::XMMATRIX inverse_isometric_transformation_matrix = DirectX::XMMatrixSet(
    -1.0f, 1.0f, 0.0f, 0.0f,  // Row 1
     1.0f, 1.0f, 0.0f, 0.0f,  // Row 2
     0.0f, 0.0f, 1.0f, 0.0f,  // Row 3
     0.0f, 0.0f, 0.0f, 1.0f   // Row 4
);

LARGE_INTEGER g_frequency;
LARGE_INTEGER g_lastTime;

Vec2 viewport_mouse = {};

u64 frame_counter = 0;
f32 frame_delta = 0.0f;
int mousewheel_delta = 0;
FrameInput frame_input = {};

ID3D11Buffer* cbuffer_view_projection = nullptr;
ID3D11Buffer* cbuffer_projection_matrix = nullptr;
ID3D11Buffer* cbuffer_model = nullptr;

HWND viewport_window_handle;
HWND camera_coords_label_handle;
HWND mouse_pos_label_handle;
HWND main_window_handle;
MSG main_window_message;

IDXGISwapChain *swapChain;
ID3D11Device* id3d11_device;
ID3D11DeviceContext *deviceContext;
ID3D11RenderTargetView *renderTargetView;
D3D11_VIEWPORT render_viewport;

ID3D11SamplerState* g_sampler;
ID3D11ShaderResourceView* g_font_texture_view = nullptr;
Texture test_texture_01 = {};
FLOAT clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };

ID3D11VertexShader* tilemap_tile_vertex_shader;
ID3D11PixelShader* tilemap_tile_pixel_shader;
ID3D11Buffer* tilemap_tile_vertex_buffer;
ID3D11InputLayout* tilemap_tile_input_layout;

ID3D11VertexShader* text_ui_vertex_shader;
ID3D11PixelShader* text_ui_pixel_shader;
ID3D11Buffer* text_ui_vertex_buffer;
ID3D11InputLayout* text_ui_input_layout;

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

LRESULT CALLBACK ViewportWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            viewport_mouse.x = (f32)x;
            viewport_mouse.y = (f32)y;
            break;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void RegisterViewportWindowClass(HINSTANCE hInstance) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ViewportWindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = viewport_window_class_name;
    RegisterClassEx(&wc);
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            //----------------------
            // Create window menus
            { 
                HMENU hFileMenu = CreateMenu();
                AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN, L"Open");
                AppendMenuW(hFileMenu, MF_STRING, ID_FILE_SAVE, L"Save");
                AppendMenuW(hFileMenu, MF_STRING, ID_FILE_QUIT, L"Quit");

                HMENU hEditMenu = CreateMenu();
                AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_COPY, L"Copy");
                AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE, L"Paste");

                HMENU hHelpMenu = CreateMenu();
                AppendMenuW(hHelpMenu, MF_STRING, ID_HELP_ABOUT, L"About");

                HMENU hMenu = CreateMenu();
                AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
                AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"Edit");
                AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");

                SetMenu(hwnd, hMenu); // Attach the main menu to the window
            }
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_FILE_QUIT:
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                    break;
                case ID_FILE_SAVE:
                    MessageBox(hwnd, L"Save selected", L"Menu", MB_OK);
                    break;
                case ID_FILE_OPEN: {
                    open_filename_struct = {};
                    open_filename_struct.lStructSize = sizeof(open_filename_struct);
                    open_filename_struct.hwndOwner = hwnd;
                    open_filename_struct.lpstrFile = open_filename_path;

                    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
                    // use the contents of open_filename_path to initialize itself.
                    open_filename_struct.lpstrFile[0] = '\0';
                    open_filename_struct.nMaxFile = sizeof(open_filename_path);
                    open_filename_struct.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
                    open_filename_struct.nFilterIndex = 1;
                    open_filename_struct.lpstrFileTitle = NULL;
                    open_filename_struct.nMaxFileTitle = 0;
                    open_filename_struct.lpstrInitialDir = NULL;
                    open_filename_struct.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&open_filename_struct) == TRUE) {
                        MessageBox(hwnd, open_filename_struct.lpstrFile, L"Menu", MB_OK);
                    }

                    break;
                }

                case ID_EDIT_COPY:
                    MessageBox(hwnd, L"Copy selected", L"Menu", MB_OK);
                    break;
                case ID_EDIT_PASTE:
                    MessageBox(hwnd, L"Paste selected", L"Menu", MB_OK);
                    break;
                case ID_HELP_ABOUT:
                    MessageBox(hwnd, L"About selected", L"Menu", MB_OK);
                    break;
            }
            break;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            const COLORREF LIGHT_GREY = RGB(211, 211, 211);
            HBRUSH hBrush = CreateSolidBrush(LIGHT_GREY);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            mousewheel_delta = delta;
            break;
        }

        case WM_SIZE: {
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
            CW_USEDEFAULT, CW_USEDEFAULT, 1600, 1200,
            NULL, NULL, hInstance, NULL
        );

        if (main_window_handle == NULL) {
            ErrorMessageAndBreak((char*)"Window Creation Failed!");
        }

        ShowWindow(main_window_handle, nCmdShow);
        UpdateWindow(main_window_handle);

        RegisterViewportWindowClass(hInstance);

        viewport_window_handle = CreateWindowEx(
            0, viewport_window_class_name, NULL,
            WS_CHILD | WS_VISIBLE,
            5, 5, 1200, 800,
            main_window_handle,
            NULL, hInstance, NULL
        );

        mouse_pos_label_handle = CreateWindowW(
            L"STATIC",
            L"Viewport mouse: (0, 0)",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            20, 1050, 300, 25,       // Position and dimensions
            main_window_handle,
            NULL,
            hInstance,
            NULL
        );

        camera_coords_label_handle = CreateWindowW(
            L"STATIC",
            L"Camera: (0, 0), Zoom: (0)",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            320, 1050, 300, 25,       // Position and dimensions
            main_window_handle,
            NULL,
            hInstance,
            NULL
        );
    }

    // ------------
    // Init timer

    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_lastTime);

    // ----------------
    // Init DirectX 11
    {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = 1200;
        scd.BufferDesc.Height = 800;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = viewport_window_handle;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &scd,
            &swapChain, &id3d11_device, nullptr, &deviceContext);

        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"D3D11CreateDeviceAndSwapChain Failed!");
        }

        ID3D11Texture2D* backBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
        id3d11_device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
        
        deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);

        render_viewport.Width = 1200.0f;
        render_viewport.Height = 800.0f;
        render_viewport.MinDepth = 0.0f;
        render_viewport.MaxDepth = 1.0f;
        deviceContext->RSSetViewports(1, &render_viewport);
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

        // Projection matrix
        D3D11_BUFFER_DESC projectionBufferDesc = {};
        projectionBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        projectionBufferDesc.ByteWidth = sizeof(ProjectionMatrixBufferType);
        projectionBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        projectionBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hresult = id3d11_device->CreateBuffer(&projectionBufferDesc, nullptr, &cbuffer_projection_matrix);
        if (FAILED(hresult)) {
            ErrorMessageAndBreak((char*)"CreateBuffer ProjectionMatrixBufferType failed!");
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
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\shaders.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain", "vs_5_0", 0, 0, &vsBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\shaders.hlsl",
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
                { DirectX::XMFLOAT3(-0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },  // Top-left
                { DirectX::XMFLOAT3(0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },  // Bottom-right
                { DirectX::XMFLOAT3(-0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }, // Bottom-left

                { DirectX::XMFLOAT3(-0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },  // Top-left
                { DirectX::XMFLOAT3(0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },   // Top-right
                { DirectX::XMFLOAT3(0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }   // Bottom-right
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

    LoadTextureFromFilepath(
        &test_texture_01, (char*)"G:\\projects\\game\\finite-engine-dev\\resources\\images\\tiles\\grassland_tile_01.png");

    FontAtlasInfo g_debug_font = {};
    int fontAtlasWidth = 0;
    int fontAtlasHeight = 0;

    // -----------------
    // Load font atlas
    {
        stbtt_fontinfo font;
        size_t file_size;
        unsigned char *fontBuffer = LoadFileToPtr(
            (wchar_t*)L"G:\\projects\\game\\finite-engine-dev\\resources\\fonts\\Roboto-Regular.ttf",
            &file_size);

        stbtt_InitFont(&font, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

        float desired_pixel_height = 64.0f;
        float scale = stbtt_ScaleForPixelHeight(&font, desired_pixel_height);

        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        
        g_debug_font.font_size_px = desired_pixel_height;
        g_debug_font.font_ascent = ascent * scale;
        g_debug_font.font_descent = descent * scale;
        g_debug_font.font_linegap = lineGap * scale;

        for (int c = 32; c < 128; c++) {
            int codepoint = c;
            int width, height, xoffset, yoffset;
            unsigned char *bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, codepoint, &width, &height, &xoffset, &yoffset);

            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&font, codepoint, &advanceWidth, &leftSideBearing);
            float advanceScaled = ((f32)advanceWidth * scale);      

            int glyph_index = c - 32;
            g_debug_font.glyphs[glyph_index].advance = advanceScaled;
            g_debug_font.glyphs[glyph_index].bitmap_height = height;
            g_debug_font.glyphs[glyph_index].bitmap_width = width;
            g_debug_font.glyphs[glyph_index].character = c;
            g_debug_font.glyphs[glyph_index].x_offset = xoffset;
            g_debug_font.glyphs[glyph_index].y_offset = -1 * yoffset;

            fontAtlasWidth += width;
            if (fontAtlasHeight < height) {
                fontAtlasHeight = height;
            }

            stbtt_FreeBitmap(bitmap, nullptr);
        }

        // ---------------
        // Atlas texture
        {
            int width, height, xoffset, yoffset;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, 'm', &width, &height, &xoffset, &yoffset);

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = bitmap;
            initData.SysMemPitch = width; // The distance in bytes between the start of each line of the texture
            
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

            hr = id3d11_device->CreateShaderResourceView(g_font_texture, &srvDesc, &g_font_texture_view);
            if (FAILED(hr)) {
                ErrorMessageAndBreak((char*)"CreateShaderResourceView font atlas failed!");
            }

            g_font_texture->Release();
        }
    }

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
                float viewWidth = viewport_camera.zoom / (4.0f/3.0f);
                float viewHeight = viewport_camera.zoom;
                float nearPlane = 0.0f;
                float farPlane = 10.0f;

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

                // Update projection matrix
                {
                    ProjectionMatrixBufferType projection = {
                        .projection_matrix = DirectX::XMMatrixTranspose(projectionMatrix),
                    };

                    D3D11_MAPPED_SUBRESOURCE mappedResource1;
                    HRESULT hr = deviceContext->Map(cbuffer_projection_matrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource1);
                    if (FAILED(hr)) {
                        ErrorMessageAndBreak((char*)"deviceContext->Map() ProjectionMatrixBufferType failed!");
                    }

                    ProjectionMatrixBufferType* projection_data_ptr = (ProjectionMatrixBufferType*)mappedResource1.pData;
                    projection_data_ptr->projection_matrix = projection.projection_matrix;
                    deviceContext->Unmap(cbuffer_projection_matrix, 0);
                    deviceContext->VSSetConstantBuffers(2, 1, &cbuffer_projection_matrix);
                }

            }

            // ---------------
            // Draw tilemaps
            {
                Tilemap map {
                    .width = 5,
                    .height = 4,
                };

                for (int y = 0; y < map.height; y++) {
                    for (int x = 0; x < map.width; x++) {
                        Vec2 coordinate = {(f32)x, f32(y)};
                        DrawTilemapTile(&test_texture_01, coordinate);
                    }
                }
            }

            // Font test
            {
                // Buffer data
                {
                    TextUiVertex vertices[] = {
                        { DirectX::XMFLOAT4(-0.5f, 0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },  // Top-left
                        { DirectX::XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },   // Top-right
                        { DirectX::XMFLOAT4(-0.5f, -0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }, // Bottom-left

                        { DirectX::XMFLOAT4(-0.5f, -0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }, // Bottom-left
                        { DirectX::XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },   // Top-right
                        { DirectX::XMFLOAT4(0.5f, -0.5f, 1.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }   // Bottom-right
                    };

                    D3D11_MAPPED_SUBRESOURCE mappedResource;
                    HRESULT hr = deviceContext->Map(text_ui_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                    if (FAILED(hr)) {
                        ErrorMessageAndBreak((char*)"Map for text_ui dynamic vertex buffer failed!");
                    }

                    memcpy(mappedResource.pData, vertices, sizeof(TextUiVertex) * 6);
                    deviceContext->Unmap(text_ui_vertex_buffer, 0);
                }

                deviceContext->IASetInputLayout(text_ui_input_layout);
                deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                deviceContext->VSSetShader(text_ui_vertex_shader, nullptr, 0);
                deviceContext->PSSetShader(text_ui_pixel_shader, nullptr, 0);

                deviceContext->PSSetShaderResources(0, 1, &g_font_texture_view);
                deviceContext->PSSetSamplers(0, 1, &g_sampler);

                UINT stride = sizeof(TextUiVertex);
                UINT offset = 0;
                deviceContext->IASetVertexBuffers(0, 1, &text_ui_vertex_buffer, &stride, &offset);
                deviceContext->Draw(6, 0);
            }

            swapChain->Present(1, 0);
        }

        // ------------------
        // Print debug info
        {
            const int size = 128;
            char buffer[size] = {};
            wchar_t wide_message[size] = {};

            sprintf(buffer, "Camera: (%.1f,%.1f), Zoom: (%.2f)", viewport_camera.position.x, viewport_camera.position.y, viewport_camera.zoom);
            StrToWideStr((char*)buffer, wide_message, size);
            SetWindowTextW(camera_coords_label_handle, wide_message);
        }

        {
            const int size = 32;
            char buffer2[size] = {};
            wchar_t wide_message2[size] = {};

            sprintf(buffer2, "Viewport mouse: (%.1f,%.1f)", viewport_mouse.x, viewport_mouse.y);
            StrToWideStr((char*)buffer2, wide_message2, size);
            SetWindowTextW(mouse_pos_label_handle, wide_message2);
        }

        frame_counter++;
    }

    return main_window_message.wParam;
}

Vec2 TilemapCoordsToIsometricScreenSpace(Vec2 tilemap_coord) {
    DirectX::XMFLOAT4 float4;
    auto vec4 = DirectX::XMVectorSet(tilemap_coord.x, tilemap_coord.y, 1.0f, 1.0f);
    auto transformedVec = DirectX::XMVector4Transform(vec4, isometric_transformation_matrix);
    XMStoreFloat4(&float4, transformedVec);
    Vec2 result = {float4.x, float4.y};
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

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

#define ID_FILE_QUIT 9001
#define ID_FILE_SAVE 9002
#define ID_FILE_OPEN 9003
#define ID_EDIT_COPY 9004
#define ID_EDIT_PASTE 9005
#define ID_HELP_ABOUT 9006

#define ID_BUTTON 2001

#define ID_COMBOBOX1 1001
#define ID_COMBOBOX2 1002

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

struct Camera2D {
    DirectX::XMFLOAT2 position;
    f32 zoom; // how many tiles can see vertically
};

struct MatrixBufferType {
    DirectX::XMMATRIX viewProjectionMatrix;
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
    ID3D11SamplerState* sampler;
    ID3D11ShaderResourceView* resource_view;
};

struct Sprite {
    Texture texture;
    Vec2 uv;
};

struct Tile {
    char name[128];
    Texture texture;
};

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv;
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

// ---------
// Globals

ID3D11Buffer* matrixBuffer;

HWND viewport_window_handle;
HWND main_window_handle;
MSG main_window_message;

IDXGISwapChain *swapChain;
ID3D11Device* id3d11_device;
ID3D11DeviceContext *deviceContext;
ID3D11RenderTargetView *renderTargetView;
D3D11_VIEWPORT render_viewport;

Texture test_texture_01 = {};
FLOAT clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };

ID3D11VertexShader* vertexShader;
ID3D11PixelShader* pixelShader;
ID3D11Buffer* vertexBuffer;
ID3D11InputLayout* inputLayout;

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

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
    hr = id3d11_device->CreateSamplerState(&samplerDesc, &texture->sampler);

    if (FAILED(hr)) {
        ErrorMessageAndBreak((char*)"Failed to create sampler state.");
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

LRESULT CALLBACK ViewportWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
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
 * 
 * @param hr HRESULT to check.
 * @param error_blob ID3DBlob to check. 
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

            // ---------------
            // Select menu 1
            {
                HWND hComboBox = CreateWindowW(
                    L"COMBOBOX",  NULL,
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, 
                    10, 10, 200, 100, 
                    hwnd, (HMENU)ID_COMBOBOX1, 
                    GetModuleHandle(NULL), NULL);

                SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"Red");
                SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"Green");
                SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"Blue");
            }

            // ---------------
            // Select menu 2
            {
                HWND hComboBox2 = CreateWindowW(
                    L"COMBOBOX",  NULL,
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, 
                    400, 10, 200, 100, 
                    hwnd, (HMENU)ID_COMBOBOX2, 
                    GetModuleHandle(NULL), NULL);

                SendMessageW(hComboBox2, CB_ADDSTRING, 0, (LPARAM)L"Item 4");
                SendMessageW(hComboBox2, CB_ADDSTRING, 0, (LPARAM)L"Item 5");
                SendMessageW(hComboBox2, CB_ADDSTRING, 0, (LPARAM)L"Item 6");
            }
            
            // Window menus
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

            // Create a button
            {
                HWND hButton = CreateWindowEx(
                    0, L"BUTTON", L"Click Me",
                    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                    10, 80, 100, 30,
                    hwnd, (HMENU)ID_BUTTON,
                    GetModuleHandle(NULL), NULL);

                if (hButton == NULL) {
                    MessageBox(hwnd, L"Failed to create button!", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }
        case WM_COMMAND: {
            switch (HIWORD(wParam)) {
                case CBN_SELCHANGE: {
                    HWND hComboBox = (HWND)lParam;
                    int comboBoxID = GetDlgCtrlID(hComboBox);

                    switch (comboBoxID) {
                        case ID_COMBOBOX1: {
                            HWND hComboBox = (HWND)lParam;
                            int itemIndex = SendMessageW(hComboBox, CB_GETCURSEL, 0, 0);

                            if (itemIndex == 0) {
                                clear_color[0] = 1.0f;
                                clear_color[1] = 0.0f;
                                clear_color[2] = 0.0f;
                            }
                            else if (itemIndex == 1) {
                                clear_color[0] = 0.0f;
                                clear_color[1] = 1.0f;
                                clear_color[2] = 0.0f;
                            }
                            else if (itemIndex == 2) {
                                clear_color[0] = 0.0f;
                                clear_color[1] = 0.0f;
                                clear_color[2] = 1.0f;
                            }

                            break;
                        }

                        case ID_COMBOBOX2: {
                            HWND hComboBox = (HWND)lParam;
                            int itemIndex = SendMessageW(hComboBox, CB_GETCURSEL, 0, 0);

                            if (itemIndex != CB_ERR) {
                                wchar_t selectedItem[256];
                                SendMessageW(hComboBox, CB_GETLBTEXT, itemIndex, (LPARAM)selectedItem);
                                MessageBox(hwnd, selectedItem, L"Item Selected 2", MB_OK);
                            }
                            break;
                        }    
                    }
                }
            }

            switch (LOWORD(wParam)) {
                case ID_BUTTON:
                    MessageBox(hwnd, L"Button clicked!", L"Info", MB_OK | MB_ICONINFORMATION);
                    break;
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

        case WM_ERASEBKGND: { // Handle background erasing
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            const COLORREF LIGHT_GREY = RGB(211, 211, 211);
            HBRUSH hBrush = CreateSolidBrush(LIGHT_GREY);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
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
            20, 300, 800, 600,
            main_window_handle,
            NULL, hInstance, NULL
        );
    }

    // ----------------
    // Init DirectX 11
    {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = 800;
        scd.BufferDesc.Height = 600;
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

        ID3D11Texture2D *backBuffer = {};
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
        id3d11_device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
        backBuffer->Release();
        deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);

        render_viewport.Width = 800.0f;
        render_viewport.Height = 600.0f;
        render_viewport.MinDepth = 0.0f;
        render_viewport.MaxDepth = 1.0f;
        deviceContext->RSSetViewports(1, &render_viewport);
    }

    // --------------------------
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

    // -------------------------------
    // Create constant matrix buffer
    {
        D3D11_BUFFER_DESC matrixBufferDesc = {};
        ZeroMemory(&matrixBufferDesc, sizeof(matrixBufferDesc));
        matrixBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
        matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        matrixBufferDesc.CPUAccessFlags = 0;

        HRESULT result = id3d11_device->CreateBuffer(&matrixBufferDesc, NULL, &matrixBuffer);
        if (FAILED(result)) {
            ErrorMessageAndBreak((char*)"CreateBuffer matrixBuffer failed!");
        }
    }

    // --------------------------
    // Create the vertex buffer
    {
        Vertex vertices[] = {
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

        HRESULT hr = id3d11_device->CreateBuffer(&bufferDesc, &initData, &vertexBuffer);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateBuffer (vertex) failed!");
        }

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* error_blob = nullptr;

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\shaders.hlsl",
            nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = D3DCompileFromFile(
            L"G:\\projects\\game\\finite-engine-dev\\resources\\shaders\\shaders.hlsl",
            nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &error_blob);
        CheckShaderCompileError(hr, error_blob);

        hr = id3d11_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateVertexShader failed!");
        }

        hr = id3d11_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreatePixelShader failed!");
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = id3d11_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
        if (FAILED(hr)) {
            ErrorMessageAndBreak((char*)"CreateInputLayout failed!");
        }

        vsBlob->Release();
        psBlob->Release();
    }

    LoadTextureFromFilepath(
        &test_texture_01, (char*)"G:\\projects\\game\\finite-engine-dev\\resources\\images\\tiles\\grassland_tile_01.png");

    // -----------
    // Game loop
    while (main_window_message.message != WM_QUIT) {

        // --------------
        // Handle input
        {
            if (PeekMessage(&main_window_message, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&main_window_message);
                DispatchMessageW(&main_window_message);
            }
        }

        if (IsWindowFocused(main_window_handle)) {
            if (IsKeyPressed(VK_LEFT)) {
                DebugMessage((char*)"Left key is down!\n");
                viewport_camera.position.x += 0.15f;
            }

            if (IsKeyPressed(VK_RIGHT)) {
                DebugMessage((char*)"Right key is down!\n");
                viewport_camera.position.x -= 0.15f;
            }

            if (IsKeyPressed(VK_UP)) {
                DebugMessage((char*)"Up key is down!\n");
                viewport_camera.position.y -= 0.15f;
            }

            if (IsKeyPressed(VK_DOWN)) {
                DebugMessage((char*)"Down key is down!\n");
                viewport_camera.position.y += 0.15f;
            }
        }

        // -------------
        // Scene logic
        {
            
        }

        // -----------------------
        // Render viewport frame
        {
            UINT stride = sizeof(Vertex);
            UINT offset = 0;

            deviceContext->ClearRenderTargetView(renderTargetView, clear_color);

            deviceContext->RSSetViewports(1, &render_viewport);
            deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);

            deviceContext->IASetInputLayout(inputLayout);
            deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            deviceContext->VSSetShader(vertexShader, nullptr, 0);
            deviceContext->PSSetShader(pixelShader, nullptr, 0);

            deviceContext->PSSetShaderResources(0, 1, &test_texture_01.resource_view);
            deviceContext->PSSetSamplers(0, 1, &test_texture_01.sampler);

            deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

            float viewWidth = viewport_camera.zoom / (4.0f/3.0f);
            float viewHeight = viewport_camera.zoom;
            float nearPlane = 0.0f;
            float farPlane = 10.0f;

            DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(viewport_camera.position.x, viewport_camera.position.y, 0.0f);
            DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
            viewMatrix = XMMatrixMultiply(viewMatrix, translationMatrix);

            DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicLH(viewWidth, viewHeight, nearPlane, farPlane);
            DirectX::XMMATRIX viewProjectionMatrix = DirectX::XMMatrixMultiply(viewMatrix, projectionMatrix);

            MatrixBufferType matrices = {
                .viewProjectionMatrix = DirectX::XMMatrixTranspose(viewProjectionMatrix),
                .modelMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()),
            };

            deviceContext->UpdateSubresource(matrixBuffer, 0, NULL, &matrices, 0, 0);
            deviceContext->VSSetConstantBuffers(0, 1, &matrixBuffer);

            deviceContext->Draw(6, 0);

            swapChain->Present(1, 0);
        }
    }

    return main_window_message.wParam;
}

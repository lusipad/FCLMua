#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Vertex structure
struct Vertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT4 Color;
};

// Constant buffer for transforms
struct ConstantBuffer
{
    XMMATRIX World;
    XMMATRIX View;
    XMMATRIX Projection;
    XMFLOAT4 Color;
    XMFLOAT4 LightDir;
};

// Mesh data
struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool Initialize(HWND hwnd, int width, int height);
    void Resize(int width, int height);

    void BeginFrame();
    void EndFrame();

    // Drawing
    void SetViewProjection(const XMMATRIX& view, const XMMATRIX& proj);
    void DrawMesh(const Mesh& mesh, const XMMATRIX& world, const XMFLOAT4& color);
    void DrawSphere(const XMFLOAT3& center, float radius, const XMFLOAT4& color);
    void DrawBox(const XMFLOAT3& center, const XMFLOAT3& extents, const XMMATRIX& rotation, const XMFLOAT4& color);
    void DrawLine(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT4& color);
    void DrawGrid(float size, int divisions, const XMFLOAT4& color);

    // Mesh creation
    static Mesh CreateSphereMesh(float radius, int segments = 32);
    static Mesh CreateBoxMesh(const XMFLOAT3& extents);
    static Mesh CreateMeshFromData(const std::vector<XMFLOAT3>& vertices, const std::vector<uint32_t>& indices);

    // Upload mesh to GPU
    bool UploadMesh(Mesh& mesh);

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

private:
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateRenderTargets();
    bool CreateDepthStencil();
    bool CreateShaders();
    bool CreateRasterizerStates();
    bool CreatePrimitiveMeshes();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11Texture2D> m_depthStencilBuffer;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_constantBuffer;

    ComPtr<ID3D11RasterizerState> m_rasterizerStateSolid;
    ComPtr<ID3D11RasterizerState> m_rasterizerStateWireframe;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    // Primitive meshes
    Mesh m_sphereMesh;
    Mesh m_boxMesh;

    int m_width;
    int m_height;

    XMMATRIX m_viewMatrix;
    XMMATRIX m_projMatrix;
};

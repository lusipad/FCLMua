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
    XMFLOAT4 CameraPos; // Added for specular calculation
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
    void SetViewProjection(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj, const DirectX::XMFLOAT3& cameraPos);
    void SetAlphaBlending(bool enable); // Enable/Disable transparency

    // Drawing functions
    void DrawSphere(const DirectX::XMFLOAT3& center, float radius, const DirectX::XMFLOAT4& color);
    void DrawBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extents, const DirectX::XMMATRIX& rotation, const DirectX::XMFLOAT4& color);
    void DrawMesh(const Mesh& mesh, const DirectX::XMMATRIX& world, const DirectX::XMFLOAT4& color);
    void DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color);
    void FlushLines(); 
    void DrawGrid(float size, int divisions, const DirectX::XMFLOAT4& color);

    // Accessors
    const Mesh* GetSphereMesh() const { return &m_sphereMesh; }

    // Mesh creation
    static Mesh CreateSphereMesh(float radius, int segments = 32);
    static Mesh CreateBoxMesh(const XMFLOAT3& extents);
    static Mesh CreateMeshFromData(const std::vector<XMFLOAT3>& vertices, const std::vector<uint32_t>& indices);
    static Mesh CreateMeshFromData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

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
    bool CreateBlendStates();
    bool CreatePrimitiveMeshes();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11Texture2D> m_depthStencilBuffer;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11PixelShader> m_pixelShaderUnlit; // For lines
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_constantBuffer;

    ComPtr<ID3D11RasterizerState> m_rasterizerStateSolid;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerStateWireframe;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendStateAlpha;

    // Resources
    Mesh m_sphereMesh;
    Mesh m_boxMesh;

    // Line rendering
    std::vector<Vertex> m_lineVertices;
    ComPtr<ID3D11Buffer> m_lineVertexBuffer;

    int m_width;
    int m_height;
    int m_uiWidth;
    
    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projMatrix;
    DirectX::XMFLOAT3 m_cameraPos;
};
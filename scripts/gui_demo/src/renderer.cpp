#include "renderer.h"
#include <d3dcompiler.h>
#include <stdexcept>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Renderer::Renderer()
    : m_width(0)
    , m_height(0)
    , m_uiWidth(300)
{
    m_viewMatrix = XMMatrixIdentity();
    m_projMatrix = XMMatrixIdentity();
}

Renderer::~Renderer()
{
}

bool Renderer::Initialize(HWND hwnd, int width, int height)
{
    m_width = width;
    m_height = height;

    if (!CreateDeviceAndSwapChain(hwnd))
        return false;

    if (!CreateRenderTargets())
        return false;

    if (!CreateDepthStencil())
        return false;

    if (!CreateShaders())
        return false;

    if (!CreateRasterizerStates())
        return false;

    if (!CreateBlendStates())
        return false;

    if (!CreatePrimitiveMeshes())
        return false;

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = static_cast<float>(m_uiWidth);
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(width - m_uiWidth);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    return true;
}

bool Renderer::CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1; // Standard for MSAA
    scd.BufferDesc.Width = m_width;
    scd.BufferDesc.Height = m_height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    
    // Disable MSAA to ensure compatibility (fixed "not displaying" issue)
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0; 
    
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // Use legacy DISCARD for max compatibility

    D3D_FEATURE_LEVEL featureLevel;
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_context
    );

    return SUCCEEDED(hr);
}

bool Renderer::CreateRenderTargets()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr))
        return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    return SUCCEEDED(hr);
}

bool Renderer::CreateDepthStencil()
{
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = m_width;
    descDepth.Height = m_height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_device->CreateTexture2D(&descDepth, nullptr, &m_depthStencilBuffer);
    if (FAILED(hr))
        return false;

    hr = m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, &m_depthStencilView);
    if (FAILED(hr))
        return false;

    // Create depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

    hr = m_device->CreateDepthStencilState(&dsDesc, &m_depthStencilState);
    return SUCCEEDED(hr);
}

bool Renderer::CreateShaders()
{
    // Simple vertex shader
    const char* vsSource = R"(
        cbuffer ConstantBuffer : register(b0)
        {
            matrix World;
            matrix View;
            matrix Projection;
            float4 Color;
            float4 LightDir;
            float4 CameraPos;
        };

        struct VS_INPUT
        {
            float3 Pos : POSITION;
            float3 Normal : NORMAL;
            float4 Color : COLOR;
        };

        struct PS_INPUT
        {
            float4 Pos : SV_POSITION;
            float3 Normal : NORMAL;
            float4 Color : COLOR;
            float3 WorldPos : TEXCOORD0;
        };

        PS_INPUT main(VS_INPUT input)
        {
            PS_INPUT output;
            float4 worldPos = mul(float4(input.Pos, 1.0), World);
            output.WorldPos = worldPos.xyz;
            output.Pos = mul(worldPos, View);
            output.Pos = mul(output.Pos, Projection);
            output.Normal = mul(input.Normal, (float3x3)World);
            output.Color = Color * input.Color;
            return output;
        }
    )";

    // Simple pixel shader with lighting (Pseudo-PBR)
    const char* psSource = R"(
        cbuffer ConstantBuffer : register(b0)
        {
            matrix World;
            matrix View;
            matrix Projection;
            float4 Color;
            float4 LightDir;
            float4 CameraPos;
        };

        struct PS_INPUT
        {
            float4 Pos : SV_POSITION;
            float3 Normal : NORMAL;
            float4 Color : COLOR;
            float3 WorldPos : TEXCOORD0;
        };

        float4 main(PS_INPUT input) : SV_TARGET
        {
            float3 N = normalize(input.Normal);
            float3 L = normalize(LightDir.xyz);
            float3 V = normalize(CameraPos.xyz - input.WorldPos);
            float3 H = normalize(L + V);

            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 0.001);

            // 1. Diffuse & Ambient
            // Very low ambient for high contrast/deep shadows
            float3 ambient = float3(0.05, 0.05, 0.06); 
            float3 diffuse = input.Color.rgb * (ambient + NdotL * 0.95);

            // 2. Material Heuristics
            float brightness = dot(input.Color.rgb, float3(0.3, 0.59, 0.11));
            bool isMatte = (brightness < 0.25);
            
            // Sharper highlights for metal
            float specPower = isMatte ? 32.0 : 256.0; 
            float specIntensity = isMatte ? 0.2 : 1.2;
            float reflectIntensity = isMatte ? 0.02 : 0.3;

            // 3. Specular (Blinn-Phong)
            float spec = pow(max(dot(N, H), 0.0), specPower) * specIntensity;
            float3 specularColor = float3(1.0, 1.0, 1.0) * spec;

            // 4. Environment Reflection (Fake Skybox)
            float3 R = reflect(-V, N);
            // Darker skybox to reduce haze
            float3 skyColor = float3(0.05, 0.2, 0.5);
            float3 horizonColor = float3(0.4, 0.4, 0.45);
            float3 groundColor = float3(0.02, 0.02, 0.02);
            
            float3 envColor;
            if (R.y > 0.0) envColor = lerp(horizonColor, skyColor, pow(R.y, 0.8));
            else           envColor = lerp(horizonColor, groundColor, pow(-R.y, 0.8));

            // 5. Fresnel
            float fresnel = pow(1.0 - NdotV, 5.0); // Sharper fresnel
            float finalReflect = reflectIntensity + fresnel * 0.5;
            if (isMatte) finalReflect *= 0.1;

            // Combine
            float3 finalRGB = diffuse + specularColor + envColor * finalReflect;

            // 6. Direct Output (No ToneMapping/Gamma) for Richer Colors
            return float4(finalRGB, input.Color.a);
        }
    )";

    // Compile vertex shader
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr))
        return false;

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_vertexShader);
    if (FAILED(hr))
        return false;

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr))
        return false;

    // Compile pixel shader
    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr))
        return false;

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_pixelShader);
    if (FAILED(hr))
        return false;

    // Unlit pixel shader for lines
    const char* psUnlitSource = R"(
        struct PS_INPUT
        {
            float4 Pos : SV_POSITION;
            float3 Normal : NORMAL;
            float4 Color : COLOR;
        };

        float4 main(PS_INPUT input) : SV_TARGET
        {
            return input.Color;
        }
    )";

    ComPtr<ID3DBlob> psUnlitBlob;
    hr = D3DCompile(psUnlitSource, strlen(psUnlitSource), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &psUnlitBlob, &errorBlob);
    if (FAILED(hr))
        return false;

    hr = m_device->CreatePixelShader(psUnlitBlob->GetBufferPointer(), psUnlitBlob->GetBufferSize(),
        nullptr, &m_pixelShaderUnlit);
    if (FAILED(hr))
        return false;

    // Create constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbd, nullptr, &m_constantBuffer);
    return SUCCEEDED(hr);
}

bool Renderer::CreateRasterizerStates()
{
    // Solid rasterizer state
    D3D11_RASTERIZER_DESC rsSolid = {};
    rsSolid.FillMode = D3D11_FILL_SOLID;
    rsSolid.CullMode = D3D11_CULL_NONE; // Disable culling to fix transparency issues on some faces
    rsSolid.FrontCounterClockwise = TRUE; 
    rsSolid.DepthClipEnable = TRUE;

    HRESULT hr = m_device->CreateRasterizerState(&rsSolid, &m_rasterizerStateSolid);
    if (FAILED(hr))
        return false;

    // Wireframe rasterizer state
    D3D11_RASTERIZER_DESC rsWireframe = rsSolid;
    rsWireframe.FillMode = D3D11_FILL_WIREFRAME;
    rsWireframe.CullMode = D3D11_CULL_NONE;

    hr = m_device->CreateRasterizerState(&rsWireframe, &m_rasterizerStateWireframe);
    return SUCCEEDED(hr);
}

bool Renderer::CreateBlendStates()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = m_device->CreateBlendState(&blendDesc, &m_blendStateAlpha);
    return SUCCEEDED(hr);
}

void Renderer::SetAlphaBlending(bool enable)
{
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(enable ? m_blendStateAlpha.Get() : nullptr, blendFactor, 0xffffffff);
}

bool Renderer::CreatePrimitiveMeshes()
{
    m_sphereMesh = CreateSphereMesh(1.0f, 32);
    if (!UploadMesh(m_sphereMesh))
        return false;

    m_boxMesh = CreateBoxMesh(XMFLOAT3(1.0f, 1.0f, 1.0f));
    if (!UploadMesh(m_boxMesh))
        return false;

    // Create dynamic vertex buffer for lines (large enough for grid)
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(Vertex) * 20000; // 20k vertices should be enough
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    HRESULT hr = m_device->CreateBuffer(&vbd, nullptr, &m_lineVertexBuffer);
    if (FAILED(hr))
        return false;

    return true;
}

void Renderer::Resize(int width, int height)
{
    if (!m_swapChain)
        return;

    m_width = width;
    m_height = height;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_depthStencilBuffer.Reset();

    m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    CreateRenderTargets();
    CreateDepthStencil();

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = static_cast<float>(m_uiWidth);
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(width - m_uiWidth);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
}

void Renderer::BeginFrame()
{
    // Clear render target to Dark Theme Background (RGB 30, 30, 30)
    // This ensures the UI panel area (which is not covered by the viewport) has the correct background.
    float clearColor[4] = { 30.0f/255.0f, 30.0f/255.0f, 30.0f/255.0f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Set render targets
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

    // Set shaders
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->RSSetState(m_rasterizerStateSolid.Get());
}

void Renderer::EndFrame()
{
    m_swapChain->Present(1, 0);
}

void Renderer::SetViewProjection(const XMMATRIX& view, const XMMATRIX& proj, const XMFLOAT3& cameraPos)
{
    m_viewMatrix = view;
    m_projMatrix = proj;
    m_cameraPos = cameraPos;
}

void Renderer::DrawMesh(const Mesh& mesh, const XMMATRIX& world, const XMFLOAT4& color)
{
    if (!mesh.vertexBuffer || mesh.indexCount == 0)
        return;

    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    ConstantBuffer* cb = static_cast<ConstantBuffer*>(mappedResource.pData);
    cb->World = XMMatrixTranspose(world);
    cb->View = XMMatrixTranspose(m_viewMatrix);
    cb->Projection = XMMatrixTranspose(m_projMatrix);
    cb->Color = color;
    cb->LightDir = XMFLOAT4(0.577f, 0.577f, 0.577f, 0.0f); // Normalized (1,1,1)
    cb->CameraPos = XMFLOAT4(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 1.0f);
    m_context->Unmap(m_constantBuffer.Get(), 0);

    // Set constant buffer
    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    // Set vertex and index buffers
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    m_context->DrawIndexed(mesh.indexCount, 0, 0);
}

void Renderer::DrawSphere(const XMFLOAT3& center, float radius, const XMFLOAT4& color)
{
    XMMATRIX world = XMMatrixScaling(radius, radius, radius) *
                     XMMatrixTranslation(center.x, center.y, center.z);
    DrawMesh(m_sphereMesh, world, color);
}

void Renderer::DrawBox(const XMFLOAT3& center, const XMFLOAT3& extents, const XMMATRIX& rotation, const XMFLOAT4& color)
{
    XMMATRIX world = XMMatrixScaling(extents.x, extents.y, extents.z) *
                     rotation *
                     XMMatrixTranslation(center.x, center.y, center.z);
    DrawMesh(m_boxMesh, world, color);
}

void Renderer::DrawLine(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT4& color)
{
    Vertex v1;
    v1.Position = start;
    v1.Normal = XMFLOAT3(0, 1, 0);
    v1.Color = color;

    Vertex v2;
    v2.Position = end;
    v2.Normal = XMFLOAT3(0, 1, 0);
    v2.Color = color;

    m_lineVertices.push_back(v1);
    m_lineVertices.push_back(v2);
}



Mesh Renderer::CreateSphereMesh(float radius, int segments)
{
    Mesh mesh;
    const int rings = segments / 2;

    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring)
    {
        float phi = static_cast<float>(M_PI) * static_cast<float>(ring) / static_cast<float>(rings);
        float y = cos(phi);
        float ringRadius = sin(phi);

        for (int seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float x = ringRadius * cos(theta);
            float z = ringRadius * sin(theta);

            Vertex v;
            v.Position = XMFLOAT3(x * radius, y * radius, z * radius);
            v.Normal = XMFLOAT3(x, y, z);
            v.Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            mesh.vertices.push_back(v);
        }
    }

    // Generate indices
    for (int ring = 0; ring < rings; ++ring)
    {
        for (int seg = 0; seg < segments; ++seg)
        {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;

            mesh.indices.push_back(current);
            mesh.indices.push_back(next);
            mesh.indices.push_back(current + 1);

            mesh.indices.push_back(current + 1);
            mesh.indices.push_back(next);
            mesh.indices.push_back(next + 1);
        }
    }

    mesh.indexCount = static_cast<UINT>(mesh.indices.size());
    return mesh;
}

Mesh Renderer::CreateBoxMesh(const XMFLOAT3& extents)
{
    Mesh mesh;

    // Define 8 vertices of a box
    XMFLOAT3 positions[8] = {
        { -extents.x, -extents.y, -extents.z },
        {  extents.x, -extents.y, -extents.z },
        {  extents.x,  extents.y, -extents.z },
        { -extents.x,  extents.y, -extents.z },
        { -extents.x, -extents.y,  extents.z },
        {  extents.x, -extents.y,  extents.z },
        {  extents.x,  extents.y,  extents.z },
        { -extents.x,  extents.y,  extents.z }
    };

    // Define 6 faces (each face has 4 vertices)
    struct Face {
        int indices[4];
        XMFLOAT3 normal;
    };

    Face faces[6] = {
        { { 0, 1, 2, 3 }, { 0, 0, -1 } },  // Front
        { { 5, 4, 7, 6 }, { 0, 0,  1 } },  // Back
        { { 4, 0, 3, 7 }, { -1, 0, 0 } },  // Left
        { { 1, 5, 6, 2 }, { 1, 0, 0 } },   // Right
        { { 4, 5, 1, 0 }, { 0, -1, 0 } },  // Bottom
        { { 3, 2, 6, 7 }, { 0, 1, 0 } }    // Top
    };

    // Generate vertices and indices
    for (int i = 0; i < 6; ++i)
    {
        const Face& face = faces[i];
        int baseVertex = static_cast<int>(mesh.vertices.size());

        // Add 4 vertices for this face
        for (int j = 0; j < 4; ++j)
        {
            Vertex v;
            v.Position = positions[face.indices[j]];
            v.Normal = face.normal;
            v.Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            mesh.vertices.push_back(v);
        }

        // Add 2 triangles (6 indices) for this face
        mesh.indices.push_back(baseVertex + 0);
        mesh.indices.push_back(baseVertex + 1);
        mesh.indices.push_back(baseVertex + 2);

        mesh.indices.push_back(baseVertex + 0);
        mesh.indices.push_back(baseVertex + 2);
        mesh.indices.push_back(baseVertex + 3);
    }

    mesh.indexCount = static_cast<UINT>(mesh.indices.size());
    return mesh;
}

Mesh Renderer::CreateMeshFromData(const std::vector<XMFLOAT3>& vertices, const std::vector<uint32_t>& indices)
{
    Mesh mesh;

    // Convert vertices to our vertex format
    for (const auto& pos : vertices)
    {
        Vertex v;
        v.Position = pos;
        v.Normal = XMFLOAT3(0, 1, 0); // Will be computed later
        v.Color = XMFLOAT4(1, 1, 1, 1);
        mesh.vertices.push_back(v);
    }

    // Copy indices
    mesh.indices = indices;
    mesh.indexCount = static_cast<UINT>(indices.size());

    // Compute normals
    for (size_t i = 0; i < mesh.indices.size(); i += 3)
    {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];

        XMVECTOR p0 = XMLoadFloat3(&mesh.vertices[i0].Position);
        XMVECTOR p1 = XMLoadFloat3(&mesh.vertices[i1].Position);
        XMVECTOR p2 = XMLoadFloat3(&mesh.vertices[i2].Position);

        XMVECTOR edge1 = XMVectorSubtract(p1, p0);
        XMVECTOR edge2 = XMVectorSubtract(p2, p0);
        XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

        XMFLOAT3 n;
        XMStoreFloat3(&n, normal);

        mesh.vertices[i0].Normal = n;
        mesh.vertices[i1].Normal = n;
        mesh.vertices[i2].Normal = n;
    }

    return mesh;
}

Mesh Renderer::CreateMeshFromData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    Mesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.indexCount = static_cast<UINT>(indices.size());
    return mesh;
}

bool Renderer::UploadMesh(Mesh& mesh)
{
    // Create vertex buffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vInitData = {};
    vInitData.pSysMem = mesh.vertices.data();

    HRESULT hr = m_device->CreateBuffer(&vbd, &vInitData, &mesh.vertexBuffer);
    if (FAILED(hr))
        return false;

    // Create index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iInitData = {};
    iInitData.pSysMem = mesh.indices.data();

    hr = m_device->CreateBuffer(&ibd, &iInitData, &mesh.indexBuffer);
    return SUCCEEDED(hr);
}

void Renderer::FlushLines()
{
    if (m_lineVertices.empty() || !m_lineVertexBuffer)
        return;

    // Update vertex buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_lineVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        // Check if buffer is large enough. For now assume it is (20k verts)
        // In production, should handle resize or chunking.
        size_t bytesToCopy = (std::min)(m_lineVertices.size() * sizeof(Vertex), (size_t)sizeof(Vertex) * 20000);
        memcpy(mapped.pData, m_lineVertices.data(), bytesToCopy);
        m_context->Unmap(m_lineVertexBuffer.Get(), 0);
    }

    // Set constant buffer (World is Identity for lines usually, but we set it anyway)
    D3D11_MAPPED_SUBRESOURCE mappedCB;
    m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB);
    ConstantBuffer* cb = static_cast<ConstantBuffer*>(mappedCB.pData);
    cb->World = XMMatrixIdentity(); // Lines are drawn in world space directly
    cb->World = XMMatrixTranspose(cb->World);
    cb->View = XMMatrixTranspose(m_viewMatrix);
    cb->Projection = XMMatrixTranspose(m_projMatrix);
    cb->Color = XMFLOAT4(1, 1, 1, 1); // Vertex color used
    cb->LightDir = XMFLOAT4(0, 1, 0, 0);
    cb->CameraPos = XMFLOAT4(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 1.0f);
    m_context->Unmap(m_constantBuffer.Get(), 0);

    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    // Use Unlit shader
    m_context->PSSetShader(m_pixelShaderUnlit.Get(), nullptr, 0);

    // Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_lineVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // Draw
    m_context->Draw(static_cast<UINT>(m_lineVertices.size()), 0);

    // Restore state
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    m_lineVertices.clear();
}

void Renderer::DrawGrid(float size, int divisions, const XMFLOAT4& color)
{
    float step = size / divisions;
    float halfSize = size / 2.0f;

    for (int i = 0; i <= divisions; ++i)
    {
        float pos = -halfSize + i * step;
        
        // Z lines (along X)
        DrawLine(XMFLOAT3(-halfSize, 0, pos), XMFLOAT3(halfSize, 0, pos), color);
        
        // X lines (along Z)
        DrawLine(XMFLOAT3(pos, 0, -halfSize), XMFLOAT3(pos, 0, halfSize), color);
    }
}

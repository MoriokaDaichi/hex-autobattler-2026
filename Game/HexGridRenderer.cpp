#include "stdafx.h"
#include "HexGridRenderer.h"

namespace
{
	// TFT風の水色の縁取りに寄せた、輝度を抑えたグリッド線色(強い光量はブルームで白飛びするため)。
	const Vector4 kGridLineColor(0.35f, 0.6f, 0.7f, 1.0f);
	const Vector4 kAllyZoneColor(0.2f, 0.5f, 0.35f, 1.0f);
	const Vector4 kEnemyZoneColor(0.55f, 0.25f, 0.25f, 1.0f);
	const Vector4 kNeutralZoneColor(0.3f, 0.45f, 0.4f, 1.0f);
	const float kEmptyTileAlpha = 0.35f;
	const float kOccupiedTileAlpha = 0.55f;

	/// <summary>
	/// pointy-topの正六角形における、中心からi番目(0〜5)の頂点への相対座標を返す。
	/// </summary>
	Vector3 HexCornerOffset(float size, int i)
	{
		float angleDeg = 60.0f * i - 30.0f;
		float angleRad = angleDeg * (3.14159265f / 180.0f);
		return Vector3(size * cosf(angleRad), 0.0f, size * sinf(angleRad));
	}
}

void HexGridRenderer::Init()
{
	if (g_graphicsEngine == nullptr) {
		return;
	}

	m_lineVertices.reserve(kMaxLineVertex);
	m_fillVertices.reserve(kMaxFillVertex);

	InitRootSignature();
	InitShaders();
	InitPipelineStates();
	InitVertexBuffers();
	InitIndexBuffers();
	InitConstantBuffer();
	InitDescriptorHeap();
}

void HexGridRenderer::InitRootSignature()
{
	m_rootSignature.Init(
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);
}

void HexGridRenderer::InitShaders()
{
	m_Vshader.LoadVS("Assets/shader/hexGrid.fx", "VSMain");
	m_Pshader.LoadPS("Assets/shader/hexGrid.fx", "PSMain");
}

void HexGridRenderer::InitPipelineStates()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc = { 0 };
	baseDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	baseDesc.pRootSignature = m_rootSignature.Get();
	baseDesc.VS = CD3DX12_SHADER_BYTECODE(m_Vshader.GetCompiledBlob());
	baseDesc.PS = CD3DX12_SHADER_BYTECODE(m_Pshader.GetCompiledBlob());
	baseDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	baseDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	baseDesc.DepthStencilState.DepthEnable = TRUE;
	baseDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	baseDesc.DepthStencilState.StencilEnable = FALSE;
	baseDesc.SampleMask = UINT_MAX;
	baseDesc.NumRenderTargets = 1;
	baseDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // g_mainRenderTargetFormatに合わせる。
	baseDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	baseDesc.SampleDesc.Count = 1;

	// グリッド線用: 不透明・深度書込みあり。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC lineDesc = baseDesc;
	lineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	lineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	m_linePipelineState.Init(lineDesc);

	// ゾーン塗り用: 半透明・深度書込みなし。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC fillDesc = baseDesc;
	fillDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	fillDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	fillDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	fillDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	fillDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	fillDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	fillDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	fillDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	fillDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	fillDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	fillDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	m_fillPipelineState.Init(fillDesc);
}

void HexGridRenderer::InitVertexBuffers()
{
	m_lineVertexBuffer.Init(sizeof(Vertex) * kMaxLineVertex, sizeof(Vertex));
	m_fillVertexBuffer.Init(sizeof(Vertex) * kMaxFillVertex, sizeof(Vertex));
}

void HexGridRenderer::InitIndexBuffers()
{
	m_lineIndexBuffer.Init(sizeof(std::uint16_t) * kMaxLineVertex, sizeof(std::uint16_t));
	static std::uint16_t lineIndices[kMaxLineVertex];
	for (int i = 0; i < kMaxLineVertex; i++) {
		lineIndices[i] = static_cast<std::uint16_t>(i);
	}
	m_lineIndexBuffer.Copy(lineIndices, 0, 0, 0);

	m_fillIndexBuffer.Init(sizeof(std::uint16_t) * kMaxFillVertex, sizeof(std::uint16_t));
	static std::uint16_t fillIndices[kMaxFillVertex];
	for (int i = 0; i < kMaxFillVertex; i++) {
		fillIndices[i] = static_cast<std::uint16_t>(i);
	}
	m_fillIndexBuffer.Copy(fillIndices, 0, 0, 0);
}

void HexGridRenderer::InitConstantBuffer()
{
	m_constantBuffer.Init(sizeof(Matrix));
}

void HexGridRenderer::InitDescriptorHeap()
{
	m_descriptorHeap.RegistConstantBuffer(0, m_constantBuffer);
	m_descriptorHeap.Commit();
}

Vector3 HexGridRenderer::CalcTileCenter(int q, int r) const
{
	// 盤面中心(kCenterQ, kCenterR)がワールド原点になるようにオフセットしてから変換する。
	float offsetQ = static_cast<float>(q) - kCenterQ;
	float offsetR = static_cast<float>(r) - kCenterR;
	float worldX = kHexSize * (sqrtf(3.0f) * offsetQ + sqrtf(3.0f) * 0.5f * offsetR);
	float worldZ = kHexSize * (1.5f * offsetR);
	return Vector3(worldX, 0.0f, worldZ);
}

void HexGridRenderer::BuildGridLines()
{
	m_lineVertices.clear();

	for (int q = kMinQ; q <= kMaxQ; ++q) {
		for (int r = kMinR; r <= kMaxR; ++r) {
			Vector3 center = CalcTileCenter(q, r);

			Vector3 corners[6];
			for (int i = 0; i < 6; ++i) {
				corners[i] = center + HexCornerOffset(kHexSize, i);
			}

			for (int i = 0; i < 6; ++i) {
				Vertex v0{ corners[i], kGridLineColor };
				Vertex v1{ corners[(i + 1) % 6], kGridLineColor };
				m_lineVertices.push_back(v0);
				m_lineVertices.push_back(v1);
			}
		}
	}
}

void HexGridRenderer::BuildTileFills(const GameState& gameState)
{
	m_fillVertices.clear();

	const Player& player = gameState.players[0];

	for (int q = kMinQ; q <= kMaxQ; ++q) {
		for (int r = kMinR; r <= kMaxR; ++r) {
			HexCoord coord(q, r);
			Vector3 center = CalcTileCenter(q, r);

			Vector4 zoneColor = kNeutralZoneColor;
			if (q <= 2) {
				zoneColor = kAllyZoneColor;
			}
			else if (q >= 6) {
				zoneColor = kEnemyZoneColor;
			}

			bool occupied = false;
			for (const auto& unit : player.board) {
				if (unit.position == coord) {
					occupied = true;
					break;
				}
			}

			Vector4 fillColor = zoneColor;
			fillColor.w = occupied ? kOccupiedTileAlpha : kEmptyTileAlpha;

			Vector3 corners[6];
			for (int i = 0; i < 6; ++i) {
				corners[i] = center + HexCornerOffset(kHexSize, i);
			}

			for (int i = 0; i < 6; ++i) {
				m_fillVertices.push_back({ center, fillColor });
				m_fillVertices.push_back({ corners[i], fillColor });
				m_fillVertices.push_back({ corners[(i + 1) % 6], fillColor });
			}
		}
	}
}

void HexGridRenderer::Draw(RenderContext& rc, const GameState& gameState)
{
	BuildGridLines();
	BuildTileFills(gameState);

	UpdateConstantBuffer();

	g_renderingEngine->AddRenderObject(this);
}

void HexGridRenderer::UpdateConstantBuffer()
{
	Matrix vp = g_camera3D->GetViewProjectionMatrix();
	m_constantBuffer.CopyToVRAM(&vp);
}

void HexGridRenderer::OnForwardRender(RenderContext& rc)
{
	if (m_lineVertices.empty()) {
		return;
	}

	m_lineVertexBuffer.Copy(&m_lineVertices.front());

	rc.SetRootSignature(m_rootSignature);
	rc.SetPipelineState(m_linePipelineState);
	rc.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	rc.SetVertexBuffer(m_lineVertexBuffer);
	rc.SetIndexBuffer(m_lineIndexBuffer);
	rc.SetDescriptorHeap(m_descriptorHeap);
	rc.DrawIndexed(static_cast<UINT>(m_lineVertices.size()));
}

void HexGridRenderer::OnTlanslucentRender(RenderContext& rc)
{
	if (m_fillVertices.empty()) {
		return;
	}

	m_fillVertexBuffer.Copy(&m_fillVertices.front());

	rc.SetRootSignature(m_rootSignature);
	rc.SetPipelineState(m_fillPipelineState);
	rc.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	rc.SetVertexBuffer(m_fillVertexBuffer);
	rc.SetIndexBuffer(m_fillIndexBuffer);
	rc.SetDescriptorHeap(m_descriptorHeap);
	rc.DrawIndexed(static_cast<UINT>(m_fillVertices.size()));
}

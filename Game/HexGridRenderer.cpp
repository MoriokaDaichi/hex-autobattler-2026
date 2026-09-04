#include "stdafx.h"
#include "HexGridRenderer.h"
#include <cmath>

namespace
{
	// TFT風の水色の縁取りに寄せた、輝度を抑えたグリッド線色(強い光量はブルームで白飛びするため)。
	const Vector4 kGridLineColor(0.35f, 0.6f, 0.7f, 1.0f);
	const Vector4 kAllyZoneColor(0.2f, 0.5f, 0.35f, 1.0f);
	const Vector4 kEnemyZoneColor(0.55f, 0.25f, 0.25f, 1.0f);
	// r2(プレイヤー最前列)とr3(敵最前列)の境目を示す帯。座標ギャップは設けず
	// (常に1つの6行×9列の戦場)、ゾーン色分けとこの帯で2陣営を視覚的に分ける
	// (board-layout-rework、ユーザー確定「ギャップ無し」)。
	// 当初は1px LINELIST で描いていたが実機でほぼ視認不可だったため、太い塗り帯(三角形)に変更
	// (board-layout-tuning 作業1)。DirectX12 に線幅の概念が無く LINELIST は常に1px のため。
	const Vector4 kZoneBoundaryColor(1.0f, 0.72f, 0.12f, 0.9f); // 明るい琥珀。グリッド線(水色)と明確に差をつける。
	const float kZoneBoundaryHalfWidth = 9.0f; // 帯の半幅(ワールド単位。kHexSize=50 に対し全幅18)。
	const float kZoneBoundaryLiftY = 0.6f;     // グリッド線・タイル塗りとの Z ファイト回避に僅かに浮かせる。
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

Vector3 HexGridRenderer::CalcTileCenter(int q, int r)
{
	// 盤面中心(kCenterQ, kCenterR)がワールド原点になるようにオフセットしてから変換する。
	float offsetQ = static_cast<float>(q) - kCenterQ;
	float offsetR = static_cast<float>(r) - kCenterR;
	float worldX = kHexSize * (sqrtf(3.0f) * offsetQ + sqrtf(3.0f) * 0.5f * offsetR);
	float worldZ = kHexSize * (1.5f * offsetR);
	return Vector3(worldX, 0.0f, worldZ);
}

bool HexGridRenderer::TryWorldPositionToHex(const Vector3& worldPos, HexCoord& outHex)
{
	// CalcTileCenterのpointy-top軸座標変換を、標準的なcube round法で逆算する。
	float offsetQf = (sqrtf(3.0f) / 3.0f * worldPos.x - 1.0f / 3.0f * worldPos.z) / kHexSize;
	float offsetRf = (2.0f / 3.0f * worldPos.z) / kHexSize;
	float qf = offsetQf + kCenterQ;
	float rf = offsetRf + kCenterR;

	// axial -> cube座標に変換して四捨五入し、誤差が一番大きい成分を残り2つから再計算することで
	// 「一番近いマス」を正しく求める(単純にq,rをそれぞれ丸めるだけでは隣接マスにズレうる)。
	float xf = qf;
	float zf = rf;
	float yf = -xf - zf;

	int xi = static_cast<int>(std::round(xf));
	int yi = static_cast<int>(std::round(yf));
	int zi = static_cast<int>(std::round(zf));

	float xDiff = std::abs(xi - xf);
	float yDiff = std::abs(yi - yf);
	float zDiff = std::abs(zi - zf);

	if (xDiff > yDiff && xDiff > zDiff) {
		xi = -yi - zi;
	}
	else if (yDiff > zDiff) {
		yi = -xi - zi;
	}
	else {
		zi = -xi - yi;
	}

	HexCoord candidate(xi, zi);
	if (!IsValidHex(candidate)) {
		return false;
	}

	outHex = candidate;
	return true;
}

bool HexGridRenderer::IsValidHex(const HexCoord& hex)
{
	return hex.q >= kMinQ && hex.q <= kMaxQ && hex.r >= kMinR && hex.r <= kMaxR;
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

	// r2/r3 境界の帯(太い塗り)は BuildTileFills() 側で m_fillVertices に積む
	// (LINELIST は 1px で実機視認不可のため。board-layout-tuning 作業1)。
}

void HexGridRenderer::BuildTileFills(const GameState& gameState)
{
	m_fillVertices.clear();

	const Player& player = gameState.players[0];

	for (int q = kMinQ; q <= kMaxQ; ++q) {
		for (int r = kMinR; r <= kMaxR; ++r) {
			HexCoord coord(q, r);
			Vector3 center = CalcTileCenter(q, r);

			// r0-2=プレイヤー陣地 / r3-5=敵陣地(中立ゾーンは廃止)。
			Vector4 zoneColor = (r <= kAllyZoneMaxR) ? kAllyZoneColor : kEnemyZoneColor;

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

	// --- r2/r3 境界の帯 ---
	// 敵最前列 r=(kAllyZoneMaxR+1) の"プレイヤー側を向いた2辺"(pointy-top下端: corner 4-5, 5-0)を
	// XZ平面内で法線方向へ ±kZoneBoundaryHalfWidth 押し出し、太い矩形帯(2三角形/辺)にする。
	// タイル塗りの後に積むことで最前面に重なる(translucentパスは深度書込み無し=描画順が奥→手前)。
	{
		const int boundaryR = kAllyZoneMaxR + 1;
		auto pushEdgeBand = [&](const Vector3& a, const Vector3& b)
		{
			Vector3 dir = b - a;
			float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
			if (len < 0.0001f) return;
			Vector3 n(-dir.z / len, 0.0f, dir.x / len); // XZ平面での法線。
			Vector3 off(n.x * kZoneBoundaryHalfWidth, 0.0f, n.z * kZoneBoundaryHalfWidth);
			Vector3 lift(0.0f, kZoneBoundaryLiftY, 0.0f);
			Vector3 a0 = a + off + lift, a1 = a - off + lift;
			Vector3 b0 = b + off + lift, b1 = b - off + lift;
			m_fillVertices.push_back({ a0, kZoneBoundaryColor });
			m_fillVertices.push_back({ a1, kZoneBoundaryColor });
			m_fillVertices.push_back({ b0, kZoneBoundaryColor });
			m_fillVertices.push_back({ a1, kZoneBoundaryColor });
			m_fillVertices.push_back({ b1, kZoneBoundaryColor });
			m_fillVertices.push_back({ b0, kZoneBoundaryColor });
		};

		for (int q = kMinQ; q <= kMaxQ; ++q) {
			Vector3 center = CalcTileCenter(q, boundaryR);
			Vector3 c4 = center + HexCornerOffset(kHexSize, 4);
			Vector3 c5 = center + HexCornerOffset(kHexSize, 5);
			Vector3 c0 = center + HexCornerOffset(kHexSize, 0);
			pushEdgeBand(c4, c5);
			pushEdgeBand(c5, c0);
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

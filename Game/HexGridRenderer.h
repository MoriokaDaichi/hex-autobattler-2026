#pragma once
#include "GameState.h"

/// <summary>
/// 盤面(ヘックスグリッド)のグリッド線とゾーン塗りを描画するクラス。
/// IRendererを継承し、RenderingEngineの描画パイプライン(ForwardRendering)に
/// 登録される形で実際のGPU描画を行う(Game::Render()内で直接DrawIndexedすると、
/// 後段のRenderingEngine::Execute()に上書きされてしまうため)。
/// </summary>
class HexGridRenderer : public IRenderer, public Noncopyable
{
public:
	void Init();

	/// <summary>
	/// CPU側の頂点リストを構築し、今フレームの描画オブジェクトとして登録する。
	/// 毎フレームGame::Render()から呼ぶことを想定している。
	/// </summary>
	void Draw(RenderContext& rc, const GameState& gameState);

	// IRendererオーバーライド。RenderingEngine::Execute()のForwardRendering()から呼ばれる。
	void OnForwardRender(RenderContext& rc) override;
	void OnTlanslucentRender(RenderContext& rc) override;

	/// <summary>
	/// axial座標(q,r)を、このグリッドが使っているワールド座標系(盤面中心が原点)に変換する。
	/// グリッド線・ゾーン塗りの座標計算そのものだが、ユニットモデルの配置(UnitModelDisplay)からも
	/// 同じ座標系を再利用するためpublic staticとして公開している。
	/// </summary>
	static Vector3 CalcTileCenter(int q, int r);

private:
	struct Vertex
	{
		Vector3 pos;
		Vector4 color;
	};

	// 盤面の範囲(axial座標)。0-2:自陣、3-5:中央緩衝、6-8:敵陣。
	static const int kMinQ = 0;
	static const int kMaxQ = 8;
	static const int kMinR = 0;
	static const int kMaxR = 2;
	static constexpr float kHexSize = 50.0f;
	// 盤面(q:0-8, r:0-2)の中心マス相当の値。この分だけ引いてから変換することで、
	// 盤面全体の中心をワールド原点付近に合わせる。
	static constexpr float kCenterQ = 4.0f;
	static constexpr float kCenterR = 1.0f;

	static const int kMaxLineVertex = 512;
	static const int kMaxFillVertex = 1024;

	void BuildGridLines();
	void BuildTileFills(const GameState& gameState);

	void InitRootSignature();
	void InitShaders();
	void InitPipelineStates();
	void InitVertexBuffers();
	void InitIndexBuffers();
	void InitConstantBuffer();
	void InitDescriptorHeap();

	void UpdateConstantBuffer();

	std::vector<Vertex> m_lineVertices;
	std::vector<Vertex> m_fillVertices;

	ConstantBuffer m_constantBuffer; // VP行列。Line/Fill両パスで共有。
	VertexBuffer m_lineVertexBuffer;
	VertexBuffer m_fillVertexBuffer;
	IndexBuffer m_lineIndexBuffer;
	IndexBuffer m_fillIndexBuffer;
	RootSignature m_rootSignature;
	Shader m_Vshader;
	Shader m_Pshader;
	PipelineState m_linePipelineState;
	PipelineState m_fillPipelineState;
	DescriptorHeap m_descriptorHeap;
};

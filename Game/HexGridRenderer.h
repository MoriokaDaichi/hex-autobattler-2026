#pragma once
#include "GameState.h"
#include "HexCoord.h"

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

	/// <summary>
	/// CalcTileCenterの逆変換。ワールド座標(y成分は無視し、xz平面上の位置として扱う)から
	/// 最も近いマスのaxial座標を求める。盤面の範囲外だった場合はfalseを返す。
	/// マウスピッキングによるヘックスカーソル(CursorSelectionSystem)から利用する。
	/// </summary>
	static bool TryWorldPositionToHex(const Vector3& worldPos, HexCoord& outHex);

	/// <summary>
	/// 指定のaxial座標が盤面の範囲内かどうかを判定する。
	/// </summary>
	static bool IsValidHex(const HexCoord& hex);

	// 自陣(プレイヤーがユニットを配置できるゾーン)のaxial r範囲。
	// board-layout-rework: 盤面を r 方向へ 0-5 に拡張し、r0-2=プレイヤー陣地 / r3-5=敵陣地 の
	// 2ゾーン制にした(中立ゾーン・座標ギャップは無し。r2とr3は直接隣接し、常に1つの6行×9列の戦場)。
	// ゾーン区分の"正"はこの2定数。IsValidHex等と同様、配置可否判定など他所からも参照するためpublicに置く。
	static constexpr int kAllyZoneMinR = 0;
	static constexpr int kAllyZoneMaxR = 2;

private:
	struct Vertex
	{
		Vector3 pos;
		Vector4 color;
	};

	// 盤面の範囲(axial座標)。列 q:0-8(9列)、行 r:0-5(6行)。r0-2:プレイヤー陣地 / r3-5:敵陣地。
	static const int kMinQ = 0;
	static const int kMaxQ = 8;
	static const int kMinR = 0;
	static const int kMaxR = 5;
	static constexpr float kHexSize = 50.0f;
	// 盤面(q:0-8, r:0-5)の中心マス相当の値。この分だけ引いてから変換することで、
	// 盤面全体の中心をワールド原点付近に合わせる(r中心は 0..5 の中点=2.5)。
	static constexpr float kCenterQ = 4.0f;
	static constexpr float kCenterR = 2.5f;

	// 6行×9列=54マス。線: 54*12=648頂点 + r2/r3境界ライン。塗り: 54*18=972頂点。余裕をみて確保する
	// (board-layout-rework で 27マス→54マスに増えたため、旧値 512/1024 では線が欠ける)。
	static const int kMaxLineVertex = 1024;
	static const int kMaxFillVertex = 1536;

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

#pragma once
#include <vector>
#include "HexCoord.h"

class UIRectRenderer;

/// <summary>
/// ユニット合成で星が上がった瞬間の演出(star-up-effect)を描くクラス。
/// エフェクト素材(.efk等)がプロジェクトに無いため、UIRectRenderer の半透明矩形だけで自作している。
///  - 光のリング(足元の地面上の円を BoardUIRenderer::WorldToUI で射影した光点の輪)
///  - 光の柱(足元から上へ伸びる縦帯)
///  - 立ち上る粒子(菱形の光点)
///  - ユニットのポップ(UnitModelDisplay が GetModelPopScale() を見てモデルを一瞬拡大する)
///  - ★3のみ: 画面フラッシュ・カメラ揺れ(Game が GetCameraShakeOffset() を見てカメラをずらす)
/// ★2と★3は同じ描画関数・同じ色で描き、パラメータ(規模・数・時間・明るさ)だけを変える
/// (パラメータ表は StarUpEffectRenderer.cpp 冒頭、設計は docs/tasks/star-up-effect/plan.md)。
///
/// ShopUIRenderer等と同じ方式: IRenderer を継承し OnRender2D で描画、準備フェーズ中に毎フレーム
/// Game::Render() から Draw() で g_renderingEngine->AddRenderObject() する。入力・進行には関与しない。
/// </summary>
class StarUpEffectRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 演出を1件開始する。onBoard=true なら盤面の boardPos のマス、false ならベンチの benchIndex 行に出す。
	/// 同じ場所で再生中の演出があれば置き換える(連鎖合成でも最新の星の演出だけが見えるように)。
	/// </summary>
	void Spawn(int newStarLevel, bool onBoard, const HexCoord& boardPos, int benchIndex);

	/// <summary>準備フェーズ中に毎フレーム呼ぶ。経過時間を進め、終わった演出を取り除く。</summary>
	void Update(float deltaTime);

	/// <summary>再生中の演出を全て破棄する(準備フェーズ以外では Game がこれを呼ぶ)。</summary>
	void Clear();

	/// <summary>準備フェーズ中に毎フレーム呼ぶ。再生中の演出があれば今フレームの描画に登録する。</summary>
	void Draw(RenderContext& rc, UIRectRenderer& rectRenderer);

	/// <summary>
	/// 盤面の pos のマスに居るユニットへ掛けるポップ倍率(1.0=等倍)。UnitModelDisplay::Update() から呼ばれる。
	/// </summary>
	float GetModelPopScale(const HexCoord& pos) const;

	/// <summary>カメラ揺れのオフセット(ワールド単位、XZ平面)。揺れていなければゼロ。</summary>
	Vector3 GetCameraShakeOffset() const;

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	/// <summary>再生中の演出1件。</summary>
	struct ActiveEffect
	{
		int starLevel = 2;
		bool onBoard = false;
		HexCoord boardPos;
		int benchIndex = 0;
		float elapsed = 0.0f;    // 開始からの経過秒。
		unsigned int seed = 0;   // 粒子の疑似乱数シード(演出ごとに変える)。
	};

	/// <summary>
	/// 演出のローカル座標(足元原点、y上向き、xz=地面)をUI空間へ変換する。盤面はワールドへ置いてカメラで射影、
	/// ベンチは行カード中心を原点にした平たい投影で代用する。画面外なら false。
	/// </summary>
	bool Project(const ActiveEffect& e, const Vector3& local, Vector2& outUI) const;

	/// <summary>1件分(リング→柱→粒子→フラッシュ)を描く。★2/★3共通。</summary>
	void DrawEffect(RenderContext& rc, const ActiveEffect& e);

	std::vector<ActiveEffect> m_effects;
	unsigned int m_nextSeed = 1;
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
};

#pragma once
#include "EconomySystem.h"

class UIRectRenderer;

/// <summary>
/// ラウンド結果の一言表示(Phase::Result)、ゲームオーバー画面(Phase::GameOver)、
/// ゲームクリア画面(Phase::Victory)をまとめて描画するクラス。
///
/// ShopUIRenderer/TitleUIRenderer等と同じ方式: IRendererを継承しOnRender2Dで描画、
/// 毎フレームGame::Render()からDraw系メソッドで状態を受け取りg_renderingEngine->AddRenderObject()で
/// 当該フレームの描画に登録する。座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
///
/// [既知のFontEngine制限、TitleUIRenderer実装時に判明したもの]
/// 1. pivotは文字列幅で正規化されたアンカーではなく生のピクセルオフセットでしかないため
///    (MeasureString相当が無い)、(0.5,0.5)等では真の中央揃えにならない。kTopLeftPivot前提で、
///    文字列の見た目の長さに合わせて開始X座標を手動調整する。
/// 2. Font(SpriteBatch経由)のcolor.wによるアルファブレンドは機能しない(常に不透明)。点滅表現は
///    アルファフェードではなく一定間隔で描画自体をON/OFFする方式で行う(TitleUIRendererと同じ)。
///    ただしSprite経由(UIRectRenderer)のアルファは機能する。GameOver/Victoryの暗幕は
///    UIRectRenderer::DrawRectで半透明矩形を1枚描いて実現する(ui-sprite-bars、plan.md参照)。
/// </summary>
class ResultUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// Phase::Result中、毎フレーム呼ぶ。「ROUND CLEAR!」/「DEFEAT...」の一言を表示する。
	/// </summary>
	void DrawRoundResult(RenderContext& rc, CombatResult result);

	/// <summary>
	/// Phase::GameOver中、毎フレーム呼ぶ。到達ラウンドを添えて表示する。
	/// </summary>
	/// <param name="reachedRound">力尽きた時点でのラウンド数(GameState::roundNumber)。</param>
	/// <param name="totalRounds">GameState::kTotalRounds。</param>
	/// <param name="deltaTime">再スタート案内の点滅用。</param>
	/// <param name="rectRenderer">暗幕(半透明矩形)を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void DrawGameOver(RenderContext& rc, int reachedRound, int totalRounds, float deltaTime, UIRectRenderer& rectRenderer);

	/// <summary>
	/// Phase::Victory中、毎フレーム呼ぶ。
	/// </summary>
	/// <param name="totalRounds">GameState::kTotalRounds。全クリア文言に添える。</param>
	/// <param name="deltaTime">再スタート案内の点滅用。</param>
	/// <param name="rectRenderer">暗幕(半透明矩形)を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void DrawVictory(RenderContext& rc, int totalRounds, float deltaTime, UIRectRenderer& rectRenderer);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	enum class Mode { None, RoundResult, GameOver, Victory };

	Mode m_mode = Mode::None;
	CombatResult m_roundResult = CombatResult::Win;
	int m_reachedRound = 1;
	int m_totalRounds = 1;
	float m_elapsedTime = 0.0f; // GameOver/Victory中の再スタート案内の点滅用。Draw()毎回積算。
	UIRectRenderer* m_rectRenderer = nullptr; // GameOver/Victoryの暗幕描画用。DrawGameOver/DrawVictoryで受け取る。

	Font m_font;
};

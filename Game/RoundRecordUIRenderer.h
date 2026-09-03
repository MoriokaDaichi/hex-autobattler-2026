#pragma once
#include <string>
#include "UIHotRegion.h"

struct GameState;
class UIRectRenderer;

/// <summary>
/// 画面右上に「現在ラウンド数 / 残りラウンド数」「(現在の敵に対する)連敗カウント」を
/// 常時表示するHUD。ShopUIRenderer/BoardUIRenderer と同じ方式: IRenderer を継承し
/// OnRender2D で描画、毎フレーム Game::Render() から Draw() で状態を受け取り
/// g_renderingEngine->AddRenderObject() で当該フレームの描画に登録する。
/// 座標系は UI_SPACE(1920x1080、中央原点・y上向き)。フェーズを問わず常時表示する
/// (準備/戦闘中に限らず、今何ラウンド目かは常に把握できてよい情報のため)。
/// </summary>
class RoundRecordUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>毎フレームGame::Render()から呼ぶ。GameStateの現在値をコピーして保持する。</summary>
	/// <param name="rectRenderer">カード背景の塗り矩形を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void Draw(RenderContext& rc, const GameState& gameState, UIRectRenderer& rectRenderer);

	/// <summary>
	/// ROUND/残りラウンド/連敗のクリック可能矩形(HudRoundDisplay)と、連勝連敗ストリーク表示の
	/// クリック可能矩形(HudStreakDisplay)をoutへ追加する。ホバーのみ・クリック無反応。
	/// レイアウトが固定のため引数無し・描画を伴わない純粋関数。
	/// </summary>
	void BuildHotRegions(UIHotRegionList& out) const;

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	Font m_font;

	int m_roundNumber = 1;    // 表示用に kTotalRounds でクランプ済み。
	int m_totalRounds = 1;
	int m_lossCount = 0;
	int m_maxLosses = 1;
	int m_winStreak = 0;      // 連勝数(Player由来、EconomySystemが更新)。lossStreakと排他。
	int m_lossStreak = 0;     // 連敗数(Player由来)。m_lossCount(現在の敵への敗北数)とは別概念。
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
	bool m_hasData = false;
};

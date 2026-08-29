#pragma once
#include <string>

struct GameState;

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
	void Draw(RenderContext& rc, const GameState& gameState);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	Font m_font;

	int m_roundNumber = 1;    // 表示用に kTotalRounds でクランプ済み。
	int m_totalRounds = 1;
	int m_lossCount = 0;
	int m_maxLosses = 1;
	bool m_hasData = false;
};

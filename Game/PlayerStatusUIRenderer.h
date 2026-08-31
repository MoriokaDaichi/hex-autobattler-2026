#pragma once

struct Player;
class UIRectRenderer;

/// <summary>
/// 所持ゴールドとプレイヤーレベル/経験値ゲージを、フェーズを問わず常時表示するHUD。
///
/// ShopUIRenderer/BoardUIRendererと同じ方式: IRendererを継承しOnRender2Dで描画、
/// 毎フレームGame::Render()の先頭でDraw()から状態を受け取りg_renderingEngine->AddRenderObject()で
/// 当該フレームの描画に登録する(準備フェーズに限らず全フェーズで呼ぶ点が他の2クラスと異なる)。
/// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。画面左上のFPS表示・BoardUIRendererの
/// ベンチ一覧(いずれも画面左側)、ShopUIRendererの下部ヘッダー行(画面下部)と重ならないよう、
/// 画面右上に配置する。
/// </summary>
class PlayerStatusUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 毎フレームGame::Render()の先頭(フェーズ分岐の外)から呼ぶ。
	/// </summary>
	/// <param name="xpForNextLevel">次レベルに必要な経験値(LevelSystem::XPForNextLevel)。最大レベル到達時は0。</param>
	/// <param name="rectRenderer">XPバーの塗り矩形を描く共通ヘルパー。OnRender2D用にポインタを保持する。</param>
	void Draw(RenderContext& rc, const Player& player, int xpForNextLevel, UIRectRenderer& rectRenderer);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	Font m_font;

	int m_gold = 0;
	int m_level = 0;
	int m_xp = 0;
	int m_xpForNextLevel = 0;
	int m_boardCount = 0;    // 盤面に配置済みのユニット数。
	int m_maxBoardSize = 0;  // 現在のレベルで置ける上限(Player::GetMaxBoardSize)。
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
	bool m_hasData = false;
};

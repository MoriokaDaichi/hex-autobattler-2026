#pragma once

/// <summary>
/// タイトル画面(Phase::Title)の2D UI(タイトルロゴ代わりの文字列 + スタート操作ガイド)を描画するクラス。
///
/// ShopUIRenderer/BoardUIRenderer等と同じ方式: IRendererを継承しOnRender2Dで描画、毎フレーム
/// Game::Render()からDraw()で状態を受け取りg_renderingEngine->AddRenderObject()で
/// 当該フレームの描画に登録する。ロゴ画像等の素材が無い前提のため、文字ベースで構成する。
/// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
/// </summary>
class TitleUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// Phase::Title中、毎フレームGame::Render()から呼ぶ。プロンプト文字列の点滅表現のため、
	/// 内部で経過時間を積算する。
	/// </summary>
	/// <param name="hasSaveData">
	/// セーブデータが存在するか。true なら「[A] CONTINUE / [X] NEW GAME」、false なら
	/// 「PRESS [A] TO START」を表示する。
	/// </param>
	void Draw(RenderContext& rc, float deltaTime, bool hasSaveData);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	Font m_font;
	float m_elapsedTime = 0.0f;
	bool m_hasSaveData = false;
};

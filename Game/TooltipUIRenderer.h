#pragma once
#include <string>
#include <vector>

class UIRectRenderer;

/// <summary>
/// マウスホバー中(またはゲームパッドでフォーカス中)のUI要素の詳細説明を、カーソル付近に
/// カード状のツールチップとして表示するクラス。ShopUIRenderer等と同じ方式: IRendererを継承し
/// OnRender2Dで描画、毎フレームGame::Update()がTooltipContentBuilderで組み立てた行を
/// Draw()で受け取りg_renderingEngine->AddRenderObject()で当該フレームの描画に登録する。
/// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
///
/// docs/tasks/ui-mouse-cards/plan.md §3-3参照。MeasureString相当が無いため、幅は最長行の
/// 文字数から概算する(全角は2文字分)。
/// </summary>
class TooltipUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 毎フレーム、表示すべきツールチップがある間だけ呼ぶ(無ければ呼ばない=非表示)。
	/// </summary>
	/// <param name="lines">表示するテキスト行(1行目はタイトル扱いで強調色にする)。空なら何もしない。</param>
	/// <param name="anchor">アンカー位置(UI_SPACE座標)。マウス操作時はカーソル位置、ゲームパッド
	/// 操作時はフォーカス中要素の位置を渡す想定。この右下にツールチップを出す(画面端は自動クランプ)。</param>
	/// <param name="rectRenderer">背景パネル/枠の塗り矩形を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void Draw(RenderContext& rc, const std::vector<std::wstring>& lines, const Vector2& anchor, UIRectRenderer& rectRenderer);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	Font m_font;
	std::vector<std::wstring> m_lines;
	Vector2 m_anchor;
	UIRectRenderer* m_rectRenderer = nullptr;
	bool m_hasData = false;
};

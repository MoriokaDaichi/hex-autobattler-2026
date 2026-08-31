#pragma once

/// <summary>
/// 単色の塗り矩形(半透明可)を描く共通ヘルパー。white.dds(32x32、無地白)1枚を
/// Sprite::SetMulColor()で任意色に染めて使う(素材9色のうち白のみ使用)。
///
/// ShopUIRenderer等の既存UIRendererと違い自身はIRendererではなく、各UIRendererの
/// OnRender2D内から直接呼ばれる薄いラッパー。Gameが1個所有し、利用する各UIRendererへ
/// 参照を渡す(Sprite 1個を毎フレーム複数回Update/Draw()し使い回す。Fontを1個のFontで
/// 使い回すのと同じ考え方)。
///
/// [呼び出し側の注意] Font::Begin()〜End()の区間の外側(前)で呼ぶこと。Sprite::Draw()は
/// 独自にRootSignature/PipelineState/DescriptorHeapを設定するため、Font::Begin()が
/// 開始したSpriteBatchの状態と競合する(docs/tasks/ui-sprite-bars/plan.md §0-8参照)。
/// </summary>
class UIRectRenderer : public Noncopyable
{
public:
	/// <summary>1回だけ呼ぶ(Game::Start())。white.ddsを読み込みSprite初期化を行う。</summary>
	void Init();

	/// <summary>
	/// 矩形を1枚描く。座標系はUI_SPACE(1920x1080、中央原点、y上向き)、Fontと共通。
	/// </summary>
	/// <param name="pos">ピボット位置のUI座標。</param>
	/// <param name="size">幅・高さ(ピクセル)。</param>
	/// <param name="color">RGBA。alphaは0〜1(半透明可、AlphaBlendMode_Trans固定)。</param>
	/// <param name="pivot">0〜1の正規化ピボット。省略時(0.5,0.5)=中心。左端基準で右に伸ばす
	/// バー表現には(0.0,0.5)を指定する。</param>
	void DrawRect(RenderContext& rc, const Vector2& pos, const Vector2& size, const Vector4& color,
		const Vector2& pivot = Vector2(0.5f, 0.5f));

private:
	Sprite m_sprite;
};

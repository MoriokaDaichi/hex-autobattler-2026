#pragma once

#include <vector>
#include <memory>

/// <summary>
/// 単色の塗り矩形(半透明可)を描く共通ヘルパー。white.dds(32x32、無地白)1枚を
/// Sprite::SetMulColor()で任意色に染めて使う(素材9色のうち白のみ使用)。
///
/// ShopUIRenderer等の既存UIRendererと違い自身はIRendererではなく、各UIRendererの
/// OnRender2D内から直接呼ばれる薄いラッパー。Gameが1個所有し、利用する各UIRendererへ
/// 参照を渡す。
///
/// [重要] このエンジンのSpriteは定数バッファを描画コールごとに分けないため、1個の
/// Spriteインスタンスを1フレーム内で複数回Draw()すると、最後の1回のMVP/乗算カラーで
/// 全矩形が描かれてしまう(＝実質1枚しか出ない)。RenderingEngineの全画面合成スプライトや
/// 各ポストエフェクト、CalcSceneLuminance(Sprite配列)がいずれも「描画1回につきSprite1個」
/// なのと同じ制約。そのためDrawRect()呼び出しごとに別のSpriteインスタンスをプールから
/// 割り当てる。プールはフレーム跨ぎで再利用し、BeginFrame()で使用カーソルを戻す。
///
/// [呼び出し側の注意] Font::Begin()〜End()の区間の外側(前)で呼ぶこと。Sprite::Draw()は
/// 独自にRootSignature/PipelineState/DescriptorHeapを設定するため、Font::Begin()が
/// 開始したSpriteBatchの状態と競合する(docs/tasks/ui-sprite-bars/plan.md §0-8参照)。
/// </summary>
class UIRectRenderer : public Noncopyable
{
public:
	/// <summary>1回だけ呼ぶ(Game::Start())。white.ddsを読み込み、矩形プールを事前確保する。</summary>
	void Init();

	/// <summary>
	/// フレームの先頭で1回呼ぶ(Game::Render()の冒頭)。矩形プールの使用カーソルを0へ戻す。
	/// これを呼ばないとプールが毎フレーム際限なく成長する。
	/// </summary>
	void BeginFrame();

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
	/// <summary>使用カーソル位置のSpriteを返す。プールが足りなければ1個生成して足す。</summary>
	Sprite& AcquireSprite();

	// DrawRect呼び出しごとに1個ずつ割り当てるSpriteプール。SpriteはNoncopyableかつ
	// ムーブ不可(仮想デストラクタあり)のためvector<Sprite>は不可。unique_ptrで保持する。
	std::vector<std::unique_ptr<Sprite>> m_pool;
	size_t m_used = 0; // 今フレームで割り当て済みの数。BeginFrame()で0に戻す。
};

#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"

struct UnitInstance;
class TraitDatabase;
class TraitSystem;
class UIRectRenderer;

/// <summary>
/// 全トレイト(TraitDatabase::GetAllTraitDefs())の発動状況を画面左側に一覧表示するクラス。
///
/// BoardUIRenderer/ItemInventoryUIRenderer等と同じ方式: IRendererを継承しOnRender2Dで描画、
/// 準備フェーズ中に毎フレームGame::Render()からDraw()で状態を受け取りg_renderingEngine->
/// AddRenderObject()で当該フレームの描画に登録する。座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
///
/// TFT本家の「発動中は上部にハイライト、未発動は下部にディム表示、各行に現在人数と次の
/// ブレイクポイント閾値」という見せ方を、テキストベースで踏襲する。
/// </summary>
class TraitPanelUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 準備フェーズ中、毎フレーム呼ぶ。boardの構成から各トレイトの発動状況を集計して保持する。
	/// </summary>
	/// <param name="rectRenderer">パネル背景・行区切りの塗り矩形を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void Draw(RenderContext& rc, const std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem, UIRectRenderer& rectRenderer);

	/// <summary>
	/// 現フレームの各トレイト行のクリック可能矩形(ホバーのみ、クリック無反応)をoutへ追加する。
	/// 描画を伴わない純粋関数。Draw()と同じ「発動中を先頭にまとめる」並べ替えロジックを行毎に
	/// 再現し、各行にそのトレイトのUIHotRegion(index=(int)TraitType)を対応付ける。
	/// </summary>
	void BuildHotRegions(const std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem, UIHotRegionList& out) const;

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	/// <summary>トレイト1行分の表示内容。</summary>
	struct TraitRow
	{
		std::wstring text;
		bool active = false;
	};

	Font m_font;
	std::vector<TraitRow> m_rows; // 発動中を先頭にまとめ、続けて未発動を並べた状態で保持する。
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
	bool m_hasData = false;
};

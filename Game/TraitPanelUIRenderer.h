#pragma once
#include <string>
#include <vector>

struct UnitInstance;
class TraitDatabase;
class TraitSystem;

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
	void Draw(RenderContext& rc, const std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem);

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
	bool m_hasData = false;
};

#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"
#include "HelpContent.h"

class UIRectRenderer;
class UIInteractionSystem;
class TraitDatabase;
class ItemDatabase;

/// <summary>
/// ヘルプボタン(画面左上)と、カテゴリ別の説明パネル(画面中央)を表示するクラス(help-panel)。
///
/// 他のUI Rendererと同じ方式: IRendererを継承し、毎フレームGame::Render()からDraw()を呼んで
/// AddRenderObject()で2D描画パスへ登録、OnRender2D()で実描画する。座標系はUI_SPACE
/// (1920x1080、中央原点・y上向き)。説明文の中身はHelpContent::Build()が組み立てる。
///
/// Game側の組み込み(docs/tasks/help-panel/plan.md §5):
///  1. Update()でヒット領域を作り終えた直後(m_uiInteraction.Update()の直前)にBuildHotRegions()。
///     パネル表示中はリストをクリアしてヘルプ用の領域だけにする(=盤面等はクリック/ホバーされない)。
///  2. m_uiInteraction.Update()の直後にUpdateInput()。開閉・カテゴリ/ページ切り替えを処理する。
///  3. IsOpen()(と、UpdateInput()前の状態)を見て、タイトル/準備/終了画面の入力処理をスキップする。
///  4. Render()の最後(ツールチップより後)にDraw()。最前面に描かれる。
/// </summary>
class HelpUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// Game::Start()でデータベースの初期化後に1回呼ぶ。説明文を組み立て、パネル幅で折り返して
	/// ページに分割しておく(毎フレームの再計算はしない)。
	/// </summary>
	void Init(const TraitDatabase& traitDatabase, const ItemDatabase& itemDatabase);

	/// <summary>パネルを開いているか。</summary>
	bool IsOpen() const { return m_open; }

	/// <summary>
	/// 今フレームのヒット領域を登録する。閉じている時はヘルプボタンを末尾に追加するだけ。
	/// 開いている時はoutをクリアし、全画面のHelpBlocker+パネル内のボタン類だけを登録する。
	/// 他のRendererのBuildHotRegions()を全て呼んだ後(m_uiInteraction.Update()の直前)に呼ぶこと。
	/// </summary>
	void BuildHotRegions(UIHotRegionList& out) const;

	/// <summary>
	/// 開閉・カテゴリ/ページ切り替えの入力を処理する。m_uiInteraction.Update()の直後に毎フレーム呼ぶ。
	/// マウス(ヘルプ用ヒット領域のクリック)、キーボード(F1/H/Esc/矢印)。
	/// </summary>
	void UpdateInput(const UIInteractionSystem& uiInteraction);

	/// <summary>
	/// 毎フレームGame::Render()の最後に呼ぶ(全フェーズ)。ボタンは常に、パネルは開いている時だけ描く。
	/// </summary>
	void Draw(RenderContext& rc, UIRectRenderer& rectRenderer);

	// IRendererオーバーライド。RenderingEngine::Execute()の2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	/// <summary>折り返し・ページ分割後の1カテゴリ。</summary>
	struct CategoryPages
	{
		std::wstring title;
		std::vector<std::vector<HelpContent::Line>> pages; // 最低1ページ(空でも1ページ)。
	};

	void SetCategory(int category);
	void ChangePage(int delta);
	int PageCount() const;

	/// <summary>1論理行を本文幅に収まるよう折り返す。</summary>
	static void WrapLine(const HelpContent::Line& line, std::vector<HelpContent::Line>& out);

	/// <summary>マウスホバー中のヘルプ用領域か。</summary>
	bool IsHovered(UIRegionKind kind, int index = -1) const;

	Font m_font;
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。

	std::vector<CategoryPages> m_categories;
	bool m_open = false;
	int m_category = 0; // 選択中のカテゴリ(開き直しても維持する)。
	int m_page = 0;     // 選択中カテゴリ内のページ。

	UIHotRegion m_hovered;
	bool m_hasHovered = false;
};

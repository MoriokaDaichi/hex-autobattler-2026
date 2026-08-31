#pragma once
#include "UIHotRegion.h"

/// <summary>
/// マウスカーソル位置と、その時点のヒット領域一覧(UIHotRegionList)から、
/// 「今フレームどこがホバー/クリックされたか」だけを解決するクラス。
///
/// これ自体は「何が起きるか」(購入/装備/配置等)を一切知らない。意味付けは呼び出し側
/// (Game::Update())が、返ってきたUIHotRegion::kind/index/hexを見て行う
/// (docs/tasks/ui-mouse-cards/plan.md §1-4)。
/// </summary>
class UIInteractionSystem
{
public:
	/// <summary>
	/// 毎フレームGame::Update()の先頭、ヒット領域(UIHotRegionList)再構築の直後に呼ぶ。
	/// hotRegionsは「今フレームの」最新のレイアウトである前提(1フレーム遅延させないこと)。
	/// </summary>
	void Update(const UIHotRegionList& hotRegions);

	/// <summary>現在マウスカーソル下にある領域を取得する。無ければfalse。</summary>
	bool GetHovered(UIHotRegion& out) const;

	/// <summary>このフレーム、左クリック(トリガー)で確定した領域を取得する。無ければfalse。</summary>
	bool GetLeftClicked(UIHotRegion& out) const;

	/// <summary>このフレーム、右クリック(トリガー)で確定した領域を取得する。無ければfalse。</summary>
	bool GetRightClicked(UIHotRegion& out) const;

private:
	UIHotRegion m_hovered;
	bool m_hasHovered = false;

	UIHotRegion m_leftClicked;
	bool m_hasLeftClicked = false;

	UIHotRegion m_rightClicked;
	bool m_hasRightClicked = false;
};

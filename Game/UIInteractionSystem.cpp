#include "stdafx.h"
#include "UIInteractionSystem.h"
#include "CursorSelectionSystem.h"

void UIInteractionSystem::Update(const UIHotRegionList& hotRegions)
{
	m_hasHovered = false;
	m_hasLeftClicked = false;
	m_hasRightClicked = false;

	Vector2 uiPos;
	if (!CursorSelectionSystem::ScreenToUISpace(g_mouse->GetPositionX(), g_mouse->GetPositionY(), uiPos))
	{
		return; // マウスがウィンドウ外。
	}

	// 後ろから探索し、最初にヒットした領域を採用する(後で登録された=描画順で手前にあるものを優先。
	// フェーズ2以降でツールチップ等が重なった場合の誤爆防止にもなる)。
	for (auto it = hotRegions.rbegin(); it != hotRegions.rend(); ++it)
	{
		if (!it->Contains(uiPos))
		{
			continue;
		}

		m_hovered = *it;
		m_hasHovered = true;

		if (g_mouse->IsTrigger(enMouseButtonLeft))
		{
			m_leftClicked = *it;
			m_hasLeftClicked = true;
		}
		if (g_mouse->IsTrigger(enMouseButtonRight))
		{
			m_rightClicked = *it;
			m_hasRightClicked = true;
		}
		break;
	}
}

bool UIInteractionSystem::GetHovered(UIHotRegion& out) const
{
	if (!m_hasHovered) return false;
	out = m_hovered;
	return true;
}

bool UIInteractionSystem::GetLeftClicked(UIHotRegion& out) const
{
	if (!m_hasLeftClicked) return false;
	out = m_leftClicked;
	return true;
}

bool UIInteractionSystem::GetRightClicked(UIHotRegion& out) const
{
	if (!m_hasRightClicked) return false;
	out = m_rightClicked;
	return true;
}

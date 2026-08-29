#include "stdafx.h"
#include "PlayerStatusUIRenderer.h"
#include "Player.h"

namespace
{
	// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。画面右上に配置する
	// (左上はFPS表示・BoardUIRendererのベンチ一覧、下部はShopUIRendererのヘッダー行と競合するため)。
	const float kX = 560.0f;
	const float kGoldY = 500.0f;
	const float kLevelY = 454.0f;

	const float kGoldScale = 0.62f;
	const float kLevelScale = 0.58f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f); // FPS表示と同じ、テキスト左上を基準にする指定。

	const Vector4 kGoldColor(1.00f, 0.80f, 0.25f, 1.0f); // ショップのコストTier5(金)と同系色。
	const Vector4 kLevelColor(0.85f, 0.90f, 1.00f, 1.0f);

	const int kXPBarCells = 10;
	// BoardUIRendererのHPバーと同じ理由(フォントに矩形・ブロック罫線が収録されていない)で、
	// XPゲージもASCII記号で表現する。
	const wchar_t kFilledCell = L'#';
	const wchar_t kEmptyCell = L'-';

	std::wstring MakeXPBar(float ratio)
	{
		if (ratio < 0.0f) ratio = 0.0f;
		if (ratio > 1.0f) ratio = 1.0f;
		int filled = (int)(ratio * kXPBarCells + 0.5f);
		if (filled <= 0 && ratio > 0.0f) filled = 1; // わずかでも溜まっていれば1マスは点ける。
		if (filled > kXPBarCells) filled = kXPBarCells;

		std::wstring s;
		s.reserve(kXPBarCells + 2);
		s += L'[';
		for (int i = 0; i < kXPBarCells; ++i) s += (i < filled) ? kFilledCell : kEmptyCell;
		s += L']';
		return s;
	}
}

void PlayerStatusUIRenderer::Draw(RenderContext& rc, const Player& player, int xpForNextLevel)
{
	m_gold = player.gold;
	m_level = player.level;
	m_xp = player.xp;
	m_xpForNextLevel = xpForNextLevel;
	m_hasData = true;

	g_renderingEngine->AddRenderObject(this);
}

void PlayerStatusUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	wchar_t goldLine[32];
	swprintf_s(goldLine, L"GOLD  %d", m_gold);
	m_font.Draw(goldLine, Vector2(kX, kGoldY), kGoldColor, 0.0f, kGoldScale, kTopLeftPivot);

	wchar_t levelLine[64];
	if (m_xpForNextLevel <= 0)
	{
		// kMaxLevel到達時はLevelSystem::XPForNextLevelが0を返す(それ以上レベルアップしない)。
		swprintf_s(levelLine, L"LV %d  (MAX)", m_level);
	}
	else
	{
		float ratio = (float)m_xp / (float)m_xpForNextLevel;
		swprintf_s(levelLine, L"LV %d  %ls %d/%d", m_level, MakeXPBar(ratio).c_str(), m_xp, m_xpForNextLevel);
	}
	m_font.Draw(levelLine, Vector2(kX, kLevelY), kLevelColor, 0.0f, kLevelScale, kTopLeftPivot);

	m_font.End(rc);
}

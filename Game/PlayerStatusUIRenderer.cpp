#include "stdafx.h"
#include "PlayerStatusUIRenderer.h"
#include "Player.h"
#include "UIRectRenderer.h"

namespace
{
	// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。画面右上に配置する
	// (左上はFPS表示・BoardUIRendererのベンチ一覧、下部はShopUIRendererのヘッダー行と競合するため)。
	const float kX = 560.0f;
	const float kGoldY = 500.0f;
	const float kLevelY = 454.0f;

	// 盤面 使用/上限 は行を増やさず GOLD 行と同じ y に x をずらして併記する
	// (下段の RoundRecordUIRenderer / ItemInventoryUIRenderer と縦位置を取り合わないため)。
	const float kBoardCol = 250.0f;

	const float kGoldScale = 0.62f;
	const float kLevelScale = 0.58f;
	const float kBoardScale = 0.58f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f); // FPS表示と同じ、テキスト左上を基準にする指定。

	const Vector4 kGoldColor(1.00f, 0.80f, 0.25f, 1.0f); // ショップのコストTier5(金)と同系色。
	const Vector4 kLevelColor(0.85f, 0.90f, 1.00f, 1.0f);
	const Vector4 kBoardFullColor(1.00f, 0.55f, 0.30f, 1.0f); // 上限到達(これ以上置けない)時の警告色。

	// XPバー(塗り矩形)。"LV n "の右側に配置する。
	const Vector2 kLeftMidPivot(0.0f, 0.5f);
	const float kXPBarX = kX + 76.0f;   // "LV %d  "の文字幅の目安(実機で調整)。
	const float kXPBarY = kLevelY - 4.0f;
	const float kXPBarBgWidth = 150.0f;
	const float kXPBarBgHeight = 14.0f;
	const float kXPBarFgWidth = 142.0f; // 背景の内側(左右4pxずつ余白)。
	const float kXPBarFgHeight = 10.0f;
	const Vector4 kXPBarBgColor(0.22f, 0.22f, 0.26f, 0.9f); // BoardUIRendererのkBarBgColorと同系(暗いスレート色)。
	const Vector4 kXPBarFgColor(0.55f, 0.85f, 1.0f, 1.0f); // 水色寄り(kLevelColorと同系)。
}

void PlayerStatusUIRenderer::Draw(RenderContext& rc, const Player& player, int xpForNextLevel, UIRectRenderer& rectRenderer)
{
	m_gold = player.gold;
	m_level = player.level;
	m_xp = player.xp;
	m_xpForNextLevel = xpForNextLevel;
	m_boardCount = (int)player.board.size();
	m_maxBoardSize = player.GetMaxBoardSize();
	m_rectRenderer = &rectRenderer;
	m_hasData = true;

	g_renderingEngine->AddRenderObject(this);
}

void PlayerStatusUIRenderer::BuildHotRegions(UIHotRegionList& out) const
{
	// GOLD行(kX, kGoldY)。
	{
		UIHotRegion region;
		region.kind = UIRegionKind::GoldDisplay;
		region.minX = kX - 4.0f;
		region.maxX = kX + 150.0f;
		region.minY = kGoldY - 24.0f;
		region.maxY = kGoldY + 4.0f;
		out.push_back(region);
	}

	// BOARD n/m(kX+kBoardCol, kGoldYと同じ行)。
	{
		UIHotRegion region;
		region.kind = UIRegionKind::HudBoardCountDisplay;
		region.minX = kX + kBoardCol - 4.0f;
		region.maxX = kX + kBoardCol + 150.0f;
		region.minY = kGoldY - 24.0f;
		region.maxY = kGoldY + 4.0f;
		out.push_back(region);
	}

	// LV行 + XPバー(kX〜kXPBarX+kXPBarBgWidth、kLevelY基準)をまとめて1領域にする。
	{
		UIHotRegion region;
		region.kind = UIRegionKind::HudLevelDisplay;
		region.minX = kX - 4.0f;
		region.maxX = kXPBarX + kXPBarBgWidth + 60.0f; // XPバー右の数値テキストぶんの余裕。
		region.minY = kLevelY - 24.0f;
		region.maxY = kLevelY + 4.0f;
		out.push_back(region);
	}
}

void PlayerStatusUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	// 矩形(Sprite)はFont::Begin()〜End()の外側で先に描き終える(plan.md §0-8)。
	if (m_rectRenderer != nullptr && m_xpForNextLevel > 0)
	{
		float ratio = (float)m_xp / (float)m_xpForNextLevel;
		if (ratio < 0.0f) ratio = 0.0f;
		if (ratio > 1.0f) ratio = 1.0f;

		m_rectRenderer->DrawRect(rc, Vector2(kXPBarX, kXPBarY), Vector2(kXPBarBgWidth, kXPBarBgHeight), kXPBarBgColor, kLeftMidPivot);
		m_rectRenderer->DrawRect(rc, Vector2(kXPBarX, kXPBarY), Vector2(kXPBarFgWidth * ratio, kXPBarFgHeight), kXPBarFgColor, kLeftMidPivot);
	}

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	wchar_t goldLine[32];
	swprintf_s(goldLine, L"GOLD  %d", m_gold);
	m_font.Draw(goldLine, Vector2(kX, kGoldY), kGoldColor, 0.0f, kGoldScale, kTopLeftPivot);

	// 盤面 使用/上限。GOLD行と同じyに併記。上限到達時は警告色。
	wchar_t boardLine[32];
	swprintf_s(boardLine, L"BOARD  %d/%d", m_boardCount, m_maxBoardSize);
	const Vector4& boardColor = (m_boardCount >= m_maxBoardSize) ? kBoardFullColor : kLevelColor;
	m_font.Draw(boardLine, Vector2(kX + kBoardCol, kGoldY), boardColor, 0.0f, kBoardScale, kTopLeftPivot);

	wchar_t levelLine[32];
	swprintf_s(levelLine, L"LV %d", m_level);
	m_font.Draw(levelLine, Vector2(kX, kLevelY), kLevelColor, 0.0f, kLevelScale, kTopLeftPivot);

	// XPバー(矩形)は上で描画済み。バーの右側に数値のみ添える。
	wchar_t xpLine[32];
	if (m_xpForNextLevel <= 0)
	{
		// kMaxLevel到達時はLevelSystem::XPForNextLevelが0を返す(それ以上レベルアップしない)。
		swprintf_s(xpLine, L"(MAX)");
	}
	else
	{
		swprintf_s(xpLine, L"%d/%d", m_xp, m_xpForNextLevel);
	}
	m_font.Draw(xpLine, Vector2(kXPBarX + kXPBarBgWidth + 10.0f, kLevelY), kLevelColor, 0.0f, kLevelScale, kTopLeftPivot);

	m_font.End(rc);
}

#include "stdafx.h"
#include "TraitPanelUIRenderer.h"
#include "UnitInstance.h"
#include "TraitDatabase.h"
#include "TraitSystem.h"
#include "UIRectRenderer.h"
#include "UIStyle.h"
#include "BoardUIRenderer.h"

namespace
{
	const Vector2 kTopLeftPivot(0.0f, 1.0f); // 実質的な左上アンカー(他のUIRenderer群と同じ扱い)。
	const Vector2 kCenterPivot(0.5f, 0.5f);
	const Vector2 kLeftMidPivot(0.0f, 0.5f);

	// BoardUIRendererのBENCH一覧と同じ左端xを使う。開始YはBENCHパネルの固定下端(表示行数を
	// kBenchMaxVisibleRowsでクランプ済みなので予測可能)からさらに余白を空けた位置にする
	// (ui-mouse-cardsフェーズ3、plan.md §4-3。以前は固定値-70でベンチ7〜8件で衝突していた)。
	const float kX = BoardUIRenderer::kBenchX;
	const float kTopY = BoardUIRenderer::kBenchPanelBottomY - 40.0f;
	const float kStepY = 24.0f;

	const float kTitleScale = 0.5f;
	const float kRowScale = 0.42f;

	const Vector4 kTitleColor(0.9f, 0.9f, 0.95f, 1.0f);
	const Vector4 kActiveColor(0.95f, 0.95f, 0.6f, 1.0f);   // 発動中: 明るい黄み寄りの白(強調)。
	const Vector4 kInactiveColor(0.5f, 0.5f, 0.55f, 1.0f);  // 未発動: ディムグレー。

	const wchar_t* kTitleText = L"TRAITS";
	const wchar_t* kActiveMarker = L"> ";   // ShopUIRendererのカーソル記号と同じ"強調"の意味で流用。
	const wchar_t* kInactiveMarker = L"  ";

	const float kPanelWidth = 240.0f;
	const float kDividerHeight = 1.0f;
}

void TraitPanelUIRenderer::Draw(RenderContext& rc, const std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem, UIRectRenderer& rectRenderer)
{
	m_rectRenderer = &rectRenderer;

	std::map<TraitType, int> traitCounts = traitSystem.CountBoardTraits(board);

	// 要求仕様通り「発動中は上部、未発動は下部」にまとめるため、いったん2グループに分けてから
	// 連結する。各グループ内はTraitDatabaseの登録順(出自3種→役割5種)を維持する。
	std::vector<TraitRow> activeRows;
	std::vector<TraitRow> inactiveRows;

	for (const auto& traitDef : traitDatabase.GetAllTraitDefs())
	{
		int count = 0;
		auto it = traitCounts.find(traitDef.type);
		if (it != traitCounts.end())
		{
			count = it->second;
		}

		const TraitTier* activeTier = traitSystem.FindActiveTier(traitDef, count);
		const TraitTier* nextTier = traitSystem.FindNextTier(traitDef, count);
		const wchar_t* marker = (activeTier != nullptr) ? kActiveMarker : kInactiveMarker;

		wchar_t buf[64];
		if (nextTier != nullptr)
		{
			// 例: "> Warrior  3/4"(発動中、次の閾値まで表示)。
			swprintf_s(buf, L"%ls%hs  %d/%d", marker, traitDef.name.c_str(), count, nextTier->requiredCount);
		}
		else
		{
			// 最終段階まで到達済み(これ以上の閾値が無い)。
			swprintf_s(buf, L"%ls%hs  %d MAX", marker, traitDef.name.c_str(), count);
		}

		TraitRow row;
		row.text = buf;
		row.active = (activeTier != nullptr);

		if (row.active)
		{
			activeRows.push_back(std::move(row));
		}
		else
		{
			inactiveRows.push_back(std::move(row));
		}
	}

	m_rows.clear();
	m_rows.reserve(activeRows.size() + inactiveRows.size());
	for (auto& row : activeRows) m_rows.push_back(std::move(row));
	for (auto& row : inactiveRows) m_rows.push_back(std::move(row));

	m_hasData = true;
	g_renderingEngine->AddRenderObject(this);
}

void TraitPanelUIRenderer::BuildHotRegions(const std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem, UIHotRegionList& out) const
{
	// Draw()と同じ「発動中を先頭にまとめる」並べ替えを行毎に再現する(TraitDatabaseの登録順を
	// 維持しつつ2グループに分ける点も含め、Draw()のロジックと一致させること)。
	std::map<TraitType, int> traitCounts = traitSystem.CountBoardTraits(board);

	std::vector<TraitType> activeTypes;
	std::vector<TraitType> inactiveTypes;
	for (const auto& traitDef : traitDatabase.GetAllTraitDefs())
	{
		int count = 0;
		auto it = traitCounts.find(traitDef.type);
		if (it != traitCounts.end())
		{
			count = it->second;
		}

		const TraitTier* activeTier = traitSystem.FindActiveTier(traitDef, count);
		if (activeTier != nullptr)
		{
			activeTypes.push_back(traitDef.type);
		}
		else
		{
			inactiveTypes.push_back(traitDef.type);
		}
	}

	std::vector<TraitType> orderedTypes;
	orderedTypes.reserve(activeTypes.size() + inactiveTypes.size());
	for (TraitType t : activeTypes) orderedTypes.push_back(t);
	for (TraitType t : inactiveTypes) orderedTypes.push_back(t);

	for (size_t i = 0; i < orderedTypes.size(); ++i)
	{
		float y = kTopY - kStepY * (float)(i + 1);

		UIHotRegion region;
		region.kind = UIRegionKind::TraitRow;
		region.index = (int)orderedTypes[i];
		region.minX = kX - 4.0f;
		region.maxX = kX + 220.0f; // 想定最大幅(実機で要微調整)。
		region.maxY = y + 4.0f;
		region.minY = y - kStepY + 6.0f;
		out.push_back(region);
	}
}

void TraitPanelUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	// パネル全体を1枚の背景で囲み、行ごとに薄い区切り線を敷く(ui-mouse-cardsフェーズ3、plan.md §4-2)。
	// Sprite矩形はFont::Begin()〜End()の外側でまとめて描く(§0-8)。
	if (m_rectRenderer != nullptr)
	{
		float panelHeight = kStepY * (float)(m_rows.size() + 1) + 12.0f; // タイトル行込み+余白。
		Vector2 panelCenter(kX + kPanelWidth * 0.5f - 6.0f, kTopY + 10.0f - panelHeight * 0.5f);
		m_rectRenderer->DrawPanel(rc, panelCenter, Vector2(kPanelWidth, panelHeight),
			UIStyle::kPanelFillColor, UIStyle::kPanelBorderColor, UIStyle::kPanelBorderThickness, kCenterPivot);

		for (size_t i = 0; i + 1 < m_rows.size(); ++i)
		{
			// 各行の下端に薄い区切り線(タイトル行の下は境目が分かりやすいよう区切らない)。
			float y = kTopY - kStepY * (float)(i + 1);
			Vector2 dividerPos(kX - 4.0f, y - kStepY + 4.0f);
			m_rectRenderer->DrawRect(rc, dividerPos, Vector2(kPanelWidth - 8.0f, kDividerHeight), UIStyle::kDividerColor, kLeftMidPivot);
		}
	}

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	m_font.Draw(kTitleText, Vector2(kX, kTopY), kTitleColor, 0.0f, kTitleScale, kTopLeftPivot);

	for (size_t i = 0; i < m_rows.size(); ++i)
	{
		float y = kTopY - kStepY * (float)(i + 1);
		const Vector4& color = m_rows[i].active ? kActiveColor : kInactiveColor;
		m_font.Draw(m_rows[i].text.c_str(), Vector2(kX, y), color, 0.0f, kRowScale, kTopLeftPivot);
	}

	m_font.End(rc);
}

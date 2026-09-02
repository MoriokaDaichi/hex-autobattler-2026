#include "stdafx.h"
#include "ItemInventoryUIRenderer.h"
#include "Player.h"
#include "ItemDef.h"
#include "UITextUtil.h"
#include "UIRectRenderer.h"
#include "UIStyle.h"

namespace
{
	// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。画面右側に配置する。
	// 右上は PlayerStatusUIRenderer(GOLD/LV、y≒454〜500)と RoundRecordUIRenderer
	// (ROUND/連敗、y≒312〜388)が占有しているため、その下(y≒+270以下)から下へ積む。
	// 画面下部の ShopUIRenderer(y≒-330〜-446)とは、想定最大アイテム数でも縦に重ならない。
	//
	// RoundRecordUIRenderer と同じく、このFontEngineには文字列幅の取得手段が無く pivot による
	// 右詰め・中央揃えは効かないため、実質左詰め(kTopLeftPivot)として開始X座標を手で調整する。
	const float kX = (float)UI_SPACE_WIDTH * 0.27f; // RoundRecordUIRenderer と同じ左端x。
	const float kTopY = (float)UI_SPACE_HEIGHT * 0.25f;
	const float kStepY = 40.0f;

	const float kTitleScale = 0.56f;
	const float kItemScale = 0.48f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f); // 実質的な左上アンカー(他のHUDと同じ扱い)。
	const Vector2 kCenterPivot(0.5f, 0.5f);

	const Vector4 kTitleColor(0.9f, 0.9f, 0.95f, 1.0f);
	const Vector4 kNormalColor(0.80f, 0.83f, 0.88f, 1.0f);
	const Vector4 kSelectedColor(1.0f, 0.92f, 0.55f, 1.0f);  // カーソルが当たっている枠。
	const Vector4 kHeldColor(0.55f, 1.0f, 0.65f, 1.0f);      // 手に持っている枠。
	const Vector4 kEmptyColor(0.6f, 0.62f, 0.66f, 1.0f);

	std::wstring EffectsText(const ItemDef* def)
	{
		std::wstring s;
		for (size_t i = 0; i < def->effects.size(); ++i)
		{
			if (i > 0) s += L" ";
			s += UITextUtil::EffectShortText(def->effects[i]);
		}
		// パッシブ効果を持つアイテムは、末尾に短いタグを付ける(現状はオンヒット火傷のみ)。
		for (const PassiveEffect& p : def->passives)
		{
			if (p.type == PassiveEffectType::OnHitBurn)
			{
				if (!s.empty()) s += L" ";
				s += L"火傷";
				break;
			}
		}
		return s;
	}
}

void ItemInventoryUIRenderer::Draw(RenderContext& rc, const Player& player, bool focused, int cursorIndex, int heldIndex, int hoveredIndex, UIRectRenderer& rectRenderer)
{
	m_items.clear();
	m_items.reserve(player.unclaimedItems.size());
	for (const ItemDef* def : player.unclaimedItems)
	{
		if (def == nullptr) continue;

		ItemView view;
		wchar_t nameBuf[64];
		swprintf_s(nameBuf, L"%hs", def->name.c_str());
		view.name = nameBuf;
		view.effects = EffectsText(def);
		m_items.push_back(std::move(view));
	}

	m_focused = focused;
	m_cursorIndex = cursorIndex;
	m_heldIndex = heldIndex;
	m_hoveredIndex = hoveredIndex;
	m_rectRenderer = &rectRenderer;
	m_hasData = true;

	g_renderingEngine->AddRenderObject(this);
}

void ItemInventoryUIRenderer::BuildHotRegions(const Player& player, UIHotRegionList& out) const
{
	// アイテム一覧の各行(kX起点、kStepY間隔で下へ伸びる。Draw()/OnRender2D()と同じ定数)。
	for (size_t i = 0; i < player.unclaimedItems.size(); ++i)
	{
		float y = kTopY - kStepY * (float)(i + 1);

		UIHotRegion region;
		region.kind = UIRegionKind::UnclaimedItem;
		region.index = (int)i;
		region.minX = kX - 4.0f;
		region.maxX = kX + 260.0f; // 想定最大幅(実機で要微調整)。
		region.maxY = y + 4.0f;
		region.minY = y - kStepY + 8.0f;
		out.push_back(region);
	}
}

void ItemInventoryUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	// アイテム一覧をカードリスト化する(ui-mouse-cardsフェーズ3、plan.md §4-2。ShopUIRendererの
	// 5枠と同じ縦版パターン)。Sprite矩形はFont::Begin()〜End()の外側でまとめて描く(§0-8)。
	if (m_rectRenderer != nullptr && !m_items.empty())
	{
		for (size_t i = 0; i < m_items.size(); ++i)
		{
			float y = kTopY - kStepY * (float)(i + 1);
			Vector2 cardCenter(kX + 125.0f, y - kStepY * 0.5f + 6.0f);
			Vector2 cardSize(258.0f, kStepY - 4.0f);

			bool held = ((int)i == m_heldIndex);
			bool selected = m_focused && ((int)i == m_cursorIndex);
			bool hovered = ((int)i == m_hoveredIndex);
			Vector4 borderColor = held ? kHeldColor
				: selected ? UIStyle::kSelectedBorderColor
				: hovered ? UIStyle::kHoveredBorderColor : UIStyle::kPanelBorderColor;
			float borderThickness = (held || selected) ? UIStyle::kSelectedBorderThickness : UIStyle::kPanelBorderThickness;

			m_rectRenderer->DrawPanel(rc, cardCenter, cardSize, UIStyle::kPanelFillColor, borderColor, borderThickness, kCenterPivot);
		}
	}

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	wchar_t title[64];
	swprintf_s(title, L"ITEMS (%d)%ls", (int)m_items.size(), m_focused ? L"  [Tab]" : L"");
	m_font.Draw(title, Vector2(kX, kTopY), kTitleColor, 0.0f, kTitleScale, kTopLeftPivot);

	if (m_items.empty())
	{
		m_font.Draw(L"  (なし)", Vector2(kX, kTopY - kStepY), kEmptyColor, 0.0f, kItemScale, kTopLeftPivot);
		m_font.End(rc);
		return;
	}

	for (size_t i = 0; i < m_items.size(); ++i)
	{
		float y = kTopY - kStepY * (float)(i + 1);

		bool held = ((int)i == m_heldIndex);
		bool selected = m_focused && ((int)i == m_cursorIndex);

		Vector4 color = kNormalColor;
		if (held) color = kHeldColor;
		else if (selected) color = kSelectedColor;

		const wchar_t* marker = held ? L"[持] " : (selected ? L"> " : L"  ");

		wchar_t line[128];
		swprintf_s(line, L"%ls%ls  %ls", marker, m_items[i].name.c_str(), m_items[i].effects.c_str());
		m_font.Draw(line, Vector2(kX, y), color, 0.0f,
			selected ? kItemScale * 1.08f : kItemScale, kTopLeftPivot);
	}

	// 操作ガイド(手に持っているときだけ表示)。
	if (m_heldIndex >= 0)
	{
		float y = kTopY - kStepY * (float)(m_items.size() + 1);
		m_font.Draw(L"  ベンチ/盤面のユニットを選び [A] で装備", Vector2(kX, y),
			Vector4(0.75f, 0.85f, 1.0f, 1.0f), 0.0f, kItemScale * 0.92f, kTopLeftPivot);
	}

	m_font.End(rc);
}

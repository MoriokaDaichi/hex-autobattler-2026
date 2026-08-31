#include "stdafx.h"
#include "ItemInventoryUIRenderer.h"
#include "Player.h"
#include "ItemDef.h"

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

	const Vector4 kTitleColor(0.9f, 0.9f, 0.95f, 1.0f);
	const Vector4 kNormalColor(0.80f, 0.83f, 0.88f, 1.0f);
	const Vector4 kSelectedColor(1.0f, 0.92f, 0.55f, 1.0f);  // カーソルが当たっている枠。
	const Vector4 kHeldColor(0.55f, 1.0f, 0.65f, 1.0f);      // 手に持っている枠。
	const Vector4 kEmptyColor(0.6f, 0.62f, 0.66f, 1.0f);

	/// <summary>StatEffect 1つを "AT+10" / "HP+20%" のような短いテキストにする。</summary>
	std::wstring EffectShortText(const StatEffect& e)
	{
		const wchar_t* label = L"?";
		bool percent = false;
		switch (e.stat)
		{
		case StatEffectType::AttackFlat:             label = L"AT"; break;
		case StatEffectType::AttackPercent:          label = L"AT"; percent = true; break;
		case StatEffectType::MagicPowerFlat:         label = L"AP"; break;
		case StatEffectType::MagicPowerPercent:      label = L"AP"; percent = true; break;
		case StatEffectType::MaxHPFlat:              label = L"HP"; break;
		case StatEffectType::MaxHPPercent:           label = L"HP"; percent = true; break;
		case StatEffectType::PhysicalDefenseFlat:    label = L"物防"; break;
		case StatEffectType::PhysicalDefensePercent: label = L"物防"; percent = true; break;
		case StatEffectType::MagicDefenseFlat:       label = L"魔防"; break;
		case StatEffectType::MagicDefensePercent:    label = L"魔防"; percent = true; break;
		case StatEffectType::SkillThresholdFlat:     label = L"技"; break;
		case StatEffectType::AttackSpeedFlat:        label = L"AS"; break;
		case StatEffectType::AttackSpeedPercent:     label = L"AS"; percent = true; break;
		}

		wchar_t buf[32];
		swprintf_s(buf, L"%ls+%d%ls", label, (int)e.value, percent ? L"%" : L"");
		return buf;
	}

	std::wstring EffectsText(const ItemDef* def)
	{
		std::wstring s;
		for (size_t i = 0; i < def->effects.size(); ++i)
		{
			if (i > 0) s += L" ";
			s += EffectShortText(def->effects[i]);
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

void ItemInventoryUIRenderer::Draw(RenderContext& rc, const Player& player, bool focused, int cursorIndex, int heldIndex)
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
	m_hasData = true;

	g_renderingEngine->AddRenderObject(this);
}

void ItemInventoryUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

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

#include "stdafx.h"
#include "ShopUIRenderer.h"
#include "Player.h"

namespace
{
	// --- 座標系 ---
	// 2D描画パス(OnRender2D)のFont::Draw()はUI空間(1920x1080、中央原点・y上向き)で指定する。
	// engine標準のFPS表示(y≒+518)と同じ空間。画面下部に置くため y は大きめのマイナス(下)へ。
	const float kLeftX = -880.0f;       // ヘッダー・フィードバック・1枠目の左端x。

	const float kFeedbackY = -330.0f;   // 操作フィードバック行(画面高の約80%)。
	const float kHeaderY = -368.0f;     // 所持ゴールド・レベル・操作ガイドの行。
	const float kNameY = -410.0f;       // カード1行目: ユニット名。
	const float kDetailY = -446.0f;     // カード2行目: コスト・ステータス・トレイト(下端 ≒ 画面高93%)。

	const float kSlotStartX = -880.0f;
	const float kSlotStepX = 360.0f;    // 5枠 → x = -880,-520,-160,200,560。

	const float kNameScale = 0.72f;
	const float kDetailScale = 0.50f;
	const float kHeaderScale = 0.60f;
	const float kFeedbackScale = 0.72f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f); // FPS表示と同じ、テキスト左上を基準にする指定。

	const float kFeedbackDuration = 3.0f;    // フィードバックがはっきり表示される秒数(経過後は薄く残す)。

	/// <summary>
	/// コストティアごとの色(TFTのグレー/緑/青/紫/金に寄せる)。
	/// </summary>
	Vector4 CostTierColor(int cost)
	{
		switch (cost)
		{
		case 1:  return Vector4(0.72f, 0.72f, 0.72f, 1.0f);
		case 2:  return Vector4(0.40f, 0.90f, 0.55f, 1.0f);
		case 3:  return Vector4(0.40f, 0.70f, 1.00f, 1.0f);
		case 4:  return Vector4(0.85f, 0.50f, 1.00f, 1.0f);
		case 5:  return Vector4(1.00f, 0.80f, 0.25f, 1.0f);
		default: return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	const wchar_t* TraitName(TraitType type)
	{
		switch (type)
		{
		case TraitType::Monster:  return L"魔物";
		case TraitType::Human:    return L"人間";
		case TraitType::Hero:     return L"英雄";
		case TraitType::Warrior:  return L"戦士";
		case TraitType::Mage:     return L"魔道士";
		case TraitType::Guardian: return L"守護者";
		case TraitType::Assassin: return L"暗殺者";
		case TraitType::Ranger:   return L"狩人";
		default:                  return L"?";
		}
	}

	Vector4 FeedbackColor(ShopUIRenderer::FeedbackLevel level)
	{
		switch (level)
		{
		case ShopUIRenderer::FeedbackLevel::Success: return Vector4(0.50f, 1.00f, 0.60f, 1.0f);
		case ShopUIRenderer::FeedbackLevel::Failure: return Vector4(1.00f, 0.45f, 0.40f, 1.0f);
		default:                                     return Vector4(0.95f, 0.95f, 0.95f, 1.0f);
		}
	}
}

void ShopUIRenderer::Draw(
	RenderContext& rc,
	const std::vector<const UnitDef*>& shop,
	const Player& player,
	int xpForNextLevel,
	int rerollCost,
	int buyXpCost,
	int shopCursorIndex,
	bool shopFocused,
	bool shopLocked)
{
	m_slots.clear();
	m_slots.reserve(shop.size());
	for (const UnitDef* def : shop)
	{
		if (def == nullptr) continue;

		SlotView view;
		wchar_t nameBuf[64];
		swprintf_s(nameBuf, L"%hs", def->name.c_str());
		view.name = nameBuf;
		view.cost = def->cost;
		view.baseHP = def->baseHP;
		view.baseAttack = def->baseAttack;

		// トレイトは横幅の都合で最大2個まで表示し、3個以上持つユニットは末尾に "+" を付ける。
		const size_t kMaxShownTraits = 2;
		for (size_t t = 0; t < def->traits.size() && t < kMaxShownTraits; ++t)
		{
			if (t > 0) view.traits += L"/";
			view.traits += TraitName(def->traits[t]);
		}
		if (def->traits.size() > kMaxShownTraits) view.traits += L"+";

		m_slots.push_back(std::move(view));
	}

	m_gold = player.gold;
	m_level = player.level;
	m_xp = player.xp;
	m_xpForNextLevel = xpForNextLevel;
	m_rerollCost = rerollCost;
	m_buyXpCost = buyXpCost;
	m_cursorIndex = shopCursorIndex;
	m_shopFocused = shopFocused;
	m_shopLocked = shopLocked;
	m_hasData = !m_slots.empty();

	if (!m_hasData)
	{
		return; // 表示すべきショップが無い。2D描画パスにも登録しない。
	}

	g_renderingEngine->AddRenderObject(this);
}

void ShopUIRenderer::PushFeedback(const wchar_t* message, FeedbackLevel level)
{
	if (message == nullptr) return;
	m_feedbackText = message;
	m_feedbackLevel = level;
	m_feedbackTimer = kFeedbackDuration;
}

void ShopUIRenderer::UpdateFeedbackTimer(float deltaTime)
{
	if (m_feedbackTimer > 0.0f)
	{
		m_feedbackTimer -= deltaTime;
		if (m_feedbackTimer < 0.0f) m_feedbackTimer = 0.0f;
	}
}

void ShopUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	// 3Dシーンの上に重なっても読めるよう、影付きで描画する。
	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	// --- 操作フィードバック行 ---
	// 直近の操作結果は薄く残し続け、新しい操作から数秒間だけはっきり見せる
	// (一瞬で消えると操作したのか分かりづらいため、常時1行残す方式にしている)。
	if (!m_feedbackText.empty())
	{
		Vector4 color = FeedbackColor(m_feedbackLevel);
		color.w = (m_feedbackTimer > 0.0f) ? 1.0f : 0.4f;
		m_font.Draw(m_feedbackText.c_str(), Vector2(kLeftX, kFeedbackY),
			color, 0.0f, kFeedbackScale, kTopLeftPivot);
	}

	// --- ヘッダー行(所持ゴールド / レベル・XP / 操作ガイド) ---
	{
		wchar_t buf[224];
		swprintf_s(buf,
			L"SHOP%ls   Gold %d   Lv %d (XP %d/%d)   [Y] Reroll -%dG   [RB1] BuyXP -%dG   [Start] Lock",
			m_shopLocked ? L" [LOCKED]" : L"",
			m_gold, m_level, m_xp, m_xpForNextLevel, m_rerollCost, m_buyXpCost);

		// ロック中はヘッダーを金色にして状態が一目で分かるようにする。
		Vector4 headerColor = m_shopLocked
			? Vector4(1.00f, 0.80f, 0.25f, 1.0f)
			: Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		m_font.Draw(buf, Vector2(kLeftX, kHeaderY), headerColor, 0.0f, kHeaderScale, kTopLeftPivot);
	}

	// --- 5枠のカード(名前行 + 詳細行の2行) ---
	for (size_t i = 0; i < m_slots.size(); ++i)
	{
		const SlotView& slot = m_slots[i];
		float slotX = kSlotStartX + kSlotStepX * (float)i;

		bool selected = m_shopFocused && ((int)i == m_cursorIndex);

		Vector4 nameColor = CostTierColor(slot.cost);
		if (!selected)
		{
			// 選択されていない枠は少し暗く落とす。
			nameColor.x *= 0.78f;
			nameColor.y *= 0.78f;
			nameColor.z *= 0.78f;
		}

		wchar_t nameLine[80];
		swprintf_s(nameLine, L"%ls%ls", selected ? L"> " : L"  ", slot.name.c_str());
		m_font.Draw(nameLine, Vector2(slotX, kNameY), nameColor, 0.0f,
			selected ? kNameScale * 1.08f : kNameScale, kTopLeftPivot);

		wchar_t detailLine[128];
		swprintf_s(detailLine, L"  C%d  HP%d AT%d  %ls",
			slot.cost, slot.baseHP, slot.baseAttack, slot.traits.c_str());
		m_font.Draw(detailLine, Vector2(slotX, kDetailY), Vector4(0.82f, 0.85f, 0.90f, 1.0f), 0.0f,
			kDetailScale, kTopLeftPivot);
	}

	m_font.End(rc);
}

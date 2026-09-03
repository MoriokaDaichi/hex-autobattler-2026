#include "stdafx.h"
#include "ShopUIRenderer.h"
#include "Player.h"
#include "UIRectRenderer.h"
#include "UITextUtil.h"
#include "UIStyle.h"

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
	const Vector2 kCenterPivot(0.5f, 0.5f);

	const float kFeedbackDuration = 3.0f;    // フィードバックがはっきり表示される秒数(経過後は薄く残す)。

	// --- マウス用の常設ボタン(Reroll/BuyXP/Lock/次の戦闘へ) ---
	// 4ボタンの合計幅(centerが -240,-80,80,240 → 外縁 ±310)がUI空間の x=0 を中心に対称になるよう
	// 配置し、Yは画面下部の混雑(ショップカード kNameY≒-410 / SHOPヘッダー行 kHeaderY≒-368 /
	// フィードバック行 kFeedbackY≒-330 / ツールチップ帯)を避けて一段上へ置く
	// (ui-mouse-cardsフェーズ3フォローアップ: F5フィードバック是正2。以前は左下寄りでツールチップや
	// ショップカードと干渉していた)。矢印記号はSpriteFont未収録でabortするため使わない(ASCIIのみ)。
	const float kButtonY = -300.0f;
	const float kButtonStartX = -240.0f;
	const float kButtonStepX = 160.0f;
	const float kButtonWidth = 140.0f;
	const float kButtonHeight = 28.0f;
	const float kButtonLabelScale = 0.5f;

	const Vector4 kButtonColor(0.28f, 0.28f, 0.34f, 0.92f);
	const Vector4 kButtonLockedColor(0.55f, 0.44f, 0.10f, 0.92f); // ロック中は琥珀寄りにして状態を示す。
	const Vector4 kButtonLabelColor(0.92f, 0.92f, 0.95f, 1.0f);

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
	bool shopLocked,
	int hoveredIndex,
	UIRectRenderer& rectRenderer)
{
	m_rectRenderer = &rectRenderer;
	m_hoveredIndex = hoveredIndex;

	m_slots.clear();
	m_slots.reserve(shop.size());
	for (const UnitDef* def : shop)
	{
		if (def == nullptr)
		{
			// 購入済みで空になった枠。位置を詰めず(indexがショップ枠番号と一致するようにする)、
			// カード背景だけ描く空スロットとして積む(ui-mouse-cardsフェーズ3フォローアップ: 是正4)。
			SlotView emptyView;
			emptyView.empty = true;
			m_slots.push_back(std::move(emptyView));
			continue;
		}

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
			view.traits += UITextUtil::TraitName(def->traits[t]);
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

	// --- マウス用の常設ボタン + 5枠のカード背景。Font::Begin()より前にまとめて描く
	// (Sprite矩形はSpriteBatchの状態と競合するため。docs/tasks/ui-sprite-bars/plan.md §0-8)。
	if (m_rectRenderer != nullptr)
	{
		auto drawButton = [&](int slotIndex, const Vector4& fillColor)
		{
			Vector2 pos(kButtonStartX + kButtonStepX * (float)slotIndex, kButtonY);
			m_rectRenderer->DrawPanel(rc, pos, Vector2(kButtonWidth, kButtonHeight),
				fillColor, UIStyle::kPanelBorderColor, UIStyle::kPanelBorderThickness, kCenterPivot);
		};

		drawButton(0, kButtonColor);                                    // Reroll
		drawButton(1, kButtonColor);                                    // BuyXP
		drawButton(2, m_shopLocked ? kButtonLockedColor : kButtonColor); // Lock
		drawButton(3, kButtonColor);                                    // NextPhase

		// ショップ5枠のカード(ui-mouse-cardsフェーズ3、plan.md §4-2)。枠色はコストティア色、
		// 選択中(キーボード/パッド)は太く金色、ホバー中(マウス)は水色でハイライトする。
		for (size_t i = 0; i < m_slots.size(); ++i)
		{
			float slotX = kSlotStartX + kSlotStepX * (float)i;
			Vector2 cardCenter(slotX + kSlotStepX * 0.5f - 20.0f, (kNameY + kDetailY) * 0.5f - 12.0f);
			Vector2 cardSize(kSlotStepX - 30.0f, (kNameY - kDetailY) + 40.0f);

			if (m_slots[i].empty)
			{
				// 空き枠: カード背景のみ(中立の枠色、選択/ホバーの強調もしない)。
				m_rectRenderer->DrawPanel(rc, cardCenter, cardSize, UIStyle::kPanelFillColor,
					UIStyle::kPanelBorderColor, UIStyle::kPanelBorderThickness, kCenterPivot);
				continue;
			}

			bool selected = m_shopFocused && ((int)i == m_cursorIndex);
			bool hovered = ((int)i == m_hoveredIndex);
			Vector4 borderColor = selected ? UIStyle::kSelectedBorderColor
				: hovered ? UIStyle::kHoveredBorderColor : CostTierColor(m_slots[i].cost);
			float borderThickness = selected ? UIStyle::kSelectedBorderThickness : UIStyle::kPanelBorderThickness;

			m_rectRenderer->DrawPanel(rc, cardCenter, cardSize, UIStyle::kPanelFillColor, borderColor, borderThickness, kCenterPivot);
		}
	}

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

	// --- ヘッダー行(所持ゴールド / レベル・XP) ---
	// 従来の"[Y] Reroll -2G [RB1] BuyXP -4G [Start] Lock"ヒットは、実際にクリックできる
	// ボタン(下記)へ置き換えたため、テキストからは外す(情報の二重管理を避ける)。
	{
		wchar_t buf[128];
		swprintf_s(buf, L"SHOP%ls   Gold %d   Lv %d (XP %d/%d)",
			m_shopLocked ? L" [LOCKED]" : L"",
			m_gold, m_level, m_xp, m_xpForNextLevel);

		// ロック中はヘッダーを金色にして状態が一目で分かるようにする。
		Vector4 headerColor = m_shopLocked
			? Vector4(1.00f, 0.80f, 0.25f, 1.0f)
			: Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		m_font.Draw(buf, Vector2(kLeftX, kHeaderY), headerColor, 0.0f, kHeaderScale, kTopLeftPivot);
	}

	// --- ボタンのラベル(矩形は上でFont::Begin()より前に描画済み) ---
	{
		auto drawLabel = [&](int slotIndex, const wchar_t* text)
		{
			// ボタン矩形の左端付近から書き出す(MeasureString相当が無いため中央揃えはしない)。
			float x = kButtonStartX + kButtonStepX * (float)slotIndex - kButtonWidth * 0.5f + 6.0f;
			m_font.Draw(text, Vector2(x, kButtonY), kButtonLabelColor, 0.0f, kButtonLabelScale, Vector2(0.0f, 0.5f));
		};

		wchar_t rerollLabel[32];
		swprintf_s(rerollLabel, L"Reroll -%dG", m_rerollCost);
		drawLabel(0, rerollLabel);

		wchar_t buyXpLabel[32];
		swprintf_s(buyXpLabel, L"BuyXP -%dG", m_buyXpCost);
		drawLabel(1, buyXpLabel);

		drawLabel(2, m_shopLocked ? L"Locked" : L"Lock");
		drawLabel(3, L"Next Round");
	}

	// --- 5枠のカード(名前行 + 詳細行の2行) ---
	for (size_t i = 0; i < m_slots.size(); ++i)
	{
		const SlotView& slot = m_slots[i];
		if (slot.empty) continue; // 空き枠は文字を描かない(カード背景は上で描画済み)。

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

void ShopUIRenderer::BuildHotRegions(const std::vector<const UnitDef*>& shop, UIHotRegionList& out) const
{
	// 5枠のショップカード。名前行(kNameY)〜詳細行(kDetailY)を覆う矩形(実機で要微調整。
	// docs/tasks/ui-mouse-cards/plan.md §6-1参照)。
	for (size_t i = 0; i < shop.size() && i < 5; ++i)
	{
		if (shop[i] == nullptr) continue;

		float slotX = kSlotStartX + kSlotStepX * (float)i;

		UIHotRegion region;
		region.kind = UIRegionKind::ShopSlot;
		region.index = (int)i;
		region.minX = slotX - 10.0f;
		region.maxX = slotX + kSlotStepX - 20.0f;
		region.maxY = kNameY + 4.0f;
		region.minY = kDetailY - 28.0f;
		out.push_back(region);
	}

	// 常設ボタン(Reroll/BuyXP/Lock/NextPhase)。shopが空でも押せるようにする(ロック解除等)。
	auto addButton = [&](int slotIndex, UIRegionKind kind)
	{
		float cx = kButtonStartX + kButtonStepX * (float)slotIndex;
		UIHotRegion region;
		region.kind = kind;
		region.minX = cx - kButtonWidth * 0.5f;
		region.maxX = cx + kButtonWidth * 0.5f;
		region.minY = kButtonY - kButtonHeight * 0.5f;
		region.maxY = kButtonY + kButtonHeight * 0.5f;
		out.push_back(region);
	};

	addButton(0, UIRegionKind::RerollButton);
	addButton(1, UIRegionKind::BuyXpButton);
	addButton(2, UIRegionKind::LockButton);
	addButton(3, UIRegionKind::NextPhaseButton);
}

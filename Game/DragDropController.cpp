#include "stdafx.h"
#include "DragDropController.h"
#include "Player.h"
#include "ItemSystem.h"
#include "ItemDatabase.h"
#include "ShopUIRenderer.h"
#include "BoardUIRenderer.h"
#include "CursorSelectionSystem.h"
#include "UIRectRenderer.h"
#include "UITextUtil.h"
#include "UIStyle.h"

namespace
{
	// --- 売却領域(ショップバー) ---
	// ShopUIRenderer.cpp の無名名前空間のレイアウト定数(kSlotStartX=-880 / kSlotStepX=360 / kNameY=-410 /
	// kDetailY=-446)から求めた5枚のカード全体(描画上 x≒-885〜885, y≒-478〜-402)を少し広げた矩形。
	// ShopUIRendererのレイアウトを変えたらここも合わせること(並行作業との衝突回避のため定数を再掲している)。
	const float kSellAreaMinX = -895.0f;
	const float kSellAreaMaxX = 895.0f;
	const float kSellAreaMinY = -488.0f;
	const float kSellAreaMaxY = -396.0f;

	// --- 色 ---
	const Vector4 kCandidateFill(0.35f, 0.75f, 1.0f, 0.16f);   // ドロップ候補(水色・薄く)。
	const Vector4 kCandidateBorder(0.55f, 0.85f, 1.0f, 0.55f);
	const Vector4 kTargetFill(1.0f, 0.85f, 0.35f, 0.30f);      // 今カーソル下の有効なドロップ先(金色)。
	const Vector4 kSellFill(0.85f, 0.20f, 0.20f, 0.20f);       // 売却領域(赤系)。
	const Vector4 kSellBorder(1.0f, 0.40f, 0.35f, 0.70f);
	const Vector4 kSellTargetFill(0.95f, 0.25f, 0.25f, 0.45f);
	const Vector4 kGhostFill(0.10f, 0.10f, 0.14f, 0.92f);      // カーソルに追従する名札。
	const Vector4 kLabelColor(1.0f, 1.0f, 1.0f, 1.0f);

	const float kLabelScale = 0.52f;
	const float kLabelLineHeight = 28.0f;
	const float kGhostOffsetX = 18.0f;  // カーソルからの名札のずらし量(カーソル自体と重ならないように右下へ)。
	const float kGhostOffsetY = -18.0f;
	const float kGhostPaddingX = 10.0f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f);
	const Vector2 kCenterPivot(0.5f, 0.5f);

	bool IsBoardKind(UIRegionKind kind)
	{
		return kind == UIRegionKind::BoardUnit || kind == UIRegionKind::BoardEmptyHex;
	}

	Vector2 RegionCenter(const UIHotRegion& r)
	{
		return Vector2((r.minX + r.maxX) * 0.5f, (r.minY + r.maxY) * 0.5f);
	}

	Vector2 RegionSize(const UIHotRegion& r)
	{
		return Vector2(r.maxX - r.minX, r.maxY - r.minY);
	}

	int FindBoardIndexAt(const Player& player, const HexCoord& hex)
	{
		for (int i = 0; i < (int)player.board.size(); ++i)
		{
			if (player.board[i].position == hex) return i;
		}
		return -1;
	}
}

UIHotRegion DragDropController::BenchArea()
{
	// ベンチパネル全体(タイトル行〜表示上限行の下端)。BoardUIRendererのpublicレイアウト定数から作る。
	UIHotRegion area;
	area.minX = BoardUIRenderer::kBenchX - 6.0f;
	area.maxX = BoardUIRenderer::kBenchX + 272.0f;
	area.maxY = BoardUIRenderer::kBenchTopY + 8.0f;
	area.minY = BoardUIRenderer::kBenchPanelBottomY;
	return area;
}

UIHotRegion DragDropController::SellArea()
{
	UIHotRegion area;
	area.minX = kSellAreaMinX;
	area.maxX = kSellAreaMaxX;
	area.minY = kSellAreaMinY;
	area.maxY = kSellAreaMaxY;
	return area;
}

const UIHotRegion* DragDropController::FindRegionAt(const UIHotRegionList& hotRegions, const Vector2& uiPos)
{
	// UIInteractionSystem::Update()と同じく、後で登録された(手前に描かれる)ものを優先する。
	for (auto it = hotRegions.rbegin(); it != hotRegions.rend(); ++it)
	{
		if (it->Contains(uiPos)) return &(*it);
	}
	return nullptr;
}

bool DragDropController::Update(
	const UIHotRegionList& hotRegions,
	bool holdingSomething,
	Player& player,
	ItemSystem& itemSystem,
	const ItemDatabase& itemDatabase,
	ShopUIRenderer& feedback)
{
	Vector2 uiPos;
	if (CursorSelectionSystem::ScreenToUISpace(uiPos))
	{
		m_cursorPos = uiPos; // ウィンドウ外へ出た間は最後の位置を保持する。
	}

	switch (m_state)
	{
	case State::Idle:
	{
		// 既存クリック操作の途中(何か掴んでいる)の押下はドラッグ化しない(plan.md §1)。
		if (!g_mouse->IsTrigger(enMouseButtonLeft) || holdingSomething) break;

		const UIHotRegion* pressed = FindRegionAt(hotRegions, m_cursorPos);
		if (pressed == nullptr) break;

		m_sourceDef = nullptr;
		m_sourceStar = 0;
		m_sourceItem = nullptr;

		if (pressed->kind == UIRegionKind::BenchUnit)
		{
			if (pressed->index < 0 || pressed->index >= (int)player.bench.size()) break;
			m_sourceDef = player.bench[pressed->index].def;
			m_sourceStar = player.bench[pressed->index].starLevel;
		}
		else if (pressed->kind == UIRegionKind::BoardUnit)
		{
			const UnitInstance* unit = player.FindBoardUnitAt(pressed->hex);
			if (unit == nullptr) break;
			m_sourceDef = unit->def;
			m_sourceStar = unit->starLevel;
		}
		else if (pressed->kind == UIRegionKind::UnclaimedItem)
		{
			if (pressed->index < 0 || pressed->index >= (int)player.unclaimedItems.size()) break;
			m_sourceItem = player.unclaimedItems[pressed->index];
		}
		else
		{
			break; // ドラッグ元にならない領域(ショップ枠・ボタン等)。
		}

		m_source = *pressed;
		m_pressPos = m_cursorPos;
		m_state = State::Pressed;
		break;
	}

	case State::Pressed:
	{
		if (!g_mouse->IsPress(enMouseButtonLeft))
		{
			// 閾値以内で離した = 従来のクリック扱い(押下フレームで既存クリック処理が発火済み)。
			m_state = State::Idle;
			break;
		}

		float dx = m_cursorPos.x - m_pressPos.x;
		float dy = m_cursorPos.y - m_pressPos.y;
		if (dx * dx + dy * dy >= kDragThresholdPx * kDragThresholdPx)
		{
			m_state = State::Dragging;

			// 押下で既存クリックが出した「掴んだ: 〜 (クリックで配置)」の案内を、ドラッグ用に上書きする。
			std::wstring fb = L"ドラッグ中: " + SourceLabel(player) + L"  (右クリックで取り消し)";
			feedback.PushFeedback(fb.c_str(), ShopUIRenderer::FeedbackLevel::Info);
		}
		break;
	}

	case State::Dragging:
	{
		if (g_mouse->IsTrigger(enMouseButtonRight))
		{
			// 右クリックでドラッグを取り消す(元に戻る)。呼び出し側はtrueを受けて掴み状態を解除し、
			// 同フレームの既存の右クリック処理(売却/ベンチ戻しの確認)は走らせないこと。
			feedback.PushFeedback(L"ドラッグを取り消しました", ShopUIRenderer::FeedbackLevel::Info);
			m_state = State::Idle;
			return true;
		}

		if (!g_mouse->IsPress(enMouseButtonLeft))
		{
			// 離した場所でドロップを確定する。
			DropTarget target = ResolveDrop(hotRegions, m_cursorPos);
			if (!IsSourceStillValid(player))
			{
				feedback.PushFeedback(L"ドラッグ元が変化したため取り消しました", ShopUIRenderer::FeedbackLevel::Failure);
			}
			else
			{
				ExecuteDrop(target, player, itemSystem, itemDatabase, feedback);
			}
			m_state = State::Idle;
			return true;
		}
		break;
	}
	}

	return false;
}

void DragDropController::Cancel()
{
	m_state = State::Idle;
	m_hasData = false;
}

DragDropController::DropTarget DragDropController::ResolveDrop(const UIHotRegionList& hotRegions, const Vector2& uiPos) const
{
	DropTarget target;
	const UIHotRegion* hit = FindRegionAt(hotRegions, uiPos);

	if (m_source.kind == UIRegionKind::UnclaimedItem)
	{
		// アイテム: ベンチ/盤面のユニットへの装備のみ。
		if (hit != nullptr && hit->kind == UIRegionKind::BenchUnit)
		{
			target.kind = DropKind::EquipBench;
			target.benchIndex = hit->index;
			target.region = *hit;
		}
		else if (hit != nullptr && hit->kind == UIRegionKind::BoardUnit)
		{
			target.kind = DropKind::EquipBoard;
			target.hex = hit->hex;
			target.region = *hit;
		}
		return target;
	}

	// ユニット(ベンチ/盤面)。
	if (hit != nullptr && IsBoardKind(hit->kind))
	{
		if (m_source.kind == UIRegionKind::BoardUnit && hit->hex == m_source.hex)
		{
			return target; // 同じマスへ戻した = 何もしない。
		}
		target.kind = DropKind::BoardHex;
		target.hex = hit->hex;
		target.region = *hit;
		return target;
	}

	if ((hit != nullptr && hit->kind == UIRegionKind::BenchUnit) || BenchArea().Contains(uiPos))
	{
		// ベンチのユニットをベンチへ落とした場合は何もしない(並べ替えはスコープ外)。
		if (m_source.kind == UIRegionKind::BoardUnit)
		{
			target.kind = DropKind::Bench;
		}
		return target;
	}

	if ((hit != nullptr && hit->kind == UIRegionKind::ShopSlot) || SellArea().Contains(uiPos))
	{
		target.kind = DropKind::Sell;
	}
	return target;
}

bool DragDropController::IsSourceStillValid(const Player& player) const
{
	switch (m_source.kind)
	{
	case UIRegionKind::BenchUnit:
		return m_source.index >= 0 && m_source.index < (int)player.bench.size()
			&& player.bench[m_source.index].def == m_sourceDef
			&& player.bench[m_source.index].starLevel == m_sourceStar;
	case UIRegionKind::BoardUnit:
	{
		const UnitInstance* unit = player.FindBoardUnitAt(m_source.hex);
		return unit != nullptr && unit->def == m_sourceDef && unit->starLevel == m_sourceStar;
	}
	case UIRegionKind::UnclaimedItem:
		return m_source.index >= 0 && m_source.index < (int)player.unclaimedItems.size()
			&& player.unclaimedItems[m_source.index] == m_sourceItem;
	default:
		return false;
	}
}

void DragDropController::ExecuteDrop(const DropTarget& target, Player& player, ItemSystem& itemSystem,
	const ItemDatabase& itemDatabase, ShopUIRenderer& feedback)
{
	// 実処理は既存クリック/パッド操作と同じPlayer::*/ItemSystem::*のエントリポイントを呼び、
	// フィードバック文言も既存に揃える(ドメインロジックの二重実装はしない)。
	switch (target.kind)
	{
	case DropKind::None:
		// 無効な場所で離した: 何も起こらず元に戻る(掴み状態は呼び出し側が解除する)。
		OutputDebugString(L"[Drag] dropped on invalid target, cancelled.\n");
		break;

	case DropKind::BoardHex:
	{
		const bool occupied = player.FindBoardUnitAt(target.hex) != nullptr;
		bool ok = false;

		if (m_source.kind == UIRegionKind::BenchUnit)
		{
			if (occupied)
			{
				ok = player.SwapBenchWithBoard(m_source.index, target.hex);
				feedback.PushFeedback(ok ? L"入れ替えました" : L"入れ替えできません",
					ok ? ShopUIRenderer::FeedbackLevel::Success : ShopUIRenderer::FeedbackLevel::Failure);
			}
			else
			{
				ok = player.PlaceUnitOnBoard(m_source.index, target.hex);
				if (ok)
				{
					wchar_t fb[128];
					swprintf_s(fb, L"配置: マス(%d,%d)  盤面 %d/%d", target.hex.q, target.hex.r,
						(int)player.board.size(), player.GetMaxBoardSize());
					feedback.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
				}
				else
				{
					feedback.PushFeedback(L"配置できません (自陣 手前3行のみ / 盤面上限 / 空きマス無し)", ShopUIRenderer::FeedbackLevel::Failure);
				}
			}
		}
		else // BoardUnit
		{
			if (occupied)
			{
				ok = player.SwapBoardUnits(m_source.hex, target.hex);
				feedback.PushFeedback(ok ? L"入れ替えました" : L"入れ替えできません",
					ok ? ShopUIRenderer::FeedbackLevel::Success : ShopUIRenderer::FeedbackLevel::Failure);
			}
			else
			{
				ok = player.MoveUnitOnBoard(m_source.hex, target.hex);
				if (ok)
				{
					wchar_t fb[128];
					swprintf_s(fb, L"移動: (%d,%d) -> (%d,%d)", m_source.hex.q, m_source.hex.r, target.hex.q, target.hex.r);
					feedback.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);
				}
				else
				{
					feedback.PushFeedback(L"移動できません (自陣 手前3行のみ / 空きマス無し)", ShopUIRenderer::FeedbackLevel::Failure);
				}
			}
		}

		wchar_t log[224];
		swprintf_s(log, L"[Drag] %hs -> hex (%d,%d) %hs: %hs, Bench count: %d, Board count: %d\n",
			m_source.kind == UIRegionKind::BenchUnit ? "bench" : "board",
			target.hex.q, target.hex.r, occupied ? "swap" : "place/move", ok ? "true" : "false",
			(int)player.bench.size(), (int)player.board.size());
		OutputDebugString(log);
		break;
	}

	case DropKind::Bench:
	{
		bool ok = player.ReturnUnitToBench(m_source.hex);
		feedback.PushFeedback(ok ? L"ベンチへ戻しました" : L"ベンチへ戻せません",
			ok ? ShopUIRenderer::FeedbackLevel::Info : ShopUIRenderer::FeedbackLevel::Failure);

		wchar_t log[192];
		swprintf_s(log, L"[Drag] return to bench: %hs, hex (%d,%d), Bench count: %d, Board count: %d\n",
			ok ? "true" : "false", m_source.hex.q, m_source.hex.r, (int)player.bench.size(), (int)player.board.size());
		OutputDebugString(log);
		break;
	}

	case DropKind::Sell:
	{
		bool ok = false;
		if (m_source.kind == UIRegionKind::BenchUnit)
		{
			ok = player.SellUnitFromBench((size_t)m_source.index);
		}
		else // BoardUnit
		{
			int boardIndex = FindBoardIndexAt(player, m_source.hex);
			ok = boardIndex >= 0 && player.SellUnitFromBoard((size_t)boardIndex);
		}

		if (ok)
		{
			wchar_t fb[128];
			swprintf_s(fb, L"売却  所持 %dG", player.gold);
			feedback.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
		}
		else
		{
			feedback.PushFeedback(L"売却できません", ShopUIRenderer::FeedbackLevel::Failure);
		}

		wchar_t log[192];
		swprintf_s(log, L"[Drag] sell from %hs: %hs, Gold: %d, Bench count: %d, Board count: %d\n",
			m_source.kind == UIRegionKind::BenchUnit ? "bench" : "board", ok ? "true" : "false",
			player.gold, (int)player.bench.size(), (int)player.board.size());
		OutputDebugString(log);
		break;
	}

	case DropKind::EquipBench:
	case DropKind::EquipBoard:
	{
		UnitInstance* targetUnit = nullptr;
		if (target.kind == DropKind::EquipBench)
		{
			if (target.benchIndex >= 0 && target.benchIndex < (int)player.bench.size())
			{
				targetUnit = &player.bench[target.benchIndex];
			}
		}
		else
		{
			targetUnit = player.FindBoardUnitAt(target.hex);
		}

		if (targetUnit == nullptr)
		{
			feedback.PushFeedback(L"装備先のユニットがいません", ShopUIRenderer::FeedbackLevel::Failure);
			break;
		}

		const ItemDef* item = player.unclaimedItems[m_source.index];
		bool equipped = itemSystem.GiveItem(*targetUnit, item, itemDatabase, player.name);
		if (equipped)
		{
			wchar_t fb[192];
			swprintf_s(fb, L"装備: %hs -> %hs", item->name.c_str(), targetUnit->def->name.c_str());
			feedback.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);

			wchar_t log[224];
			swprintf_s(log, L"[Equip] (drag) %hs -> %hs (unclaimed left=%d, unit items=%d)\n",
				item->name.c_str(), targetUnit->def->name.c_str(),
				(int)player.unclaimedItems.size() - 1, (int)targetUnit->items.size());
			OutputDebugString(log);

			player.unclaimedItems.erase(player.unclaimedItems.begin() + m_source.index);
		}
		else
		{
			feedback.PushFeedback(L"装備できません (アイテム枠が満杯)", ShopUIRenderer::FeedbackLevel::Failure);
		}
		break;
	}
	}
}

std::wstring DragDropController::SourceLabel(const Player& player) const
{
	wchar_t buf[128];
	buf[0] = L'\0';

	if (m_source.kind == UIRegionKind::UnclaimedItem)
	{
		if (m_sourceItem != nullptr) swprintf_s(buf, L"%hs", m_sourceItem->name.c_str());
	}
	else if (m_sourceDef != nullptr)
	{
		// ★は既存のベンチ/HPバー表示(BoardUIRenderer::StarSuffix)と同じ " *2" 表記にする。
		if (m_sourceStar >= 2) swprintf_s(buf, L"%hs *%d", m_sourceDef->name.c_str(), m_sourceStar);
		else swprintf_s(buf, L"%hs", m_sourceDef->name.c_str());
	}
	return buf;
}

void DragDropController::Draw(RenderContext& rc, const UIHotRegionList& hotRegions, const Player& player, UIRectRenderer& rectRenderer)
{
	m_rects.clear();
	m_texts.clear();
	m_rectRenderer = &rectRenderer;
	m_hasData = false;

	if (m_state != State::Dragging) return;

	const DropTarget current = ResolveDrop(hotRegions, m_cursorPos);
	const bool isItem = (m_source.kind == UIRegionKind::UnclaimedItem);

	auto addRegion = [&](const UIHotRegion& r, const Vector4& fill, const Vector4& border, float thickness)
	{
		RectView v;
		v.center = RegionCenter(r);
		v.size = RegionSize(r);
		v.fill = fill;
		v.border = border;
		v.borderThickness = thickness;
		m_rects.push_back(v);
	};

	// --- ドロップ候補(マス・ユニット)のハイライト ---
	for (const auto& r : hotRegions)
	{
		bool candidate = false;
		if (isItem)
		{
			candidate = (r.kind == UIRegionKind::BenchUnit || r.kind == UIRegionKind::BoardUnit);
		}
		else
		{
			candidate = IsBoardKind(r.kind) && !(m_source.kind == UIRegionKind::BoardUnit && r.hex == m_source.hex);
		}
		if (!candidate) continue;

		bool isCurrent = false;
		if (current.kind == DropKind::BoardHex || current.kind == DropKind::EquipBoard)
		{
			isCurrent = IsBoardKind(r.kind) && r.hex == current.hex;
		}
		else if (current.kind == DropKind::EquipBench)
		{
			isCurrent = (r.kind == UIRegionKind::BenchUnit && r.index == current.benchIndex);
		}

		if (isCurrent)
		{
			addRegion(r, kTargetFill, UIStyle::kSelectedBorderColor, UIStyle::kSelectedBorderThickness);
		}
		else
		{
			addRegion(r, kCandidateFill, kCandidateBorder, 1.5f);
		}
	}

	if (!isItem)
	{
		// --- ベンチ領域(盤面ユニットのみ「ベンチへ戻す」先になる) ---
		if (m_source.kind == UIRegionKind::BoardUnit)
		{
			UIHotRegion bench = BenchArea();
			bool isCurrent = (current.kind == DropKind::Bench);
			addRegion(bench, isCurrent ? kTargetFill : kCandidateFill,
				isCurrent ? UIStyle::kSelectedBorderColor : kCandidateBorder,
				isCurrent ? UIStyle::kSelectedBorderThickness : 1.5f);

			TextView t;
			t.topLeft = Vector2(bench.minX + 8.0f, bench.minY + kLabelLineHeight + 6.0f);
			t.text = L"ここで離すとベンチへ戻す";
			t.color = kLabelColor;
			t.scale = kLabelScale;
			m_texts.push_back(t);
		}

		// --- 売却領域(ショップバー) ---
		{
			UIHotRegion sell = SellArea();
			bool isCurrent = (current.kind == DropKind::Sell);
			addRegion(sell, isCurrent ? kSellTargetFill : kSellFill, kSellBorder,
				isCurrent ? UIStyle::kSelectedBorderThickness : 1.5f);

			int sellValue = 0;
			if (m_source.kind == UIRegionKind::BenchUnit && m_source.index >= 0 && m_source.index < (int)player.bench.size())
			{
				sellValue = player.CalculateSellValue(player.bench[m_source.index]);
			}
			else if (const UnitInstance* unit = player.FindBoardUnitAt(m_source.hex))
			{
				sellValue = player.CalculateSellValue(*unit);
			}

			wchar_t buf[96];
			swprintf_s(buf, L"ここで離すと売却  +%dG", sellValue);
			TextView t;
			t.topLeft = Vector2(RegionCenter(sell).x - UITextUtil::EstimateTextWidth(buf, kLabelScale) * 0.5f,
				RegionCenter(sell).y + kLabelLineHeight * 0.5f);
			t.text = buf;
			t.color = kLabelColor;
			t.scale = kLabelScale;
			m_texts.push_back(t);
		}
	}

	// --- カーソルに追従する名札(掴んでいるもの) ---
	{
		std::wstring label = SourceLabel(player);
		if (!label.empty())
		{
			float width = UITextUtil::EstimateTextWidth(label, kLabelScale) + kGhostPaddingX * 2.0f;
			Vector2 topLeft(m_cursorPos.x + kGhostOffsetX, m_cursorPos.y + kGhostOffsetY);

			RectView v;
			v.center = Vector2(topLeft.x + width * 0.5f, topLeft.y - kLabelLineHeight * 0.5f);
			v.size = Vector2(width, kLabelLineHeight);
			v.fill = kGhostFill;
			v.border = UIStyle::kSelectedBorderColor;
			v.borderThickness = UIStyle::kPanelBorderThickness;
			m_rects.push_back(v);

			TextView t;
			t.topLeft = Vector2(topLeft.x + kGhostPaddingX, topLeft.y - 2.0f);
			t.text = label;
			t.color = kLabelColor;
			t.scale = kLabelScale;
			m_texts.push_back(t);
		}
	}

	m_hasData = true;
	g_renderingEngine->AddRenderObject(this);
}

void DragDropController::OnRender2D(RenderContext& rc)
{
	if (!m_hasData || m_rectRenderer == nullptr) return;

	// 矩形(Sprite)はFont::Begin()〜End()の外側でまとめて描き終える(SpriteBatchの状態と競合するため。
	// docs/tasks/ui-sprite-bars/plan.md §0-8)。
	// 半透明の塗りの上から見えるよう、枠は(DrawPanelの「大きい矩形の上に塗りを重ねる」方式ではなく)
	// 4辺の細い矩形として描く。DrawPanel方式だと塗りが半透明のため枠色が全面に透けてしまう。
	for (const auto& r : m_rects)
	{
		m_rectRenderer->DrawRect(rc, r.center, r.size, r.fill, kCenterPivot);

		float halfW = r.size.x * 0.5f;
		float halfH = r.size.y * 0.5f;
		float t = r.borderThickness;
		m_rectRenderer->DrawRect(rc, Vector2(r.center.x, r.center.y + halfH), Vector2(r.size.x + t, t), r.border, kCenterPivot); // 上
		m_rectRenderer->DrawRect(rc, Vector2(r.center.x, r.center.y - halfH), Vector2(r.size.x + t, t), r.border, kCenterPivot); // 下
		m_rectRenderer->DrawRect(rc, Vector2(r.center.x - halfW, r.center.y), Vector2(t, r.size.y + t), r.border, kCenterPivot); // 左
		m_rectRenderer->DrawRect(rc, Vector2(r.center.x + halfW, r.center.y), Vector2(t, r.size.y + t), r.border, kCenterPivot); // 右
	}

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);
	for (const auto& t : m_texts)
	{
		m_font.Draw(t.text.c_str(), t.topLeft, t.color, 0.0f, t.scale, kTopLeftPivot);
	}
	m_font.End(rc);
}

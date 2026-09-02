#include "stdafx.h"
#include "TooltipContentBuilder.h"
#include "UnitDef.h"
#include "UnitDatabase.h"
#include "ItemDef.h"
#include "ItemDatabase.h"
#include "TraitDef.h"
#include "TraitDatabase.h"
#include "TraitSystem.h"
#include "UnitInstance.h"
#include "Player.h"
#include "GameState.h"
#include "UITextUtil.h"

namespace
{
	// UnitDef::skillType等から必殺技の説明文を1行合成する(自由記述フィールドが無いため。
	// plan.md §3-2-1参照)。
	std::wstring BuildSkillDescriptionText(const UnitDef& def)
	{
		wchar_t buf[160];
		switch (def.skillType)
		{
		case SkillEffectType::Damage:
			swprintf_s(buf, L"必殺技: 単体に大ダメージ");
			break;
		case SkillEffectType::AreaDamage:
			swprintf_s(buf, L"必殺技: 範囲ダメージ(半径%d, 周囲へ%.0f%%)", def.skillSplashRadius, def.skillSplashPercent);
			break;
		case SkillEffectType::DamageAndHeal:
			swprintf_s(buf, L"必殺技: ダメージ+自己回復(与ダメージの%.0f%%)", def.skillHealPercent);
			break;
		case SkillEffectType::DamageAndShield:
			swprintf_s(buf, L"必殺技: ダメージ+自身にシールド%d", def.skillShieldAmount);
			break;
		default:
			swprintf_s(buf, L"必殺技: 不明");
			break;
		}
		return buf;
	}

	// UnitDefのステータス概要2行(名前/星は呼び出し側で別途足す)。
	void AppendUnitDefLines(std::vector<std::wstring>& out, const UnitDef& def)
	{
		wchar_t buf1[128];
		swprintf_s(buf1, L"HP%d AT%d AP%d 物防%d 魔防%d", def.baseHP, def.baseAttack, def.magicPower, def.physicalDefense, def.magicDefense);
		out.push_back(buf1);

		wchar_t buf2[128];
		swprintf_s(buf2, L"攻撃速度%.2f/s  射程%d(必殺%d)", def.attackSpeed, def.attackRange, def.skillRange);
		out.push_back(buf2);

		out.push_back(BuildSkillDescriptionText(def));

		if (!def.traits.empty())
		{
			std::wstring traitsLine = L"トレイト: ";
			for (size_t i = 0; i < def.traits.size(); ++i)
			{
				if (i > 0) traitsLine += L"/";
				traitsLine += UITextUtil::TraitName(def.traits[i]);
			}
			out.push_back(traitsLine);
		}
	}

	// bench/board上のUnitInstance向け。UnitDef概要 + 星 + 適用中ボーナス + 装備アイテムを追加する。
	void AppendUnitInstanceLines(std::vector<std::wstring>& out, const UnitInstance& unit, const Player& player)
	{
		// 星表記はBoardUIRenderer::StarSuffixと同じくASCIIの"*"を使う(スプライトフォント未収録の
		// 装飾記号グリフを描くとFontEngineが例外でアプリごとクラッシュするため、"★"等は使わない)。
		wchar_t title[80];
		swprintf_s(title, L"%hs  *%d", unit.def->name.c_str(), unit.starLevel);
		out.push_back(title);

		AppendUnitDefLines(out, *unit.def);

		// 適用中ボーナス(0でないものだけ)。トレイト・アイテムの合算値で、内訳は区別しない
		// (bonus*系フィールド自体が合算値のため。既存UIも同様の割り切り)。
		std::wstring bonusLine;
		auto appendBonus = [&](const wchar_t* label, int value)
		{
			if (value == 0) return;
			wchar_t buf[32];
			swprintf_s(buf, L"%ls%+d ", label, value);
			bonusLine += buf;
		};
		appendBonus(L"AT", unit.bonusAttack);
		appendBonus(L"AP", unit.bonusMagicPower);
		appendBonus(L"HP", unit.bonusMaxHP);
		appendBonus(L"物防", unit.bonusPhysicalDefense);
		appendBonus(L"魔防", unit.bonusMagicDefense);
		if (unit.bonusAttackSpeed != 0.0f)
		{
			wchar_t buf[32];
			swprintf_s(buf, L"AS%+.2f ", unit.bonusAttackSpeed);
			bonusLine += buf;
		}
		if (!bonusLine.empty())
		{
			out.push_back(L"ボーナス(アイテム/トレイト込み): " + bonusLine);
		}

		if (!unit.items.empty())
		{
			std::wstring itemsLine = L"装備: ";
			for (size_t i = 0; i < unit.items.size(); ++i)
			{
				if (i > 0) itemsLine += L", ";
				wchar_t nameBuf[64];
				swprintf_s(nameBuf, L"%hs", unit.items[i]->name.c_str());
				itemsLine += nameBuf;
			}
			out.push_back(itemsLine);
		}

		wchar_t sellLine[64];
		swprintf_s(sellLine, L"クリックで選択/移動  右クリックで売却 (+%dG)", player.CalculateSellValue(unit));
		out.push_back(sellLine);
	}

	std::vector<std::wstring> BuildForShopSlot(const UIHotRegion& region, const std::vector<const UnitDef*>& shop, const Player& player)
	{
		std::vector<std::wstring> lines;
		if (region.index < 0 || region.index >= (int)shop.size() || shop[region.index] == nullptr) return lines;

		const UnitDef& def = *shop[region.index];
		wchar_t title[80];
		swprintf_s(title, L"%hs  (コスト%d)", def.name.c_str(), def.cost);
		lines.push_back(title);

		AppendUnitDefLines(lines, def);

		if (player.gold >= def.cost)
		{
			wchar_t buf[64];
			swprintf_s(buf, L"クリックで購入 (-%dG)", def.cost);
			lines.push_back(buf);
		}
		else
		{
			wchar_t buf[64];
			swprintf_s(buf, L"ゴールド不足 (要%dG、所持%dG)", def.cost, player.gold);
			lines.push_back(buf);
		}
		return lines;
	}

	std::vector<std::wstring> BuildForBenchUnit(const UIHotRegion& region, const Player& player)
	{
		std::vector<std::wstring> lines;
		if (region.index < 0 || region.index >= (int)player.bench.size()) return lines;
		AppendUnitInstanceLines(lines, player.bench[region.index], player);
		return lines;
	}

	std::vector<std::wstring> BuildForBoardUnit(const UIHotRegion& region, const Player& player)
	{
		std::vector<std::wstring> lines;
		const UnitInstance* unit = player.FindBoardUnitAt(region.hex);
		if (unit == nullptr) return lines;
		AppendUnitInstanceLines(lines, *unit, player);
		return lines;
	}

	std::wstring EffectsFullText(const ItemDef& def)
	{
		std::wstring s;
		for (size_t i = 0; i < def.effects.size(); ++i)
		{
			if (i > 0) s += L"  ";
			s += UITextUtil::EffectShortText(def.effects[i]);
		}
		return s;
	}

	std::vector<std::wstring> BuildForUnclaimedItem(const UIHotRegion& region, const Player& player, const ItemDatabase& itemDatabase)
	{
		std::vector<std::wstring> lines;
		if (region.index < 0 || region.index >= (int)player.unclaimedItems.size()) return lines;
		const ItemDef& def = *player.unclaimedItems[region.index];

		wchar_t nameBuf[64];
		swprintf_s(nameBuf, L"%hs", def.name.c_str());
		lines.push_back(nameBuf);

		if (!def.effects.empty())
		{
			lines.push_back(EffectsFullText(def));
		}
		for (const PassiveEffect& p : def.passives)
		{
			if (p.type == PassiveEffectType::OnHitBurn)
			{
				// "×"(乗算記号)もスプライトフォント未収録の恐れがあるためASCIIの"x"を使う
				// (★と同じ理由。BoardUIRenderer::StarSuffixの割り切りに倣う)。
				wchar_t buf[128];
				swprintf_s(buf, L"パッシブ: 通常攻撃時に火傷付与(%dx%d回, %.1fs間隔)", p.magnitude, p.ticks, p.interval);
				lines.push_back(buf);
			}
		}

		if (def.category == ItemCategory::Component)
		{
			bool any = false;
			for (const ItemRecipe& recipe : itemDatabase.GetAllRecipes())
			{
				const std::string* other = nullptr;
				if (recipe.componentA == def.name) other = &recipe.componentB;
				else if (recipe.componentB == def.name) other = &recipe.componentA;
				if (other == nullptr) continue;

				if (!any)
				{
					lines.push_back(L"この素材でできる完成品:");
					any = true;
				}
				wchar_t buf[96];
				swprintf_s(buf, L"  + %hs -> %hs", other->c_str(), recipe.resultName.c_str());
				lines.push_back(buf);
			}
		}

		lines.push_back(L"クリックで手に持つ -> ユニットをクリックで装備");
		return lines;
	}

	std::vector<std::wstring> BuildForTraitRow(const UIHotRegion& region, const Player& player, const TraitDatabase& traitDatabase, const TraitSystem& traitSystem, const UnitDatabase& unitDatabase)
	{
		std::vector<std::wstring> lines;
		TraitType type = (TraitType)region.index;
		const TraitDef* traitDef = traitDatabase.FindTraitDef(type);
		if (traitDef == nullptr) return lines;

		auto traitCounts = traitSystem.CountBoardTraits(player.board);
		int count = 0;
		auto it = traitCounts.find(type);
		if (it != traitCounts.end()) count = it->second;

		const TraitTier* activeTier = traitSystem.FindActiveTier(*traitDef, count);

		wchar_t title[64];
		swprintf_s(title, L"%hs  (現在%d体)", traitDef->name.c_str(), count);
		lines.push_back(title);

		for (const TraitTier& tier : traitDef->tiers)
		{
			bool isActiveTier = (activeTier == &tier);
			std::wstring line = isActiveTier ? L"> " : L"  ";
			wchar_t head[32];
			swprintf_s(head, L"%d体: ", tier.requiredCount);
			line += head;
			for (size_t i = 0; i < tier.effects.size(); ++i)
			{
				if (i > 0) line += L" ";
				line += UITextUtil::EffectShortText(tier.effects[i]);
			}
			lines.push_back(line);
		}

		std::wstring unitsLine = L"該当ユニット: ";
		bool firstUnit = true;
		for (const UnitDef& def : unitDatabase.GetAllUnitDefs())
		{
			bool hasTrait = false;
			for (TraitType t : def.traits)
			{
				if (t == type) { hasTrait = true; break; }
			}
			if (!hasTrait) continue;

			if (!firstUnit) unitsLine += L"/";
			wchar_t nameBuf[64];
			swprintf_s(nameBuf, L"%hs", def.name.c_str());
			unitsLine += nameBuf;
			firstUnit = false;
		}
		lines.push_back(unitsLine);

		lines.push_back(activeTier != nullptr ? L"発動中" : L"未発動");
		return lines;
	}
}

namespace TooltipContentBuilder
{
	std::vector<std::wstring> Build(
		const UIHotRegion& region,
		const std::vector<const UnitDef*>& shop,
		const Player& player,
		const GameState& gameState,
		int xpForNextLevel,
		const UnitDatabase& unitDatabase,
		const ItemDatabase& itemDatabase,
		const TraitDatabase& traitDatabase,
		const TraitSystem& traitSystem)
	{
		switch (region.kind)
		{
		case UIRegionKind::ShopSlot:
			return BuildForShopSlot(region, shop, player);

		case UIRegionKind::BenchUnit:
			return BuildForBenchUnit(region, player);

		case UIRegionKind::BoardUnit:
			return BuildForBoardUnit(region, player);

		case UIRegionKind::UnclaimedItem:
			return BuildForUnclaimedItem(region, player, itemDatabase);

		case UIRegionKind::TraitRow:
			return BuildForTraitRow(region, player, traitDatabase, traitSystem, unitDatabase);

		case UIRegionKind::RerollButton:
			return { L"クリックでショップをリロール (-2G)" };

		case UIRegionKind::BuyXpButton:
			return { L"クリックで経験値を購入 (-4G)" };

		case UIRegionKind::LockButton:
			return { L"クリックでショップのロックを切り替え", L"(ロック中はラウンドを跨いでも維持)" };

		case UIRegionKind::NextPhaseButton:
			return { L"クリックで戦闘フェーズへ進む" };

		case UIRegionKind::TitleStartButton:
			return { L"クリックで開始/続きから再開" };

		case UIRegionKind::TitleNewGameButton:
			return { L"クリックでセーブを無視して新規開始" };

		case UIRegionKind::RestartButton:
			return { L"クリックでタイトルへ戻る(1プレイをリセット)" };

		case UIRegionKind::GoldDisplay:
		{
			wchar_t buf[64];
			swprintf_s(buf, L"所持ゴールド: %dG", player.gold);
			return { buf };
		}

		case UIRegionKind::HudLevelDisplay:
		{
			std::vector<std::wstring> lines;
			wchar_t buf1[64];
			swprintf_s(buf1, L"レベル %d", player.level);
			lines.push_back(buf1);
			if (xpForNextLevel > 0)
			{
				wchar_t buf2[64];
				swprintf_s(buf2, L"経験値 %d / %d (次のレベルまで)", player.xp, xpForNextLevel);
				lines.push_back(buf2);
			}
			else
			{
				lines.push_back(L"最大レベルに到達済み");
			}
			return lines;
		}

		case UIRegionKind::HudBoardCountDisplay:
		{
			wchar_t buf[64];
			swprintf_s(buf, L"盤面 %d / %d体(レベルが上限)", (int)player.board.size(), player.GetMaxBoardSize());
			return { buf };
		}

		case UIRegionKind::HudRoundDisplay:
		{
			std::vector<std::wstring> lines;
			int roundNumber = gameState.roundNumber;
			if (roundNumber > GameState::kTotalRounds) roundNumber = GameState::kTotalRounds;
			wchar_t buf1[64];
			swprintf_s(buf1, L"ラウンド %d / %d", roundNumber, GameState::kTotalRounds);
			lines.push_back(buf1);
			wchar_t buf2[64];
			swprintf_s(buf2, L"現在の敵への敗北 %d / %d(超えるとゲームオーバー)", gameState.lossCount, GameState::kMaxLossesPerEnemy);
			lines.push_back(buf2);
			return lines;
		}

		case UIRegionKind::HudStreakDisplay:
		{
			if (player.winStreak > 0)
			{
				wchar_t buf[48];
				swprintf_s(buf, L"%d連勝中(勝利報酬のゴールドが増える)", player.winStreak);
				return { buf };
			}
			if (player.lossStreak > 0)
			{
				wchar_t buf[48];
				swprintf_s(buf, L"%d連敗中", player.lossStreak);
				return { buf };
			}
			return { L"連勝連敗なし" };
		}

		default:
			return {};
		}
	}
}

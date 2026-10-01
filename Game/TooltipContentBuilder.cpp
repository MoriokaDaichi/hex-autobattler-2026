#include "stdafx.h"
#include "TooltipContentBuilder.h"
#include "UnitDef.h"
#include "UnitDatabase.h"
#include "ItemDef.h"
#include "ItemDatabase.h"
#include "TraitDef.h"
#include "TraitDatabase.h"
#include "TraitSystem.h"
#include "ItemSystem.h"
#include "StarLevelSystem.h"
#include "UnitInstance.h"
#include "Player.h"
#include "GameState.h"
#include "EconomySystem.h"
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

	// --- 準備フェーズ用の補正込みプレビュー(playtest-quickfix-1 B) ---
	// UnitInstance::bonus*系は戦闘突入時のApply*でしか更新されないため、準備フェーズでは前回戦闘時点の
	// 値(初回は0)が残っている。ツールチップでは盤面/ベンチの"写し"に対して戦闘突入時と同じ順序
	// (Trait → Item → Star)で再計算した値を表示する。Apply*はcurrentHP全回復・シールドリセットの副作用を
	// 持つが、写しに対してのみ呼ぶので実際のplayers[0]の状態(戦闘開始時の状態・HP表示)には影響しない。
	// 毎フレーム組み立て直すため、盤面/ベンチ/装備が変化すれば自動的に最新の補正になる。

	// bonus*系をゼロに戻す(TraitSystem::ApplyTraitBonusesの冒頭リセットと同じ項目)。
	void ResetBonusFields(UnitInstance& unit)
	{
		unit.bonusAttack = 0;
		unit.bonusMagicPower = 0;
		unit.bonusMaxHP = 0;
		unit.bonusPhysicalDefense = 0;
		unit.bonusMagicDefense = 0;
		unit.bonusSkillThreshold = 0;
		unit.bonusAttackSpeed = 0.0f;
		unit.shieldAmount = 0;
	}

	// プレイヤー盤面の写しを作り、戦闘突入時(Game::Update()のCombat突入フレーム)と同じ順序で補正を適用して返す。
	// 各SystemはステートレスなのでGameのメンバーを借りずにローカルで生成する。ログは毎フレーム出さないよう抑止する。
	std::vector<UnitInstance> BuildBoardPreview(const Player& player, const TraitDatabase& traitDatabase)
	{
		std::vector<UnitInstance> preview = player.board;
		TraitSystem traitSystem;
		ItemSystem itemSystem;
		StarLevelSystem starLevelSystem;
		traitSystem.ApplyTraitBonuses(preview, traitDatabase, player.name, /*logEnabled=*/false);
		itemSystem.ApplyItemBonuses(preview, player.name, /*logEnabled=*/false);
		starLevelSystem.ApplyStarBonuses(preview, player.name, /*logEnabled=*/false);
		return preview;
	}

	// ベンチのユニット1体分の写し。トレイトは盤面に出ているユニットにしか発動しないため、
	// bonus*をゼロに戻してからアイテムと星の補正だけを適用する。
	UnitInstance BuildBenchPreview(const UnitInstance& unit)
	{
		std::vector<UnitInstance> preview{ unit };
		ResetBonusFields(preview[0]);
		ItemSystem itemSystem;
		StarLevelSystem starLevelSystem;
		itemSystem.ApplyItemBonuses(preview, "", /*logEnabled=*/false);
		starLevelSystem.ApplyStarBonuses(preview, "", /*logEnabled=*/false);
		return preview[0];
	}

	// 「基礎値+補正」表記の1項目を追記する(補正0なら基礎値のみ)。例: "HP650+520 "、"AT55 "。
	void AppendBaseWithBonus(std::wstring& line, const wchar_t* label, int baseValue, int bonusValue)
	{
		wchar_t buf[48];
		if (bonusValue != 0)
		{
			swprintf_s(buf, L"%ls%d%+d ", label, baseValue, bonusValue);
		}
		else
		{
			swprintf_s(buf, L"%ls%d ", label, baseValue);
		}
		line += buf;
	}

	// bench/board上のUnitInstance向け。名前/星 + 補正込みステータス(基礎+補正) + 装備アイテムを追加する。
	// unitは呼び出し側でBuildBoardPreview/BuildBenchPreviewにより補正を再計算済みの写しを渡す。
	// isOnBoard: 末尾の操作案内を、右クリックで「売却」(bench)か「ベンチへ戻す」(board、GOLD増えない)
	// かで出し分けるために必要(実機検証で判明: 両者は挙動が異なるため文言を混同してはいけない。
	// Player::ReturnUnitToBenchは売却ではなくベンチへ戻すだけでゴールドは増えない)。
	void AppendUnitInstanceLines(std::vector<std::wstring>& out, const UnitInstance& unit, const Player& player, bool isOnBoard)
	{
		// 星表記はBoardUIRenderer::StarSuffixと同じくASCIIの"*"を使う(スプライトフォント未収録の
		// 装飾記号グリフを描くとFontEngineが例外でアプリごとクラッシュするため、"★"等は使わない)。
		wchar_t title[80];
		swprintf_s(title, L"%hs  *%d", unit.def->name.c_str(), unit.starLevel);
		out.push_back(title);

		const UnitDef& def = *unit.def;

		// ステータスは「基礎値+補正」で表示する(補正 = トレイト/アイテム/星の合算。bonus*系フィールド
		// 自体が合算値のため内訳は区別しない)。
		std::wstring statLine;
		AppendBaseWithBonus(statLine, L"HP", def.baseHP, unit.bonusMaxHP);
		AppendBaseWithBonus(statLine, L"AT", def.baseAttack, unit.bonusAttack);
		AppendBaseWithBonus(statLine, L"AP", def.magicPower, unit.bonusMagicPower);
		AppendBaseWithBonus(statLine, L"物防", def.physicalDefense, unit.bonusPhysicalDefense);
		AppendBaseWithBonus(statLine, L"魔防", def.magicDefense, unit.bonusMagicDefense);
		out.push_back(statLine);

		wchar_t speedLine[128];
		if (unit.bonusAttackSpeed != 0.0f)
		{
			swprintf_s(speedLine, L"攻撃速度%.2f%+.2f/s  射程%d(必殺%d)", def.attackSpeed, unit.bonusAttackSpeed, def.attackRange, def.skillRange);
		}
		else
		{
			swprintf_s(speedLine, L"攻撃速度%.2f/s  射程%d(必殺%d)", def.attackSpeed, def.attackRange, def.skillRange);
		}
		out.push_back(speedLine);

		if (unit.bonusSkillThreshold != 0)
		{
			wchar_t buf[64];
			swprintf_s(buf, L"必殺技まで %d%+d回", def.skillThreshold, unit.bonusSkillThreshold);
			out.push_back(buf);
		}

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

		bool hasAnyBonus = unit.bonusMaxHP != 0 || unit.bonusAttack != 0 || unit.bonusMagicPower != 0
			|| unit.bonusPhysicalDefense != 0 || unit.bonusMagicDefense != 0
			|| unit.bonusAttackSpeed != 0.0f || unit.bonusSkillThreshold != 0;
		if (hasAnyBonus)
		{
			out.push_back(isOnBoard
				? L"(基礎+補正: トレイト/アイテム/星込み)"
				: L"(基礎+補正: アイテム/星込み。トレイトは盤面配置時)");
		}
		else if (!isOnBoard)
		{
			out.push_back(L"(トレイト補正は盤面配置時に発動)");
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

		wchar_t actionLine[64];
		if (isOnBoard)
		{
			// Player::ReturnUnitToBenchはベンチへ戻すだけで売却ではない(ゴールドは増えない)。
			swprintf_s(actionLine, L"クリックで選択/移動  右クリックでベンチへ戻す");
		}
		else
		{
			swprintf_s(actionLine, L"クリックで選択/移動  右クリックで売却 (+%dG)", player.CalculateSellValue(unit));
		}
		out.push_back(actionLine);
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
		UnitInstance preview = BuildBenchPreview(player.bench[region.index]);
		AppendUnitInstanceLines(lines, preview, player, /*isOnBoard=*/false);
		return lines;
	}

	std::vector<std::wstring> BuildForBoardUnit(const UIHotRegion& region, const Player& player, const TraitDatabase& traitDatabase)
	{
		std::vector<std::wstring> lines;
		if (player.FindBoardUnitAt(region.hex) == nullptr) return lines;

		// 盤面全体の写しで補正を再計算し(トレイトは盤面構成で決まるため1体だけでは計算できない)、
		// 写しの中から対象マスのユニットを引く(写しはboardと同じ並び・同じpositionを持つ)。
		std::vector<UnitInstance> previewBoard = BuildBoardPreview(player, traitDatabase);
		for (const UnitInstance& unit : previewBoard)
		{
			if (unit.position == region.hex)
			{
				AppendUnitInstanceLines(lines, unit, player, /*isOnBoard=*/true);
				break;
			}
		}
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
			return BuildForBoardUnit(region, player, traitDatabase);

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
			// 次の戦闘で勝った(負けた)場合のボーナス額を、EconomySystemと同じ関数で計算して表示する。
			if (player.winStreak > 0)
			{
				const int nextWinStreak = player.winStreak + 1;
				wchar_t buf1[48];
				swprintf_s(buf1, L"%d連勝中", player.winStreak);
				wchar_t buf2[64];
				swprintf_s(buf2, L"次の勝利で連勝ボーナス +%dG", EconomySystem::GetWinStreakBonus(nextWinStreak));
				std::vector<std::wstring> lines = { buf1, buf2 };
				if (EconomySystem::HasWinStreakItemBonus(nextWinStreak))
				{
					lines.push_back(L"次の勝利で素材アイテム +1");
				}
				return lines;
			}
			if (player.lossStreak > 0)
			{
				wchar_t buf1[48];
				swprintf_s(buf1, L"%d連敗中", player.lossStreak);
				wchar_t buf2[64];
				swprintf_s(buf2, L"次の敗北で連敗ボーナス +%dG", EconomySystem::GetLossStreakBonus(player.lossStreak + 1));
				return { buf1, buf2 };
			}
			return { L"連勝連敗なし" };
		}

		default:
			return {};
		}
	}
}

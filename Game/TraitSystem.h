#pragma once
#include <map>
#include <string>
#include <vector>
#include "UnitInstance.h"
#include "TraitDatabase.h"

/// <summary>
/// 盤面のトレイト(シナジー)構成を集計し、該当ユニットへ効果を適用するクラス。
/// </summary>
class TraitSystem
{
public:
	/// <summary>
	/// boardのトレイト構成を集計し、各ユニットのbonusAttack/bonusMaxHPを再計算する。
	/// 併せてcurrentHPを(baseHP + bonusMaxHP)まで全回復させる。
	/// トレイト構成は戦闘開始時点で固定するため、戦闘直前(SimulateCombatの前)に呼ぶことを想定している。
	/// </summary>
	void ApplyTraitBonuses(std::vector<UnitInstance>& board, const TraitDatabase& traitDatabase, const std::string& ownerName)
	{
		// この関数は戦闘直前(=毎ラウンド開始時)に1回だけ呼ばれ、その中でHPも全回復させる。
		// つまりboard上のユニットは前回の戦闘結果に関わらず全員が今回の参加メンバーなので、
		// currentHPで生死を判定せず、boardにいる全員をトレイト集計の対象にする。
		std::map<TraitType, int> traitCounts;
		for (const auto& unit : board)
		{
			for (TraitType trait : unit.def->traits)
			{
				traitCounts[trait]++;
			}
		}

		for (auto& unit : board)
		{
			unit.bonusAttack = 0;
			unit.bonusMagicPower = 0;
			unit.bonusMaxHP = 0;
			unit.bonusPhysicalDefense = 0;
			unit.bonusMagicDefense = 0;
			unit.bonusSkillThreshold = 0;
			unit.bonusAttackSpeed = 0.0f;
			unit.shieldAmount = 0; // シールドは戦闘ごとにリセットする(前回の戦闘の残りを持ち越さない)。

			for (TraitType trait : unit.def->traits)
			{
				const TraitDef* traitDef = traitDatabase.FindTraitDef(trait);
				if (traitDef == nullptr) continue;

				const TraitTier* activeTier = FindActiveTier(*traitDef, traitCounts[trait]);
				if (activeTier == nullptr) continue;

				for (const auto& effect : activeTier->effects)
				{
					ApplyEffect(unit, effect);
				}
			}

			unit.currentHP = unit.def->baseHP + unit.bonusMaxHP;
		}

		LogActiveTraits(traitCounts, traitDatabase, ownerName);
	}

private:
	/// <summary>
	/// countが満たす最高段階のTraitTierを返す(どの段階も満たさなければnullptr)。
	/// tiersはrequiredCount昇順の登録を前提とする。
	/// </summary>
	const TraitTier* FindActiveTier(const TraitDef& traitDef, int count)
	{
		const TraitTier* result = nullptr;
		for (const auto& tier : traitDef.tiers)
		{
			if (count >= tier.requiredCount)
			{
				result = &tier;
			}
		}
		return result;
	}

	void ApplyEffect(UnitInstance& unit, const StatEffect& effect)
	{
		switch (effect.stat)
		{
		case StatEffectType::AttackFlat:
			unit.bonusAttack += (int)effect.value;
			break;
		case StatEffectType::AttackPercent:
			unit.bonusAttack += (int)(unit.def->baseAttack * effect.value / 100.0f);
			break;
		case StatEffectType::MagicPowerFlat:
			unit.bonusMagicPower += (int)effect.value;
			break;
		case StatEffectType::MagicPowerPercent:
			unit.bonusMagicPower += (int)(unit.def->magicPower * effect.value / 100.0f);
			break;
		case StatEffectType::MaxHPFlat:
			unit.bonusMaxHP += (int)effect.value;
			break;
		case StatEffectType::MaxHPPercent:
			unit.bonusMaxHP += (int)(unit.def->baseHP * effect.value / 100.0f);
			break;
		case StatEffectType::PhysicalDefenseFlat:
			unit.bonusPhysicalDefense += (int)effect.value;
			break;
		case StatEffectType::PhysicalDefensePercent:
			unit.bonusPhysicalDefense += (int)(unit.def->physicalDefense * effect.value / 100.0f);
			break;
		case StatEffectType::MagicDefenseFlat:
			unit.bonusMagicDefense += (int)effect.value;
			break;
		case StatEffectType::MagicDefensePercent:
			unit.bonusMagicDefense += (int)(unit.def->magicDefense * effect.value / 100.0f);
			break;
		case StatEffectType::SkillThresholdFlat:
			unit.bonusSkillThreshold += (int)effect.value;
			break;
		case StatEffectType::AttackSpeedFlat:
			unit.bonusAttackSpeed += effect.value;
			break;
		case StatEffectType::AttackSpeedPercent:
			unit.bonusAttackSpeed += unit.def->attackSpeed * effect.value / 100.0f;
			break;
		}
	}

	void LogActiveTraits(const std::map<TraitType, int>& traitCounts, const TraitDatabase& traitDatabase, const std::string& ownerName)
	{
		for (const auto& pair : traitCounts)
		{
			const TraitDef* traitDef = traitDatabase.FindTraitDef(pair.first);
			if (traitDef == nullptr) continue;

			const TraitTier* activeTier = FindActiveTier(*traitDef, pair.second);

			wchar_t buf[256];
			swprintf_s(buf, L"[%hs] Trait %hs: %d (%hs)\n",
				ownerName.c_str(), traitDef->name.c_str(), pair.second,
				activeTier != nullptr ? "active" : "inactive");
			OutputDebugString(buf);
		}
	}
};

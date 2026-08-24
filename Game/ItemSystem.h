#pragma once
#include <string>
#include <vector>
#include "UnitInstance.h"
#include "ItemDatabase.h"

/// <summary>
/// ユニットへのアイテム付与(素材の自動合成を含む)と、アイテム効果の適用を行うクラス。
/// </summary>
class ItemSystem
{
public:
	static const int kMaxItemSlots = 3; // TFT同様、1体につき最大3つまで。

	/// <summary>
	/// unitにアイテムを持たせる。渡すアイテムが素材で、unitが既に持っている素材との間に
	/// レシピが存在すれば、その2つを1つの完成アイテムへ自動的に合成する(スロットは1つ減る)。
	/// 合成が成立せず、かつスロットが満杯の場合は何もせずfalseを返す。
	/// </summary>
	bool GiveItem(UnitInstance& unit, const ItemDef* newItem, const ItemDatabase& itemDatabase, const std::string& ownerName)
	{
		if (newItem == nullptr) return false;

		if (newItem->category == ItemCategory::Component)
		{
			for (size_t i = 0; i < unit.items.size(); ++i)
			{
				if (unit.items[i]->category != ItemCategory::Component) continue;

				const ItemRecipe* recipe = itemDatabase.FindRecipe(unit.items[i]->name, newItem->name);
				if (recipe == nullptr) continue;

				const ItemDef* result = itemDatabase.FindItemDefByName(recipe->resultName);
				if (result == nullptr) continue;

				wchar_t buf[256];
				swprintf_s(buf, L"[%hs] %hs combines %hs + %hs -> %hs!\n",
					ownerName.c_str(), unit.def->name.c_str(),
					unit.items[i]->name.c_str(), newItem->name.c_str(), result->name.c_str());
				OutputDebugString(buf);

				unit.items[i] = result; // 素材2つ分のスロットを、完成アイテム1つに置き換える。
				return true;
			}
		}

		if ((int)unit.items.size() >= kMaxItemSlots)
		{
			return false; // アイテム欄が満杯で、合成も成立しなかった。
		}

		unit.items.push_back(newItem);
		return true;
	}

	/// <summary>
	/// boardの各ユニットが持つアイテムの効果を集計し、bonusAttack/bonusMaxHPに加算する。
	/// TraitSystem::ApplyTraitBonusesの後に呼ぶことを想定している(トレイト分に上乗せする)。
	/// 併せてcurrentHPを(baseHP + bonusMaxHP)まで全回復させる。
	/// </summary>
	void ApplyItemBonuses(std::vector<UnitInstance>& board, const std::string& ownerName)
	{
		for (auto& unit : board)
		{
			for (const ItemDef* item : unit.items)
			{
				for (const auto& effect : item->effects)
				{
					ApplyEffect(unit, effect);
				}
			}

			unit.currentHP = unit.def->baseHP + unit.bonusMaxHP;

			if (!unit.items.empty())
			{
				LogEquippedItems(unit, ownerName);
			}
		}
	}

private:
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

	void LogEquippedItems(const UnitInstance& unit, const std::string& ownerName)
	{
		for (const ItemDef* item : unit.items)
		{
			wchar_t buf[256];
			swprintf_s(buf, L"[%hs] %hs equips %hs\n", ownerName.c_str(), unit.def->name.c_str(), item->name.c_str());
			OutputDebugString(buf);
		}
	}
};

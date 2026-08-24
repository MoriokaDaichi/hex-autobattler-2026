#pragma once
#include <string>
#include <vector>
#include "UnitInstance.h"

/// <summary>
/// スターレベル(★2, ★3)によるステータス倍率を適用するクラス。
/// TFT同様、★1→★2で約1.8倍、★2→★3でさらに1.8倍(★1比で約3.24倍)になるようにしている。
/// </summary>
class StarLevelSystem
{
public:
	static constexpr float kStarMultiplierStep = 1.8f;

	/// <summary>
	/// boardの各ユニットについて、スターレベルに応じたステータス倍率分をbonus系フィールドに加算する。
	/// TraitSystem::ApplyTraitBonuses/ItemSystem::ApplyItemBonusesの後に呼ぶことを想定している
	/// (それらのボーナスに上乗せする)。併せてcurrentHPも最終的な最大値まで再度全回復させる。
	/// </summary>
	void ApplyStarBonuses(std::vector<UnitInstance>& board, const std::string& ownerName)
	{
		for (auto& unit : board)
		{
			if (unit.starLevel <= 1) continue;

			float bonusMultiplier = GetStarMultiplier(unit.starLevel) - 1.0f; // 基礎値からの上乗せ分。

			unit.bonusAttack += (int)(unit.def->baseAttack * bonusMultiplier);
			unit.bonusMagicPower += (int)(unit.def->magicPower * bonusMultiplier);
			unit.bonusMaxHP += (int)(unit.def->baseHP * bonusMultiplier);
			unit.bonusPhysicalDefense += (int)(unit.def->physicalDefense * bonusMultiplier);
			unit.bonusMagicDefense += (int)(unit.def->magicDefense * bonusMultiplier);

			unit.currentHP = unit.def->baseHP + unit.bonusMaxHP;

			wchar_t buf[256];
			swprintf_s(buf, L"[%hs] %hs is %d-star!\n", ownerName.c_str(), unit.def->name.c_str(), unit.starLevel);
			OutputDebugString(buf);
		}
	}

	/// <summary>
	/// starLevelに応じた累積倍率を返す(★1なら1.0倍)。
	/// </summary>
	static float GetStarMultiplier(int starLevel)
	{
		float multiplier = 1.0f;
		for (int i = 1; i < starLevel; ++i)
		{
			multiplier *= kStarMultiplierStep;
		}
		return multiplier;
	}
};

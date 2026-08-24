#pragma once
#include <vector>
#include <random>
#include "UnitDatabase.h"

/// <summary>
/// ショップの抽選ロジック。プレイヤーのレベルに応じて、コスト帯(1〜5)ごとの出現率が変わる
/// (レベルが高いほど高コストのユニットが出やすくなる、TFT風の抽選テーブル)。
/// </summary>
class ShopSystem
{
public:
	static const int kMinUnitCost = 1;
	static const int kMaxUnitCost = 5;

	/// <summary>
	/// 指定したUnitDatabaseから、levelに応じたコスト帯の出現率で、ランダムに5枠分のユニットを選ぶ。
	/// </summary>
	std::vector<const UnitDef*> RollShop(const UnitDatabase& database, int level)
	{
		std::vector<const UnitDef*> result;
		const auto& allDefs = database.GetAllUnitDefs();

		if (allDefs.empty())
		{
			return result; // データが無い場合は空を返す。
		}

		// コスト帯(1〜5)ごとに、そのコストのユニット定義一覧を集めておく。
		std::vector<const UnitDef*> byCost[kMaxUnitCost + 1]; // index 0は未使用。
		for (const auto& def : allDefs)
		{
			if (def.cost >= kMinUnitCost && def.cost <= kMaxUnitCost)
			{
				byCost[def.cost].push_back(&def);
			}
		}

		const int* odds = GetCostOdds(level);

		std::random_device rd;
		std::mt19937 rng(rd());

		const int shopSlotCount = 5;
		for (int i = 0; i < shopSlotCount; ++i)
		{
			int cost = PickCostTier(odds, byCost, rng);
			const std::vector<const UnitDef*>& pool = byCost[cost];

			std::uniform_int_distribution<size_t> unitDist(0, pool.size() - 1);
			result.push_back(pool[unitDist(rng)]);
		}

		return result;
	}

private:
	/// <summary>
	/// レベル(1〜9)に応じた、コスト1〜5ユニットの出現率テーブル(%、各行合計100)を返す。
	/// レベルが上がるほど高コスト帯の出現率が上がっていく。
	/// </summary>
	const int* GetCostOdds(int level) const
	{
		static const int kOddsTable[9][5] =
		{
			// cost:   1    2    3    4    5
			/*Lv1*/ { 100,   0,   0,   0,   0 },
			/*Lv2*/ { 100,   0,   0,   0,   0 },
			/*Lv3*/ {  75,  25,   0,   0,   0 },
			/*Lv4*/ {  55,  30,  15,   0,   0 },
			/*Lv5*/ {  45,  33,  20,   2,   0 },
			/*Lv6*/ {  30,  35,  25,   8,   2 },
			/*Lv7*/ {  19,  30,  35,  14,   2 },
			/*Lv8*/ {  17,  24,  32,  24,   3 },
			/*Lv9*/ {  15,  20,  25,  30,  10 },
		};

		int index = level - 1;
		if (index < 0) index = 0;
		if (index > 8) index = 8;
		return kOddsTable[index];
	}

	/// <summary>
	/// oddsの重みに従って、コスト帯(1〜5)を1つ抽選する。そのコスト帯に該当するユニットが
	/// 1体も存在しない(byCost[cost]が空)場合は、その帯の重みを除外して抽選し直す。
	/// </summary>
	int PickCostTier(const int* odds, const std::vector<const UnitDef*>* byCost, std::mt19937& rng) const
	{
		int totalWeight = 0;
		for (int cost = kMinUnitCost; cost <= kMaxUnitCost; ++cost)
		{
			if (!byCost[cost].empty()) totalWeight += odds[cost - 1];
		}

		if (totalWeight <= 0)
		{
			// 念のためのフォールバック: 出現率が全て0でも、ユニットが存在する最小コスト帯を返す。
			for (int cost = kMinUnitCost; cost <= kMaxUnitCost; ++cost)
			{
				if (!byCost[cost].empty()) return cost;
			}
			return kMinUnitCost;
		}

		std::uniform_int_distribution<int> pick(1, totalWeight);
		int roll = pick(rng);

		int cumulative = 0;
		for (int cost = kMinUnitCost; cost <= kMaxUnitCost; ++cost)
		{
			if (byCost[cost].empty()) continue;

			cumulative += odds[cost - 1];
			if (roll <= cumulative) return cost;
		}

		return kMaxUnitCost; // 到達しないはずだが念のため。
	}
};

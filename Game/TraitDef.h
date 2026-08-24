#pragma once
#include <string>
#include <vector>
#include "TraitType.h"
#include "StatEffect.h"

/// <summary>
/// トレイトの1段階(閾値)分の定義。
/// </summary>
struct TraitTier
{
	int requiredCount = 1; // このトレイトを持つ生存ユニットがこの数以上いれば発動する。
	std::vector<StatEffect> effects;
};

/// <summary>
/// トレイトの静的データ(マスターデータ)。
/// tiersはrequiredCountの昇順で登録する想定(最後に条件を満たした段階が適用される)。
/// </summary>
struct TraitDef
{
	TraitType type = TraitType::Monster;
	std::string name;
	std::vector<TraitTier> tiers;
};

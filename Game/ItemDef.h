#pragma once
#include <string>
#include <vector>
#include "StatEffect.h"

/// <summary>
/// アイテムの分類。
/// </summary>
enum class ItemCategory
{
	Component, // 素材アイテム。同じ/別の素材とあと1つ組み合わせると完成アイテムになる。
	Completed, // 完成アイテム。素材2つを合成して作る、より強力な付帯効果を持つアイテム。
};

/// <summary>
/// アイテムの静的データ(マスターデータ)。
/// </summary>
struct ItemDef
{
	std::string name;
	ItemCategory category = ItemCategory::Component;
	std::vector<StatEffect> effects; // 装備した対象ユニットに付与される効果。
};

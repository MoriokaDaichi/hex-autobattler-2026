#pragma once
#include <string>
#include <vector>
#include "ItemDef.h"

/// <summary>
/// 素材アイテム2つ(順不同、同名の重複可)から完成アイテムを作るレシピ。
/// </summary>
struct ItemRecipe
{
	std::string componentA;
	std::string componentB;
	std::string resultName;
};

/// <summary>
/// アイテムのマスターデータ一覧(素材・完成アイテム)と、合成レシピ。
/// </summary>
class ItemDatabase
{
public:
	void Init()
	{
		m_itemDefs.clear();
		m_recipes.clear();

		// --- 素材アイテム ---
		ItemDef power;
		power.name = "PowerCrystal"; // 力の結晶
		power.category = ItemCategory::Component;
		power.effects = { { StatEffectType::AttackFlat, 10.0f } };
		m_itemDefs.push_back(power);

		ItemDef vital;
		vital.name = "VitalCrystal"; // 生命の結晶
		vital.category = ItemCategory::Component;
		vital.effects = { { StatEffectType::MaxHPFlat, 150.0f } };
		m_itemDefs.push_back(vital);

		ItemDef guard;
		guard.name = "GuardCrystal"; // 守りの結晶
		guard.category = ItemCategory::Component;
		guard.effects = { { StatEffectType::MaxHPPercent, 20.0f } };
		m_itemDefs.push_back(guard);

		ItemDef iron;
		iron.name = "IronCrystal"; // 鉄の結晶
		iron.category = ItemCategory::Component;
		iron.effects = { { StatEffectType::PhysicalDefenseFlat, 15.0f } };
		m_itemDefs.push_back(iron);

		ItemDef wisdom;
		wisdom.name = "WisdomCrystal"; // 英知の結晶
		wisdom.category = ItemCategory::Component;
		wisdom.effects = { { StatEffectType::MagicPowerFlat, 10.0f } };
		m_itemDefs.push_back(wisdom);

		ItemDef swift;
		swift.name = "SwiftCrystal"; // 迅速の結晶
		swift.category = ItemCategory::Component;
		swift.effects = { { StatEffectType::AttackSpeedPercent, 10.0f } };
		m_itemDefs.push_back(swift);

		// --- 完成アイテム(素材2つの合成。単純な合算より強い付帯効果を持たせる) ---
		ItemDef greatBlade;
		greatBlade.name = "GreatBlade"; // 大剣(Power+Power)
		greatBlade.category = ItemCategory::Completed;
		greatBlade.effects = { { StatEffectType::AttackFlat, 25.0f } };
		m_itemDefs.push_back(greatBlade);

		ItemDef titansPlate;
		titansPlate.name = "TitansPlate"; // 巨人の鎧(Vital+Vital)
		titansPlate.category = ItemCategory::Completed;
		titansPlate.effects = { { StatEffectType::MaxHPFlat, 400.0f } };
		m_itemDefs.push_back(titansPlate);

		ItemDef bulwark;
		bulwark.name = "Bulwark"; // 要塞の盾(Guard+Guard)
		bulwark.category = ItemCategory::Completed;
		bulwark.effects = { { StatEffectType::MaxHPPercent, 50.0f } };
		m_itemDefs.push_back(bulwark);

		ItemDef berserkersAxe;
		berserkersAxe.name = "BerserkersAxe"; // 狂戦士の斧(Power+Vital)
		berserkersAxe.category = ItemCategory::Completed;
		berserkersAxe.effects = { { StatEffectType::AttackFlat, 20.0f }, { StatEffectType::MaxHPFlat, 150.0f } };
		m_itemDefs.push_back(berserkersAxe);

		ItemDef duelistsEdge;
		duelistsEdge.name = "DuelistsEdge"; // 決闘者の刃(Power+Guard)
		duelistsEdge.category = ItemCategory::Completed;
		duelistsEdge.effects = { { StatEffectType::AttackFlat, 20.0f }, { StatEffectType::MaxHPPercent, 25.0f } };
		m_itemDefs.push_back(duelistsEdge);

		ItemDef guardiansHeart;
		guardiansHeart.name = "GuardiansHeart"; // 守護者の心臓(Vital+Guard)
		guardiansHeart.category = ItemCategory::Completed;
		guardiansHeart.effects = { { StatEffectType::MaxHPFlat, 250.0f }, { StatEffectType::MaxHPPercent, 25.0f } };
		m_itemDefs.push_back(guardiansHeart);

		// IronCrystal(物理防御寄り)/WisdomCrystal(魔力寄り)を絡めた完成アイテム。
		// Knight/Paladinのような物理防御寄りのユニット、Priest/YoungDragonのような
		// 魔力寄りのユニットに似合う効果を狙って設計している。

		ItemDef aegis;
		aegis.name = "Aegis"; // 絶対防壁(Iron+Iron)
		aegis.category = ItemCategory::Completed;
		aegis.effects = { { StatEffectType::PhysicalDefenseFlat, 40.0f } };
		m_itemDefs.push_back(aegis);

		ItemDef archmageStaff;
		archmageStaff.name = "ArchmageStaff"; // 大魔導の杖(Wisdom+Wisdom)
		archmageStaff.category = ItemCategory::Completed;
		archmageStaff.effects = { { StatEffectType::MagicPowerFlat, 30.0f } };
		m_itemDefs.push_back(archmageStaff);

		ItemDef warplate;
		warplate.name = "Warplate"; // 闘士の鎧(Iron+Power)。物理アタッカー向け。
		warplate.category = ItemCategory::Completed;
		warplate.effects = { { StatEffectType::AttackFlat, 15.0f }, { StatEffectType::PhysicalDefenseFlat, 20.0f } };
		m_itemDefs.push_back(warplate);

		ItemDef ironWall;
		ironWall.name = "IronWall"; // 鉄壁の鎧(Iron+Vital)。Knight/Paladin向けの純タンク装備。
		ironWall.category = ItemCategory::Completed;
		ironWall.effects = { { StatEffectType::PhysicalDefenseFlat, 20.0f }, { StatEffectType::MaxHPFlat, 200.0f } };
		m_itemDefs.push_back(ironWall);

		ItemDef fortressShield;
		fortressShield.name = "FortressShield"; // 要塞の大盾(Iron+Guard)。Knight/Paladin向け。
		fortressShield.category = ItemCategory::Completed;
		fortressShield.effects = { { StatEffectType::PhysicalDefenseFlat, 25.0f }, { StatEffectType::MaxHPPercent, 30.0f } };
		m_itemDefs.push_back(fortressShield);

		ItemDef spellBlade;
		spellBlade.name = "SpellBlade"; // 魔剣(Wisdom+Power)。物理と魔法の複合アタッカー向け。
		spellBlade.category = ItemCategory::Completed;
		spellBlade.effects = { { StatEffectType::AttackFlat, 15.0f }, { StatEffectType::MagicPowerFlat, 15.0f } };
		m_itemDefs.push_back(spellBlade);

		ItemDef arcaneTome;
		arcaneTome.name = "ArcaneTome"; // 秘術の魔導書(Wisdom+Vital)。YoungDragonのような高火力魔法アタッカー向け。
		arcaneTome.category = ItemCategory::Completed;
		arcaneTome.effects = { { StatEffectType::MagicPowerFlat, 20.0f }, { StatEffectType::MaxHPFlat, 150.0f } };
		m_itemDefs.push_back(arcaneTome);

		ItemDef mysticRobe;
		mysticRobe.name = "MysticRobe"; // 秘術のローブ(Wisdom+Guard)。Priestのような魔法寄りの後衛向け。
		mysticRobe.category = ItemCategory::Completed;
		mysticRobe.effects = { { StatEffectType::MagicPowerFlat, 20.0f }, { StatEffectType::MagicDefenseFlat, 20.0f } };
		m_itemDefs.push_back(mysticRobe);

		ItemDef spiritGuard;
		spiritGuard.name = "SpiritGuard"; // 精霊の守り(Iron+Wisdom)。物理・魔法どちらの防御も底上げする汎用タンク装備。
		spiritGuard.category = ItemCategory::Completed;
		spiritGuard.effects = { { StatEffectType::PhysicalDefenseFlat, 15.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } };
		m_itemDefs.push_back(spiritGuard);

		ItemDef rapidBlade;
		rapidBlade.name = "RapidBlade"; // 迅速の刃(Swift+Swift)。攻撃速度に特化し、通常攻撃・必殺技どちらの
		                                 // 発動頻度も底上げする。
		rapidBlade.category = ItemCategory::Completed;
		rapidBlade.effects = { { StatEffectType::AttackSpeedPercent, 25.0f } };
		m_itemDefs.push_back(rapidBlade);

		// SwiftCrystal(攻撃速度)と他5種との複合アイテム。
		ItemDef furiousEdge;
		furiousEdge.name = "FuriousEdge"; // 激情の刃(Swift+Power)。攻撃力・攻撃速度ともに底上げする物理アタッカー向け。
		furiousEdge.category = ItemCategory::Completed;
		furiousEdge.effects = { { StatEffectType::AttackFlat, 15.0f }, { StatEffectType::AttackSpeedPercent, 20.0f } };
		// パッシブ: 通常攻撃を当てた敵へ火傷(継続ダメージ)を付与する。1刻み40ダメージ×3回、1.0秒間隔。
		// 火傷は防御・シールドを貫通する確定ダメージ(CombatEngine::TickBurnsForBoard)。
		furiousEdge.passives = { { PassiveEffectType::OnHitBurn, 40, 3, 1.0f } };
		m_itemDefs.push_back(furiousEdge);

		ItemDef tirelessHeart;
		tirelessHeart.name = "TirelessHeart"; // 不屈の心臓(Swift+Vital)。HPを保ちながら攻撃頻度を上げる、粘り強いアタッカー向け。
		tirelessHeart.category = ItemCategory::Completed;
		tirelessHeart.effects = { { StatEffectType::MaxHPFlat, 150.0f }, { StatEffectType::AttackSpeedPercent, 20.0f } };
		m_itemDefs.push_back(tirelessHeart);

		ItemDef vigorousBlade;
		vigorousBlade.name = "VigorousBlade"; // 活力の刃(Swift+Guard)。最大HP%と攻撃速度を両立する継戦向けデュエリスト装備。
		vigorousBlade.category = ItemCategory::Completed;
		vigorousBlade.effects = { { StatEffectType::MaxHPPercent, 25.0f }, { StatEffectType::AttackSpeedPercent, 20.0f } };
		m_itemDefs.push_back(vigorousBlade);

		ItemDef rapidBulwark;
		rapidBulwark.name = "RapidBulwark"; // 迅速なる防壁(Swift+Iron)。物理防御を保ちつつ手数で削る前衛アタッカー向け。
		rapidBulwark.category = ItemCategory::Completed;
		rapidBulwark.effects = { { StatEffectType::PhysicalDefenseFlat, 20.0f }, { StatEffectType::AttackSpeedPercent, 20.0f } };
		m_itemDefs.push_back(rapidBulwark);

		ItemDef arcaneHaste;
		arcaneHaste.name = "ArcaneHaste"; // 秘術の迅速(Swift+Wisdom)。魔力と攻撃速度を両立する、連射型の魔法アタッカー向け。
		arcaneHaste.category = ItemCategory::Completed;
		arcaneHaste.effects = { { StatEffectType::MagicPowerFlat, 15.0f }, { StatEffectType::AttackSpeedPercent, 20.0f } };
		m_itemDefs.push_back(arcaneHaste);

		// --- レシピ(素材6種の組み合わせ21通り全てに完成アイテムを割り当ててある) ---
		m_recipes.push_back({ "PowerCrystal", "PowerCrystal", "GreatBlade" });
		m_recipes.push_back({ "VitalCrystal", "VitalCrystal", "TitansPlate" });
		m_recipes.push_back({ "GuardCrystal", "GuardCrystal", "Bulwark" });
		m_recipes.push_back({ "PowerCrystal", "VitalCrystal", "BerserkersAxe" });
		m_recipes.push_back({ "PowerCrystal", "GuardCrystal", "DuelistsEdge" });
		m_recipes.push_back({ "VitalCrystal", "GuardCrystal", "GuardiansHeart" });
		m_recipes.push_back({ "IronCrystal", "IronCrystal", "Aegis" });
		m_recipes.push_back({ "WisdomCrystal", "WisdomCrystal", "ArchmageStaff" });
		m_recipes.push_back({ "IronCrystal", "PowerCrystal", "Warplate" });
		m_recipes.push_back({ "IronCrystal", "VitalCrystal", "IronWall" });
		m_recipes.push_back({ "IronCrystal", "GuardCrystal", "FortressShield" });
		m_recipes.push_back({ "WisdomCrystal", "PowerCrystal", "SpellBlade" });
		m_recipes.push_back({ "WisdomCrystal", "VitalCrystal", "ArcaneTome" });
		m_recipes.push_back({ "WisdomCrystal", "GuardCrystal", "MysticRobe" });
		m_recipes.push_back({ "IronCrystal", "WisdomCrystal", "SpiritGuard" });
		m_recipes.push_back({ "SwiftCrystal", "SwiftCrystal", "RapidBlade" });
		m_recipes.push_back({ "SwiftCrystal", "PowerCrystal", "FuriousEdge" });
		m_recipes.push_back({ "SwiftCrystal", "VitalCrystal", "TirelessHeart" });
		m_recipes.push_back({ "SwiftCrystal", "GuardCrystal", "VigorousBlade" });
		m_recipes.push_back({ "SwiftCrystal", "IronCrystal", "RapidBulwark" });
		m_recipes.push_back({ "SwiftCrystal", "WisdomCrystal", "ArcaneHaste" });
	}

	const std::vector<ItemDef>& GetAllItemDefs() const
	{
		return m_itemDefs;
	}

	const ItemDef* FindItemDefByName(const std::string& name) const
	{
		for (const auto& def : m_itemDefs)
		{
			if (def.name == name)
			{
				return &def;
			}
		}
		return nullptr;
	}

	/// <summary>
	/// 2つの素材アイテム名(順不同)に対応するレシピを探す。
	/// </summary>
	const ItemRecipe* FindRecipe(const std::string& nameA, const std::string& nameB) const
	{
		for (const auto& recipe : m_recipes)
		{
			bool matchesForward = (recipe.componentA == nameA && recipe.componentB == nameB);
			bool matchesReverse = (recipe.componentA == nameB && recipe.componentB == nameA);
			if (matchesForward || matchesReverse)
			{
				return &recipe;
			}
		}
		return nullptr;
	}

private:
	std::vector<ItemDef> m_itemDefs;
	std::vector<ItemRecipe> m_recipes;
};

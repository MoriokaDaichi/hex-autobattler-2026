#pragma once
#include <vector>
#include "TraitDef.h"

/// <summary>
/// トレイトのマスターデータ一覧。
/// </summary>
class TraitDatabase
{
public:
	void Init()
	{
		m_traitDefs.clear();

		// 魔物: 頭数で押す種族。数が揃うと攻撃力が伸びる。4体揃うと野生の魔力にも目覚める。
		TraitDef monster;
		monster.type = TraitType::Monster;
		monster.name = "Monster";
		monster.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 10.0f } } });
		monster.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 25.0f }, { StatEffectType::MagicPowerPercent, 15.0f } } });
		m_traitDefs.push_back(monster);

		// 人間: 連携が得意な種族。数が揃うと耐久力が伸びる。4体揃うと魔法への備えも固くなる。
		TraitDef human;
		human.type = TraitType::Human;
		human.name = "Human";
		human.tiers.push_back({ 2, { { StatEffectType::MaxHPPercent, 15.0f } } });
		human.tiers.push_back({ 4, { { StatEffectType::MaxHPPercent, 35.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		m_traitDefs.push_back(human);

		// 英雄: 希少で、1体でも強い。2体揃うとさらに強力になり、必殺技も早く撃てるようになる。
		TraitDef hero;
		hero.type = TraitType::Hero;
		hero.name = "Hero";
		hero.tiers.push_back({ 1, { { StatEffectType::AttackFlat, 5.0f }, { StatEffectType::MaxHPFlat, 20.0f } } });
		hero.tiers.push_back({ 2, { { StatEffectType::AttackFlat, 15.0f }, { StatEffectType::MaxHPFlat, 50.0f }, { StatEffectType::SkillThresholdFlat, -1.0f } } });
		m_traitDefs.push_back(hero);

		// --- ここから役割(クラス)系トレイト。出身系トレイトと組み合わせて持つユニットもいる。 ---

		// 戦士: 前線で殴り合う近接職。数が揃うと攻撃力が伸び、頑丈にもなる。
		TraitDef warrior;
		warrior.type = TraitType::Warrior;
		warrior.name = "Warrior";
		warrior.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 15.0f } } });
		warrior.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 30.0f }, { StatEffectType::PhysicalDefenseFlat, 15.0f } } });
		m_traitDefs.push_back(warrior);

		// 魔道士: 魔力を操る職。数が揃うと魔力が伸び、魔法への耐性も付く。
		TraitDef mage;
		mage.type = TraitType::Mage;
		mage.name = "Mage";
		mage.tiers.push_back({ 2, { { StatEffectType::MagicPowerPercent, 20.0f } } });
		mage.tiers.push_back({ 4, { { StatEffectType::MagicPowerPercent, 40.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		m_traitDefs.push_back(mage);

		// 守護者: 味方を守る盾役。数が揃うと物理・魔法どちらの防御力も伸びる。
		TraitDef guardian;
		guardian.type = TraitType::Guardian;
		guardian.name = "Guardian";
		guardian.tiers.push_back({ 2, { { StatEffectType::PhysicalDefenseFlat, 15.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		guardian.tiers.push_back({ 4, { { StatEffectType::PhysicalDefenseFlat, 35.0f }, { StatEffectType::MagicDefenseFlat, 35.0f }, { StatEffectType::MaxHPPercent, 20.0f } } });
		m_traitDefs.push_back(guardian);

		// 暗殺者: 少数精鋭で必殺技を連発する。数が揃うと必殺技がより早く撃てるようになる。
		TraitDef assassin;
		assassin.type = TraitType::Assassin;
		assassin.name = "Assassin";
		assassin.tiers.push_back({ 2, { { StatEffectType::SkillThresholdFlat, -1.0f } } });
		assassin.tiers.push_back({ 4, { { StatEffectType::SkillThresholdFlat, -2.0f }, { StatEffectType::AttackPercent, 20.0f } } });
		m_traitDefs.push_back(assassin);

		// 狩人: 遠距離攻撃を得意とする。数が揃うと攻撃力が伸び、必殺技の準備も早まる。
		TraitDef ranger;
		ranger.type = TraitType::Ranger;
		ranger.name = "Ranger";
		ranger.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 15.0f } } });
		ranger.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 30.0f }, { StatEffectType::SkillThresholdFlat, -1.0f } } });
		m_traitDefs.push_back(ranger);
	}

	const std::vector<TraitDef>& GetAllTraitDefs() const
	{
		return m_traitDefs;
	}

	const TraitDef* FindTraitDef(TraitType type) const
	{
		for (const auto& def : m_traitDefs)
		{
			if (def.type == type)
			{
				return &def;
			}
		}
		return nullptr;
	}

private:
	std::vector<TraitDef> m_traitDefs;
};

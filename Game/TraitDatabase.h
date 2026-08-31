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

		// 魔物: 頭数で押す種族。数が揃うと攻撃力が伸びる。4体で魔力にも目覚め、6体で暴走域に入る。
		TraitDef monster;
		monster.type = TraitType::Monster;
		monster.name = "Monster";
		monster.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 10.0f } } });
		monster.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 25.0f }, { StatEffectType::MagicPowerPercent, 15.0f } } });
		monster.tiers.push_back({ 6, { { StatEffectType::AttackPercent, 45.0f }, { StatEffectType::MagicPowerPercent, 30.0f } } });
		m_traitDefs.push_back(monster);

		// 人間: 連携が得意な種族。数が揃うと耐久力が伸びる。4体で魔法にも固くなり、6体で鉄壁になる。
		TraitDef human;
		human.type = TraitType::Human;
		human.name = "Human";
		human.tiers.push_back({ 2, { { StatEffectType::MaxHPPercent, 15.0f } } });
		human.tiers.push_back({ 4, { { StatEffectType::MaxHPPercent, 35.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		human.tiers.push_back({ 6, { { StatEffectType::MaxHPPercent, 60.0f }, { StatEffectType::MagicDefenseFlat, 30.0f } } });
		m_traitDefs.push_back(human);

		// 英雄: 希少で、1体でも強い。2体でさらに強力に、6体そろえば戦場を支配する。
		TraitDef hero;
		hero.type = TraitType::Hero;
		hero.name = "Hero";
		hero.tiers.push_back({ 1, { { StatEffectType::AttackFlat, 5.0f }, { StatEffectType::MaxHPFlat, 20.0f } } });
		hero.tiers.push_back({ 2, { { StatEffectType::AttackFlat, 15.0f }, { StatEffectType::MaxHPFlat, 50.0f }, { StatEffectType::SkillThresholdFlat, -1.0f } } });
		hero.tiers.push_back({ 6, { { StatEffectType::AttackFlat, 30.0f }, { StatEffectType::MaxHPFlat, 100.0f }, { StatEffectType::SkillThresholdFlat, -2.0f } } });
		m_traitDefs.push_back(hero);

		// --- ここから役割(クラス)系トレイト。出身系トレイトと組み合わせて持つユニットもいる。 ---

		// 戦士: 前線で殴り合う近接職。数が揃うと攻撃力が伸び、頑丈にもなる。6体で圧倒的な破壊力。
		TraitDef warrior;
		warrior.type = TraitType::Warrior;
		warrior.name = "Warrior";
		warrior.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 15.0f } } });
		warrior.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 30.0f }, { StatEffectType::PhysicalDefenseFlat, 15.0f } } });
		warrior.tiers.push_back({ 6, { { StatEffectType::AttackPercent, 50.0f }, { StatEffectType::PhysicalDefenseFlat, 30.0f } } });
		m_traitDefs.push_back(warrior);

		// 魔道士: 魔力を操る職。数が揃うと魔力が伸び、魔法への耐性も付く。6体で魔力が爆発的に高まる。
		TraitDef mage;
		mage.type = TraitType::Mage;
		mage.name = "Mage";
		mage.tiers.push_back({ 2, { { StatEffectType::MagicPowerPercent, 20.0f } } });
		mage.tiers.push_back({ 4, { { StatEffectType::MagicPowerPercent, 40.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		mage.tiers.push_back({ 6, { { StatEffectType::MagicPowerPercent, 65.0f }, { StatEffectType::MagicDefenseFlat, 30.0f } } });
		m_traitDefs.push_back(mage);

		// 守護者: 味方を守る盾役。数が揃うと物理・魔法どちらの防御力も伸びる。6体でほぼ不落。
		TraitDef guardian;
		guardian.type = TraitType::Guardian;
		guardian.name = "Guardian";
		guardian.tiers.push_back({ 2, { { StatEffectType::PhysicalDefenseFlat, 15.0f }, { StatEffectType::MagicDefenseFlat, 15.0f } } });
		guardian.tiers.push_back({ 4, { { StatEffectType::PhysicalDefenseFlat, 35.0f }, { StatEffectType::MagicDefenseFlat, 35.0f }, { StatEffectType::MaxHPPercent, 20.0f } } });
		guardian.tiers.push_back({ 6, { { StatEffectType::PhysicalDefenseFlat, 55.0f }, { StatEffectType::MagicDefenseFlat, 55.0f }, { StatEffectType::MaxHPPercent, 35.0f } } });
		m_traitDefs.push_back(guardian);

		// 暗殺者: 少数精鋭で必殺技を連発する。数が揃うと必殺技がより早く撃てるようになる。6体で技を撃ち続ける。
		TraitDef assassin;
		assassin.type = TraitType::Assassin;
		assassin.name = "Assassin";
		assassin.tiers.push_back({ 2, { { StatEffectType::SkillThresholdFlat, -1.0f } } });
		assassin.tiers.push_back({ 4, { { StatEffectType::SkillThresholdFlat, -2.0f }, { StatEffectType::AttackPercent, 20.0f } } });
		assassin.tiers.push_back({ 6, { { StatEffectType::SkillThresholdFlat, -3.0f }, { StatEffectType::AttackPercent, 40.0f } } });
		m_traitDefs.push_back(assassin);

		// 狩人: 遠距離攻撃を得意とする。数が揃うと攻撃力が伸び、必殺技の準備も早まる。6体で手数と火力が跳ね上がる。
		TraitDef ranger;
		ranger.type = TraitType::Ranger;
		ranger.name = "Ranger";
		ranger.tiers.push_back({ 2, { { StatEffectType::AttackPercent, 15.0f } } });
		ranger.tiers.push_back({ 4, { { StatEffectType::AttackPercent, 30.0f }, { StatEffectType::SkillThresholdFlat, -1.0f } } });
		ranger.tiers.push_back({ 6, { { StatEffectType::AttackPercent, 50.0f }, { StatEffectType::SkillThresholdFlat, -2.0f } } });
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

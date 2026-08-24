#pragma once
#include <vector>
#include "UnitDef.h"

class UnitDatabase
{
public:
	void Init()
	{
		m_unitDefs.clear();

		// name, cost, HP, ATK, skillThreshold, skillType, attackRange, skillRange, moveSpeed
		// magicPower(魔力)/physicalDefense/magicDefense/traitsは末尾で個別に設定する。
		// magicPowerには、以前この位置にあったskillPower(必殺技の固定ダメージ量)と同じ値をそのまま設定し、
		// 数値バランスは変えていない(必殺技のダメージ計算がmagicPower基準に変わったのみ)。

		UnitDef slime{ "Slime", 1, 100, 10, 3, SkillEffectType::Damage, 1, 1, 1 }; // 近接、足も遅い
		slime.magicPower = 25;
		slime.physicalDefense = 5;
		slime.magicDefense = 5;
		slime.traits = { TraitType::Monster, TraitType::Warrior };
		slime.attackSpeed = 0.8f; // 素早さに欠ける下級モンスター。
		m_unitDefs.push_back(slime);

		UnitDef goblin{ "Goblin", 1, 90, 14, 4, SkillEffectType::Damage, 1, 1, 2 }; // 近接だが素早く距離を詰める
		goblin.magicPower = 30;
		goblin.physicalDefense = 8;
		goblin.magicDefense = 3;
		goblin.traits = { TraitType::Monster, TraitType::Assassin };
		goblin.attackSpeed = 1.1f; // アサシンらしく手数も多い。
		m_unitDefs.push_back(goblin);

		UnitDef orcBerserker{ "OrcBerserker", 3, 160, 22, 4, SkillEffectType::DamageAndHeal, 1, 1, 2 }; // 魔物の英雄。頑丈で足も速い
		orcBerserker.magicPower = 50;
		orcBerserker.physicalDefense = 20;
		orcBerserker.magicDefense = 10;
		orcBerserker.skillHealPercent = 40.0f; // 与えたダメージの40%を自分のHPとして回復する(狂戦士の血の渇き)。
		orcBerserker.traits = { TraitType::Monster, TraitType::Hero, TraitType::Warrior };
		orcBerserker.attackSpeed = 1.1f; // 狂戦士らしく攻撃の手数が多い。
		m_unitDefs.push_back(orcBerserker);

		UnitDef archer{ "Archer", 1, 70, 18, 5, SkillEffectType::Damage, 3, 3, 1 }; // 遠距離から攻撃できる
		archer.magicPower = 35;
		archer.physicalDefense = 5;
		archer.magicDefense = 8;
		archer.traits = { TraitType::Human, TraitType::Ranger };
		archer.attackSpeed = 1.0f; // レンジャーの標準的な連射速度。
		m_unitDefs.push_back(archer);

		UnitDef swordsman{ "Swordsman", 2, 110, 16, 4, SkillEffectType::Damage, 1, 1, 1 }; // 標準的な近接人間
		swordsman.magicPower = 35;
		swordsman.physicalDefense = 15;
		swordsman.magicDefense = 5;
		swordsman.traits = { TraitType::Human, TraitType::Warrior };
		swordsman.attackSpeed = 1.0f; // 全ユニットの基準となる標準速度。
		m_unitDefs.push_back(swordsman);

		UnitDef priest{ "Priest", 2, 80, 12, 3, SkillEffectType::DamageAndHeal, 2, 2, 1 }; // 中距離の魔法職。祈りの力で自分を癒やす
		priest.magicPower = 28;
		priest.physicalDefense = 5;
		priest.magicDefense = 15;
		priest.skillHealPercent = 50.0f; // 与えたダメージの50%を自分のHPとして回復する。
		priest.traits = { TraitType::Human, TraitType::Mage };
		priest.attackSpeed = 0.8f; // 詠唱系のメイジらしく、通常攻撃の手数は控えめ。
		m_unitDefs.push_back(priest);

		UnitDef paladin{ "Paladin", 3, 150, 18, 4, SkillEffectType::DamageAndShield, 1, 1, 1 }; // 人間の英雄。頑丈な前衛
		paladin.magicPower = 45;
		paladin.physicalDefense = 25;
		paladin.magicDefense = 20;
		paladin.skillShieldAmount = 60; // 攻撃後、聖なる加護で自分にシールドを付与する。
		paladin.traits = { TraitType::Human, TraitType::Hero, TraitType::Guardian };
		paladin.attackSpeed = 0.8f; // ガーディアンらしく一撃は重いが手数は少なめ。
		m_unitDefs.push_back(paladin);

		UnitDef youngDragon{ "YoungDragon", 4, 200, 26, 5, SkillEffectType::AreaDamage, 2, 2, 1 }; // 魔物の英雄。ブレスで周囲を巻き込む
		youngDragon.magicPower = 60;
		youngDragon.physicalDefense = 20;
		youngDragon.magicDefense = 25;
		youngDragon.skillSplashRadius = 1;
		youngDragon.skillSplashPercent = 50.0f; // 対象の周囲1マス以内の敵にも50%のダメージ。
		youngDragon.traits = { TraitType::Monster, TraitType::Hero, TraitType::Mage };
		youngDragon.attackSpeed = 0.7f; // 巨躯のドラゴンらしく、一撃は重いが動きはゆったり。
		m_unitDefs.push_back(youngDragon);

		UnitDef knight{ "Knight", 1, 120, 12, 4, SkillEffectType::DamageAndShield, 1, 1, 1 }; // 安価で硬い前衛
		knight.magicPower = 25;
		knight.physicalDefense = 25;
		knight.magicDefense = 10;
		knight.skillShieldAmount = 25; // 安価なユニットなのでシールド量は控えめ。
		knight.traits = { TraitType::Human, TraitType::Guardian };
		knight.attackSpeed = 0.8f; // 硬いガーディアン。手数より耐久重視。
		m_unitDefs.push_back(knight);

		UnitDef chimeraLord{ "ChimeraLord", 5, 220, 28, 5, SkillEffectType::AreaDamage, 1, 2, 2 }; // 魔物にして人間にして英雄という特異な存在
		chimeraLord.magicPower = 70;
		chimeraLord.physicalDefense = 30;
		chimeraLord.magicDefense = 30;
		chimeraLord.skillSplashRadius = 2; // 最上位ユニットらしく、巻き込む範囲も割合も最大級。
		chimeraLord.skillSplashPercent = 60.0f;
		chimeraLord.traits = { TraitType::Monster, TraitType::Human, TraitType::Hero, TraitType::Warrior };
		chimeraLord.attackSpeed = 0.8f; // 最上位ユニットらしく一撃が重く、手数はやや控えめ。
		m_unitDefs.push_back(chimeraLord);

		// --- ここから新規ユニット。役割(クラス)トレイトの人数を増やすために追加する。 ---

		UnitDef cultist{ "Cultist", 2, 75, 8, 4, SkillEffectType::AreaDamage, 2, 2, 1 }; // 通常攻撃も必殺技も魔法で行う純魔法職
		cultist.magicPower = 20;
		cultist.physicalDefense = 5;
		cultist.magicDefense = 10;
		cultist.attackType = AttackType::Magic; // 通常攻撃も魔力ベースの魔法弾になる。
		cultist.skillSplashRadius = 1;
		cultist.skillSplashPercent = 40.0f; // 安価なユニットなので巻き込み割合は控えめ。
		cultist.traits = { TraitType::Human, TraitType::Mage };
		cultist.attackSpeed = 0.8f; // 純魔法職らしく、詠唱に時間がかかり手数は少なめ。
		m_unitDefs.push_back(cultist);

		UnitDef direwolf{ "Direwolf", 2, 85, 16, 3, SkillEffectType::DamageAndHeal, 1, 1, 3 }; // 魔物の暗殺者。俊足で距離を詰める
		direwolf.magicPower = 20;
		direwolf.physicalDefense = 6;
		direwolf.magicDefense = 4;
		direwolf.skillHealPercent = 35.0f; // 獲物に噛みつき、与えたダメージの35%を吸収する。
		direwolf.traits = { TraitType::Monster, TraitType::Assassin };
		direwolf.attackSpeed = 1.2f; // 俊足の暗殺者らしく、手数も多い。
		m_unitDefs.push_back(direwolf);

		UnitDef griffin{ "Griffin", 3, 100, 20, 4, SkillEffectType::Damage, 3, 3, 2 }; // 空を飛ぶ魔物の狩人
		griffin.magicPower = 25;
		griffin.physicalDefense = 10;
		griffin.magicDefense = 12;
		griffin.traits = { TraitType::Monster, TraitType::Ranger };
		griffin.attackSpeed = 1.0f; // 飛行する狩人。レンジャーとして標準的な速さ。
		m_unitDefs.push_back(griffin);

		UnitDef shadowStalker{ "ShadowStalker", 2, 80, 17, 3, SkillEffectType::Damage, 1, 1, 2 }; // 人間の暗殺者
		shadowStalker.magicPower = 22;
		shadowStalker.physicalDefense = 8;
		shadowStalker.magicDefense = 6;
		shadowStalker.traits = { TraitType::Human, TraitType::Assassin };
		shadowStalker.attackSpeed = 1.2f; // 暗殺者らしい素早い連撃。
		m_unitDefs.push_back(shadowStalker);

		UnitDef behemoth{ "Behemoth", 3, 180, 14, 4, SkillEffectType::DamageAndHeal, 1, 1, 1 }; // 魔物の守護者。非常に硬い
		behemoth.magicPower = 15;
		behemoth.physicalDefense = 30;
		behemoth.magicDefense = 20;
		behemoth.skillHealPercent = 60.0f; // 与えたダメージの60%を自分のHPとして回復する、屈指の粘り強さ。
		behemoth.traits = { TraitType::Monster, TraitType::Guardian };
		behemoth.attackSpeed = 0.7f; // 屈指の巨体を誇るガーディアン。動きは鈍重。
		m_unitDefs.push_back(behemoth);

		UnitDef warlord{ "Warlord", 4, 160, 24, 4, SkillEffectType::DamageAndShield, 1, 1, 1 }; // 人間の英雄戦士。攻守に優れる
		warlord.magicPower = 20;
		warlord.physicalDefense = 22;
		warlord.magicDefense = 12;
		warlord.skillShieldAmount = 70; // 高コストの英雄らしく、大きめのシールドを自分に付与する。
		warlord.traits = { TraitType::Human, TraitType::Hero, TraitType::Warrior };
		warlord.attackSpeed = 0.9f; // 攻守に優れる英雄戦士。標準よりやや手数控えめだが安定。
		m_unitDefs.push_back(warlord);

		UnitDef nightBlade{ "NightBlade", 4, 140, 26, 3, SkillEffectType::DamageAndHeal, 1, 1, 3 }; // 人間の英雄暗殺者。最速で必殺技を撃つ
		nightBlade.magicPower = 20;
		nightBlade.physicalDefense = 15;
		nightBlade.magicDefense = 15;
		nightBlade.skillHealPercent = 45.0f; // 血を吸う高コストの暗殺者。必殺技を連発しながら自分も回復する。
		nightBlade.traits = { TraitType::Human, TraitType::Hero, TraitType::Assassin };
		nightBlade.attackSpeed = 1.3f; // 全ユニット中最速の英雄暗殺者。
		m_unitDefs.push_back(nightBlade);

		UnitDef flameDrake{ "FlameDrake", 4, 190, 18, 5, SkillEffectType::AreaDamage, 2, 2, 1 }; // 魔物の英雄魔道士。ブレスで周囲を焼く
		flameDrake.magicPower = 55;
		flameDrake.physicalDefense = 18;
		flameDrake.magicDefense = 22;
		flameDrake.skillSplashRadius = 1;
		flameDrake.skillSplashPercent = 55.0f;
		flameDrake.traits = { TraitType::Monster, TraitType::Hero, TraitType::Mage };
		flameDrake.attackSpeed = 0.8f; // 英雄魔道士らしく、一撃は重いが手数は控えめ。
		m_unitDefs.push_back(flameDrake);
	}

	const std::vector<UnitDef>& GetAllUnitDefs() const
	{
		return m_unitDefs;
	}

	const UnitDef* FindUnitDefByName(const std::string& name) const
	{
		for (const auto& def : m_unitDefs)
		{
			if (def.name == name)
			{
				return &def;
			}
		}
		return nullptr;
	}

private:
	std::vector<UnitDef> m_unitDefs;
};
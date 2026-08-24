#pragma once
#include <vector>
#include "HexCoord.h"
#include "UnitDef.h"
#include "ItemDef.h"

/// <summary>
/// 盤面/ベンチに実際に存在する1体分のユニット状態。
/// </summary>
struct UnitInstance
{
	const UnitDef* def = nullptr;
	int starLevel = 1;
	int currentHP = 0;
	HexCoord position;     // 現在位置(戦闘中の移動で変化する)。
	HexCoord homePosition; // 盤面に配置した時点の位置。毎ラウンド戦闘開始前にpositionをここへ戻す。

	int normalAttackCount = 0;   // 自分が通常攻撃した回数(ゲージの一部)。
	int receivedAttackCount = 0; // 相手から攻撃を受けた回数(ゲージの一部)。

	float nextActionTime = 0.0f; // 次に行動する予定の戦闘内時刻(秒)。CombatEngineが戦闘開始時に0へリセットし、
	                              // 行動するたびに (行動した時刻 + 実効攻撃間隔) へ更新する(CombatEngine::GetEffectiveAttackInterval参照)。

	// トレイト・アイテム効果などによる各ステータスの加算値(戦闘開始時に再計算される)。
	int bonusAttack = 0;         // 攻撃力(物理)への加算値。
	int bonusMagicPower = 0;     // 魔力(魔法)への加算値。
	int bonusMaxHP = 0;          // 最大HPへの加算値。
	int bonusPhysicalDefense = 0; // 物理防御力への加算値。
	int bonusMagicDefense = 0;    // 魔法防御力への加算値。
	int bonusSkillThreshold = 0;  // 必殺技発動までの必要回数への加算値(マイナスで早く発動)。
	float bonusAttackSpeed = 0.0f; // 攻撃速度(1秒あたりの行動回数)への加算値。

	int shieldAmount = 0; // 現在持っているシールド量(DamageAndShield系の必殺技で付与される)。HPより先に減る。

	std::vector<const ItemDef*> items; // 装備しているアイテム(最大数はItemSystem::kMaxItemSlotsで管理)。

	UnitInstance() = default;
	UnitInstance(const UnitDef* unitDef) : def(unitDef), currentHP(unitDef->baseHP) {}
};
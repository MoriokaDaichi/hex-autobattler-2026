#pragma once

/// <summary>
/// ユニットが属するカテゴリ(シナジーの対象となるタグ)。
/// 1体のユニットが複数のTraitTypeを持つこともある(例: 魔物かつ英雄)。
/// </summary>
enum class TraitType
{
	// 出身(種族)系トレイト。
	Monster, // 魔物
	Human,   // 人間
	Hero,    // 英雄

	// 役割(クラス)系トレイト。出身系と組み合わせて持つ。
	Warrior,  // 戦士: 前線で殴り合う近接職。
	Mage,     // 魔道士: 魔力を操る職。
	Guardian, // 守護者: 味方を守る盾役。
	Assassin, // 暗殺者: 少数精鋭で必殺技を連発する。
	Ranger,   // 狩人: 遠距離攻撃を得意とする。
};

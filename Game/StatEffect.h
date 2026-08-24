#pragma once

/// <summary>
/// ステータス効果が影響を与える項目の種類。
/// トレイト・アイテムなど、ユニットに数値バフを与える仕組み全般で共有する。
/// </summary>
enum class StatEffectType
{
	AttackFlat,             // 攻撃力(物理タイプの計算に使用)に固定値を加算。
	AttackPercent,          // 攻撃力に基礎値からの割合(%)を加算。
	MagicPowerFlat,         // 魔力(魔法タイプの計算に使用)に固定値を加算。
	MagicPowerPercent,      // 魔力に基礎値からの割合(%)を加算。
	MaxHPFlat,              // 最大HPに固定値を加算。
	MaxHPPercent,           // 最大HPに基礎値からの割合(%)を加算。
	PhysicalDefenseFlat,    // 物理防御力に固定値を加算。
	PhysicalDefensePercent, // 物理防御力に基礎値からの割合(%)を加算。
	MagicDefenseFlat,       // 魔法防御力に固定値を加算。
	MagicDefensePercent,    // 魔法防御力に基礎値からの割合(%)を加算。
	SkillThresholdFlat,     // 必殺技発動までに必要な回数に固定値を加算(マイナス値で早く発動するようになる)。
	AttackSpeedFlat,        // 攻撃速度(1秒あたりの行動回数)に固定値を加算。
	AttackSpeedPercent,     // 攻撃速度に基礎値からの割合(%)を加算。
};

/// <summary>
/// ステータス効果1つ分。
/// </summary>
struct StatEffect
{
	StatEffectType stat;
	float value = 0.0f; // Percent系は%単位(10なら+10%)。Flat系はそのまま加算値。
};

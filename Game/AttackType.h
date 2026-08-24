#pragma once

/// <summary>
/// 攻撃の属性。どちらの防御力で軽減されるかを決める。
/// </summary>
enum class AttackType
{
	Physical, // 物理攻撃。通常攻撃はこちら。physicalDefenseで軽減される。
	Magic,    // 魔法攻撃。必殺技はこちら。magicDefenseで軽減される。
};

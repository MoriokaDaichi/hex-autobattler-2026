#pragma once
#include <string>
#include "AttackType.h"

/// <summary>
/// 戦闘中に発生した出来事の種類。
/// </summary>
enum class CombatEventType
{
	Move,         // 移動。
	NormalAttack, // 通常攻撃。
	SkillAttack,  // 必殺技の直撃ダメージ。
	SplashDamage, // 必殺技(範囲ダメージ)の巻き込み分。
	Heal,         // 自己回復(ドレイン)。
	Shield,       // シールド付与。
	ShieldAbsorb, // シールドによるダメージ吸収。
	Death,        // 撃破。
	Warning,      // その他の警告(最大ターン到達など)。
};

/// <summary>
/// 戦闘中に発生した1つの出来事を表す。CombatEngineは戦闘のシミュレーションに専念し、
/// 何が起きたかをこの構造体の列として記録する。実際の表示(デバッグログ、将来のUI/
/// リプレイなど)は別のクラス(CombatLogPrinter等)がこの列を読んで行う。
/// </summary>
struct CombatEvent
{
	CombatEventType type = CombatEventType::NormalAttack;
	float time = 0.0f;       // このイベントが発生した戦闘内時刻(秒)。CombatEngineの内部時計基準。
	                          // [将来メモ] 画面描画を実装する際は、同じtime値を持つイベント同士は
	                          // 同時(同フレーム)に再生すること。outEvents内での並び順は、あくまで
	                          // CombatEngineがログ出力用に決めた便宜的な処理順でしかない。

	std::string actorOwner;  // 行動主の陣営名。
	std::string actorName;   // 行動主のユニット名。
	std::string targetOwner; // 対象の陣営名(Heal/Shield/Warningでは未使用)。
	std::string targetName;  // 対象のユニット名(Heal/Shield/Warningでは未使用)。

	// 行動主・対象の board 配列上の添字(-1 = 該当なし)。同名ユニットを一意に識別できないと
	// 画面再生時にどのインスタンスのHPを更新すべきか特定できないため、CombatEngineが記録する。
	// シミュレーションの解決ロジック・処理順・イベント内容には影響しない識別用メタ情報。
	int actorIndex = -1;
	int targetIndex = -1;

	AttackType attackType = AttackType::Physical; // NormalAttack/SkillAttack/SplashDamageで使用。

	int amount = 0;      // ダメージ量・回復量・シールド付与量・吸収量・移動距離の変化量など。
	int beforeValue = 0; // 変化前の値(HP/シールド/距離)。
	int afterValue = 0;  // 変化後の値。

	std::string message; // Warning用の自由記述。
};

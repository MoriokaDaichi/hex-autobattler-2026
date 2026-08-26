#pragma once
#include <string>
#include <vector>
#include "TraitType.h"
#include "AttackType.h"

/// <summary>
/// 必殺技の効果の種類。
/// </summary>
enum class SkillEffectType
{
	Damage,          // 単体にダメージを与えるのみ。
	AreaDamage,      // 本来の対象にダメージを与え、その周囲(skillSplashRadius以内)の敵にも
	                 // skillSplashPercent%のダメージを巻き込む(範囲攻撃)。
	DamageAndHeal,   // 単体にダメージを与え、そのダメージのskillHealPercent%分だけ自分のHPを回復する(ドレイン)。
	DamageAndShield, // 単体にダメージを与え、自分にskillShieldAmount分のシールド(ダメージ吸収)を付与する。
};

/// <summary>
/// ユニットの静的データ(マスターデータ)。
/// </summary>
struct UnitDef
{
	UnitDef() = default;

	/// <summary>
	/// UnitDatabase.cppで使われている既存の初期化順(name, cost, HP, ATK, skillThreshold,
	/// skillType, attackRange, skillRange, moveSpeed)と完全に一致するコンストラクタ。
	/// name以外の残りのフィールドはUnitDatabase.cpp側でこの後メンバ代入で設定される。
	/// nameを受け取った時点でモデル/アニメーションのファイルパスを自動的に組み立てて
	/// 格納するため、UnitDatabase.cpp側でモデルパスを個別に設定する必要はない。
	/// </summary>
	UnitDef(const std::string& name_, int cost_, int baseHP_, int baseAttack_,
		int skillThreshold_, SkillEffectType skillType_, int attackRange_, int skillRange_, float moveSpeed_)
		: name(name_)
		, cost(cost_)
		, baseHP(baseHP_)
		, baseAttack(baseAttack_)
		, skillThreshold(skillThreshold_)
		, skillType(skillType_)
		, attackRange(attackRange_)
		, skillRange(skillRange_)
		, moveSpeed(moveSpeed_)
	{
		// モデル/アニメーションのファイルパスは、命名規則(Assets/modelData/{name}.tkm 等)から
		// 100%機械的に導出できるため、name確定時にここで自動生成する。
		// テクスチャ(albedo/normal/metallicRoughness)はtkmに焼き込まれた参照から自動ロードされるため、
		// パスを個別に保持する必要はない。
		const std::string kModelDir = "Assets/modelData/";
		modelPath = kModelDir + name + ".tkm";
		idleAnimPath = kModelDir + name + "_Idle.tka";
		moveAnimPath = kModelDir + name + "_Move.tka";
		normalAttackAnimPath = kModelDir + name + "_NormalAttack.tka";
		skillAnimPath = kModelDir + name + "_Skill.tka";
		deathAnimPath = kModelDir + name + "_Death.tka";
	}

	std::string name;
	int cost = 1;
	int baseHP = 100;
	int baseAttack = 10;                            // 攻撃力。Physical属性の攻撃のダメージ計算に使う。

	// 以下はnameから自動的に導出されるモデル/アニメーションのファイルパス(コンストラクタ参照)。
	std::string modelPath;                          // モデル(.tkm)のファイルパス。
	std::string idleAnimPath;                        // 待機アニメーション(.tka)のファイルパス。
	std::string moveAnimPath;                        // 移動アニメーション(.tka)のファイルパス。
	std::string normalAttackAnimPath;                // 通常攻撃アニメーション(.tka)のファイルパス。
	std::string skillAnimPath;                       // 必殺技アニメーション(.tka)のファイルパス。
	std::string deathAnimPath;                       // 死亡アニメーション(.tka)のファイルパス。

	int skillThreshold = 3;                        // このゲージ値で必殺技が発動する。
	SkillEffectType skillType = SkillEffectType::Damage; // 必殺技の効果種別。

	int attackRange = 1;                            // 通常攻撃の射程(ヘックス数)。1なら隣接マスのみ。
	int skillRange = 1;                             // 必殺技の射程(ヘックス数)。
	float moveSpeed = 1.0f;                         // 移動速度。1秒あたりに移動できるヘックス数を表す基礎値。
	                                                 // 実際に1回の行動で移動できるヘックス数は、CombatEngineが
	                                                 // moveSpeed × 実効行動間隔(秒) として計算する(四捨五入、最低1歩)。

	int magicPower = 0;                             // 魔力。Magic属性の攻撃のダメージ計算に使う。
	int physicalDefense = 0;                        // 物理防御力。Physical属性の攻撃のダメージを軽減する。
	int magicDefense = 0;                           // 魔法防御力。Magic属性の攻撃のダメージを軽減する。

	AttackType attackType = AttackType::Physical;   // 通常攻撃の属性。基本は物理。
	AttackType skillAttackType = AttackType::Magic;  // 必殺技の属性。基本は魔法。

	// 以下はskillTypeの値に応じてのみ意味を持つ、必殺技の追加効果パラメータ。
	float skillHealPercent = 0.0f;                  // DamageAndHeal用: 与えたダメージの何%を自分のHPとして回復するか。
	int skillShieldAmount = 0;                       // DamageAndShield用: 自分に付与するシールド量。
	int skillSplashRadius = 1;                       // AreaDamage用: 本来の対象からこの距離以内の敵を巻き込む。
	float skillSplashPercent = 50.0f;                // AreaDamage用: 巻き込み対象への、本来のダメージに対する割合(%)。

	std::vector<TraitType> traits;                  // このユニットが持つトレイト(カテゴリ)。複数持ってもよい。

	float attackSpeed = 1.0f;                       // 攻撃速度。1秒間に何回行動(通常攻撃/必殺技/移動)できるかを表す基礎値。
	                                                 // 値が大きいほど頻繁に行動する。全ユニット未設定=1.0なので、
	                                                 // 現状は全員同じ頻度で行動する(トレイト・アイテムのボーナスで変わる)。
	                                                 // 実際の行動間隔(秒)は、CombatEngineが
	                                                 // 1.0f / (attackSpeed + bonusAttackSpeed) として計算する。
};
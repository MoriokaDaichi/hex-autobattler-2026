#pragma once

/// <summary>
/// アイテムが持つ「戦闘中に発動するパッシブ効果」の種別。
/// 現状はオンヒット火傷のみ。将来 種別を足す場合はここに追加する。
/// </summary>
enum class PassiveEffectType
{
	OnHitBurn, // 通常攻撃を当てたとき、対象へ継続ダメージ(火傷)を付与する。
};

/// <summary>
/// パッシブ効果1つ分。パラメータの意味は type によって決まる。
/// OnHitBurn: magnitude = 1刻みあたりの固定ダメージ / ticks = 刻み回数 / interval = 刻み間隔(秒)。
/// </summary>
struct PassiveEffect
{
	PassiveEffectType type = PassiveEffectType::OnHitBurn;
	int magnitude = 0;
	int ticks = 0;
	float interval = 1.0f;
};

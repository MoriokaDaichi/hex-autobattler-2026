#pragma once
#include <string>
#include "Player.h"

/// <summary>
/// その戦闘の結果。EconomySystemが収入・連勝連敗の計算に使う。
/// </summary>
enum class CombatResult
{
	Win,
	Loss,
	Draw,
};

/// <summary>
/// ラウンドごとの収入(基本収入+利子+連勝/連敗ボーナス)を計算し、Player::goldに反映するクラス。
/// </summary>
class EconomySystem
{
public:
	static const int kBaseIncome = 5;       // 毎ラウンド必ずもらえる基本収入。
	static const int kGoldPerInterest = 10; // このゴールドごとに利子が+1される。
	static const int kMaxInterest = 5;      // 利子の上限(50ゴールド以上持っていても+5で頭打ち)。

	/// <summary>
	/// 直前の戦闘結果を受けて連勝・連敗数を更新し、基本収入+利子+連勝/連敗ボーナスを
	/// player.goldに加算する。Combatフェーズの決着直後に1回呼ぶことを想定している。
	/// </summary>
	void GrantRoundIncome(Player& player, CombatResult result, const std::string& ownerName)
	{
		UpdateStreak(player, result);

		int interest = player.gold / kGoldPerInterest;
		if (interest > kMaxInterest) interest = kMaxInterest;

		int streak = (result == CombatResult::Win) ? player.winStreak : player.lossStreak;
		int streakBonus = (result == CombatResult::Draw) ? 0 : GetStreakBonus(streak);

		int totalIncome = kBaseIncome + interest + streakBonus;
		player.gold += totalIncome;

		wchar_t buf[256];
		swprintf_s(buf, L"[%hs] Income: +%d (base %d, interest %d, streak bonus %d) -> Gold: %d\n",
			ownerName.c_str(), totalIncome, kBaseIncome, interest, streakBonus, player.gold);
		OutputDebugString(buf);
	}

private:
	void UpdateStreak(Player& player, CombatResult result)
	{
		if (result == CombatResult::Win)
		{
			player.winStreak++;
			player.lossStreak = 0;
		}
		else if (result == CombatResult::Loss)
		{
			player.lossStreak++;
			player.winStreak = 0;
		}
		else
		{
			player.winStreak = 0;
			player.lossStreak = 0;
		}
	}

	/// <summary>
	/// 連勝(または連敗)数に応じたボーナスを返す。2で+1, 3-4で+2, 5以上で+3。
	/// </summary>
	int GetStreakBonus(int streak)
	{
		if (streak >= 5) return 3;
		if (streak >= 3) return 2;
		if (streak >= 2) return 1;
		return 0;
	}
};

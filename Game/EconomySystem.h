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
	static const int kWinStreakGoldPerWin = 5;     // 連勝ボーナス = この値 × 連勝数(上限なし)。
	static const int kWinStreakBonusMinStreak = 2; // 連勝ボーナスが付き始める連勝数。
	static const int kWinStreakItemMinStreak = 5;  // この連勝数以上では、勝つたびに素材アイテムが追加で1つ付く。

	/// <summary>
	/// 直前の戦闘結果を受けて連勝・連敗数を更新し、基本収入+利子+連勝/連敗ボーナスを
	/// player.goldに加算する。Combatフェーズの決着直後に1回呼ぶことを想定している。
	/// </summary>
	void GrantRoundIncome(Player& player, CombatResult result, const std::string& ownerName)
	{
		UpdateStreak(player, result);

		int interest = player.gold / kGoldPerInterest;
		if (interest > kMaxInterest) interest = kMaxInterest;

		// 連勝と連敗は別の表(連勝は5ずつ伸び続け、連敗は+3で頭打ち)。引き分けはボーナス無し。
		int streakBonus = 0;
		if (result == CombatResult::Win) streakBonus = GetWinStreakBonus(player.winStreak);
		else if (result == CombatResult::Loss) streakBonus = GetLossStreakBonus(player.lossStreak);

		int totalIncome = kBaseIncome + interest + streakBonus;
		player.gold += totalIncome;

		wchar_t buf[256];
		swprintf_s(buf, L"[%hs] Income: +%d (base %d, interest %d, streak bonus %d) -> Gold: %d\n",
			ownerName.c_str(), totalIncome, kBaseIncome, interest, streakBonus, player.gold);
		OutputDebugString(buf);
	}

	/// <summary>
	/// 連勝数に応じたゴールドボーナスを返す。2連勝以上で 5×連勝数(2で+10, 3で+15, ...上限なし)。
	/// ツールチップからも参照するためstatic。
	/// </summary>
	static int GetWinStreakBonus(int winStreak)
	{
		if (winStreak < kWinStreakBonusMinStreak) return 0;
		return kWinStreakGoldPerWin * winStreak;
	}

	/// <summary>
	/// 連敗数に応じたゴールドボーナスを返す。2で+1, 3-4で+2, 5以上で+3。
	/// </summary>
	static int GetLossStreakBonus(int lossStreak)
	{
		if (lossStreak >= 5) return 3;
		if (lossStreak >= 3) return 2;
		if (lossStreak >= 2) return 1;
		return 0;
	}

	/// <summary>
	/// 連勝数が素材アイテムの追加報酬(通常の勝利報酬とは別に+1個)の条件を満たしているか。
	/// 勝利時、GrantRoundIncomeで連勝数を更新した後に呼ぶ。
	/// </summary>
	static bool HasWinStreakItemBonus(int winStreak)
	{
		return winStreak >= kWinStreakItemMinStreak;
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
};

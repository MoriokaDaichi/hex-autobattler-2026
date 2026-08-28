#pragma once
#include "Player.h"

/// <summary>
/// プレイヤーレベル(経験値・レベルアップ)を管理するクラス。
/// レベルが上がると、Player::GetMaxBoardSize()の盤面上限が増え、ShopSystem::RollShopに
/// levelを渡すことで高コスト帯のユニットが出やすくなる。
/// </summary>
class LevelSystem
{
public:
	static const int kMaxLevel = 9;          // これ以上はレベルアップしない上限。
	static const int kBuyXPCost = 4;         // 経験値を1回買うのに必要なゴールド。
	static const int kBuyXPAmount = 4;       // 1回の購入で得られる経験値。
	static const int kPassiveXPPerRound = 2; // 何もしなくても毎ラウンドもらえる経験値。

	/// <summary>
	/// ゴールドを払って経験値を購入する。購入に成功すればtrueを返す
	/// (ゴールド不足、または既に最大レベルの場合はfalse)。
	/// </summary>
	bool BuyXP(Player& player)
	{
		if (player.level >= kMaxLevel) return false;
		if (player.gold < kBuyXPCost) return false;

		player.gold -= kBuyXPCost;
		GrantXP(player, kBuyXPAmount);
		return true;
	}

	/// <summary>
	/// ラウンド経過による経験値を付与する(何もしなくても自然にレベルが上がっていく)。
	/// Combatフェーズの決着直後、全プレイヤー分を1回ずつ呼ぶことを想定している。
	/// </summary>
	void GrantRoundXP(Player& player)
	{
		GrantXP(player, kPassiveXPPerRound);
	}

	/// <summary>
	/// currentLevelから次のレベルに上がるために必要な経験値(UI表示用の公開アクセサ)。
	/// 最大レベル到達時は0。
	/// </summary>
	int XPForNextLevel(int currentLevel) const
	{
		return GetXPRequiredForNextLevel(currentLevel);
	}

private:
	/// <summary>
	/// currentLevelから次のレベルに上がるために必要な経験値を返す(終盤ほど重くなる)。
	/// kMaxLevelに到達している場合は0(それ以上レベルアップしない)。
	/// </summary>
	int GetXPRequiredForNextLevel(int currentLevel) const
	{
		static const int kRequirements[kMaxLevel] = { 2, 2, 6, 10, 20, 36, 48, 72, 0 };

		if (currentLevel < 1 || currentLevel > kMaxLevel) return 0;
		return kRequirements[currentLevel - 1];
	}

	void GrantXP(Player& player, int amount)
	{
		player.xp += amount;

		while (player.level < kMaxLevel)
		{
			int required = GetXPRequiredForNextLevel(player.level);
			if (player.xp < required) break;

			player.xp -= required;
			player.level++;

			wchar_t buf[128];
			swprintf_s(buf, L"[%hs] Level Up! -> Level %d\n", player.name.c_str(), player.level);
			OutputDebugString(buf);
		}
	}
};

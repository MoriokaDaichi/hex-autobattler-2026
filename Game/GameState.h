#pragma once
#include <vector>
#include "Player.h"

/// <summary>
/// ゲームが現在どのフェーズにいるか。
/// </summary>
enum class Phase
{
	Preparation,  // 準備フェーズ(ショップ・配置)
	Combat,       // 戦闘フェーズ
	Result,       // 結果表示・ラウンド終了処理
	GameOver,     // 同じ敵に既定回数(kMaxLossesPerEnemy)負け、ゲームが終了した状態。
	Victory,      // 全ラウンド(kTotalRounds)をクリアし、ゲームに勝利した状態。
};

/// <summary>
/// ゲーム全体の状態。
/// </summary>
struct GameState
{
	static const int kTotalRounds = 10;      // クリアに必要なラウンド数。
	static const int kMaxLossesPerEnemy = 3; // 同じ敵に何回負けたらゲームオーバーになるか。

	std::vector<Player> players; // players[0]のみを使用する(人間プレイヤー)。
	int roundNumber = 1;
	int lossCount = 0; // 現在の敵(roundNumber)に対する連続敗北回数。勝つか敵が変わるとリセットされる。
	Phase currentPhase = Phase::Preparation;
};
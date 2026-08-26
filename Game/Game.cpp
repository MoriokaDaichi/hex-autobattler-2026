#include "stdafx.h"
#include "Game.h"
#include "HexCoord.h"
#include "UnitDef.h"
#include "UnitInstance.h"
#include "Player.h"

bool Game::Start()
{
	m_unitDatabase.Init();
	m_traitDatabase.Init();
	m_itemDatabase.Init();
	m_hexGridRenderer.Init();

	// 盤面(ヘックスグリッド q:0-8, r:0-2、HexGridRenderer::CalcTileCenterの座標系で
	// 概ねX:±430, Z:±125に収まる)全体が、自陣・中立・敵陣とも余裕を持って画角に入るよう、
	// 盤面中心(ワールド原点)を見下ろす角度・距離を大きめに取ってカメラを配置する。
	// (ユニットモデルが等倍スケールで大きすぎる既知問題の影響も、離すことである程度緩和される)
	g_camera3D->SetPosition({ 0.0f, 900.0f, -650.0f });
	g_camera3D->SetTarget({ 0.0f, 0.0f, 0.0f });

	// カメラとほぼ同じ側(斜め上・やや背後)からユニット正面に光が回り込む方向に
	// ディレクションライトを設定し、シルエット化を避けつつ陰影で立体感を出す。
	Vector3 mainLightDir(0.35f, -0.75f, 0.55f);
	mainLightDir.Normalize();
	g_renderingEngine->SetDirectionLight(0, mainLightDir, Vector3(1.05f, 1.0f, 0.92f));
	g_renderingEngine->SetAmbient(Vector3(0.35f, 0.35f, 0.4f));

	// 盤面以外に何も無い(背景が真っ黒に近い)シーンだと、自動露出(ミドルグレー基準)が
	// 平均輝度の低さを補おうとして過剰に明るさを持ち上げ、ブルームが暴れてしまうため、
	// 露出目標を下げつつブルームの発光しきい値を高くして抑える。
	g_renderingEngine->SetSceneMiddleGray(0.03f);
	g_renderingEngine->SetBloomThreshold(10.0f);

	// プレイヤー1(操作するプレイヤー、唯一のplayers要素)
	m_gameState.players.push_back(Player("You"));
	m_gameState.players[0].gold = 10;

	Player& player = m_gameState.players[0];

	// 10ラウンド分の固定敵編成をあらかじめ組み立てておく。各ラウンドの戦闘直前に
	// EnemyFactoryがこのデータから即座に敵の盤面を生成する(BotAIのような段階的購入は行わない)。
	m_enemyStages = BuildEnemyStages();

	// --- デバッグ用: 合成ロジックの動作確認。ベンチにSlimeを3体追加すると、
	// Player::TryMergeUnitsにより自動的に★2のSlime1体(ベンチ)にまとまるはず。
	// (盤面は空のままなので、アイテム付与やスターアップの見た目確認は通常のプレイ操作で行う)。
	const UnitDef* slimeDef = m_unitDatabase.FindUnitDefByName("Slime");
	player.bench.push_back(UnitInstance(slimeDef));
	player.bench.push_back(UnitInstance(slimeDef));
	player.bench.push_back(UnitInstance(slimeDef));
	while (player.TryMergeUnits()) {}

	wchar_t mergeLogBuf[256];
	swprintf_s(mergeLogBuf, L"[DEBUG] After merge test: bench count=%d\n", (int)player.bench.size());
	OutputDebugString(mergeLogBuf);

	return true;
}

void Game::Update()
{
	// players[0](唯一の人間プレイヤー)の盤面(board)のユニットモデルを表示・更新する。
	// 敵ユニットや戦闘進行自体はリアルタイム3D表示の対象外(スコープ外、別タスク)。
	m_unitModelDisplay.Update(m_gameState.players[0]);

	if (m_gameState.currentPhase == Phase::Preparation)
	{
		// まだショップが無ければ抽選する。
		if (m_currentShop.empty())
		{
			Player& shopPlayer = m_gameState.players[0];
			m_currentShop = m_shopSystem.RollShop(m_unitDatabase, shopPlayer.level);

			wchar_t shopHeaderBuf[128];
			swprintf_s(shopHeaderBuf, L"--- Shop (Level %d, XP %d) ---\n", shopPlayer.level, shopPlayer.xp);
			OutputDebugString(shopHeaderBuf);
			for (size_t i = 0; i < m_currentShop.size(); ++i)
			{
				wchar_t buf[256];
				swprintf_s(buf, L"[%d] %hs (Cost:%d)\n",
					(int)i, m_currentShop[i]->name.c_str(), m_currentShop[i]->cost);
				OutputDebugString(buf);
			}
		}

		// Aボタンで、ショップの0番目のユニットを買う。
		if (g_pad[0]->IsTrigger(enButtonA))
		{
			Player& player = m_gameState.players[0];
			bool success = player.BuyUnit(m_currentShop[0]);

			wchar_t buf[256];
			swprintf_s(buf, L"Buy result: %hs, Gold left: %d, Bench count: %d\n",
				success ? "true" : "false", player.gold, (int)player.bench.size());
			OutputDebugString(buf);
		}

		// Bボタンで次のフェーズに進む。
		if (g_pad[0]->IsTrigger(enButtonB))
		{
			m_currentShop.clear();
			m_gameState.currentPhase = Phase::Combat;
		}

		// Xボタンで、ベンチの0番目を盤面の(0,0)に配置する。
		if (g_pad[0]->IsTrigger(enButtonX))
		{
			Player& player = m_gameState.players[0];
			bool success = player.PlaceUnitOnBoard(0, HexCoord(0, 0));

			wchar_t buf[256];
			swprintf_s(buf, L"Place result: %hs, Bench count: %d, Board count: %d\n",
				success ? "true" : "false", (int)player.bench.size(), (int)player.board.size());
			OutputDebugString(buf);
		}

		// Yボタンで、ゴールドを払ってショップをリロールする。
		if (g_pad[0]->IsTrigger(enButtonY))
		{
			Player& player = m_gameState.players[0];
			const int kRerollCost = 2;

			if (player.gold >= kRerollCost)
			{
				player.gold -= kRerollCost;
				m_currentShop = m_shopSystem.RollShop(m_unitDatabase, player.level);

				wchar_t headerBuf[256];
				swprintf_s(headerBuf, L"--- Shop Reroll (Gold: %d) ---\n", player.gold);
				OutputDebugString(headerBuf);
				for (size_t i = 0; i < m_currentShop.size(); ++i)
				{
					wchar_t buf[256];
					swprintf_s(buf, L"[%d] %hs (Cost:%d)\n",
						(int)i, m_currentShop[i]->name.c_str(), m_currentShop[i]->cost);
					OutputDebugString(buf);
				}
			}
			else
			{
				OutputDebugString(L"Not enough gold to reroll the shop.\n");
			}
		}

		// LB1ボタンで、ベンチの0番目のユニットを売却してゴールドを得る。
		if (g_pad[0]->IsTrigger(enButtonLB1))
		{
			Player& player = m_gameState.players[0];
			bool success = player.SellUnitFromBench(0);

			wchar_t buf[256];
			swprintf_s(buf, L"Sell result: %hs, Gold: %d, Bench count: %d\n",
				success ? "true" : "false", player.gold, (int)player.bench.size());
			OutputDebugString(buf);
		}

		// RB1ボタンで、ゴールドを払って経験値を購入する(LevelSystemの中でレベルアップ処理も行う)。
		if (g_pad[0]->IsTrigger(enButtonRB1))
		{
			Player& player = m_gameState.players[0];
			bool success = m_levelSystem.BuyXP(player);

			wchar_t buf[256];
			swprintf_s(buf, L"Buy XP result: %hs, Gold: %d, Level: %d, XP: %d\n",
				success ? "true" : "false", player.gold, player.level, player.xp);
			OutputDebugString(buf);
		}
	}
	else if (m_gameState.currentPhase == Phase::Combat)
	{
		Player& player = m_gameState.players[0];

		// 現在のラウンドに対応する固定敵編成から、戦闘直前に敵の盤面を即座に組み立てる。
		// 毎回新しく生成するため、前回の戦闘結果(HP減少・移動)は持ち越さない。
		const EnemyStage& stage = m_enemyStages[m_gameState.roundNumber - 1];
		Player enemy = m_enemyFactory.CreateEnemyBoard(stage, m_unitDatabase, m_itemDatabase, m_itemSystem);

		// 前回の戦闘での移動をリセットし、各ユニットを配置した位置に戻す。
		player.ResetBoardPositions();

		// トレイト(シナジー)構成を集計し、各ユニットにボーナスを適用してHPを全回復させる。
		// 戦闘中に味方が減ってもトレイト構成は再計算しない(TFT同様、戦闘開始時点で固定)。
		m_traitSystem.ApplyTraitBonuses(player.board, m_traitDatabase, player.name);
		m_traitSystem.ApplyTraitBonuses(enemy.board, m_traitDatabase, enemy.name);

		// アイテム効果をトレイト分に上乗せする(この中でcurrentHPも最終的な最大値まで再度全回復させる)。
		m_itemSystem.ApplyItemBonuses(player.board, player.name);
		m_itemSystem.ApplyItemBonuses(enemy.board, enemy.name);

		// スターレベルによる倍率をさらに上乗せする(この中でcurrentHPも最終的な最大値まで再度全回復させる)。
		m_starLevelSystem.ApplyStarBonuses(player.board, player.name);
		m_starLevelSystem.ApplyStarBonuses(enemy.board, enemy.name);

		OutputDebugString(L"=== Combat Start ===\n");

		// 戦闘そのものはCombatEventの列として記録し、シミュレーション完了後にまとめて表示する
		// (CombatEngineは表示処理を持たず、CombatLogPrinterが別途イベント列を読んで表示する)。
		m_combatEvents.clear();
		m_combatEngine.SimulateCombat(player, enemy, m_combatEvents);
		m_combatLogPrinter.Print(m_combatEvents);

		bool playerWiped = m_combatEngine.IsBoardWiped(player.board);
		bool enemyWiped = m_combatEngine.IsBoardWiped(enemy.board);

		// 相打ち(両者全滅・両者無傷)もリトライ扱い(敗北)にする。
		CombatResult result = (enemyWiped && !playerWiped) ? CombatResult::Win : CombatResult::Loss;

		if (result == CombatResult::Win)
		{
			OutputDebugString(L"You win this combat!\n");
		}
		else if (playerWiped && enemyWiped)
		{
			OutputDebugString(L"Draw (both wiped). Treated as a loss for retry purposes.\n");
		}
		else
		{
			OutputDebugString(L"You lose this combat!\n");
		}

		// 経済(基本収入+利子+連勝/連敗ボーナス)をプレイヤーのゴールドに反映する。
		m_economySystem.GrantRoundIncome(player, result, player.name);

		// ラウンド経過による経験値をプレイヤーに付与する(勝敗に関わらず、何もしなくても自然にレベルが上がっていく)。
		m_levelSystem.GrantRoundXP(player);

		OutputDebugString(L"=== Combat End ===\n");

		if (result == CombatResult::Win)
		{
			wchar_t buf[128];
			swprintf_s(buf, L"=== Round %d Clear! ===\n", m_gameState.roundNumber);
			OutputDebugString(buf);

			m_gameState.lossCount = 0;
			m_gameState.roundNumber++;

			if (m_gameState.roundNumber > GameState::kTotalRounds)
			{
				OutputDebugString(L"=== ALL ROUNDS CLEARED! YOU WIN! ===\n");
				m_gameState.currentPhase = Phase::Victory;
			}
			else
			{
				m_gameState.currentPhase = Phase::Result;
			}
		}
		else
		{
			m_gameState.lossCount++;

			wchar_t buf[128];
			swprintf_s(buf, L"=== Defeat! (%d/%d losses against this enemy) ===\n",
				m_gameState.lossCount, GameState::kMaxLossesPerEnemy);
			OutputDebugString(buf);

			if (m_gameState.lossCount >= GameState::kMaxLossesPerEnemy)
			{
				OutputDebugString(L"=== GAME OVER ===\n");
				m_gameState.currentPhase = Phase::GameOver;
			}
			else
			{
				m_gameState.currentPhase = Phase::Result;
			}
		}
	}
	else if (m_gameState.currentPhase == Phase::GameOver)
	{
		// ゲーム終了状態。ここでは何もしない(ラウンドを進めない)。
	}
	else if (m_gameState.currentPhase == Phase::Victory)
	{
		// 全ラウンドクリア状態。ここでは何もしない。
	}
	else // Result
	{
		// ラウンドの進行(勝利時のroundNumber++・敗北時のlossCount++)はCombatフェーズ側で
		// 既に確定しているので、ここでは準備フェーズへ戻るだけでよい。
		m_gameState.currentPhase = Phase::Preparation;
	}
}

/// <summary>
/// 10ラウンド分の固定敵編成を組み立てて返す。体数・コスト・スターレベル・装備が
/// ラウンドが進むほど段階的に強くなるように設計している。配置座標は既存Bot編成と同じ
/// HexCoord(6,0)〜(8,2)の3x3ブロックを埋める形で割り当てる。
/// </summary>
std::vector<EnemyStage> Game::BuildEnemyStages()
{
	auto unitPlan = [](const std::string& name, int starLevel, HexCoord pos,
		std::vector<std::string> itemComponents = {})
	{
		EnemyUnitPlan p;
		p.unitName = name;
		p.starLevel = starLevel;
		p.position = pos;
		p.itemComponentNames = itemComponents;
		return p;
	};

	std::vector<EnemyStage> stages;

	// Round 1: チュートリアル。1体のみ。
	stages.push_back({ "Round1: Slime", {
		unitPlan("Slime", 1, HexCoord(6, 1)),
	} });

	// Round 2: 2体。
	stages.push_back({ "Round2: Goblin Pair", {
		unitPlan("Slime", 1, HexCoord(6, 0)),
		unitPlan("Goblin", 1, HexCoord(7, 1)),
	} });

	// Round 3: 3体。遠距離ユニット導入。
	stages.push_back({ "Round3: Archer Squad", {
		unitPlan("Goblin", 1, HexCoord(6, 0)),
		unitPlan("Archer", 1, HexCoord(7, 1)),
		unitPlan("Slime", 1, HexCoord(8, 2)),
	} });

	// Round 4: 4体。初めてアイテム(単品)が付く。
	stages.push_back({ "Round4: Knight's Line", {
		unitPlan("Knight", 1, HexCoord(6, 0), { "IronCrystal" }),
		unitPlan("Swordsman", 1, HexCoord(6, 2)),
		unitPlan("Archer", 1, HexCoord(7, 1)),
		unitPlan("Cultist", 1, HexCoord(8, 1)),
	} });

	// Round 5: 5体。初★2、初の完成アイテム。
	stages.push_back({ "Round5: Priest's Guard", {
		unitPlan("Knight", 1, HexCoord(6, 0), { "IronCrystal", "GuardCrystal" }),   // FortressShield
		unitPlan("Priest", 1, HexCoord(6, 2), { "WisdomCrystal", "GuardCrystal" }), // MysticRobe
		unitPlan("Swordsman", 1, HexCoord(7, 0)),
		unitPlan("Archer", 1, HexCoord(7, 2)),
		unitPlan("Goblin", 2, HexCoord(8, 1)),
	} });

	// Round 6: 6体。
	stages.push_back({ "Round6: Berserker Warband", {
		unitPlan("OrcBerserker", 1, HexCoord(6, 0), { "PowerCrystal", "VitalCrystal" }), // BerserkersAxe
		unitPlan("Knight", 1, HexCoord(6, 2), { "IronCrystal", "GuardCrystal" }),         // FortressShield
		unitPlan("Direwolf", 1, HexCoord(7, 0)),
		unitPlan("ShadowStalker", 1, HexCoord(7, 2)),
		unitPlan("Priest", 1, HexCoord(8, 0), { "WisdomCrystal", "GuardCrystal" }),       // MysticRobe
		unitPlan("Goblin", 2, HexCoord(8, 2)),
	} });

	// Round 7: 7体。コスト4ユニット導入。
	stages.push_back({ "Round7: Paladin Vanguard", {
		unitPlan("Paladin", 1, HexCoord(6, 0), { "IronCrystal", "VitalCrystal" }),        // IronWall
		unitPlan("Warlord", 1, HexCoord(6, 1)),
		unitPlan("OrcBerserker", 1, HexCoord(6, 2), { "PowerCrystal", "VitalCrystal" }),  // BerserkersAxe
		unitPlan("Griffin", 1, HexCoord(7, 0)),
		unitPlan("Cultist", 1, HexCoord(7, 1)),
		unitPlan("Priest", 2, HexCoord(7, 2)),
		unitPlan("Direwolf", 1, HexCoord(8, 1)),
	} });

	// Round 8: 8体。
	stages.push_back({ "Round8: Dragon's Roost", {
		unitPlan("YoungDragon", 1, HexCoord(6, 0), { "WisdomCrystal", "VitalCrystal" }), // ArcaneTome
		unitPlan("Paladin", 1, HexCoord(6, 1), { "IronCrystal", "VitalCrystal" }),       // IronWall
		unitPlan("Warlord", 2, HexCoord(6, 2)),
		unitPlan("NightBlade", 1, HexCoord(7, 0)),
		unitPlan("FlameDrake", 1, HexCoord(7, 1)),
		unitPlan("Griffin", 1, HexCoord(7, 2)),
		unitPlan("Behemoth", 1, HexCoord(8, 0)),
		unitPlan("ShadowStalker", 1, HexCoord(8, 1)),
	} });

	// Round 9: 9体(満員)。コスト5ユニット導入。
	stages.push_back({ "Round9: Hero's Assembly", {
		unitPlan("ChimeraLord", 1, HexCoord(6, 0), { "PowerCrystal", "GuardCrystal" }),   // DuelistsEdge
		unitPlan("YoungDragon", 2, HexCoord(6, 1), { "WisdomCrystal", "VitalCrystal" }),  // ArcaneTome
		unitPlan("Paladin", 2, HexCoord(6, 2), { "IronCrystal", "VitalCrystal" }),        // IronWall
		unitPlan("Warlord", 1, HexCoord(7, 0)),
		unitPlan("NightBlade", 1, HexCoord(7, 1)),
		unitPlan("FlameDrake", 1, HexCoord(7, 2)),
		unitPlan("Behemoth", 1, HexCoord(8, 0)),
		unitPlan("Griffin", 1, HexCoord(8, 1)),
		unitPlan("Cultist", 1, HexCoord(8, 2)),
	} });

	// Round 10: 最終ボス。9体全員★2、豊富な装備。
	stages.push_back({ "Round10: Final Vanguard", {
		unitPlan("ChimeraLord", 2, HexCoord(6, 0), { "PowerCrystal", "GuardCrystal" }),  // DuelistsEdge
		unitPlan("FlameDrake", 2, HexCoord(6, 1), { "WisdomCrystal", "WisdomCrystal" }), // ArchmageStaff
		unitPlan("Warlord", 2, HexCoord(6, 2), { "IronCrystal", "PowerCrystal" }),       // Warplate
		unitPlan("NightBlade", 2, HexCoord(7, 0), { "SwiftCrystal", "PowerCrystal" }),   // FuriousEdge
		unitPlan("Paladin", 2, HexCoord(7, 1), { "IronCrystal", "VitalCrystal" }),       // IronWall
		unitPlan("YoungDragon", 2, HexCoord(7, 2), { "WisdomCrystal", "VitalCrystal" }), // ArcaneTome
		unitPlan("Behemoth", 2, HexCoord(8, 0)),
		unitPlan("Griffin", 2, HexCoord(8, 1)),
		unitPlan("OrcBerserker", 2, HexCoord(8, 2), { "PowerCrystal", "VitalCrystal" }), // BerserkersAxe
	} });

	return stages;
}

void Game::Render(RenderContext& rc)
{
	m_hexGridRenderer.Draw(rc, m_gameState);

	m_unitModelDisplay.Draw(rc);
}
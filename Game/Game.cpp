#include "stdafx.h"
#include "Game.h"
#include "HexCoord.h"
#include "UnitDef.h"
#include "UnitInstance.h"
#include "Player.h"
#include <random>

namespace
{
	// Phase::Result滞在時間。「ROUND CLEAR!」/「DEFEAT...」を一言表示してからPreparationへ進む。
	const float kResultPhaseDurationSec = 1.5f;

	/// <summary>
	/// アイテムDBの素材(ItemCategory::Component)から1つをランダムに返す。ラウンド勝利報酬用。
	/// ShopSystemと同じくmt19937を使う(呼び出しごとに再シードしないよう関数内staticで保持)。
	/// </summary>
	const ItemDef* PickRandomComponent(const ItemDatabase& itemDatabase)
	{
		static std::mt19937 rng(std::random_device{}());

		std::vector<const ItemDef*> pool;
		for (const auto& def : itemDatabase.GetAllItemDefs())
		{
			if (def.category == ItemCategory::Component)
			{
				pool.push_back(&def);
			}
		}
		if (pool.empty()) return nullptr;

		std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
		return pool[dist(rng)];
	}
}

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

	InitializeNewRun();

	// 10ラウンド分の固定敵編成をあらかじめ組み立てておく。各ラウンドの戦闘直前に
	// EnemyFactoryがこのデータから即座に敵の盤面を生成する(BotAIのような段階的購入は行わない)。
	m_enemyStages = BuildEnemyStages();

	return true;
}

/// <summary>
/// 1プレイ分の状態を初期状態へ戻す。Start()から1回、GameOver/Victoryからのリスタート時に
/// 都度呼ばれる。呼び出し後のcurrentPhaseは呼び出し側が決める(Start()はGameStateの
/// デフォルト値のままPhase::Title、リスタート時は呼び出し側で明示的にPhase::Titleへ戻す)。
/// </summary>
void Game::InitializeNewRun()
{
	// プレイヤー1(操作するプレイヤー、唯一のplayers要素)を作り直す。
	m_gameState.players.clear();
	m_gameState.players.push_back(Player("You"));
	m_gameState.players[0].gold = 10;

	m_gameState.roundNumber = 1;
	m_gameState.lossCount = 0;

	m_currentShop.clear();
	m_heldUnclaimedIndex = -1;
	m_combatSimDone = false;
	m_pendingPhaseAfterCombat = Phase::Result;
	m_lastCombatResult = CombatResult::Win;
	m_resultPhaseTimer = 0.0f;

	// UnitModelDisplayはplayers[0].boardの内容を毎フレーム比較して表示モデルを同期する
	// (UnitModelDisplay::RebuildIfBoardChanged)ため、boardを空にしたこの状態を次のUpdate()が
	// 読み取れば、古い盤面のモデルは自動的に消える。明示的なリセット呼び出しは不要。
}

void Game::Update()
{
	// players[0](唯一の人間プレイヤー)の盤面(board)のユニットモデルを表示・更新する。
	// 敵ユニットや戦闘進行自体はリアルタイム3D表示の対象外(スコープ外、別タスク)。
	m_unitModelDisplay.Update(m_gameState.players[0]);

	// マウス・キーボード・ゲームパッドを横断するカーソル/選択状態を更新する。
	m_cursorSelection.Update();

	// ショップUIの操作フィードバック(数秒で自動的に消える)の残り時間を進める。
	m_shopUI.UpdateFeedbackTimer(g_gameTime->GetFrameDeltaTime());

	if (m_gameState.currentPhase == Phase::Title)
	{
		// Aボタンで準備フェーズへ進む。
		if (g_pad[0]->IsTrigger(enButtonA))
		{
			m_gameState.currentPhase = Phase::Preparation;
		}
	}
	else if (m_gameState.currentPhase == Phase::Preparation)
	{
		// フォーカス中の一覧の実際の要素数に合わせて、カーソルが範囲外を指さないようにする。
		Player& prepPlayer = m_gameState.players[0];
		if (m_cursorSelection.GetFocus() == InputFocus::Shop)
		{
			m_cursorSelection.ClampListCursor((int)m_currentShop.size());
		}
		else if (m_cursorSelection.GetFocus() == InputFocus::Bench)
		{
			m_cursorSelection.ClampListCursor((int)prepPlayer.bench.size());
		}
		else if (m_cursorSelection.GetFocus() == InputFocus::Items)
		{
			m_cursorSelection.ClampListCursor((int)prepPlayer.unclaimedItems.size());
		}

		// 手に持っているアイテムのindexが、装備等でリストが縮んで範囲外になっていたら解除する。
		if (m_heldUnclaimedIndex >= (int)prepPlayer.unclaimedItems.size())
		{
			m_heldUnclaimedIndex = -1;
		}

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

		// Aボタン(またはマウス左クリック/Enter/Space)。フォーカスと「アイテムを手に持っているか」で
		// 意味が変わる:
		//  - Itemsフォーカス中: カーソルのアイテムを手に持つ / もう一度押すと戻す。
		//  - アイテムを手に持った状態でBench/Boardフォーカス中: 選択中のユニットへ装備を確定する。
		//  - それ以外(ショップフォーカス中はカーソルのユニット、他は0番目): 従来通りユニットを買う。
		if (g_pad[0]->IsTrigger(enButtonA))
		{
			Player& player = m_gameState.players[0];
			InputFocus focus = m_cursorSelection.GetFocus();
			bool holdingItem = (m_heldUnclaimedIndex >= 0 && m_heldUnclaimedIndex < (int)player.unclaimedItems.size());

			if (focus == InputFocus::Items)
			{
				if (player.unclaimedItems.empty())
				{
					m_shopUI.PushFeedback(L"未装備アイテムがありません", ShopUIRenderer::FeedbackLevel::Failure);
				}
				else
				{
					int idx = m_cursorSelection.GetListCursorIndex();
					if (idx < 0 || idx >= (int)player.unclaimedItems.size()) idx = 0;

					if (m_heldUnclaimedIndex == idx)
					{
						m_heldUnclaimedIndex = -1; // 同じアイテムをもう一度選んだら手放す。
						m_shopUI.PushFeedback(L"アイテムを戻しました", ShopUIRenderer::FeedbackLevel::Info);
					}
					else
					{
						m_heldUnclaimedIndex = idx;
						wchar_t fb[160];
						swprintf_s(fb, L"アイテム選択: %hs  (ユニットを選び[A]で装備)", player.unclaimedItems[idx]->name.c_str());
						m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
					}
				}
			}
			else if (holdingItem && (focus == InputFocus::Bench || focus == InputFocus::Board))
			{
				const ItemDef* heldItem = player.unclaimedItems[m_heldUnclaimedIndex];

				UnitInstance* targetUnit = nullptr;
				if (focus == InputFocus::Bench)
				{
					int benchIndex = m_cursorSelection.GetListCursorIndex();
					if (benchIndex >= 0 && benchIndex < (int)player.bench.size())
					{
						targetUnit = &player.bench[benchIndex];
					}
				}
				else // InputFocus::Board
				{
					HexCoord hex(0, 0);
					if (m_cursorSelection.GetHexCursor(hex))
					{
						for (auto& unit : player.board)
						{
							if (unit.position == hex) { targetUnit = &unit; break; }
						}
					}
				}

				if (targetUnit == nullptr)
				{
					m_shopUI.PushFeedback(L"装備先のユニットがいません", ShopUIRenderer::FeedbackLevel::Failure);
				}
				else
				{
					bool equipped = m_itemSystem.GiveItem(*targetUnit, heldItem, m_itemDatabase, player.name);
					if (equipped)
					{
						wchar_t fb[192];
						swprintf_s(fb, L"装備: %hs -> %hs", heldItem->name.c_str(), targetUnit->def->name.c_str());
						m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);

						player.unclaimedItems.erase(player.unclaimedItems.begin() + m_heldUnclaimedIndex);
						m_heldUnclaimedIndex = -1;

						wchar_t log[224];
						swprintf_s(log, L"[Equip] %hs -> %hs (unclaimed left=%d, unit items=%d)\n",
							heldItem->name.c_str(), targetUnit->def->name.c_str(),
							(int)player.unclaimedItems.size(), (int)targetUnit->items.size());
						OutputDebugString(log);
					}
					else
					{
						m_shopUI.PushFeedback(L"装備できません (アイテム枠が満杯)", ShopUIRenderer::FeedbackLevel::Failure);
					}
				}
			}
			else
			{
				int shopIndex = (focus == InputFocus::Shop) ? m_cursorSelection.GetListCursorIndex() : 0;
				const UnitDef* target = (shopIndex >= 0 && shopIndex < (int)m_currentShop.size()) ? m_currentShop[shopIndex] : nullptr;
				bool success = target != nullptr && player.BuyUnit(target);

				wchar_t buf[256];
				swprintf_s(buf, L"Buy result: %hs, Shop index: %d, Gold left: %d, Bench count: %d\n",
					success ? "true" : "false", shopIndex, player.gold, (int)player.bench.size());
				OutputDebugString(buf);

				if (success)
				{
					wchar_t fb[128];
					swprintf_s(fb, L"購入: %hs  (-%dG)", target->name.c_str(), target->cost);
					m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);
				}
				else if (target != nullptr)
				{
					wchar_t fb[128];
					swprintf_s(fb, L"ゴールド不足: %hs は %dG 必要 (所持 %dG)", target->name.c_str(), target->cost, player.gold);
					m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Failure);
				}
			}
		}

		// Bボタンで次のフェーズに進む。
		if (g_pad[0]->IsTrigger(enButtonB))
		{
			m_currentShop.clear();
			m_heldUnclaimedIndex = -1; // 手に持ったままのアイテムは戦闘に持ち越さず、一覧へ戻す。
			m_gameState.currentPhase = Phase::Combat;
		}

		// Xボタンで、ベンチフォーカス中はカーソルが指しているユニットを、それ以外は0番目を、
		// 盤面フォーカス中はマウスホバー/矢印キーで選んだマスへ、それ以外は(0,0)へ配置する。
		if (g_pad[0]->IsTrigger(enButtonX))
		{
			Player& player = m_gameState.players[0];
			int benchIndex = (m_cursorSelection.GetFocus() == InputFocus::Bench) ? m_cursorSelection.GetListCursorIndex() : 0;
			HexCoord targetHex(0, 0);
			m_cursorSelection.GetHexCursor(targetHex); // 未選択ならデフォルトの(0,0)のまま。
			bool success = player.PlaceUnitOnBoard(benchIndex, targetHex);

			wchar_t buf[256];
			swprintf_s(buf, L"Place result: %hs, Bench index: %d, Hex: (%d,%d), Bench count: %d, Board count: %d\n",
				success ? "true" : "false", benchIndex, targetHex.q, targetHex.r, (int)player.bench.size(), (int)player.board.size());
			OutputDebugString(buf);

			if (success)
			{
				wchar_t fb[128];
				swprintf_s(fb, L"配置: マス(%d,%d)  盤面 %d/%d", targetHex.q, targetHex.r, (int)player.board.size(), player.GetMaxBoardSize());
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
			}
			else
			{
				m_shopUI.PushFeedback(L"配置できません (自陣 q0-2 のみ / 盤面上限 / 空きマス無し)", ShopUIRenderer::FeedbackLevel::Failure);
			}
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

				wchar_t fb[128];
				swprintf_s(fb, L"リロール (-%dG)  所持 %dG", kRerollCost, player.gold);
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
			}
			else
			{
				OutputDebugString(L"Not enough gold to reroll the shop.\n");

				wchar_t fb[128];
				swprintf_s(fb, L"ゴールド不足: リロールに %dG 必要 (所持 %dG)", kRerollCost, player.gold);
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Failure);
			}
		}

		// LB1ボタンで、ベンチフォーカス中はカーソルが指しているユニットを、それ以外は0番目を売却する。
		if (g_pad[0]->IsTrigger(enButtonLB1))
		{
			Player& player = m_gameState.players[0];
			int benchIndex = (m_cursorSelection.GetFocus() == InputFocus::Bench) ? m_cursorSelection.GetListCursorIndex() : 0;
			bool success = player.SellUnitFromBench(benchIndex);

			wchar_t buf[256];
			swprintf_s(buf, L"Sell result: %hs, Bench index: %d, Gold: %d, Bench count: %d\n",
				success ? "true" : "false", benchIndex, player.gold, (int)player.bench.size());
			OutputDebugString(buf);

			if (success)
			{
				wchar_t fb[128];
				swprintf_s(fb, L"売却  所持 %dG", player.gold);
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
			}
			else
			{
				m_shopUI.PushFeedback(L"売却できません (ベンチが空)", ShopUIRenderer::FeedbackLevel::Failure);
			}
		}

		// RB1ボタンで、ゴールドを払って経験値を購入する(LevelSystemの中でレベルアップ処理も行う)。
		if (g_pad[0]->IsTrigger(enButtonRB1))
		{
			Player& player = m_gameState.players[0];
			int levelBefore = player.level;
			bool success = m_levelSystem.BuyXP(player);

			wchar_t buf[256];
			swprintf_s(buf, L"Buy XP result: %hs, Gold: %d, Level: %d, XP: %d\n",
				success ? "true" : "false", player.gold, player.level, player.xp);
			OutputDebugString(buf);

			if (success)
			{
				wchar_t fb[128];
				if (player.level > levelBefore)
				{
					swprintf_s(fb, L"XP購入 +%d  ->  Lv %d! (XP %d/%d)",
						LevelSystem::kBuyXPAmount, player.level, player.xp, m_levelSystem.XPForNextLevel(player.level));
				}
				else
				{
					swprintf_s(fb, L"XP購入 +%d  (XP %d/%d)",
						LevelSystem::kBuyXPAmount, player.xp, m_levelSystem.XPForNextLevel(player.level));
				}
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);
			}
			else if (player.level >= LevelSystem::kMaxLevel)
			{
				m_shopUI.PushFeedback(L"既に最大レベルです", ShopUIRenderer::FeedbackLevel::Failure);
			}
			else
			{
				wchar_t fb[128];
				swprintf_s(fb, L"ゴールド不足: XP購入に %dG 必要 (所持 %dG)", LevelSystem::kBuyXPCost, player.gold);
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Failure);
			}
		}
	}
	else if (m_gameState.currentPhase == Phase::Combat && !m_combatSimDone)
	{
		// --- 戦闘フェーズ突入フレーム: シミュレーション(瞬時解決)と集計をここで1回だけ行う ---
		// フェーズ遷移はまだ行わず、遷移先を m_pendingPhaseAfterCombat に退避しておく。
		// 実際の遷移は、下の再生ブロックで CombatPlayback の再生が終わってから行う。
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
		m_lastCombatResult = result; // Phase::Result突入後、ResultUIRendererでの一言表示に使う。

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

		// 勝敗によるラウンド進行の確定と、再生完了後に遷移するフェーズの決定。
		// 「画面上何も見えないまま一瞬で終わる」のを避けるため、遷移自体は再生後まで遅延させる。
		Phase nextPhase = Phase::Result;
		if (result == CombatResult::Win)
		{
			wchar_t buf[128];
			swprintf_s(buf, L"=== Round %d Clear! ===\n", m_gameState.roundNumber);
			OutputDebugString(buf);

			// ラウンド勝利報酬: 未装備の素材アイテムを1つ入手する(準備フェーズでユニットに装備できる)。
			const ItemDef* reward = PickRandomComponent(m_itemDatabase);
			if (reward != nullptr)
			{
				player.unclaimedItems.push_back(reward);

				wchar_t rewardLog[192];
				swprintf_s(rewardLog, L"[Reward] Obtained item: %hs (unclaimed total=%d)\n",
					reward->name.c_str(), (int)player.unclaimedItems.size());
				OutputDebugString(rewardLog);

				wchar_t fb[128];
				swprintf_s(fb, L"アイテム入手: %hs", reward->name.c_str());
				m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);
			}

			m_gameState.lossCount = 0;
			m_gameState.roundNumber++;

			if (m_gameState.roundNumber > GameState::kTotalRounds)
			{
				OutputDebugString(L"=== ALL ROUNDS CLEARED! YOU WIN! ===\n");
				nextPhase = Phase::Victory;
			}
			else
			{
				nextPhase = Phase::Result;
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
				nextPhase = Phase::GameOver;
			}
			else
			{
				nextPhase = Phase::Result;
			}
		}

		m_pendingPhaseAfterCombat = nextPhase;

		// 戦闘の時系列再生を開始する(enemy盤面は再生側が必要な値をコピーする)。
		m_combatPlayback.Begin(player.board, player.name, enemy.board, enemy.name, m_combatEvents);
		m_combatSimDone = true;
	}
	else if (m_gameState.currentPhase == Phase::Combat)
	{
		// --- 戦闘フェーズ 再生中フレーム: クロックを進め、HPバー等の表示を追従させる ---
		m_combatPlayback.Update(g_gameTime->GetFrameDeltaTime());

		if (m_combatPlayback.IsFinished())
		{
			// 再生完了。盤面ユニットを配置位置へ戻し、退避しておいた遷移先へ進む。
			m_gameState.players[0].ResetBoardPositions();
			m_gameState.currentPhase = m_pendingPhaseAfterCombat;
			m_combatSimDone = false;

			if (m_gameState.currentPhase == Phase::Result)
			{
				// 「ROUND CLEAR!」/「DEFEAT...」を一定時間表示してからPreparationへ進む。
				m_resultPhaseTimer = kResultPhaseDurationSec;
			}
		}
	}
	else if (m_gameState.currentPhase == Phase::GameOver || m_gameState.currentPhase == Phase::Victory)
	{
		// ゲーム終了状態。Aボタンでリスタート(1プレイ分をリセットしてタイトルへ戻る)。
		if (g_pad[0]->IsTrigger(enButtonA))
		{
			InitializeNewRun();
			m_gameState.currentPhase = Phase::Title;
		}
	}
	else // Result
	{
		// ラウンドの進行(勝利時のroundNumber++・敗北時のlossCount++)はCombatフェーズ側で
		// 既に確定しているので、ここでは一言表示の残り時間を消化してから準備フェーズへ戻る。
		m_resultPhaseTimer -= g_gameTime->GetFrameDeltaTime();
		if (m_resultPhaseTimer <= 0.0f)
		{
			m_gameState.currentPhase = Phase::Preparation;
		}
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
	// タイトル画面中は盤面・各種HUDを一切出さず、タイトル文字列のみを表示する。
	if (m_gameState.currentPhase == Phase::Title)
	{
		m_titleUI.Draw(rc, g_gameTime->GetFrameDeltaTime());
		return;
	}

	m_hexGridRenderer.Draw(rc, m_gameState);

	m_unitModelDisplay.Draw(rc);

	// 所持ゴールド・レベル/XPゲージは、フェーズを問わず常時表示する
	// (準備フェーズ限定のShopUIRendererのヘッダー行とは別に、右上へ常設する)。
	{
		const Player& player = m_gameState.players[0];
		m_playerStatusUI.Draw(rc, player, m_levelSystem.XPForNextLevel(player.level));
	}

	// フェーズを問わず常時、画面右上にラウンド数・戦績(連敗カウント)を表示する。
	m_roundRecordUI.Draw(rc, m_gameState);

	// 準備フェーズのみ、画面下部にショップバーとベンチ一覧を表示する。
	if (m_gameState.currentPhase == Phase::Preparation)
	{
		const Player& player = m_gameState.players[0];
		bool shopFocused = m_cursorSelection.GetFocus() == InputFocus::Shop;
		int shopCursorIndex = m_cursorSelection.GetListCursorIndex();
		const int kRerollCost = 2; // Game::Update()のYボタン処理と同じ値。

		m_shopUI.Draw(
			rc,
			m_currentShop,
			player,
			m_levelSystem.XPForNextLevel(player.level),
			kRerollCost,
			LevelSystem::kBuyXPCost,
			shopFocused ? shopCursorIndex : -1,
			shopFocused);

		m_boardUI.DrawPreparation(rc, player);

		// 未装備アイテム一覧(画面右側)。Itemsフォーカス中のみカーソル位置を渡して強調する。
		bool itemsFocused = m_cursorSelection.GetFocus() == InputFocus::Items;
		m_itemInventoryUI.Draw(rc, player, itemsFocused,
			itemsFocused ? m_cursorSelection.GetListCursorIndex() : -1, m_heldUnclaimedIndex);
	}
	// 戦闘の再生中は、各ユニットの頭上にHPバーを表示する。
	else if (m_gameState.currentPhase == Phase::Combat && m_combatSimDone)
	{
		m_boardUI.DrawCombat(rc, m_combatPlayback);
	}
	// ラウンド結果の一言表示。盤面(直前の配置に戻したユニット)を背景にしたまま重ねて表示する。
	else if (m_gameState.currentPhase == Phase::Result)
	{
		m_resultUI.DrawRoundResult(rc, m_lastCombatResult);
	}
	// ゲームオーバー/ゲームクリア画面。こちらも盤面を背景に残したまま重ねて表示する。
	else if (m_gameState.currentPhase == Phase::GameOver)
	{
		m_resultUI.DrawGameOver(rc, m_gameState.roundNumber, GameState::kTotalRounds, g_gameTime->GetFrameDeltaTime());
	}
	else if (m_gameState.currentPhase == Phase::Victory)
	{
		m_resultUI.DrawVictory(rc, GameState::kTotalRounds, g_gameTime->GetFrameDeltaTime());
	}
}
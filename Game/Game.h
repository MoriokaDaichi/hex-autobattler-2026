#pragma once
#include "Level3DRender/LevelRender.h"
#include "GameState.h"
#include "UnitDatabase.h"
#include "ShopSystem.h"
#include "CombatEngine.h"
#include "TraitDatabase.h"
#include "TraitSystem.h"
#include "ItemDatabase.h"
#include "ItemSystem.h"
#include "StarLevelSystem.h"
#include "EconomySystem.h"
#include "LevelSystem.h"
#include "CombatEvent.h"
#include "CombatLogPrinter.h"
#include "CombatPlayback.h"
#include "EnemyStage.h"
#include "EnemyFactory.h"
#include "HexGridRenderer.h"
#include "UnitModelDisplay.h"
#include "ShopUIRenderer.h"
#include "BoardUIRenderer.h"
#include "PlayerStatusUIRenderer.h"
#include "RoundRecordUIRenderer.h"
#include "ItemInventoryUIRenderer.h"
#include "TitleUIRenderer.h"
#include "ResultUIRenderer.h"
#include "CursorSelectionSystem.h"

class Game : public IGameObject
{
public:
	Game() {}
	~Game() {}
	bool Start();
	void Update();
	void Render(RenderContext& rc);

private:
	std::vector<EnemyStage> BuildEnemyStages();

	/// <summary>
	/// 1プレイ分(GameState::players・roundNumber・lossCount等)を初期状態へ戻す。
	/// Start()の初回起動時と、GameOver/Victory画面からのリスタート時の両方から呼ぶ。
	/// UnitDatabase/TraitDatabase等の一度きりの初期化やm_enemyStagesの構築、カメラ・
	/// ライティング設定、Start()末尾のデバッグ用合成テストブロックはここに含めない
	/// (前者は不変のデータ、後者は別タスクで整理予定のためリスタート時は呼ばない)。
	/// </summary>
	void InitializeNewRun();

	GameState m_gameState;
	UnitDatabase m_unitDatabase;
	std::vector<const UnitDef*> m_currentShop;
	ShopSystem m_shopSystem;
	CombatEngine m_combatEngine;
	TraitDatabase m_traitDatabase;
	TraitSystem m_traitSystem;
	ItemDatabase m_itemDatabase;
	ItemSystem m_itemSystem;
	StarLevelSystem m_starLevelSystem;
	EconomySystem m_economySystem;
	LevelSystem m_levelSystem;
	CombatLogPrinter m_combatLogPrinter;
	std::vector<CombatEvent> m_combatEvents;
	EnemyFactory m_enemyFactory;
	std::vector<EnemyStage> m_enemyStages;
	HexGridRenderer m_hexGridRenderer;
	UnitModelDisplay m_unitModelDisplay; // players[0].boardのユニットを3D表示する。
	ShopUIRenderer m_shopUI; // 準備フェーズのショップバー(5枠のカード・操作フィードバック)を2D表示する。
	BoardUIRenderer m_boardUI; // 戦闘中のHPバー / 準備フェーズのベンチ一覧を2D表示する。
	PlayerStatusUIRenderer m_playerStatusUI; // 所持ゴールド・レベル/XPゲージをフェーズを問わず常時表示する。
	RoundRecordUIRenderer m_roundRecordUI; // 現在ラウンド数/残りラウンド数・連敗カウントを常時2D表示する。
	ItemInventoryUIRenderer m_itemInventoryUI; // 準備フェーズ、未装備アイテム一覧を画面右側に2D表示する。
	TitleUIRenderer m_titleUI; // タイトル画面(Phase::Title)のタイトル文字列・スタート操作ガイドを2D表示する。
	ResultUIRenderer m_resultUI; // ラウンド結果一言・ゲームオーバー/ゲームクリア画面を2D表示する。
	CursorSelectionSystem m_cursorSelection; // マウス・キーボード・ゲームパッドを横断するカーソル/選択状態。

	// 準備フェーズで「手に持っている」未装備アイテムの、players[0].unclaimedItems上のindex(-1で無し)。
	// Itemsフォーカス中にAで持ち、Bench/Boardのユニットを選んでAで装備確定するまでの一時状態。
	int m_heldUnclaimedIndex = -1;

	// 戦闘フェーズの複数フレーム化用。突入時に1回だけシミュレーション+集計を行い(m_combatSimDone=true)、
	// 以降は m_combatPlayback で時系列再生する。再生完了後に m_pendingPhaseAfterCombat へ遷移する。
	CombatPlayback m_combatPlayback;
	bool m_combatSimDone = false;
	Phase m_pendingPhaseAfterCombat = Phase::Result;

	// Phase::Resultの表示・滞在時間管理用。Combatフェーズ解決時にその回の勝敗を控えておき、
	// CombatPlayback再生完了でPhase::Resultへ入った瞬間にタイマーを開始する。
	CombatResult m_lastCombatResult = CombatResult::Win;
	float m_resultPhaseTimer = 0.0f;
};
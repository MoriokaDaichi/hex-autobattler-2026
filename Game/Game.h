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
	CursorSelectionSystem m_cursorSelection; // マウス・キーボード・ゲームパッドを横断するカーソル/選択状態。

	// 戦闘フェーズの複数フレーム化用。突入時に1回だけシミュレーション+集計を行い(m_combatSimDone=true)、
	// 以降は m_combatPlayback で時系列再生する。再生完了後に m_pendingPhaseAfterCombat へ遷移する。
	CombatPlayback m_combatPlayback;
	bool m_combatSimDone = false;
	Phase m_pendingPhaseAfterCombat = Phase::Result;
};
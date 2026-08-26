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
#include "EnemyStage.h"
#include "EnemyFactory.h"
#include "HexGridRenderer.h"
#include "UnitModelDisplay.h"

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
};
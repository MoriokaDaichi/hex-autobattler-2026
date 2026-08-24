#pragma once
#include "Player.h"
#include "EnemyStage.h"
#include "UnitDatabase.h"
#include "ItemDatabase.h"
#include "ItemSystem.h"

/// <summary>
/// EnemyStage(固定編成データ)から、戦闘直前に敵の盤面を即座に組み立てるクラス。
/// BotAIのようなショップ購入シミュレーションは行わず、指定されたスターレベル・位置・アイテムで
/// 完成済みのユニットをそのまま並べる。呼び出す側(Game::Update)が戦闘のたびに新しく生成することで、
/// 前回の戦闘結果(HP減少・移動)を持ち越さないようにする。
/// </summary>
class EnemyFactory
{
public:
	Player CreateEnemyBoard(const EnemyStage& stage, const UnitDatabase& unitDatabase,
		const ItemDatabase& itemDatabase, ItemSystem& itemSystem) const
	{
		Player enemy(stage.name);

		for (const auto& unitPlan : stage.units)
		{
			const UnitDef* def = unitDatabase.FindUnitDefByName(unitPlan.unitName);
			if (def == nullptr) continue;

			UnitInstance unit(def);
			unit.starLevel = unitPlan.starLevel;
			unit.position = unitPlan.position;
			unit.homePosition = unitPlan.position;

			for (const std::string& componentName : unitPlan.itemComponentNames)
			{
				const ItemDef* component = itemDatabase.FindItemDefByName(componentName);
				itemSystem.GiveItem(unit, component, itemDatabase, enemy.name);
			}

			enemy.board.push_back(unit);
		}

		return enemy;
	}
};

#pragma once
#include <string>
#include <vector>
#include "HexCoord.h"

/// <summary>
/// 固定敵編成のうち、1体分のユニット配置データ。BotUnitPlanと違い「徐々に買い集める目標」ではなく
/// 「戦闘開始時に即座にこの状態で並べる」ためのデータなので、minLevelは持たない。
/// </summary>
struct EnemyUnitPlan
{
	std::string unitName;                        // UnitDatabase上の名前と一致させる。
	int starLevel = 1;                            // 生成時に直接設定するスターレベル(合成シミュレーションは行わない)。
	HexCoord position;                            // 盤面上で配置するマス。
	std::vector<std::string> itemComponentNames;  // 持たせる素材アイテム名(2つ指定すると完成アイテムへ自動合成される)。
};

/// <summary>
/// 1ラウンド分の固定敵編成。EnemyFactoryがこのデータから即座に敵の盤面を組み立てる。
/// </summary>
struct EnemyStage
{
	std::string name;
	std::vector<EnemyUnitPlan> units;
};

#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"

struct UnitDef;
struct Player;
struct GameState;
class UnitDatabase;
class ItemDatabase;
class TraitDatabase;
class TraitSystem;

/// <summary>
/// UIHotRegion(+関連データ)から、ツールチップに表示するテキスト行を組み立てるヘルパー群。
/// 描画は一切行わない(TooltipUIRendererの担当)。既存の各UI Rendererに個別実装されていた
/// 短縮表記ヘルパー(UITextUtil)を再利用し、装備品/スキル/トレイトの内容を"詳しく"文章化する
/// (各UI Rendererの短縮表示より詳細な情報を出す点が違い)。
/// docs/tasks/ui-mouse-cards/plan.md §3-2参照。
/// </summary>
namespace TooltipContentBuilder
{
	/// <summary>
	/// 指定のUIHotRegionに対応するツールチップの表示行を組み立てる。対応する内容が無ければ
	/// 空のvectorを返す(呼び出し側はその場合ツールチップを表示しない)。
	/// </summary>
	std::vector<std::wstring> Build(
		const UIHotRegion& region,
		const std::vector<const UnitDef*>& shop,
		const Player& player,
		const GameState& gameState,
		int xpForNextLevel,
		const UnitDatabase& unitDatabase,
		const ItemDatabase& itemDatabase,
		const TraitDatabase& traitDatabase,
		const TraitSystem& traitSystem);
}

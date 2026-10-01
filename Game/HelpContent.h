#pragma once
#include <string>
#include <vector>

class TraitDatabase;
class ItemDatabase;

/// <summary>
/// ヘルプパネル(HelpUIRenderer)に表示する説明文を組み立てる(help-panel)。
/// 描画は持たない純粋なデータ生成。数値はEconomySystem/LevelSystem/ShopSystem/ItemSystem/
/// StarLevelSystem/GameStateの定数と、TraitDatabase/ItemDatabaseの中身から生成するため、
/// 定数・マスターデータを変えればヘルプも自動で追従する(docs/tasks/help-panel/plan.md §3)。
///
/// 注意: myfile.spritefontに無い文字(★ → × … ● など)は「?」で表示されるため使わない
/// (plan.md §7)。矢印は"->"、星は「星2」、倍は"x"を使う。
/// </summary>
namespace HelpContent
{
	/// <summary>1行分。headingなら見出しとして強調色・字下げ無しで描く。空文字は空行。</summary>
	struct Line
	{
		std::wstring text;
		bool heading = false;
	};

	/// <summary>1カテゴリ(左側のタブ1つ)分。</summary>
	struct Category
	{
		std::wstring title;
		std::vector<Line> lines;
	};

	/// <summary>全カテゴリを表示順で返す。Game::Start()でデータベース初期化後に1回呼ぶ想定。</summary>
	std::vector<Category> Build(const TraitDatabase& traitDatabase, const ItemDatabase& itemDatabase);
}

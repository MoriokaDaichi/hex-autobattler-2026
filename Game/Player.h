#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "UnitInstance.h"

/// <summary>
/// 1人のプレイヤーの状態。
/// </summary>
struct Player
{
	std::string name;
	int hp = 100;
	int gold = 0;
	int level = 1;
	int xp = 0; // 現在のレベルに到達してからの経験値。LevelSystemが加算・レベルアップ処理を行う。

	int winStreak = 0;  // 連勝数(EconomySystemが更新する)。
	int lossStreak = 0; // 連敗数(EconomySystemが更新する)。

	/// <summary>
	/// 現在のレベルで盤面に置けるユニット数の上限。TFT同様、レベルの値がそのまま上限になる
	/// (レベル1なら1体、レベル9なら9体まで)。
	/// </summary>
	int GetMaxBoardSize() const { return level; }

	std::vector<UnitInstance> bench;   // 控え(まだ盤面に出していないユニット)
	std::vector<UnitInstance> board;   // 盤面に配置済みのユニット

	// まだどのユニットにも装備していない、入手済みアイテム(ラウンド勝利報酬で増える)。
	// 準備フェーズでプレイヤーが選び、ItemSystem::GiveItemでbench/boardのユニットへ移す。
	std::vector<const ItemDef*> unclaimedItems;

	Player() = default;
	Player(const std::string& playerName) : name(playerName) {}

	bool IsAlive() const { return hp > 0; }

	/// <summary>
	/// boardの各ユニットの位置を、配置した時点の位置(homePosition)に戻す。
	/// 戦闘中の移動をリセットするため、毎ラウンドの戦闘開始前に呼ぶことを想定している。
	/// </summary>
	void ResetBoardPositions()
	{
		for (auto& unit : board)
		{
			unit.position = unit.homePosition;
		}
	}

	bool BuyUnit(const UnitDef* def)
	{
		if (def == nullptr) return false;
		if (gold < def->cost) return false; // ゴールドが足りない。

		gold -= def->cost;
		bench.push_back(UnitInstance(def));

		while (TryMergeUnits()) {} // 同じユニットが3体そろっていれば、上位スターへ自動合成する。

		return true;
	}

	// プレイヤーがユニットを配置できる自陣のaxial q範囲。
	// 盤面の区分けの正はHexGridRenderer(kAllyZoneMinQ/kAllyZoneMaxQ、0-2:自陣 / 3-5:中立 / 6-8:敵陣)。
	// HexGridRenderer.h → GameState.h → Player.h の循環includeを避けるため、同値をここへ再掲している。
	static constexpr int kAllyZoneMinQ = 0;
	static constexpr int kAllyZoneMaxQ = 2;

	bool PlaceUnitOnBoard(int benchIndex, const HexCoord& targetPos)
	{
		if (benchIndex < 0 || benchIndex >= (int)bench.size())
		{
			return false; // ベンチの範囲外
		}

		if (targetPos.q < kAllyZoneMinQ || targetPos.q > kAllyZoneMaxQ)
		{
			return false; // 自陣(q0-2)以外には配置できない。
		}

		if ((int)board.size() >= GetMaxBoardSize())
		{
			return false; // レベルによる盤面上限に達している。
		}

		// 既にそのマスに誰かいないか確認する。
		for (const auto& unit : board)
		{
			if (unit.position == targetPos)
			{
				return false; // そのマスは既に埋まっている
			}
		}

		// ベンチから取り出して、盤面用の位置を設定してboardに追加する。
		UnitInstance unit = bench[benchIndex];
		unit.position = targetPos;
		unit.homePosition = targetPos; // 毎ラウンド戦闘開始前に戻る先として記録しておく。
		board.push_back(unit);

		// ベンチから削除する。
		bench.erase(bench.begin() + benchIndex);

		while (TryMergeUnits()) {} // 同じユニットが3体そろっていれば、上位スターへ自動合成する。

		return true;
	}

	/// <summary>
	/// 盤面の指定マスに居るユニットへのポインタを返す(居なければ nullptr)。
	/// Game::Update() が「X で盤面ユニットを拾うのか、従来のベンチ配置なのか」を
	/// 判定するために使う。
	/// </summary>
	const UnitInstance* FindBoardUnitAt(const HexCoord& pos) const
	{
		for (const auto& unit : board)
		{
			if (unit.position == pos)
			{
				return &unit;
			}
		}
		return nullptr;
	}

	/// <summary>
	/// 盤面上のユニットを from マスから to マスへ移動する。
	/// to は自陣(q0-2)かつ空きマスであること。成功で true。
	/// 盤面ユニット数は増減しないため GetMaxBoardSize チェックは行わない。
	/// </summary>
	bool MoveUnitOnBoard(const HexCoord& from, const HexCoord& to)
	{
		if (from == to)
		{
			return false; // 同じマスへの移動は無効(呼び出し側でキャンセル扱い)。
		}

		if (to.q < kAllyZoneMinQ || to.q > kAllyZoneMaxQ)
		{
			return false; // 自陣(q0-2)以外へは移動できない。
		}

		int fromIndex = -1;
		for (int i = 0; i < (int)board.size(); ++i)
		{
			if (board[i].position == to)
			{
				return false; // 移動先が他ユニットで埋まっている。
			}
			if (board[i].position == from)
			{
				fromIndex = i;
			}
		}
		if (fromIndex < 0)
		{
			return false; // from に自分のユニットが居ない。
		}

		board[fromIndex].position = to;
		board[fromIndex].homePosition = to; // 毎ラウンド戦闘開始前に戻る先も更新する。
		return true;
	}

	/// <summary>
	/// 盤面上の from マスのユニットをベンチへ戻す。成功で true。
	/// ベンチ枚数上限は設けない(BuyUnit と整合)。
	/// </summary>
	bool ReturnUnitToBench(const HexCoord& from)
	{
		for (int i = 0; i < (int)board.size(); ++i)
		{
			if (board[i].position == from)
			{
				bench.push_back(board[i]);
				board.erase(board.begin() + i);
				// 盤面⇔ベンチをまたぐ同一ユニット3体は購入/配置時点で必ず合成済みのため、
				// ここで新たな3体そろいは通常発生しないが、既存 PlaceUnitOnBoard と同じ
				// 防御的呼び出しとして残す。
				while (TryMergeUnits()) {}
				return true;
			}
		}
		return false;
	}

	/// <summary>
	/// ユニット1体分の売却額を返す。★2は素材3体分、★3は9体分の価値として扱う
	/// (3体合成でスターアップする仕組みと整合させている)。
	/// </summary>
	int CalculateSellValue(const UnitInstance& unit) const
	{
		int multiplier = 1;
		for (int i = 1; i < unit.starLevel; ++i)
		{
			multiplier *= 3;
		}
		return unit.def->cost * multiplier;
	}

	/// <summary>
	/// ベンチのindex番目のユニットを売却し、ゴールドを得る。
	/// </summary>
	bool SellUnitFromBench(size_t index)
	{
		if (index >= bench.size()) return false;

		gold += CalculateSellValue(bench[index]);
		bench.erase(bench.begin() + index);
		return true;
	}

	/// <summary>
	/// 盤面のindex番目のユニットを売却し、ゴールドを得る。
	/// </summary>
	bool SellUnitFromBoard(size_t index)
	{
		if (index >= board.size()) return false;

		gold += CalculateSellValue(board[index]);
		board.erase(board.begin() + index);
		return true;
	}

	/// <summary>
	/// bench/boardをまたいで、同じユニット(同じUnitDef・同じスターレベル)が3体そろっている
	/// 組み合わせを1つ探し、見つかれば3体を消して1体上位のスターレベルのユニットに置き換える。
	/// 合成できた場合はtrueを返す(呼び出し側でtrueが返る間ループすれば、3→2スターの連鎖合成にも対応できる)。
	/// 3体のうち盤面(board)にいたものがあれば、合成後のユニットはその位置にそのまま配置される。
	/// </summary>
	bool TryMergeUnits()
	{
		struct Location { bool onBoard; size_t index; };
		std::map<std::pair<const UnitDef*, int>, std::vector<Location>> groups;

		for (size_t i = 0; i < bench.size(); ++i)
		{
			groups[{ bench[i].def, bench[i].starLevel }].push_back({ false, i });
		}
		for (size_t i = 0; i < board.size(); ++i)
		{
			groups[{ board[i].def, board[i].starLevel }].push_back({ true, i });
		}

		for (auto& entry : groups)
		{
			std::vector<Location>& locations = entry.second;
			if (locations.size() < 3) continue;

			const UnitDef* def = entry.first.first;
			int newStarLevel = entry.first.second + 1;

			bool placeOnBoard = false;
			HexCoord mergedPos;
			std::vector<size_t> boardIndices, benchIndices;
			for (int i = 0; i < 3; ++i)
			{
				if (locations[i].onBoard)
				{
					placeOnBoard = true;
					mergedPos = board[locations[i].index].homePosition; // 戦闘中の移動先ではなく、配置した元のマスを引き継ぐ。
					boardIndices.push_back(locations[i].index);
				}
				else
				{
					benchIndices.push_back(locations[i].index);
				}
			}

			// インデックスの大きい方から削除しないと、削除のたびに残りのインデックスがずれてしまう。
			std::sort(boardIndices.rbegin(), boardIndices.rend());
			std::sort(benchIndices.rbegin(), benchIndices.rend());
			for (size_t idx : boardIndices) board.erase(board.begin() + idx);
			for (size_t idx : benchIndices) bench.erase(bench.begin() + idx);

			UnitInstance merged(def);
			merged.starLevel = newStarLevel;

			if (placeOnBoard)
			{
				merged.position = mergedPos;
				merged.homePosition = mergedPos;
				board.push_back(merged);
			}
			else
			{
				bench.push_back(merged);
			}

			return true;
		}

		return false;
	}
};


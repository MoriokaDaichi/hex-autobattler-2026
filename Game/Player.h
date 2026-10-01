#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "UnitInstance.h"
#include "ItemSystem.h"

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

	// 売却・合成で装備がアイテム欄へ戻った/引き継がれた時の通知文(item-carryover)。
	// Player側は積むだけで、Game::Update()が次フレーム頭でShopUIRendererのフィードバックへ流してclearする。
	std::vector<std::wstring> itemNotices;

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

	// プレイヤーがユニットを配置できる自陣のaxial r範囲(board-layout-rework)。
	// 盤面の区分けの正はHexGridRenderer(kAllyZoneMinR/kAllyZoneMaxR、r0-2:プレイヤー陣地 / r3-5:敵陣地)。
	// HexGridRenderer.h → GameState.h → Player.h の循環includeを避けるため、同値をここへ再掲している。
	static constexpr int kAllyZoneMinR = 0;
	static constexpr int kAllyZoneMaxR = 2;

	bool PlaceUnitOnBoard(int benchIndex, const HexCoord& targetPos)
	{
		if (benchIndex < 0 || benchIndex >= (int)bench.size())
		{
			return false; // ベンチの範囲外
		}

		if (targetPos.r < kAllyZoneMinR || targetPos.r > kAllyZoneMaxR)
		{
			return false; // 自陣(手前3行 r0-2)以外には配置できない。
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
	/// FindBoardUnitAt()の非const版。アイテム装備等、対象ユニットを書き換える必要がある
	/// 呼び出し元(Game::Update()のAボタン/マウス左クリック処理)向け。
	/// </summary>
	UnitInstance* FindBoardUnitAt(const HexCoord& pos)
	{
		for (auto& unit : board)
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
	/// to は自陣(手前3行 r0-2)かつ空きマスであること。成功で true。
	/// 盤面ユニット数は増減しないため GetMaxBoardSize チェックは行わない。
	/// </summary>
	bool MoveUnitOnBoard(const HexCoord& from, const HexCoord& to)
	{
		if (from == to)
		{
			return false; // 同じマスへの移動は無効(呼び出し側でキャンセル扱い)。
		}

		if (to.r < kAllyZoneMinR || to.r > kAllyZoneMaxR)
		{
			return false; // 自陣(手前3行 r0-2)以外へは移動できない。
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
	/// ベンチの benchIndex 番目のユニットと、盤面 boardPos のユニットを入れ替える(drag-and-drop)。
	/// ベンチのユニットは boardPos へ配置され、盤面にいたユニットは同じベンチ位置へ入る。
	/// 盤面のユニット数は変わらないため GetMaxBoardSize チェックは行わない。成功で true。
	/// </summary>
	bool SwapBenchWithBoard(int benchIndex, const HexCoord& boardPos)
	{
		if (benchIndex < 0 || benchIndex >= (int)bench.size())
		{
			return false; // ベンチの範囲外
		}

		for (auto& unit : board)
		{
			if (unit.position == boardPos)
			{
				UnitInstance fromBench = bench[benchIndex];
				fromBench.position = boardPos;
				fromBench.homePosition = boardPos; // 毎ラウンド戦闘開始前に戻る先として記録しておく。

				bench[benchIndex] = unit; // 盤面にいた方はベンチの同じ位置へ(ベンチでは位置を使わない)。
				unit = fromBench;

				while (TryMergeUnits()) {} // PlaceUnitOnBoard と同じ防御的呼び出し。
				return true;
			}
		}
		return false; // boardPos にユニットが居ない(空きマスならPlaceUnitOnBoardを使う)。
	}

	/// <summary>
	/// 盤面上の a マスと b マスのユニットの位置を入れ替える(drag-and-drop)。
	/// 両方にユニットが居ること。成功で true。
	/// </summary>
	bool SwapBoardUnits(const HexCoord& a, const HexCoord& b)
	{
		if (a == b) return false;

		UnitInstance* unitA = FindBoardUnitAt(a);
		UnitInstance* unitB = FindBoardUnitAt(b);
		if (unitA == nullptr || unitB == nullptr) return false;

		unitA->position = b;
		unitA->homePosition = b;
		unitB->position = a;
		unitB->homePosition = a;
		return true;
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

		ReturnItemsToUnclaimed(bench[index]); // 装備はアイテム欄へ戻す(item-carryover)。
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

		ReturnItemsToUnclaimed(board[index]); // 装備はアイテム欄へ戻す(item-carryover)。
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
			int survivorSlot = 0; // 合成後に「残る1体」とみなす素材(位置を引き継ぐ盤面の1体。盤面に居なければ先頭)。
			std::vector<size_t> boardIndices, benchIndices;
			for (int i = 0; i < 3; ++i)
			{
				if (locations[i].onBoard)
				{
					placeOnBoard = true;
					mergedPos = board[locations[i].index].homePosition; // 戦闘中の移動先ではなく、配置した元のマスを引き継ぐ。
					survivorSlot = i;
					boardIndices.push_back(locations[i].index);
				}
				else
				{
					benchIndices.push_back(locations[i].index);
				}
			}

			// 装備の引き継ぎ(item-carryover): 残る1体の既存装備 → 他の2体の装備の順。erase前に控えておく。
			auto unitAt = [&](const Location& loc) -> const UnitInstance&
			{
				return loc.onBoard ? board[loc.index] : bench[loc.index];
			};
			std::vector<const ItemDef*> survivorItems = unitAt(locations[survivorSlot]).items;
			std::vector<const ItemDef*> otherItems;
			for (int i = 0; i < 3; ++i)
			{
				if (i == survivorSlot) continue;
				const auto& items = unitAt(locations[i]).items;
				otherItems.insert(otherItems.end(), items.begin(), items.end());
			}

			// インデックスの大きい方から削除しないと、削除のたびに残りのインデックスがずれてしまう。
			std::sort(boardIndices.rbegin(), boardIndices.rend());
			std::sort(benchIndices.rbegin(), benchIndices.rend());
			for (size_t idx : boardIndices) board.erase(board.begin() + idx);
			for (size_t idx : benchIndices) bench.erase(bench.begin() + idx);

			UnitInstance merged(def);
			merged.starLevel = newStarLevel;

			// 残る1体の装備はそのまま、他2体の装備はGiveItemの規則(素材の自動合成・上限)で追加し、
			// 装備しきれなかった分はアイテム欄へ戻す。
			merged.items = survivorItems;
			std::vector<const ItemDef*> overflow;
			ItemSystem::CarryOverItems(merged, otherItems, overflow, name);
			unclaimedItems.insert(unclaimedItems.end(), overflow.begin(), overflow.end());
			NotifyMergeCarryOver(merged, survivorItems.size() + otherItems.size(), overflow);

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

private:
	static std::wstring JoinItemNames(const std::vector<const ItemDef*>& items)
	{
		std::wstring text;
		for (const ItemDef* item : items)
		{
			if (item == nullptr) continue;
			if (!text.empty()) text += L", ";
			text += std::wstring(item->name.begin(), item->name.end()); // アイテム名はASCII。
		}
		return text;
	}

	/// <summary>
	/// 売却されるunitの装備を全てunclaimedItemsの末尾へ戻し、通知を積む(item-carryover)。
	/// </summary>
	void ReturnItemsToUnclaimed(UnitInstance& unit)
	{
		if (unit.items.empty()) return;

		std::wstring names = JoinItemNames(unit.items);
		unclaimedItems.insert(unclaimedItems.end(), unit.items.begin(), unit.items.end());
		unit.items.clear();

		itemNotices.push_back(L"売却: 装備をアイテム欄に戻しました (" + names + L")");

		std::wstring log = L"[" + std::wstring(name.begin(), name.end()) + L"] sell: items returned to unclaimed ("
			+ names + L")\n";
		OutputDebugString(log.c_str());
	}

	/// <summary>
	/// 合成時の装備引き継ぎ結果を通知・ログに出す。素材3体とも装備が無ければ何もしない(item-carryover)。
	/// </summary>
	void NotifyMergeCarryOver(const UnitInstance& merged, size_t carriedCount, const std::vector<const ItemDef*>& overflow)
	{
		if (carriedCount == 0) return;

		wchar_t head[64];
		swprintf_s(head, L"合成★%d: ", merged.starLevel);
		std::wstring notice = head;
		if (!merged.items.empty())
		{
			notice += L"装備を引き継ぎました (" + JoinItemNames(merged.items) + L")";
		}
		if (!overflow.empty())
		{
			if (!merged.items.empty()) notice += L" / ";
			notice += L"枠超過でアイテム欄へ (" + JoinItemNames(overflow) + L")";
		}
		itemNotices.push_back(notice);

		std::wstring log = L"[" + std::wstring(name.begin(), name.end()) + L"] merge "
			+ std::wstring(merged.def->name.begin(), merged.def->name.end())
			+ L": items=(" + JoinItemNames(merged.items) + L") overflow=(" + JoinItemNames(overflow) + L")\n";
		OutputDebugString(log.c_str());
	}
};


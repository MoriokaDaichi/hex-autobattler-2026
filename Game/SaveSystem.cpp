#include "stdafx.h"
#include "SaveSystem.h"
#include "GameState.h"
#include "Player.h"
#include "UnitInstance.h"
#include "UnitDef.h"
#include "ItemDef.h"
#include "UnitDatabase.h"
#include "ItemDatabase.h"
#include <fstream>
#include <string>

namespace
{
	// セーブファイル先頭のマジック文字列とフォーマットバージョン。
	// どちらかが一致しなければロードを失敗扱いにする(古い/壊れたファイルを弾く)。
	const char* kMagic = "HEXARENA_SAVE";
	const int kVersion = 1;

	// 想定しうる要素数の上限。壊れたファイルで巨大ループ/巨大確保に走らないための歯止め。
	const int kMaxUnitsPerList = 64;
	const int kMaxItemsPerUnit = 16;
	const int kMaxUnclaimedItems = 64;

	void WriteUnit(std::ostream& os, const UnitInstance& u)
	{
		os << "unit "
			<< (u.def ? u.def->name : std::string("?")) << ' '
			<< u.starLevel << ' '
			<< u.currentHP << ' '
			<< u.position.q << ' ' << u.position.r << ' '
			<< u.homePosition.q << ' ' << u.homePosition.r << ' '
			<< u.items.size();
		for (const ItemDef* item : u.items)
		{
			os << ' ' << (item ? item->name : std::string("?"));
		}
		os << '\n';
	}

	/// <summary>
	/// "unit ..." 行を1つ読む。トークン列が壊れていれば false(ロード全体を失敗させる)。
	/// ユニット名が DB に無い場合は、トークンだけ消費して outResolved=false を返す
	/// (呼び出し側でそのユニットをスキップする。ストリーム位置はずれない)。
	/// </summary>
	bool ReadUnit(std::istream& is, const UnitDatabase& unitDb, const ItemDatabase& itemDb,
		UnitInstance& outUnit, bool& outResolved)
	{
		std::string tag;
		if (!(is >> tag) || tag != "unit") return false;

		std::string name;
		int starLevel = 1;
		int currentHP = 0;
		int pq = 0, pr = 0, hq = 0, hr = 0;
		int itemCount = 0;
		if (!(is >> name >> starLevel >> currentHP >> pq >> pr >> hq >> hr >> itemCount)) return false;
		if (itemCount < 0 || itemCount > kMaxItemsPerUnit) return false;

		const UnitDef* def = unitDb.FindUnitDefByName(name);
		outResolved = (def != nullptr);

		UnitInstance unit;
		if (def != nullptr)
		{
			unit = UnitInstance(def);
		}
		unit.starLevel = starLevel;
		unit.currentHP = currentHP;
		unit.position = HexCoord(pq, pr);
		unit.homePosition = HexCoord(hq, hr);

		for (int i = 0; i < itemCount; ++i)
		{
			std::string itemName;
			if (!(is >> itemName)) return false;

			const ItemDef* item = itemDb.FindItemDefByName(itemName);
			if (item != nullptr && def != nullptr)
			{
				unit.items.push_back(item);
			}
		}

		outUnit = std::move(unit);
		return true;
	}
}

bool SaveSystem::SaveFileExists() const
{
	std::ifstream file(kSaveFilePath);
	return file.good();
}

bool SaveSystem::Save(const GameState& state) const
{
	if (state.players.empty()) return false;
	const Player& p = state.players[0];

	std::ofstream os(kSaveFilePath, std::ios::out | std::ios::trunc);
	if (!os) return false;

	os << kMagic << ' ' << kVersion << '\n';
	os << "round " << state.roundNumber << '\n';
	os << "loss " << state.lossCount << '\n';
	os << "player " << p.gold << ' ' << p.level << ' ' << p.xp << ' '
		<< p.winStreak << ' ' << p.lossStreak << '\n';

	os << "bench " << p.bench.size() << '\n';
	for (const auto& u : p.bench) WriteUnit(os, u);

	os << "board " << p.board.size() << '\n';
	for (const auto& u : p.board) WriteUnit(os, u);

	os << "unclaimed " << p.unclaimedItems.size();
	for (const ItemDef* item : p.unclaimedItems)
	{
		os << ' ' << (item ? item->name : std::string("?"));
	}
	os << '\n';

	return os.good();
}

bool SaveSystem::Load(GameState& state, const UnitDatabase& unitDb, const ItemDatabase& itemDb) const
{
	std::ifstream is(kSaveFilePath);
	if (!is) return false;

	std::string magic;
	int version = 0;
	if (!(is >> magic >> version) || magic != kMagic || version != kVersion) return false;

	std::string tag;
	int roundNumber = 1;
	int lossCount = 0;
	int gold = 0, level = 1, xp = 0, winStreak = 0, lossStreak = 0;

	if (!(is >> tag >> roundNumber) || tag != "round") return false;
	if (!(is >> tag >> lossCount) || tag != "loss") return false;
	if (!(is >> tag >> gold >> level >> xp >> winStreak >> lossStreak) || tag != "player") return false;

	// roundNumber は m_enemyStages[roundNumber-1] の添字に使われるため厳密に検証する。
	if (roundNumber < 1 || roundNumber > GameState::kTotalRounds) return false;
	if (level < 1) level = 1;
	if (lossCount < 0) lossCount = 0;

	Player player("You");
	player.gold = gold;
	player.level = level;
	player.xp = xp;
	player.winStreak = winStreak;
	player.lossStreak = lossStreak;

	int benchCount = 0;
	if (!(is >> tag >> benchCount) || tag != "bench" || benchCount < 0 || benchCount > kMaxUnitsPerList) return false;
	for (int i = 0; i < benchCount; ++i)
	{
		UnitInstance u;
		bool resolved = false;
		if (!ReadUnit(is, unitDb, itemDb, u, resolved)) return false;
		if (resolved) player.bench.push_back(std::move(u));
	}

	int boardCount = 0;
	if (!(is >> tag >> boardCount) || tag != "board" || boardCount < 0 || boardCount > kMaxUnitsPerList) return false;
	for (int i = 0; i < boardCount; ++i)
	{
		UnitInstance u;
		bool resolved = false;
		if (!ReadUnit(is, unitDb, itemDb, u, resolved)) return false;
		if (resolved) player.board.push_back(std::move(u));
	}

	int unclaimedCount = 0;
	if (!(is >> tag >> unclaimedCount) || tag != "unclaimed" || unclaimedCount < 0 || unclaimedCount > kMaxUnclaimedItems) return false;
	for (int i = 0; i < unclaimedCount; ++i)
	{
		std::string itemName;
		if (!(is >> itemName)) return false;

		const ItemDef* item = itemDb.FindItemDefByName(itemName);
		if (item != nullptr) player.unclaimedItems.push_back(item);
	}

	// ここまで到達したら成功。ここで初めて state を書き換える(途中失敗時は state を汚さない)。
	state.players.clear();
	state.players.push_back(std::move(player));
	state.roundNumber = roundNumber;
	state.lossCount = lossCount;
	state.currentPhase = Phase::Preparation;

	const Player& loaded = state.players[0];
	wchar_t log[256];
	swprintf_s(log, L"[Load] round=%d loss=%d gold=%d lv=%d xp=%d bench=%d board=%d unclaimed=%d\n",
		roundNumber, lossCount, loaded.gold, loaded.level, loaded.xp,
		(int)loaded.bench.size(), (int)loaded.board.size(), (int)loaded.unclaimedItems.size());
	OutputDebugString(log);

	return true;
}

#pragma once

struct GameState;
class UnitDatabase;
class ItemDatabase;

/// <summary>
/// GameState(1プレイ分の進行状況)を自前のテキスト形式でファイルへ保存・復元する
/// ステートレスなサービス。外部ライブラリは使わず <fstream>/<sstream> のみで実装する。
///
/// 保存対象は players[0](ゴールド/レベル/XP/連勝連敗/bench/board/未装備アイテム)と
/// roundNumber・lossCount。UnitDef/ItemDef への生ポインタは永続化できないため、
/// 名前で書き出し、ロード時に UnitDatabase/ItemDatabase から引き直して再結線する。
///
/// セーブタイミングは準備フェーズ中の手動セーブ(Game 側で F5 に割り当て)、
/// ロードはタイトル画面から行う想定。
/// </summary>
class SaveSystem
{
public:
	// 実行時の作業ディレクトリからの相対パス(VS 実行時は Game/ 配下に置かれる)。
	static constexpr const char* kSaveFilePath = "savedata.txt";

	/// <summary>セーブファイルが存在し読み取り可能かを返す。</summary>
	bool SaveFileExists() const;

	/// <summary>
	/// state.players[0] と roundNumber/lossCount を kSaveFilePath へ書き出す。
	/// players が空の場合や書き込みに失敗した場合は false。
	/// </summary>
	bool Save(const GameState& state) const;

	/// <summary>
	/// kSaveFilePath を読み、state.players を作り直して players[0] を復元、
	/// roundNumber/lossCount を復元し、currentPhase を Phase::Preparation にする。
	/// ファイル無し・マジック/バージョン不一致・フォーマット不正の場合は false を返し、state は変更しない。
	/// DB に存在しないユニット名/アイテム名はその要素だけスキップしてロードを継続する。
	/// </summary>
	bool Load(GameState& state, const UnitDatabase& unitDatabase, const ItemDatabase& itemDatabase) const;
};

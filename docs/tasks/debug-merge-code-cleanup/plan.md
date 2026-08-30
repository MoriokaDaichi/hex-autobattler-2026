# 設計: デバッグ用の合成ロジック確認コードの整理

## 変更ファイル

### `Game/Game.cpp` — `Game::Start()` のみ

削除対象は2箇所(いずれも `Game::Start()` 内、`m_enemyStages = BuildEnemyStages();` の前後)。

1. デバッグブロック本体(コメント3行 + 実処理):

   ```cpp
   // --- デバッグ用: 合成ロジックの動作確認。ベンチにSlimeを3体追加すると、
   // Player::TryMergeUnitsにより自動的に★2のSlime1体(ベンチ)にまとまるはず。
   // (盤面は空のままなので、アイテム付与やスターアップの見た目確認は通常のプレイ操作で行う)。
   const UnitDef* slimeDef = m_unitDatabase.FindUnitDefByName("Slime");
   player.bench.push_back(UnitInstance(slimeDef));
   player.bench.push_back(UnitInstance(slimeDef));
   player.bench.push_back(UnitInstance(slimeDef));
   while (player.TryMergeUnits()) {}

   wchar_t mergeLogBuf[256];
   swprintf_s(mergeLogBuf, L"[DEBUG] After merge test: bench count=%d\n", (int)player.bench.size());
   OutputDebugString(mergeLogBuf);
   ```

2. このブロック専用にしか使われていないローカル変数:

   ```cpp
   Player& player = m_gameState.players[0];
   ```

   (`InitializeNewRun();` と `m_enemyStages = BuildEnemyStages();` の間にある行。削除後、
   `Game::Start()` 内で `player` を参照する箇所は無くなる。)

## 削除後の `Game::Start()` 末尾の姿

```cpp
	InitializeNewRun();

	// 10ラウンド分の固定敵編成をあらかじめ組み立てておく。各ラウンドの戦闘直前に
	// EnemyFactoryがこのデータから即座に敵の盤面を生成する(BotAIのような段階的購入は行わない)。
	m_enemyStages = BuildEnemyStages();

	return true;
```

## 影響範囲

- ヘッダー変更なし。他 `.cpp` 変更なし。`Game.vcxproj` 変更なし。
- `Game::Start()` は `main.cpp` の `NewGO<Game>` 経由で1回だけ呼ばれる。削除により初期ベンチが
  空になる以外の挙動変化はない。
- `#include` の増減なし(`UnitDef.h` / `UnitInstance.h` / `Player.h` は他所でも使用)。

## 確認手順

1. `Game/Game.sln` を Debug/x64 でビルド → 0 error、警告を増やさない(未使用変数 C4189 を出さない)。
2. 既存の他デバッグ/検証コードの有無を `Game.cpp` 全体の目視 + `grep`(デバッグ|検証|DEBUG|TODO|FIXME|HACK|暫定|仮)で確認。
   - 現時点の grep 結果: 当該ブロックのみ。通常の状態遷移ログ(`OutputDebugString`)は設計上の表示手段のため対象外。

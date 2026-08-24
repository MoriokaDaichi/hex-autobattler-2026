# CLAUDE.md

このファイルは、Claude Code (claude.ai/code) がこのリポジトリで作業する際のガイドです。

## これは何か

自作の階層型エンジン（ゲーム専門学校のカリキュラム由来の「k2Engine」）の上に構築された、Windows/DirectX12のC++ゲームです。実際のゲーム本体――ヘックスグリッド戦闘を伴うTFT／オートバトラー風のユニット収集ゲーム――は全て `Game/` に置かれています。それ以外はエンジン／ミドルウェアであり、編集する必要はほとんどありません。

## タスク管理

このプロジェクトのタスクはNotionで管理されています。作業指示もNotion上のタスクを読み取って実行する形をとっているため、作業を始める前にNotion側の該当タスクを確認してください。

## ビルド

このリポジトリにはCLIビルドスクリプト・パッケージマネージャー・テストフレームワークはありません。普通のVisual Studioソリューションです。

- `Game/Game.sln` をVisual Studio（ツールセット v145、Windows SDK 10.0）で開き、`Game` プロジェクト（スタートアッププロジェクトに設定済み）をビルド／実行する。構成: `Debug`, `Preview`, `Release`。プラットフォーム: `x64`（メイン）と `x86`。
- コマンドライン版: `msbuild Game/Game.sln /p:Configuration=Debug /p:Platform=x64`
- 自動テストは存在しません。変更の確認は、ゲームを実行(F5)して該当するゲームパッド操作を行うか、デバッガの出力ウィンドウに出る `OutputDebugString` のログを読むことで行います（このゲームは画面上にUIテキストを描く代わりに、状態遷移の大半をここに出力しています）。

`Game.sln` は以下の順に依存関係を解決してビルドします: `DirectXTK12`、`BulletCollision`/`LinearMath`/`BulletDynamics`（Bullet Physics）→ `k2EngineLow` → `k2Engine` → `Game`。

## 階層構造

```
Game/          自作のゲームコード（stdafx.h経由でnsK2Engine + nsK2EngineLowをusingしているためグローバル名前空間）
k2Engine/      高レベルエンジン: 描画パイプライン、ライト、ポストエフェクト、レベル/マップチップ描画、カメラ、コリジョン（名前空間 nsK2Engine）
k2EngineLow/   低レベルエンジン: DirectX12デバイス/リソース、GameObjectシステム、入力(HID)、サウンド、AIパスファインディング、数学ライブラリ（名前空間 nsK2EngineLow）
k2EngineLow/ExEngine/  外部から取り込んだサードパーティミドルウェア（Bullet Physics、Effekseer、DirectXTK、Photon/cocos2dのデモ等）― 変更しないこと
tools/         独立したエディタツール群（例: シェーダーノードエディタ k2SLEditor）
```

`Game/stdafx.h` は `k2EnginePreCompile.h` を取り込み、`using namespace nsK2EngineLow; using namespace nsK2Engine;` を行っているため、ゲームコードからはエンジンの型を名前空間修飾なしで参照できます。

### エンジンのエントリポイントとゲームループ

- `Game/main.cpp` が `InitGame(...)`（`Game/system/system.cpp` 内）を呼び出し、続けて `NewGO<Game>(0, "game")` でルートとなるゲームオブジェクトを生成し、その後 `while (DispatchWindowMessage()) { ... K2Engine::GetInstance()->Execute(); }` というループに入ります。
- `K2Engine`（`k2Engine/k2Engine.h`）は、`K2EngineLow`・`CollisionObjectManager`・`RenderingEngine` を内包するシングルトンです。`K2Engine::Execute()` が毎フレームの処理を駆動します。
- すべてのゲームエンティティは `IGameObject`（`k2EngineLow/gameObject/IGameobject.h`）を継承し、仮想関数 `Start()/Update()/Render()` を持ちます。これらは `NewGO<T>(...)` で生成され、`CGameObjectManager` によって管理されます。`CGameObjectManager` は `m_isActive`/`m_isStart`/`m_isDead` を見て呼び出しを制御する `*Wrapper()` 系メソッド経由で各処理を呼びます。
- 独自の描画クラス（例: `HexGridRenderer`）は `IRenderer` を実装し、`RenderingEngine` のパイプライン（`OnForwardRender`/`OnTlanslucentRender`）に登録する形で描画します。`Game::Render()` から直接ドローコールを発行しないのは、その後に走る `RenderingEngine::Execute()` に上書きされてしまうためです。

## Game/ ― オートバトラー本体

`Game.h`/`Game.cpp` がルートの `IGameObject` です。`Game::Update()` はフェーズ管理のステートマシン（`Phase::Preparation → Combat → Result`、終了状態として `GameOver`/`Victory`）になっており、ゲームパッド入力（`g_pad[0]`、ボタンA/B/X/Y/LB1/RB1）で直接駆動されます。メニューやUI層はまだ無く、`OutputDebugString` によるログ出力のみです。

主要な型とその関係:

- **`GameState`** ― 全体状態: `players`（`players[0]` のみ使用――人間プレイヤー1人 vs スクリプト敵）、`roundNumber`、`lossCount`、`currentPhase`。
- **`Player`** ― 片陣営のユニット群（`bench`=未配置、`board`=盤面配置済みの `UnitInstance`）、ゴールド、レベル/経験値を保持。ユニット経済まわりの操作（`BuyUnit`、`PlaceUnitOnBoard`、`Sell*`、`TryMergeUnits`）を持つ。`TryMergeUnits` は同じユニット・同じスターが3体そろうと自動的に上位スターへ合成する処理で、bench/boardが変化するたびにチェックされる。
- **`UnitDef`**（`UnitDatabase` が持つマスターデータ）と **`UnitInstance`**（`starLevel`・`currentHP`・位置・毎ラウンド再計算される `bonus*` 系ステータスを持つ実体）の分離パターンが随所で使われている（`ItemDef`/`TraitDef` と、その適用済み効果の関係も同様）。
- **`HexCoord`** ― axial座標系のヘックスグリッド座標。盤面はq:0〜8, r:0〜2（0〜2が自陣、3〜5が中立地帯、6〜8が敵陣）で、`ToWorldPosition()` でワールド座標に変換される。
- `Game::Update()` の `Combat` フェーズにおける1ラウンド分の処理順序: `EnemyFactory::CreateEnemyBoard`（`Game::BuildEnemyStages()` が返す、あらかじめ手で組んだ固定の `EnemyStage` テーブルから構築――ゴールドを消費して段階的に購入していくようなAIではない）→ `Player::ResetBoardPositions()` → `TraitSystem::ApplyTraitBonuses` → `ItemSystem::ApplyItemBonuses` → `StarLevelSystem::ApplyStarBonuses`（これらはいずれも `bonus*` 系フィールドを再計算し `currentHP` を全回復させるため、呼び出し順が重要）→ `CombatEngine::SimulateCombat`（描画を持たない純粋なシミュレーションで、`std::vector<CombatEvent>` を出力する）→ `CombatLogPrinter::Print`（イベント列をテキストとして `OutputDebugString` に出す。シミュレーションと表示をあえて分離した設計）→ `EconomySystem`/`LevelSystem` がラウンド終了時のゴールド/経験値を反映。
- **`CombatEngine`** はユニットごとの `nextActionTime` という内部時計を使ったイベント駆動シミュレーションで（固定の行動順ではない）、両陣営が「ウェーブ」単位で交互に行動する。移動は最も近い敵に向かう貪欲な隣接ヘックス移動のみ（本格的な経路探索・障害物回避は無し）。通常攻撃か必殺技かは、ゲージ（`normalAttackCount + receivedAttackCount`）と `skillThreshold` の比較で決まる。
- **`TraitSystem`**/**`ItemSystem`**/**`StarLevelSystem`** はいずれもステートレスなサービスで、盤面と該当するデータベースを受け取り、毎ラウンド各ユニットの `bonus*` 系フィールドをゼロから再計算する（持続・蓄積するバフは無い）。
- データ定義用のヘッダー（`TraitDef.h`, `ItemDef.h`, `AttackType.h`, `TraitType.h`, `StatEffect.h`, `EnemyStage.h`）や一部のシステム系ヘッダーには対応する `.cpp` が存在しない。ヘッダーオンリー（構造体定義、または完全にインライン実装されたクラス）のためで、実装ファイルが欠落しているわけではない。

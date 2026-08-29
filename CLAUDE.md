# CLAUDE.md

このファイルは、Claude Code (claude.ai/code) がこのリポジトリで作業する際のガイドです。

## これは何か

自作の階層型エンジン（ゲーム専門学校のカリキュラム由来の「k2Engine」）の上に構築された、Windows/DirectX12のC++ゲームです。実際のゲーム本体――ヘックスグリッド戦闘を伴うTFT／オートバトラー風のユニット収集ゲーム――は全て `Game/` に置かれています。それ以外はエンジン／ミドルウェアであり、編集する必要はほとんどありません。

## タスク管理

このプロジェクトのタスクはNotionで管理されています。作業指示もNotion上のタスクを読み取って実行する形をとっているため、作業を始める前にNotion側の該当タスクを確認してください。

## 作業の進め方（マネージャー/サブエージェント体制）

このリポジトリでの作業では、Claude（自分）は「マネージャー」として文脈把握・作業委譲・節目の判断を担当し、実作業（コード編集・ビルド確認・スクリプト実行・調査など）はAgentツールで起動するサブエージェントに任せる。これはユーザーの事前承認済みの標準運用であり、都度「サブエージェントを使って」と指示される必要はない。ただし、1ファイルを読むだけのような些細な確認まで無理に委譲する必要はない。

- **着手前の自動確認**: Notion側の該当タスクに加え、作業対象領域に「次セッション引継ぎ用」の資料（`HANDOFF.md`等）が存在する場合は、着手前に必ず読む。ユーザーから「引継ぎ資料を確認して」と言われるのを待たない。
- **委譲時**: 背景・目的・対象ファイル・既に分かっていることを含めた自己完結したプロンプトをサブエージェントに渡す。サブエージェントの報告は鵜呑みにせず、重要な変更は実際に差分やビルド結果を確認してから完了と判断する。
- **「ひと段落」時の自動フロー**: ユーザーの合図を待たず、タスクが完了した・意味のある区切りに達したとClaudeが判断した時点で、以下を自動的に行う。
  1. （3Dモデルパイプライン作業等、対応する引継ぎ資料がある場合）該当スキル（例: `update-model-handoff`）でHANDOFF.md/WORKLOG.mdを更新する。
  2. `git-commit-ja`スキルでコミットを自動実行する（確認不要）。
  3. pushは自動実行せず、コミット後に「pushしますか？」とユーザーに一声かけてから実行する。
- **別セッションで進行中の作業の把握**: Agentツールのサブエージェントだけでなく、`ListAgents`に見える他のインタラクティブなClaudeセッション（このマシン上の別ウィンドウ等）が並行して作業していることがある。それらを直接操作せず、`SendMessage`の`notify_when_idle`でアイドル通知を登録し、通知が届いたら（同じ作業ツリーを触っている場合の衝突を避けつつ）その完了を上記の「ひと段落」時の自動フローに組み込んで引き取る。完了時は`PushNotification`でデスクトップ+スマホに知らせる（ユーザーが離席していても気づけるように）。
- **役割分担された常設セッション体制**: ユーザーが「実装用」「デバッグ・確認用」「Notion進捗管理用」「Git作業用」「外部調査用」という役割ごとの専用セッションを別ウィンドウで立ち上げておく運用を取ることがある。この場合Claude（自分）はこれらに指示を出す純粋なマネージャーに徹し、実装作業は実装用セッションへ、ビルド後の画面表示確認等は デバッグ・確認用セッションへ、Notionのステータス更新はNotion進捗管理用セッションへ、`git add`/コミット/pushはGit作業用セッションへ、他ゲーム・外部サイト・動画等の調査は外部調査用セッションへ、それぞれ`SendMessage`で依頼する（自分で直接手を動かさない）。実装用は並列実行可能なタスクであれば複数セッションに分担してよい。同じファイルを複数セッションが同時に編集すると衝突するため、対象が重なる場合は`git worktree add`で別ワークツリー（別ブランチ）を切って並行作業させ、完了後にGit作業用セッションでmainへマージする。
  - **セッション名の固定化(重要)**: 素の`claude`起動だと再起動のたびに名前が`gametemplate-xx`のようなランダムなハッシュに変わり、毎回`ListAgents`で全セッションに役割を尋ね直す必要があった。これを避けるため、各ワーカーは`claude --name worker-impl`（実装用）/`claude --name worker-debug`（デバッグ・確認用）/`claude --name worker-notion`（Notion進捗管理用）/`claude --name worker-git`（Git作業用）/`claude --name worker-research`（外部調査用）のように`--name`（`-n`）を付けて起動してもらう運用にする。これは同じセッションを復元するわけではなく毎回新規セッションだが、名前は固定されるため`ListAgents`で毎回同じ名前を頼りに直接メッセージを送れる（役割を尋ね直す手間が不要になる）。ユーザーにまだこの命名で起動されていないセッションが混ざっている場合のみ、これまで通り役割を尋ねて確認する。
    - **`--name`未使用セッションの再識別(フォールバック)**: セッションが利用上限やクラッシュで停止し、ユーザー操作（またはツール側の自動再起動）で`--name`無しの新規プロセスとして立ち上がり直すことがあり、これは自分（マネージャー）からは制御できない。この場合、ユーザーにいちいち名前を確認させるのではなく、まず自分から`ListAgents`に出てくる見慣れない名前のセッションへ「状況確認」の一言（自己完結した依頼内容の再掲つき）を送り、相手が保持している会話履歴から役割・進捗を自己申告させて識別する。これで大抵は解決する。それでも特定できない場合のみ、ユーザーに該当ウィンドウの表示名を尋ねる。
  - **push承認の伝達**: pushについて、Git作業用セッションがユーザー本人からの直接確認を求めることがある。その場合、マネージャーが会話内でユーザーから得たpushの承認をそのまま伝えればよく、ユーザーにそのセッションのウィンドウへ直接入力させる必要はない（ユーザーの明示指示）。ただし、これはpush操作に限った話であり、CLAUDE.md自体の変更や権限設定など、他のリスクのある操作にまで一般化しない。
  - **外部調査用セッション**: 実装内容の参考にするため他ゲーム・外部サイト（OPGG等）・YouTube動画等を調査させる役割。調査結果は会話に流すだけでなく、`docs/research/<テーマ名>.md`にログとして必ず保存する（例: `docs/research/TFT.md`）。同じテーマを後日追加調査した場合は同じファイルに追記していく。
  4. 日本語で簡潔に、何を更新・コミット（・push）したかを報告する。
- **ユーザー不在時(就寝中等)の自動進行**: 「寝るので夜間は自動でタスクを進めて」のような指示を受けた場合、Notionタスクキューを締切順に、上記の役割分担・レビューチェーンに沿って自動で進めて構わない。判断が必要な細かい点（実装方針の些細な選択等）は都度確認を待たず、自分で妥当なデフォルトを選んで進めてよい。ただし**pushの都度確認ルールはこの「自動で進めて」という指示によって免除されない**（一般的な自動化の許可を、個別具体のpush承認の代わりに解釈してはならない）。コミットまでは進めてよいが、pushはユーザーが戻って直接確認するまで保留し、ユーザー復帰時にまとめて何が完了・コミット済みでpush待ちかを報告する。

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

# combat-movement-playback — 要件定義 (intent.md)

## 背景

F5 プレイで「戦闘フェーズ中、ユニットがほとんど動かない」という指摘。コード調査の結論:

- **`CombatPlayback` が `Move` イベントを再生していない**。[`Game/CombatPlayback.cpp`](../../../Game/CombatPlayback.cpp) の
  `ApplyEvent()` で `case CombatEventType::Move:` は明示的に `break`（コメント「位置再生はスコープ外」）。
  各ユニットの表示ワールド座標は `Begin()` で1回だけ `homePosition` から設定され、以降不変。
- 戦闘中に描かれる3Dモデルの位置:
  - **プレイヤーモデル**: [`Game/Game.cpp`](../../../Game/Game.cpp) 冒頭で毎フレーム
    `m_unitModelDisplay.Update(m_gameState.players[0].board)`。その `board[i].position` は、戦闘突入フレームの
    `m_combatEngine.SimulateCombat(player, enemy, ...)`（`player` は `m_gameState.players[0]` への参照）が
    **最終位置へ一括で書き換える**。→ 戦闘開始時に1回だけワープして以降停止。再生終了時に
    `ResetBoardPositions()` で配置位置へ戻る。
  - **敵モデル**: `m_enemyModelDisplay.Update(m_enemyPreview.board)`。`m_enemyPreview` は sim が使う
    ローカル `enemy` とは**別オブジェクト**（ラウンド変化時に `EnemyFactory::CreateEnemyBoard` で作り直す
    プレビュー専用）。sim に一切触られない → 戦闘中まったく静止。
  - **HPバー/ゲージ**: `CombatPlayback::UnitView` 経由で正しく時間追従するが、`worldPos` は
    `homePosition`（`CalcTileCenter`）に固定（「1v1で両者が同じマスに寄りバーが重なるのを避けるため
    位置はアニメーションしない前提」というコメントあり）。
- **board-layout-rework で悪化**: 以前はプレイヤー(q0-2) と敵(q6-8) の間に広い中立ゾーンがあり sim が
  大きな移動を生成 → 開始時のワープが目に見えた。現在は 2 ゾーンが隣接 (r2/r3、ギャップ無し) で前列が
  最初から攻撃射程内 → 移動がほぼ発生せず、その 1 回のワープすら見えない。
- **`CombatEvent`（`Move`）が移動先を記録していない**。[`Game/CombatEvent.h`](../../../Game/CombatEvent.h) の
  `Move` は `beforeValue = 開始距離` / `afterValue = 最終距離` のみ。移動後の `HexCoord` を持たないため、
  イベント列だけでは再生側がユニットをどこへ動かせばよいか分からない。

## 目的

戦闘フェーズの再生中に、ユニットが**実際に盤面上を移動して見える**ようにする。ユーザー選択スコープは
**B（見た目の仕上げまで）**:

- ヘックス間の位置補間（カクカクではなく滑らかに歩く）。
- 進行方向 / 攻撃対象への向き（yaw 回転）。
- アニメーションクリップの切り替え（`UnitModelDisplay` は既に idle/move/normalAttack/skill/death の
  5 クリップを index 0-4 でロード済み。現状 `PlayAnimation` は一度も呼ばれていない）。
  - 移動中: move クリップ / 攻撃ヒット時: normalAttack or skill を単発 / 撃破時: death → 以後停止(または最終姿勢で静止) / それ以外: idle。
- プレイヤー・敵の**両方**が動く（敵モデルも再生駆動に統一する）。
- HPバー/ゲージが移動するユニットの頭上に追従する。

## スコープ（In）

### 1. Move イベントに移動先を持たせる

- `CombatEvent` に移動元・移動先ヘックスを追加（例: `int fromQ, fromR, toQ, toR;`、または `HexCoord` を
  持たせる。`CombatEvent.h` は現在 `HexCoord.h` を include していないので追加要）。
- `CombatEngine::MoveTowards()` は既に 1 歩ずつ経路を決めている。ループ後の `attacker.position` を
  移動先として `Move` イベントに書き込む。移動元はループ開始前の位置。
  - 中間ステップまで個別イベント化するかは plan.md で判断（B は補間するので「開始→終了」の 2 点で十分。
    ただし障害物迂回で経路が曲がるケースを綺麗に見せたいなら中間点も持たせてよい）。
- **`SimulateCombat` の解決ロジック（行動順・ダメージ・勝敗）は一切変更しない**。Move イベントへの
  フィールド追加と書き込みのみ。既存の `CombatLogPrinter` / ログ出力の互換も保つ。

### 2. CombatPlayback で位置を再生する

- `UnitView` に「現在の論理ヘックス」と「補間中の移動情報（移動元/先ワールド座標・移動開始クロック・
  所要時間）」を追加。`worldPos` は毎 `Update()` で補間結果に更新する。初期値は `homePosition`。
- `Move` イベント到達時に移動先をセットして補間を開始。補間は再生クロック `m_clock`（`deltaTime * m_speed`）
  基準で行い、総尺 `kTargetPlaybackSeconds`(6s) + `kTailSeconds`(1s) をはみ出さないこと。隣接 1 マスの
  移動所要は短め（戦闘内時刻で 0.15〜0.3s 目安、plan.md で確定）にして、多段移動でも次の行動時刻前に
  到着し切れるようにする。
- 攻撃・被弾・回復・シールド・撃破・ゲージ変化の反映は現状どおり index 解決で不変。
- `UnitView` にアニメーション状態のヒント（今 idle か move か、直近で attack/death をトリガーしたか）を
  持たせ、`UnitModelDisplay` がそれを読んでクリップを切り替えられるようにする。
- 敵モデルを再生駆動にするため、`GetUnitViews()` をプレイヤー分 / 敵分に区別して取れるようにする
  （`m_playerCount` で分割できる。アクセサを足すか、既存の添字規約を呼び出し側に明示する）。
- **`UnitView` にモデル識別情報が足りない**問題: 現状 `UnitView` は `name`(wstring) と `starLevel` しか
  持たず、`UnitDef*` / `modelPath` / アニメパスが無い。`UnitModelDisplay` がモデルをロード・対応付け
  するには不足。`UnitView` に `const UnitDef*`（または必要な path 群）を持たせる。plan.md で最小の
  追加セットを決める。

### 3. UnitModelDisplay を再生駆動できるようにする

- 戦闘フェーズ中は、`players[0].board` / `m_enemyPreview.board` ではなく **`CombatPlayback` の
  ユニットビュー（ワールド座標・向き・アニメ状態）から**モデルの TRS とアニメを更新する新経路を足す
  （準備/結果フェーズは既存の board 駆動のまま）。
  - 案: `UnitModelDisplay::UpdateFromPlayback(const std::vector<CombatPlayback::UnitView>& views)` を追加し、
    `Game` がプレイヤー用ビュー slice を `m_unitModelDisplay` に、敵用 slice を `m_enemyModelDisplay` に渡す。
    モデルの再構築（`RebuildIfBoardChanged` 相当）は「ビューの構成（UnitDef*+star の並び）」で判定。
- 向き: 移動中は進行方向、静止攻撃中は対象方向へ yaw を向ける。基準として敵は -Z（プレイヤー側）、
  プレイヤーは +Z を向く。現状 `SetTRS(worldPos, Quaternion::Identity, scale)` の回転を差し替える。
- アニメ: `ModelRender::PlayAnimation` 系 API（index 指定・補間時間・ループ有無）を使う。実装者は
  `ModelRender` の実際のアニメ API を確認して plan.md に書くこと（クリップ配列は `Init` に渡し済み）。
  - 撃破後: death クリップ再生 → 完了後はドローから外す or 最終フレームで静止（ゴーストにならないこと）。
- Y 持ち上げ（`kUnitModelHalfHeightAtScale1`）とスケール（`kUnitModelScale` 4.0 + star 倍率）は現状踏襲。

### 4. HPバー/ゲージの追従

- `BoardUIRenderer::DrawCombat` はバー位置を `UnitView.worldPos`（＝補間後の位置）から取る。
  `homePosition` 固定をやめる。
- 1v1 終盤などで両者が同一マスへ寄るとバーが重なる問題への対処を plan.md で決める
  （陣営で頭上バーの Y をずらす / わずかに XZ をずらす / 重なりは許容 のいずれか）。

### 5. カメラ・その他確認

- board-layout-rework で再設定済みのカメラで、動くユニットが両ゾーンとも画角に収まるか F5 で確認。
  必要なら微調整のみ（カメラ挙動の新規実装はしない）。

## スコープ（Out）

- `CombatEngine` の経路探索・行動順・ダメージ式・勝敗判定・バランスの変更 → 一切しない（Move イベントへの
  データ追加を除く）。
- 新規 VFX / パーティクル / ヒットストップ / 画面シェイク → 対象外。
- 戦闘 SE / BGM → 対象外（別タスク「サウンド実装」）。
- 攻撃の弾・飛び道具の軌跡表示 → 対象外。
- 準備フェーズでのモデル表示・タイトル/GameOver のゴースト対策（board-layout-rework §D）の仕様変更
  → 対象外（回帰させないことのみ）。
- ネットワーク / リプレイ保存 → 無関係。

## 受け入れ条件

- 戦闘再生中、プレイヤー・敵の**両陣営**のユニットが、ヘックス間を滑らかに移動して対象へ近づき、
  射程内で攻撃する様子が見える。多段移動でも次の行動までに到着し切る。
- 移動中は move、攻撃ヒット時は normalAttack / skill、撃破時は death のアニメーションが再生され、
  それ以外は idle。ユニットは進行方向 / 攻撃対象の方を向く。
- HPバー/ゲージが移動するユニットの頭上に追従する。バーが読めなくなる深刻な重なりが無い。
- 撃破されたユニットは death 後に画面から消える（または最終姿勢で静止）。Title/GameOver/Victory に
  前プレイのモデル・シルエットが残らない（board-layout-rework §D の回帰なし）。
- 再生は従来どおり `IsFinished()` で終了し、`m_pendingPhaseAfterCombat`（Result/GameOver/Victory）へ
  遷移する。総再生尺は概ね 6〜8 秒（`kTargetPlaybackSeconds` + `kTailSeconds` 準拠、大幅に伸びない）。
- 既存のゲームパッド操作・キーボード（F5 等）が従来どおり動く。準備/結果フェーズの表示に回帰なし。
- `Debug|x64` ビルドが 0 エラー。新規コード起因の警告なし。実機 F5 で各フェーズの表示が破綻しない。

## 制約・注意

- 独自 Renderer は既存の `IRenderer` パイプラインに乗せる。`Game::Render()` から直接ドローコールを撃たない。
- `SimulateCombat` は「1 フレーム瞬時解決」を維持。`CombatPlayback` はその結果イベント列を読むだけ、を維持。
- 同じ `CombatEvent::time` のイベントは同フレームでまとめて反映する既存規約を維持（`CombatEvent.h` メモ参照）。
- `UIRectRenderer` はプール方式。`Game::Render()` 冒頭の `BeginFrame()` を維持。`g_camera2D` は書き換えない。
- ソースは UTF-8 (BOM 付き)。`docs/tasks/` の Markdown は BOM 無しで可。
- `Game/Assets/modelData/*.dds` の phantom modified は commit も discard もしない。main マージ時に abort する
  ことがある（対処は Git 作業用が把握済み）。
- 作業は **worktree `feature/combat-movement-playback`** で。フェーズ引き継ぎは
  `docs/tasks/combat-movement-playback/` の Markdown で自己完結（intent.md → plan.md → 実装 → レビュー）。
- **plan.md の以下は実装細部の裁量を超える設計判断。案を固めたうえで着手前に game-ae（マネージャー）の
  レビューを挟むこと:**
  1. Move イベントの持たせ方（開始/終了の 2 点か、中間ステップも持つか）。
  2. `CombatPlayback` の位置補間モデルと移動所要時間（再生尺 6s + tail 1s をはみ出さない設計）。
  3. `UnitView` に足すモデル識別情報の最小セットと、`UnitModelDisplay` の再生駆動経路の形。
  4. 同一マス集中時の HPバー重なり対処方針。

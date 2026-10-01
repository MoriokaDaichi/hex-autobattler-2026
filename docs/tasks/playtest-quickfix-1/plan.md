# playtest-quickfix-1 — 設計 (plan.md)

背景・要求・受け入れ条件は [`intent.md`](intent.md) を参照。このファイルだけで実装/レビューに着手できるよう、
調査結果と具体設計(変更ファイル・方針・既知の制約への対応・未解決の懸念)をまとめる。

---

## A. 連勝/連敗HUDの先バレ

### 調査結果
- `Game::Update()` の「戦闘フェーズ突入フレーム」ブロック(`m_combatSimDone == false`)で、
  `SimulateCombat` の直後に以下を**全て即時に**実行していた:
  - `EconomySystem::GrantRoundIncome`(`winStreak`/`lossStreak` 更新 + ゴールド加算)
  - `LevelSystem::GrantRoundXP`(XP/レベル)
  - 勝利時: 素材アイテム報酬(`unclaimedItems` 追加 + ショップフィードバック)、`lossCount = 0`、`roundNumber++`
  - 敗北時: `lossCount++`
- HUD(`RoundRecordUIRenderer` = ROUND/残り/連勝連敗/連敗N/M、`PlayerStatusUIRenderer` = GOLD/LV/XP)は
  フェーズを問わず毎フレーム `m_gameState` を読むため、再生開始フレームから**連勝連敗・ゴールド・XP・
  ROUND番号・連敗カウント**が全て結果を先バレさせていた(連勝連敗だけでなく、勝利時は ROUND が先に進む)。

### 方針
- 結果の**判定**(`CombatResult`、`m_lastCombatResult`)は従来通り突入フレームで行う(ログ・Result表示用)。
- 結果の**反映**(上記の全ての状態変更)を新設の `Phase Game::ApplyCombatOutcome(CombatResult)` へ移し、
  `CombatPlayback::IsFinished()` になった(=再生+余韻が終わった)フレームで呼ぶ。戻り値が遷移先フェーズ。
- 遷移先を退避していた `m_pendingPhaseAfterCombat` は不要になるため削除する(遷移先は反映時に決まる)。
- 経済計算(利子)は `player.gold` を参照するが、戦闘再生中はゴールドを変える操作が無いため、
  反映を遅らせても計算結果は同一。

### 変更ファイル
- `Game/Game.h`: `ApplyCombatOutcome` 宣言追加、`m_pendingPhaseAfterCombat` 削除、コメント更新。
- `Game/Game.cpp`: 突入フレームから反映処理を切り出し、再生完了ブロックで呼ぶ。`InitializeNewRun` から
  `m_pendingPhaseAfterCombat` のリセットを削除。

---

## B. ホバー時ステータスに補正が反映されない

### 調査結果
- `TooltipContentBuilder::AppendUnitInstanceLines` は `UnitDef` の素の値 + `unit.bonus*` を表示している。
- `bonus*` は戦闘突入時の `Trait→Item→Star` の Apply* でしか更新されないため、準備フェーズでは
  「前回戦闘時点の値(初回は0)」が残っている。ベンチへ戻したユニットも古い値を持ったまま。
- 各 Apply* は `currentHP` 全回復・`shieldAmount` リセットの副作用と、`OutputDebugString` のログ出力を伴う。

### 方針(実盤面に触れない「写し」での再計算)
- ツールチップ組み立て時に、**プレイヤー盤面の写し(`std::vector<UnitInstance>`)**に対して戦闘突入時と
  同じ順序 `ApplyTraitBonuses → ApplyItemBonuses → ApplyStarBonuses` を実行し、その写しの値を表示する。
  - 実際の `players[0].board/bench` は一切変更しないため、`currentHP` 全回復の副作用が戦闘開始時の状態や
    HP表示を壊すことは無い(戦闘突入時は従来通り本物の盤面で Apply* される)。
  - 盤面/ベンチ/装備が変わればツールチップは毎フレーム最新状態から組み立て直されるため、
    「変化したら再計算」を満たす。盤面は最大9体程度なので毎フレームのコピー+計算は軽い。
- ベンチのユニット: トレイトは盤面にいるユニットにしか発動しないため、`bonus*` をゼロクリアした1体分の写しに
  `Item → Star` だけ適用する(戦闘に出ていない=トレイト補正は無い、と注記を出す)。
- 毎フレームの再計算でログが洪水にならないよう、3つの Apply* に `bool logEnabled = true` の
  デフォルト引数を追加し、プレビュー計算時だけ `false` を渡す(既存呼び出しは挙動不変)。
- 表示形式は「基礎値+補正」: `HP650+520 AT55+44 AP0 物防20+16 魔防20+16` / `攻撃速度0.80+0.10/s`。
  補正0の項目は基礎値のみ。補正が1つでもあれば「(基礎+補正: トレイト/アイテム/星込み)」の凡例行を出す。
  必殺技の必要回数に補正(`bonusSkillThreshold`)がある場合も表示する。

### 変更ファイル
- `Game/TraitSystem.h` / `Game/ItemSystem.h` / `Game/StarLevelSystem.h`: Apply* に `logEnabled` 引数追加。
- `Game/TooltipContentBuilder.cpp`: プレビュー計算ヘルパー追加、ユニット行の表示を基礎+補正形式に変更。
  `Build()` のシグネチャは変えない(必要な `TraitDatabase` は既に受け取っている)。

---

## C. 戦闘開始前の敵ユニットの向き

### 調査結果
- 準備/結果フェーズ(board 駆動)の `UnitModelDisplay::Update()` は `SetTRS(pos, Quaternion::Identity, ...)`。
- 戦闘再生(`UpdateFromPlayback`)は `SetRotationY(atan2(dir.x, dir.z) + kModelYawOffsetRad)` で向きを決める
  (`kModelYawOffsetRad = 0`)。カメラは -Z 側(手前)から +Z を見ており、自陣 r0-2 が手前(-Z)、敵陣 r3-5 が奥(+Z)。
- Identity = +Z 向き = 「奥向き」。プレイヤー側は敵(奥)を向くので正しいが、敵側も奥を向いてしまう。

### 方針
- `UnitModelDisplay` に board 駆動時の向き `m_idleFacingDir`(既定 +Z = 従来の Identity と同値)と
  セッター `SetIdleFacingDir()` を追加。board 駆動の回転は再生駆動と同じ式
  (`atan2(dir.x, dir.z) + kModelYawOffsetRad`)で求める。
- `Game::Start()` で `m_enemyModelDisplay.SetIdleFacingDir((0,0,-1))`(手前=プレイヤー側)を設定。
- board 駆動中に毎フレーム `e.lastRot = Identity` へリセットしていた箇所を `e.lastRot = idleRot` にする。
  再生開始時の Slerp が「表示中の向き」から始まり、敵が戦闘開始時に180度クルッと回る不自然さも出ない。
- 戦闘中(`UpdateFromPlayback`)の向き制御は変更しない。

### 変更ファイル
- `Game/UnitModelDisplay.h/.cpp`、`Game/Game.cpp`(Start)。

---

## D. 盤面が薄暗い

### 調査結果
- `tonemap.fx` は自動露出: `k = middleGray / 平均輝度(対数平均、1フレームあたり2%で順応)` を掛けてから ACES。
  背景がほぼ黒(対数平均が0.001でクランプされた画素に引っ張られる)のため、露出目標 `SetSceneMiddleGray(0.03)`
  (標準値0.18の1/6)という低い値で全体が暗く出ている。
- 盤面のタイル塗り/グリッド線(`HexGridRenderer`)はライティング非依存の頂点カラーなので、ライトを強くしても
  明るくならない(むしろ平均輝度が上がり露出 k が下がるので相対的に暗くなる)。盤面とユニットの両方を
  持ち上げられるレバーは露出目標 `middleGray`。

### 方針(数値)
| 項目 | 現在値 | 変更後 | 意図 |
|---|---|---|---|
| `SetSceneMiddleGray` | 0.03 | **0.05** | 露出前の輝度を約1.67倍。盤面タイル・ユニット共に持ち上がる |
| `SetAmbient` | (0.35, 0.35, 0.40) | **(0.45, 0.45, 0.50)** | ユニットの影側の潰れを緩和(陰影は直接光で残る) |
| メイン/フィル/リム | 変更なし | 変更なし | 方向光を上げると平均輝度経由で露出が下がりタイルが暗くなるため |
| `SetBloomThreshold` | 10.0 | 変更なし | 露出後輝度は概算で最大3.5程度、しきい値10に届かない |

- 白飛びの見積り: 露出前 1.0 前後の明るい面が k 倍されても ACES は 1.0 付近で漸近的に飽和するため、
  ハイライト(露出後 3 程度)でも 0.95 前後に収まる。中間調(露出後 0.7→1.1)は 0.71→0.83 程度に上がる見込み。
  ただし平均輝度は実画面依存のため、F5 で目視確認して 0.04〜0.06 の範囲で微調整する前提の出発値とする。
- 数値は `Game::Start()` の既存設定箇所を書き換える(エンジン側は触らない)。

---

## E. 戦闘開始ボタンの配置

### 調査結果
- `ShopUIRenderer` の常設ボタン4つ(Reroll/BuyXP/Lock/NextPhase)が SHOP ヘッダー行(y=-368)に
  x=-300 から 160 間隔・140x28 の同サイズで横並び。NextPhase は 4 番目(x=180)。
- 画面右下の空き: 盤面(カメラ投影で概ね x≒-300〜+260、y≒-160〜+60)とは離れており、
  右側の ITEMS 一覧(x≒518〜、y=+270 から下へ 40px/行)、ショップカード5枚目(x≒555〜885、y≒-478〜-402)、
  ヘッダー行のボタン(x≦250)、フィードバック行(y=-330、左端から)の間に、x≒620〜940・y≒-260〜-350 が空いている。

### 方針
- NextPhase ボタンだけ専用レイアウト定数で、**画面右下・ショップカード5枚目の上**に大きく置く:
  - 中心 (790, -305)、サイズ 300x72(UI_SPACE 1920x1080、中央原点・y上向き)。右端 940 で画面端 960 の内側。
    下端 -341 とカード上端 -402 の間に約 60px、ITEMS 一覧は12件を超えない限り上端 -269 に届かない。
  - 塗り色は目立つ朱/橙系 `(0.78, 0.32, 0.12, 0.95)`、枠は選択色(`UIStyle::kSelectedBorderColor`)で太枠。
  - ラベル「戦闘開始 [B]」をスケール 0.85 で中央寄せ(ゲームパッド B ボタンと対応することも併記)。
- 残り3ボタン(Reroll/BuyXP/Lock)は従来位置のまま。
- ヒット領域(`BuildHotRegions`)も同じ定数から作るので描画とズレない。盤面ヘックスのヒット領域とは重ならない。

### 変更ファイル
- `Game/ShopUIRenderer.cpp`(+ `Game.cpp` のコメントの "[Next Round]" 表記を更新)。

---

## 未解決の懸念 / 目視確認ポイント
- D の明るさは自動露出の実平均輝度に依存するため、実機での見え方は要確認(0.04〜0.06 で微調整)。
- C は `kModelYawOffsetRad = 0`(モデル前方 +Z)前提。プレイヤー側が敵を向いて見えているなら敵側も正しく手前を向く。
- E のボタン位置は投影計算と既存定数からの見積り。ITEMS が13件以上溜まると縦に近接する可能性がある。
- A により、勝利報酬アイテムのフィードバック「アイテム入手: ...」は再生完了時に出る(準備フェーズで見える)。

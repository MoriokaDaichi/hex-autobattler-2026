# board-layout-rework — 設計 (plan.md)

このファイルだけを読めば実装フェーズに着手できるよう、調査結果と具体仕様を書く。
背景・受け入れ条件は [`intent.md`](intent.md) を参照。**実装コードはこのフェーズでは書かない**。

> ★ intent.md §B（戦闘時の2盤面接続モデル）と座標系設計は、着手前に game-53（マネージャー）の
> レビューが必要。§1・§2 がその決定案。

---

## 0. 調査結果サマリ（重要な発見）

1. **`HexGridRenderer::CalcTileCenter()` が唯一のワールド射影関数。** 盤面グリッド描画・
   `UnitModelDisplay`・`CombatPlayback`（HPバー位置＝`homePosition`をCalcTileCenter）・
   `Game::Update()` の盤面ヒット領域・`CursorSelectionSystem::TryMouseToHex`（y=0平面レイ→
   `TryWorldPositionToHex`）が **すべてこれ経由**。ここと `TryWorldPositionToHex` の逆変換さえ
   一致していれば、座標系を広げても各所は自動追従する。
2. **`CombatEngine` は完全に座標系非依存。** `HexCoord::Distance()` と `GetNeighbors()`（axial）
   だけで動く。盤面範囲（`IsValidHex`）のチェックも一切しない（チェイス中は敵へ収束するので実害なし）。
   → **2盤面を1つの連続axial空間に置けば、CombatEngine は無改修で「連結戦場」になる。**
3. **プレイヤー配置可否の判定は `Player::PlaceUnitOnBoard` / `MoveUnitOnBoard` の中だけ**
   （`targetPos.q` が `kAllyZoneMinQ..MaxQ`=0..2 か）。`CursorSelectionSystem` のヘックスカーソルは
   既に `kBoardCenter(4,1)` 始点で全 q0-8 を移動できる（配置だけが弾かれる）。
4. **`HexGridRenderer` の頂点バッファ上限が現状ギリギリ。** `kMaxLineVertex=512` /
   `kMaxFillVertex=1024`。現盤面27マスで line=27×12=324, fill=27×18=486。
   **54マスにすると line=648 で 512 を超える**（fill=972 は 1024 内だが余裕なし）。→ 上限引き上げ必須（§5）。
5. **`UnitModelDisplay` はプレイヤー `board` 専用。** `Update(const Player&)` で `player.board` のみ。
   敵モデルを盤面に出す仕組みが無い（受け入れ条件「敵盤面に敵編成が出る」に対応が要る、§4）。
6. **ゴーストの原因（intent §D）**: `Game::Render()` は Title 以外の全フェーズで
   `m_unitModelDisplay.Draw(rc)` を無条件に呼ぶ（L1337）。GameOver/Victory では戦闘で
   HP0 になった `players[0].board` のモデルがそのまま描かれ続ける。Title はリスタート時
   `InitializeNewRun()` が `players[0]` を作り直す（board空）ため本来は消えるが、フェーズ跨ぎで
   `Draw` を止める／`Clear()` する明示処理が無い（§4-D）。
7. **カメラは perspective、`SetPosition/SetTarget/SetViewAngle/SetNear/SetFar/SetUp` を持つ**
   （`k2EngineLow/graphics/Camera.h`、既定 viewAngle=60°）。現状 `Start()` で
   `pos{0,900,-650}` / `target{0,0,0}` 固定。
8. **`BoardUIRenderer::kBarWorldY = 150`**（頭上バーのYオフセット）はモデルスケール前提の値。
   スケールを縮めたら比例して下げる。

---

## 1. 座標系の設計（★要レビュー）

### 採用案：1つの連続 axial 空間を r 方向へ拡張（6行×9列）

| 項目 | 現状 | 変更後 |
|---|---|---|
| 列（9） | `q` 0..8 | `q` 0..8（**不変**） |
| 行（3→6） | `r` 0..2 | `r` 0..**5** |
| プレイヤー陣地 | `q` 0..2（3列×3行=9マス） | **`r` 0..2（9列×3行=27マス）＝現在のプレイヤー可視グリッドそのもの** |
| 敵陣地 | `q` 6..8 | **`r` 3..5（9列×3行=27マス、新設）** |
| 中立ゾーン | `q` 3..5 | **廃止**（座標ギャップ無し） |
| ゾーン区分の軸 | `q`（`kAllyZoneMinQ/MaxQ`） | **`r`（`kAllyZoneMinR/MaxR`）** |
| `CalcTileCenter` 中心 | `kCenterQ=4, kCenterR=1` | `kCenterQ=4`（不変）, **`kCenterR=2.5`** |
| `IsValidHex` | q0-8, r0-2 | q0-8, **r0-5** |

**なぜこの案か**
- プレイヤー盤面（`r` 0-2, `q` 0-8）の各マスのワールド座標は **1ミリも動かない**
  （`CalcTileCenter` は `r - kCenterR` を使う。`kCenterR` を 1→2.5 に変えると全マスが Z 方向へ
   一律 `+kHexSize*1.5*1.5 = +112.5` 平行移動するだけ。相対レイアウトは不変）。実際には
  「盤面全体をカメラで引く」ので体感は変わらない。
- 敵盤面は既存の pointy-top axial 式そのままで `r` 3-5 に並ぶ。カメラ座標系で `worldZ` は
  `r` とともに +Z（＝画面奥／上）へ増える → 敵盤面は自然に「画面の上側」に出る（intent §A 一致）。
- **`CombatEngine` 無改修**。プレイヤー最前列 `r=2` と敵最前列 `r=3` は同 `q` で axial 距離 1
  ＝隣接。準備中も戦闘中も常に1つの連続 6×9 戦場。intent §B の「最小案（戦闘時 6×9 連結戦場）」は
  **接続処理を書くまでもなく自動的に成立**する。
- `TryWorldPositionToHex` は `kCenterR` を対称に使うので逆変換も自動整合。マウス／キーボードの
  ヘックス選択、`BoardUIRenderer::WorldToUI`、`CombatPlayback` の HP バー位置もすべて追従。

### 却下案
- **`HexCoord` に盤面サイド enum を追加**：`operator==` / `Distance` / `GetNeighbors` / 各所の
  `position ==` 比較すべてに影響。`CombatEngine` の座標非依存性という最大の資産を壊す。過剰。
- **盤面ごとに独立座標**：`CombatEngine` が「1つの座標空間で殴り合う」前提なので、戦闘のたびに
  座標マージ層が要る。intent §B が言う「接続モデル」を新規実装することになり、リスクとコスト大。

### 見た目上の分離（intent §A「中立ゾーン廃止 or 細い境界」）
- **既定案**：座標・ワールド座標にギャップを入れず、(a) ゾーン塗り色を `r0-2`=味方色 /
  `r3-5`=敵色 に塗り分け、(b) `r=2` と `r=3` の境界に1本の区切りライン（`BuildGridLines` に
  細い矩形かライン2頂点を追加）を引く。**戦闘中に「存在しないギャップを飛び越える」違和感が出ない**
  のが利点。
- **代替案**：`CalcTileCenter` 内で `r >= 3` のとき `worldZ` に定数 `kBoardGapZ`（例 +40）を足す。
  射影関数が唯一の真実なので全レンダラー・ヒット判定・HPバーが自動整合し、`CombatEngine`（座標のみ）
  には無影響。戦闘開始時に両軍が少し離れて見える。→ **どちらにするか §「レビュー確認事項」参照。**

---

## 2. 戦闘時の2盤面接続（intent §B）（★要レビュー）

§1採用案では **接続コードは不要**。準備フェーズ＝プレイヤーは `r0-2` にのみ配置、敵は
`EnemyFactory` が `r3-5` に配置。戦闘フェーズ＝`SimulateCombat(player, enemy)` は今と同じく
両 `board` を1空間で扱い、`r=2/r=3` 境界を越えて隣接移動・交戦する。決着判定
（`IsBoardWiped` / 相打ちも敗北）も現状のまま無改修。

**確認したい点（レビュー）**：
- プレイヤー最前列と敵最前列が距離1（＝ほぼ密着）で戦闘開始することを許容するか。
  許容しないなら §1「代替案」（`CalcTileCenter` に `r>=3` の Z ギャップ）を採用し、
  必要なら `EnemyStage` を `r4-6`（座標上も1行空ける）に置く選択もある（その場合 `IsValidHex`
  は r0-6、中央 `r=3` は「誰も置けない緩衝行」）。**実装簡潔性・戦闘の見栄えの両面から、
  既定は「座標ギャップ無し＋見た目は色＋境界ライン」。**

---

## 3. カメラ再設定（intent §A 末尾）

新グリッドのワールド範囲（`kCenterQ=4, kCenterR=2.5, kHexSize=50`）:
- X: `kHexSize*sqrt(3)*(q-4) + kHexSize*sqrt(3)/2*(r-2.5)` → 概ね **X ∈ ±470**
- Z: `kHexSize*1.5*(r-2.5)` → **Z ∈ ±187.5**（現状の ±75 から約2.5倍に拡大）

**出発値（F5で微調整前提）**:
- `SetPosition({ 0.0f, 1180.0f, -1000.0f })`（Yを上げ、Zを引く。俯角はほぼ維持）
- `SetTarget({ 0.0f, 0.0f, 40.0f })`（原点は `r=2/3` 境界付近。ターゲットをわずかに +Z して
  手前のプレイヤー盤面に画面配分を寄せる。0 のままでも可）
- 必要なら `SetViewAngle(Math::DegToRad(62.0f))` 程度で微増。
- `SetFar` は現状の既定値を確認し、カメラ後退分（+350程度）で足りなければ増やす。
- `Start()` L45-50 のコメント（q:0-8, r:0-2 / X:±430,Z:±125）も新数値へ更新。

---

## 4. ユニットモデル表示

### C. モデル縮小（`UnitModelDisplay.cpp`）
- `kUnitModelScale` **10.0 → 5.0**（出発値、★1基準。F5で 4.5〜5.5 を調整）。
  `GetStarModelScaleMultiplier`（★2=1.15 / ★3=1.32）は据え置き。
- `BoardUIRenderer.cpp` `kBarWorldY` **150 → 75**（モデル高に追随。F5調整）。
- ベンチのモデル表示は無し（`BoardUIRenderer` はテキストのみ）＝影響なし。

### 敵盤面のモデル表示（受け入れ条件「敵編成がそこに出る」）
- **`UnitModelDisplay` を board 非依存に一般化**：
  `Update(const Player&)` → `Update(const std::vector<UnitInstance>& board)` へ変更
  （`RebuildIfBoardChanged` / `m_lastBoardSignature` も `board` 受け取りに）。呼び出し側
  `m_unitModelDisplay.Update(m_gameState.players[0].board)` に直すだけ。
- `Game` に **`UnitModelDisplay m_enemyModelDisplay;`** と
  **`Player m_enemyPreview;`（または `std::vector<UnitInstance>`）** を追加。
  - 準備フェーズ開始時 or `roundNumber` が変わったフレームで
    `m_enemyPreview = m_enemyFactory.CreateEnemyBoard(m_enemyStages[roundNumber-1], …)` を1回。
    `RebuildIfBoardChanged` がシグネチャ比較するので毎フレーム呼んでも再ロードは起きない。
  - `Game::Update()`：`m_enemyModelDisplay.Update(m_enemyPreview.board)`（表示フェーズのみ、下記）。
  - `Game::Render()`：`m_enemyModelDisplay.Draw(rc)`（表示フェーズのみ）。
- 戦闘フェーズの敵モデルは **今回は静止表示（`homePosition` 固定）で可**（移動アニメは intent
  スコープ外）。`m_enemyPreview.board` をそのまま出す。HPバーは従来どおり `CombatPlayback` が担当。
  ※ プレイヤー側モデルも戦闘中は `SimulateCombat` 後の最終位置で静止（現状仕様）。挙動を揃える。

### D. Title / GameOver / Victory のゴースト修正
- **`UnitModelDisplay::Clear()` を追加**（`m_displayEntries.clear(); m_lastBoardSignature.clear();`）。
- `Game::Render()`：`m_unitModelDisplay.Draw(rc)` と `m_enemyModelDisplay.Draw(rc)` を
  **`currentPhase ∈ { Preparation, Combat, Result }` のときだけ**呼ぶ（Title は既に early-return、
  GameOver/Victory では呼ばない）。
- `Game::Update()`：同フェーズ以外では `UnitModelDisplay::Update()` を呼ばない、かつ
  GameOver/Victory/Title へ遷移する箇所で `m_unitModelDisplay.Clear()` /
  `m_enemyModelDisplay.Clear()` を呼ぶ（保険。次の描画で確実に消える）。
- これで「盤面を背景に残す」Result では従来どおり両軍モデルが見え、GameOver/Victory では消える。

---

## 5. `HexGridRenderer` の描画対応

- `kMinR/kMaxR`：0/2 → **0/5**。
- `kCenterR`：1.0 → **2.5**。
- `kAllyZoneMinQ/MaxQ` を廃し **`kAllyZoneMinR=0 / kAllyZoneMaxR=2`**（public。ゾーン区分の唯一の正）。
- `BuildTileFills()`：ゾーン色判定を `r <= kAllyZoneMaxR ? 味方色 : 敵色`（中立廃止）。
- `BuildGridLines()`：`r=2` と `r=3` の境界に区切りライン（太め・明色）を追加。
- `IsValidHex()`：`r0-5`。
- **頂点バッファ上限**（§0-4）：
  - `kMaxLineVertex` 512 → **1024**（54マス×12＝648＋境界ライン数本。索引配列 `lineIndices[]` も同数）。
  - `kMaxFillVertex` 1024 → **1536**（54マス×18＝972。索引配列 `fillIndices[]` も同数）。
  - `m_lineVertices.reserve` / `m_fillVertexBuffer.Init` 等の割り当てサイズも連動。
- `HexGridRenderer.h` L45-46 のコメント（0-2:自陣 / 3-5:中立 / 6-8:敵陣）を新区分へ更新。

---

## 6. `Player`（配置ルール）

- `kAllyZoneMinQ/MaxQ` → **`kAllyZoneMinR=0 / kAllyZoneMaxR=2`**（`HexGridRenderer` の同名を再掲、
  循環include回避の既存パターン踏襲）。
- `PlaceUnitOnBoard` / `MoveUnitOnBoard`：`targetPos.q` 範囲チェック → **`targetPos.r` 範囲チェック**。
- ドックコメント・`return false` 理由コメントの「自陣(q0-2)」表記を「自陣(手前3行 r0-2)」へ。
- `GetMaxBoardSize()`（＝level）は不変。27マス開放でもレベル上限で置ける数は変わらない。

---

## 7. `Game.cpp`

- **`Start()`**：カメラ（§3）。コメント更新。
- **`BuildEnemyStages()`**：全 `HexCoord` リテラルを新座標へ書き換え。
  旧 `q6-8 × r0-2` → 新 `r3-5 × q?`。**ミラー写像**（陣形の形を保ち、敵が手前を向く）:
  `HexCoord(oldQ, oldR)` → `HexCoord(oldR + 3, oldQ - 3)`
  （旧 `q6`=敵最前 → 新 `r3`；旧 `r0..2`（3幅）→ 新 `q3..5`（9幅の中央寄せ））。
  例）`HexCoord(6,1)` → `HexCoord(4,3)`、`HexCoord(8,2)` → `HexCoord(5,5)`。
  ※ マスが増えたぶん左右に散らす調整は「破綻しない範囲の微調整」まで（数値バランス再設計は
  スコープ外）。まずは中央3列（q3-5）に写して F5 で確認。
- **`Game::Update()` 盤面ヒット領域ループ（L140-167）**：
  `for q in kAllyZoneMinQ..MaxQ, r in 0..2` → **`for r in kAllyZoneMinR..MaxR, q in kMinQ..kMaxQ`**
  （27マス）。`kHexHitHalfWidth/Height`（35/16）は新カメラ＆小型モデルに合わせ再調整
  （モデルが小さくなるぶん詰められる。F5で決定。出発値そのまま試す）。
- 敵盤面（r3-5）のヒット領域は **配置不可なので不要**。フェーズ2ツールチップで敵ユニットに
  ホバー情報を出すなら `UIRegionKind::BoardUnit` を r3-5 にも足す — **今回は見送り（別途 ui 側タスク）**。
- **モデル表示の駆動**（§4）：`m_unitModelDisplay.Update(players[0].board)` に引数変更、
  `m_enemyModelDisplay` / `m_enemyPreview` の追加とフェーズ遷移でのビルド／`Clear()`。
- フィードバック文字列 L556/583/728/772 の「自陣 q0-2」→「自陣 手前3行」。
- `m_enemyModelDisplay.Draw()` は `m_unitModelDisplay.Draw()` の直後（L1337 付近）に、同じフェーズ条件で。

---

## 8. 追加／変更ファイル一覧

| ファイル | 変更 |
|---|---|
| `Game/HexGridRenderer.h` | `kMaxR`=5, `kCenterR`=2.5, `kAllyZoneMinR/MaxR`, 頂点上限, コメント |
| `Game/HexGridRenderer.cpp` | 中心オフセット, ゾーン色判定を r 基準に, 境界ライン, `IsValidHex` r0-5, 索引配列サイズ |
| `Game/Player.h` | `kAllyZoneMinR/MaxR`, Place/Move の `.r` チェック, コメント |
| `Game/UnitModelDisplay.h/.cpp` | `kUnitModelScale`=5.0, `Update`を`board`受け取りに一般化, `Clear()` 追加 |
| `Game/BoardUIRenderer.cpp` | `kBarWorldY`=75 |
| `Game/Game.h` | `UnitModelDisplay m_enemyModelDisplay;`, `Player m_enemyPreview;` |
| `Game/Game.cpp` | カメラ, `BuildEnemyStages` 座標, ヒット領域ループ(r0-2×q0-8), 敵モデル駆動＆`Clear`, ゴースト用フェーズゲート, 文言 |
| `Game/CursorSelectionSystem.cpp` | （ロジック変更なしの見込み。文言があれば更新。`kBoardCenter(4,1)` は流用可） |
| `docs/tasks/board-layout-rework/plan.md` | 本書 |

新規ファイルは原則作らない（`m_enemyModelDisplay` は既存 `UnitModelDisplay` の2個目のインスタンス）。

---

## 9. 既知の懸念点・レビュー確認事項

1. **（★レビュー）座標ギャップ**：既定＝座標／ワールドとも隙間なし＋色と境界ラインで分離。
   代替＝`CalcTileCenter` の `r>=3` に Z ギャップ。戦闘の見栄え（両軍が密着開始）を許容するか。
2. **（★レビュー）§2 の接続モデル**：§1採用案では接続コード不要＝「常時1つの 6×9 戦場」。
   intent の最小案と同義だが、"準備は各3×9・戦闘時のみ連結" という段階連結を敢えて実装する
   必要はない、という判断でよいか。
3. `CombatEngine::MoveTowards` は盤面外への移動を止めない（既存仕様）。行が増えても両軍は中央へ
   収束するので実害は増えない見込み。F5 でユニットが可視グリッド外へさまよう場合のみ
   `IsValidHex` クランプ追加を検討（スコープ増）。
4. カメラ数値・`kHexHitHalfWidth/Height`・`kUnitModelScale`・`kBarWorldY` はすべて F5 反復前提の
   出発値。実機確認（HPバー位置・ツールチップ位置・マスのクリック当たり）まで実装フェーズに含める。
5. `HexGridRenderer` 頂点上限を上げ忘れると 54 マス目で線が欠ける（§0-4）。実装時の必須項目。
6. 敵プレビュー生成（`EnemyFactory::CreateEnemyBoard`）は `ModelRender::Init` によるtkm再ロードを
   伴うが、`roundNumber` 変化時のみ＝1ラウンド1回。プレイヤー盤面が既に払っているコストと同等。
7. セーブ／ロード：`m_currentShop` 等は座標を持たない。`board` の `HexCoord` はセーブ対象か要確認
   （対象なら旧セーブの `q0-2` 座標が新ルールで "手前3行" と解釈がずれる可能性。移行は
   「旧セーブ無効化 or 変換」を実装時に判断。ゲームの性質上、旧セーブ切り捨てで可の見込み）。

---

## 10. 受け入れ確認（実装後 F5）

- 準備：プレイヤー盤面 27マス（r0-2 × q0-8）全部にマウス／キーボード／パッドで配置・再配置できる。
- 準備：画面上側に敵盤面 3×9 が表示され、そのラウンドの `EnemyStage` の敵モデルが並ぶ。
- 戦闘：両軍が交戦し、勝敗（撃破・全滅・相打ち）が従来どおり確定。HPバーが各ユニット頭上に出る。
- モデルが概ね1マスに収まり、隣と大きく重ならない。マスのクリックがモデルにほぼ邪魔されない。
- Title / GameOver / Victory に前プレイのモデル（シルエット含む）が残らない。Result では残る（仕様）。
- 既存のパッド操作・F5 等が従来どおり。`Debug|x64` 0 エラー。

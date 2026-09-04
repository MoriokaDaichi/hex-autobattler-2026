# combat-movement-playback — 設計 (plan.md)

このファイルだけで実装フェーズに着手できるよう、調査結果と具体設計を書く。
背景・受け入れ条件・スコープ In/Out は [`intent.md`](intent.md) を参照。**実装コードはこのフェーズでは書かない。**

> ★ intent.md「制約・注意」の設計判断4点(§1〜§4 の「決定案」)は着手前に game-ae のレビュー必須。

---

## 0. 調査結果サマリ(実装前提)

1. **`CombatEngine::MoveTowards()` は 1 アクションターン分の移動を 1 個の `Move` イベントに集約している。**
   ループ前の `attacker.position`(= `PerformAction` が計算した `startDistance` 時点の位置)から、
   ループ後の `attacker.position` まで。1 イベントの移動量は `GetEffectiveMoveSteps`(= `moveSpeed *
   1/attackSpeed` 四捨五入、最低 1)歩 ≒ 1〜2 マス。多段接近は複数ウェーブに渡る複数 `Move` イベント。
2. **`CombatEvent`(`Move`)は移動先ヘックスを持たない。** `beforeValue`/`afterValue` は距離のみ。
   → 再生側がユニットをどこへ動かすか分からない。ここへ from/to マスの追加が必要(§1)。
3. **`SimulateCombat(player, enemy, ...)` の `player` は `m_gameState.players[0]` への参照**で、
   sim が `board[i].position` を最終位置へ書き換える。だが **`CombatPlayback::Begin` → `MakeView` は
   `unit.homePosition` を使う**ので、再生の初期ワールド座標は「配置マス」。sim の最終位置には影響されない。
   `enemy` は sim ローカルの `Player`(`Begin` 後に破棄)。`MakeView` が必要値をコピーする。
4. **`CombatPlayback::UnitView` は `name`(wstring)/`starLevel` しか持たない。** `UnitDef*`/`modelPath`/
   アニメパスが無く、`UnitModelDisplay` がモデルをロード・対応付けできない。→ `def` の追加が必要(§3)。
5. **`m_views` は `[0..m_playerCount)` = プレイヤー board、以降 = 敵 board。** `m_playerCount` は private。
   → プレイヤー分/敵分を切り出すアクセサが必要(§3)。
6. **`UnitModelDisplay` は `players[0].board` / `m_enemyPreview.board`(= sim と別オブジェクト)から
   毎フレーム TRS を更新するだけ。`PlayAnimation` は一度も呼ばれていない**(モデルは常にバインドポーズ)。
   `Init` に idle/move/normalAttack/skill/death の 5 クリップは渡し済み。クリップのループフラグは
   未設定(`AnimationClip::m_isLoop = false` 既定)。
7. **アニメ API**(確認済み):
   - `ModelRender::PlayAnimation(int animNo, float interpolateTime = 0.0f)` → `Animation::Play`。
   - `ModelRender::IsPlayingAnimation() const`(非ループクリップ再生中に true)。
   - `ModelRender::SetAnimationSpeed(float)`。`ModelRender::Update()` が
     `m_animation.Progress(frameDelta * m_animationSpeed)` を回す。
   - `AnimationClip::SetLoopFlag(bool)` / `IsLoop()`。Load 直後に設定する。
8. **向き API**: `Quaternion::SetRotationYFromDirectionXZ(const Vector3& dir)`
   (= `SetRotationY(atan2f(dir.x, dir.z))`)、`Quaternion::Slerp(t, q1, q2)`。
   現状 `SetTRS(pos, Quaternion::Identity, scale)` の第2引数を差し替える。
9. **HPバー**: `BoardUIRenderer::DrawCombat` L155-157 で `world = v.worldPos; world.y += kBarWorldY;`
   → `WorldToUI`。`v.worldPos` を補間後の値にすればバーは自動追従する。`bar.isEnemy` 済み。
10. **再生尺**: `CombatPlayback::Begin` で `m_speed = clamp(total / 6.0, 1.0, 5.0)`。`m_clock` は
    `deltaTime * m_speed` で進み、`ev.time`(戦闘内時刻)と直接比較。実再生時間 ≒ `total / m_speed`
    ≒ 6s + tail 1s。**位置補間も `m_clock`(= 戦闘内時刻)基準で行う**と尺に自動追従する。

---

## 1. Move イベントに移動元・移動先を持たせる 【★決定案 = 要レビュー #1】

### 決定案: **2 点(from/to)のみ。`CombatEvent` に int 4 個を追加。中間ステップは持たない。**

- `Game/CombatEvent.h` の `struct CombatEvent` に追加:
  ```cpp
  // Move イベント用。移動元・移動先の axial 座標(それ以外のイベントでは未使用、既定 0)。
  int moveFromQ = 0, moveFromR = 0;
  int moveToQ   = 0, moveToR   = 0;
  ```
  → `HexCoord.h` を include しない(int 4 個で足りる。include 依存を増やさない)。
- `Game/CombatEngine.h` `MoveTowards()`:
  - ループ**前**に `HexCoord moveFrom = attacker.position;` を捕捉。
  - 末尾のイベント生成で `e.moveFromQ = moveFrom.q; e.moveFromR = moveFrom.r;
    e.moveToQ = attacker.position.q; e.moveToR = attacker.position.r;` を追記。
  - **既存フィールド(`beforeValue`/`afterValue` = 距離)はそのまま**。`CombatLogPrinter` 互換維持。
  - `SimulateCombat` の行動順・ダメージ・勝敗・`attacker.position` の更新ロジックは**一切変更しない**。
    追加はフィールド書き込みのみ。
- 迂回で経路が曲がるケース: from→to の直線 lerp が角を軽く切るが、B(見た目仕上げ)の許容範囲。
  中間点を持たせるとエンジン側(N イベント/ターン)と再生側の両方が増える。**採用しない。**
- from == to(周囲が塞がって動けなかった)ケース: イベントは出るが再生側は「移動なし(idle 継続)」で扱う。

---

## 2. CombatPlayback で位置を再生する 【★決定案 = 要レビュー #2】

### 2-1. `UnitView` への追加フィールド

```cpp
struct UnitView
{
    // ... 既存(name, starLevel, worldPos, displayHP, maxHP, displayShield, displayGauge,
    //          skillThreshold, alive, isEnemy) ...

    const UnitDef* def = nullptr;   // モデル/アニメのロードと対応付け用(§3)。MakeViewで unit.def を入れる。

    // --- 位置補間(すべて m_clock = 戦闘内時刻 基準) ---
    Vector3 homeWorldPos;           // homePosition の CalcTileCenter(初期値・フォールバック)。
    // worldPos は「現在の表示ワールド座標(y=0)」。毎 Update() で補間結果に更新。初期 = homeWorldPos。
    Vector3 moveFromPos;            // 補間区間の始点(y=0)。
    Vector3 moveToPos;              // 補間区間の終点(y=0)。
    float   moveStartClock = -1.0f; // 補間開始時の m_clock。< 0 で「補間なし(静止)」。
    float   moveEndClock   = -1.0f; // 補間終了時の m_clock。

    // --- 向き ---
    Vector3 facingDir;              // XZ 単位ベクトル。初期: 敵 = (0,0,-1)、味方 = (0,0,+1)。

    // --- アニメ状態ヒント(UnitModelDisplay が読む) ---
    bool  isMoving = false;         // このフレーム補間中か(UpdateでmoveStartClock>=0かつ未完なら true)。
    int   attackAnimSeq = 0;        // 攻撃(通常/必殺)を出すたび +1。UnitModelDisplay が前回値と比較して発火検出。
    bool  attackAnimIsSkill = false;// 直近 attackAnimSeq 更新が必殺技か。
    bool  deathAnimTriggered = false; // Death イベントで true(以後不変)。
};
```

- `MakeView()`: `v.def = unit.def;`、`v.homeWorldPos = v.worldPos =
  CalcTileCenter(unit.homePosition.q, unit.homePosition.r);`、`v.facingDir = isEnemy ? {0,0,-1} : {0,0,+1};`。
- `CombatEvent.h` は既に `struct UnitInstance;` 前方宣言のみ。`UnitDef*` を持つには `struct UnitDef;`
  前方宣言を `CombatPlayback.h` に足す(ポインタのみ、include 不要)。`CombatPlayback.cpp` は
  `UnitInstance.h`(→ `UnitDef.h`)を include 済み。

### 2-2. 補間の総尺と 1 ホップ所要 【★#2 の肝】

- **1 アクションターン分の Move(1〜2 マス)の補間所要 `dur` を、次の同ユニットイベントまでの間隔の
  8 割で決める(下限・上限クランプ付き)。** これで「ホップ→静止→ホップ」ではなく「ほぼ歩き続ける」。
  - `Begin()` で 1 回だけ前処理: `m_events` を走査し、各 Move イベント i について
    「同じ (owner, actorIndex) を持つ次のイベント j」を探し、
    `gap = (j があれば m_events[j].time - m_events[i].time、無ければ kMoveDurationFallback)`、
    `dur = clamp(gap * 0.8, kMoveDurMin, kMoveDurMax)` を **その Move イベント専用の所要**として
    別テーブル(`std::vector<float> m_moveDur;` = m_events と同添字)に格納。
  - 定数(戦闘内時刻・秒):
    `kMoveDurMin = 0.12f`(最速でも見える下限)、
    `kMoveDurMax = 0.9f`(次アクション ≒ 1/attackSpeed ≒ 0.7〜1.4s。到着し切るため 0.9 で頭打ち)、
    `kMoveDurationFallback = 0.35f`(そのユニットの最後の Move で次イベントが無い場合)。
  - hop 数(1 or 2)での区別はしない(gap ベースなので 2 マスでも同じ枠に収まる)。
- **総尺への影響なし**: 補間は `m_clock` 基準で、次イベント時刻までに必ず完了する設計。
  `kTargetPlaybackSeconds`(6) + `kTailSeconds`(1) は不変。`m_speed` も不変。

### 2-3. `ApplyEvent` の追加分岐

- **`case CombatEventType::Move:`**(現在 `break` している):
  ```cpp
  UnitView* v = ResolveActor(ev);
  if (!v) break;
  Vector3 to = CalcTileCenter(ev.moveToQ, ev.moveToR);
  bool moved = (ev.moveFromQ != ev.moveToQ) || (ev.moveFromR != ev.moveToR);
  if (moved) {
      v->moveFromPos   = v->worldPos;                 // 現在の表示位置から繋ぐ(連続性)。
      v->moveToPos     = to;
      v->moveStartClock = m_clock;
      v->moveEndClock   = m_clock + m_moveDur[m_nextIndex];  // §2-2 の専用所要
      // 向きは補間中に毎 Update で進行方向へ更新(下記)。
  }
  // logicalHex 相当は今回 UnitView に別途持たなくてよい(from/to は毎回イベントが持つ)。
  ```
- **`ResolveActor` を使う既存の Move には `actorIndex` があり `ResolveActor` がそのまま効く**(確認済み)。
- **攻撃系**(`NormalAttack` / `SkillAttack`): 既存の「target の displayHP 更新」に加え、
  `if (UnitView* a = ResolveActor(ev)) { a->attackAnimSeq++; a->attackAnimIsSkill = (ev.type ==
  SkillAttack); if (UnitView* t = ResolveTarget(ev)) { Vector3 d = t->worldPos - a->worldPos; d.y = 0;
  if (lengthXZ(d) > 1e-3) a->facingDir = normalize(d); } }`。
  `SplashDamage` は攻撃モーションを二重発火させない(SkillAttack で既にトリガー済み)。既存どおり HP のみ。
- **`Death`**: 既存の `alive=false; displayHP=0; displayShield=0` に `v->deathAnimTriggered = true;` を追加。
  補間中でも Death が来たら `v->moveStartClock = -1;`(その場で崩れる)。

### 2-4. `CombatPlayback::Update()` 末尾に「毎フレームの補間評価」を追加

`while` のイベント消化の後、`m_views` 全体に対して:
```cpp
for (auto& v : m_views) {
    if (v.moveStartClock >= 0.0f) {
        if (m_clock >= v.moveEndClock) {
            v.worldPos = v.moveToPos;
            v.moveStartClock = -1.0f;
            v.isMoving = false;
        } else {
            float t = (m_clock - v.moveStartClock) / (v.moveEndClock - v.moveStartClock);
            t = t * t * (3.0f - 2.0f * t);           // smoothstep(加減速)
            v.worldPos = Lerp(v.moveFromPos, v.moveToPos, t);
            v.isMoving = true;
            Vector3 d = v.moveToPos - v.moveFromPos; d.y = 0.0f;
            if (lengthXZ(d) > 1e-3f) v.facingDir = normalizeXZ(d);
        }
    } else {
        v.isMoving = false;
    }
}
```
- `Lerp` / `normalizeXZ` は `CombatPlayback.cpp` 内の小ヘルパー(既存 `MakeView` と同じ匿名 namespace)。
- 死亡ユニット(`!alive`)は補間評価をスキップしてよい(Death 到達時に `moveStartClock=-1` 済み)。

---

## 3. UnitView のモデル識別情報 と UnitModelDisplay 再生駆動 【★決定案 = 要レビュー #3】

### 3-1. `CombatPlayback` にビュー分割アクセサを追加
```cpp
size_t GetPlayerViewCount() const { return m_playerCount; }
// 既存 GetUnitViews() はそのまま(全体)。呼び出し側が data() + count で slice する。
```

### 3-2. `UnitModelDisplay` に再生駆動経路を追加(新メソッド)
```cpp
// 戦闘再生中のみ使う。CombatPlayback のユニットビュー配列(プレイヤー slice / 敵 slice のどちらか)を
// 受け取り、TRS(位置=worldPos+Yリフト、向き=facingDir、スケール=既存)とアニメを更新する。
void UpdateFromPlayback(const CombatPlayback::UnitView* views, size_t count);
```
- `UnitModelDisplay.h` に `#include "CombatPlayback.h"` を追加(循環なし: CombatPlayback.h は
  CombatEvent.h しか include しない)。
- **モデル再構築の判定**: 既存 `RebuildIfBoardChanged` と同型の
  `RebuildIfViewsChanged(views, count)` を追加。シグネチャは `(view.def, view.starLevel)` の並び。
  → `m_lastBoardSignature`(既存)を流用。board 経路と playback 経路で同じ vector を使い回す
  (どちらの経路も `{UnitDef*, int}` のペア列なので、フェーズ跨ぎで不要な再ロードが起きても
  実害は「1 回の再 Init」のみ。許容)。
- **各エントリの更新**(`m_displayEntries[i]` ↔ `views[i]`):
  - `worldPos = views[i].worldPos; worldPos.y += kUnitModelHalfHeightAtScale1 * kUnitModelScale.x *
    starMul;`(§既存の持ち上げ式そのまま。`starMul = GetStarModelScaleMultiplier(views[i].starLevel)`)
  - `Quaternion rot; rot.SetRotationYFromDirectionXZ(views[i].facingDir);`
    さらに `kModelYawOffsetDeg`(既定 0、F5 で 0/180 を確定)を `rot` に乗算。
    急な向き反転が気になれば前フレーム rot と `Slerp(clamp(turnRate*dt,0,1), prev, target)`
    (`m_displayEntries` に `Quaternion lastRot` を保持)。→ **まず Slerp あり(turnRate ≒ 12 rad/s)で実装**。
  - `modelScale = kUnitModelScale * starMul`(既存)。
  - `modelRender.SetTRS(worldPos, rot, modelScale); modelRender.Update();`
  - **アニメ状態機**(`m_displayEntries[i]` に `int seenAttackSeq; bool seenDeath; int curClip;` を保持):
    1. `views[i].deathAnimTriggered && !seenDeath` → `PlayAnimation(4, 0.15f)`、`seenDeath = true`、
       `curClip = 4`。
    2. else if `views[i].attackAnimSeq != seenAttackSeq` →
       `PlayAnimation(views[i].attackAnimIsSkill ? 3 : 2, 0.1f)`、`seenAttackSeq = views[i].attackAnimSeq`、
       `curClip = (skill?3:2)`。
    3. else if `curClip`(2 or 3)が非ループ再生中(`IsPlayingAnimation()`)→ 何もしない(攻撃モーション優先)。
    4. else if `seenDeath` → 何もしない(death クリップの最終フレームで静止)。
    5. else 望ましいループクリップ = `views[i].isMoving ? 1 : 0`。`curClip` と違えば
       `PlayAnimation(desired, 0.15f)`、`curClip = desired`。
  - `modelRender.SetAnimationSpeed(clamp(m_speedHint, 1.0f, 2.0f))`(足のスケート軽減。`m_speedHint`
    は `CombatPlayback::GetPlaybackSpeed()` を新設して渡す。省略可・優先度低)。
- **`GetOrLoadAnimClips` の loop フラグ設定を追加**:
  Load 直後に `clips[0].SetLoopFlag(true); clips[1].SetLoopFlag(true);`(idle, move)。
  2/3/4(attack/skill/death)は非ループのまま。
  → board 経路(準備/結果)は従来どおり `PlayAnimation` を呼ばないので**バインドポーズ維持=回帰なし**。

### 3-3. `Game::Update()` の駆動切り替え(L130-156 のブロックを分岐追加)
```cpp
Phase modelPhase = m_gameState.currentPhase;
if (modelPhase == Phase::Combat && m_combatSimDone && m_combatPlayback.IsActive()) {
    // 再生駆動: CombatPlayback のビューで両ディスプレイを更新
    const auto& views = m_combatPlayback.GetUnitViews();
    size_t pc = m_combatPlayback.GetPlayerViewCount();
    m_unitModelDisplay.UpdateFromPlayback(views.data(), pc);
    m_enemyModelDisplay.UpdateFromPlayback(views.data() + pc, views.size() - pc);
    // ※ 敵プレビュー再生成(m_enemyPreview)はこのフレームでは不要。
} else if (modelPhase == Phase::Preparation || modelPhase == Phase::Combat || modelPhase == Phase::Result) {
    // 既存の board 駆動(戦闘突入フレーム = simDone 前、および 準備/結果)
    m_unitModelDisplay.Update(m_gameState.players[0].board);
    ... 既存の m_enemyPreview 再生成 + m_enemyModelDisplay.Update(m_enemyPreview.board) ...
} else {
    m_unitModelDisplay.Clear(); m_enemyModelDisplay.Clear(); m_enemyPreviewRound = -1;
}
```
- **`Game::Render()` L1395-1399 のモデル Draw 条件は変更不要**(Preparation/Combat/Result で Draw、
  それ以外で非 Draw、は現状のまま。board-layout-rework §D の回帰なし)。
- 戦闘 → Result 遷移時: `Update()` L1117 で `ResetBoardPositions()`。次フレームは
  `modelPhase == Result` → board 駆動に戻り、`players[0].board` の配置位置でモデルが立つ
  (死亡ユニットも board には残っているので全員立った状態で Result 背景になる=現状と同じ挙動)。
- **死亡モデルの後始末**: 再生中は「death クリップ最終フレームで静止・描画継続」。
  Combat 中は他の生存モデルがあるため RT インスタンス数 > 0 で board-layout-rework の RT ゴースト
  ガードは効いたまま。Result/GameOver へ抜ければ board 駆動 or Clear() に切り替わる。
  → **`UnitModelDisplay` 側で死亡エントリを破棄する処理は入れない**(intent「最終姿勢で静止」で可)。
  F5 で戦闘中に死体の GI ゴーストが目立つ場合のみ、`kDeathLingerSeconds` 経過後に
  `m_displayEntries[i].modelRender.reset()` する後日対応(スコープメモに記載)。

---

## 4. HPバー/ゲージの追従 と 重なり対処 【★決定案 = 要レビュー #4】

- `BoardUIRenderer::DrawCombat` L155-157:
  `v.worldPos` が補間後の位置になるので **バーは自動追従**(コード変更ほぼ不要)。
  `MakeView` の「`homePosition` に固定」コメントは実態に合わせて更新。
- **重なり対処 = 陣営で頭上バーの Y をずらす。**
  `world.y += kBarWorldY + (v.isEnemy ? kEnemyBarYBonus : 0.0f);`
  `kEnemyBarYBonus = 26.0f`(ワールド単位。近接で両者が隣接マス ≒ 86 世界単位差 → 画面上で
  横 40〜60px 差、バー幅 114px で横は重なりうる。縦に 1 段ずらせば両方読める)。
  XZ はいじらない(位置の正確さを保つ)。「重なりは許容」は選ばない(近接戦は毎回発生するため)。
- ゲージバー(`kGaugeY`)は HP バー相対オフセットのままなので一緒にずれる。ラベル行(`+16`)も同様。

---

## 5. カメラ・その他

- カメラは board-layout-rework/board-layout-tuning で調整済み(`pos {0,1230,-975}`, `target {0,0,75}`)。
  **新規のカメラ挙動実装はしない。** 動くユニットが r0〜r5 全域で画角に収まるかを F5 で確認し、
  はみ出す場合のみ `Game::Start()` の pos/target 定数を微調整(出発値の微修正のみ)。
- `SimulateCombat` は 1 フレーム瞬時解決を維持。`CombatPlayback` はイベント列を読むだけを維持。
- 同一 `time` のイベントは同フレームでまとめて反映する既存 `while` 条件を維持(§2-4 はその後段)。
- `UIRectRenderer` プール、`Game::Render()` 冒頭 `BeginFrame()`、`g_camera2D` 不干渉は現状維持。
- ソースは UTF-8 (BOM 付き)。`Game/Assets/modelData/*.dds` の phantom modified は触らない。

---

## 6. 追加/変更ファイル一覧

| ファイル | 変更 |
|---|---|
| `Game/CombatEvent.h` | `Move` 用 `moveFromQ/R`, `moveToQ/R`(int×4)を追加 |
| `Game/CombatEngine.h` | `MoveTowards()`: ループ前に移動元を捕捉、`Move` イベントに from/to を書き込む(他は不変) |
| `Game/CombatPlayback.h` | `UnitView` に `def` / 補間・向き・アニメヒントの各フィールド追加。`GetPlayerViewCount()` / `GetPlaybackSpeed()` 追加。`struct UnitDef;` 前方宣言。`m_moveDur` メンバ |
| `Game/CombatPlayback.cpp` | `MakeView` で新フィールド初期化。`Begin()` で `m_moveDur` 前処理。`ApplyEvent` に `Move` 補間開始 / 攻撃・死亡のアニメヒント。`Update()` 末尾に毎フレーム補間評価。`Lerp`/`normalizeXZ` ヘルパー |
| `Game/UnitModelDisplay.h` | `#include "CombatPlayback.h"`。`UpdateFromPlayback(const CombatPlayback::UnitView*, size_t)` 宣言。`DisplayEntry` に `Quaternion lastRot; int seenAttackSeq; bool seenDeath; int curClip;` |
| `Game/UnitModelDisplay.cpp` | `UpdateFromPlayback` 実装 + `RebuildIfViewsChanged`。`GetOrLoadAnimClips` に idle/move の loop フラグ設定。アニメ状態機・向き Slerp。`kModelYawOffsetDeg` 定数 |
| `Game/BoardUIRenderer.cpp` | `DrawCombat` の頭上バー Y に `kEnemyBarYBonus` を陣営別加算。`kBarWorldY` 付近にコメント更新 |
| `Game/Game.cpp` | `Update()` L130-156: 再生駆動分岐を追加(playback → `UpdateFromPlayback`、それ以外 → 既存 board 駆動) |
| `docs/tasks/combat-movement-playback/plan.md` | 本書 |

新規ファイルは作らない。

---

## 7. 既知の懸念・F5 で見る点(レビュー後の実装で確認)

1. **モデルの前方軸**: `kModelYawOffsetDeg` が 0 か 180 か(あるいは ±90)は tkm 次第。F5 で即判明。
   実装では定数 1 個で切り替えられる形にしておく。
2. **高速再生(m_speed=5)での歩きの見え方**: §2-2 の gap ベース所要なら m_speed に関わらず
   「次アクションまで歩き続ける」ので破綻しにくいが、`kMoveDurMin=0.12` に張り付くケース
   (超短gap)ではホップ気味になる。F5 で `kMoveDurMin` を詰める余地。
3. **同時多数移動**: 9v9 全員移動で `UpdateFromPlayback` が毎フレーム最大 18 体の TRS + アニメ更新。
   `ModelRender::Update()` はスキン計算を伴うが準備フェーズで既に最大 9+9 体動かしているため
   負荷増は限定的。要 F5 でカクつき確認。
4. **death クリップが無い/短いユニット**: `Slime_Death.tka` 等は存在確認済み(board-layout-rework
   調査)。クリップ長が極端に短いと一瞬で最終フレーム静止になるが破綻はしない。
5. **`Begin()` の `m_moveDur` 前処理コスト**: O(events^2) の素朴実装だと最悪 4000 イベントで重い。
   → owner+index をキーに「最後に見たイベント時刻」を持つ 1 パス(O(events))で組む。実装時に注意。
6. **相打ち・全滅時**: 最後のユニットが死んで `m_active=false` → tail 1s。死亡モーション中に
   再生が終わる可能性。tail は据え置き(尺を伸ばさない方針)。死体は Result で board 駆動に
   切り替わり立ち上がる(現状と同じ)。
7. **`isMoving` と攻撃モーションの優先**: §3-2 の状態機は「攻撃/死亡 > 移動 > idle」。
   移動しながら攻撃(射程内で歩かず殴る)は sim 上 Move と Attack が別ターンなので競合しない。

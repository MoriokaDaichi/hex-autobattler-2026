# 設計: 完成アイテムのパッシブ効果（オンヒット火傷 / DoT）

「シミュレーションと表示の分離」（`CombatEngine` → `CombatEvent[]` → `CombatLogPrinter` / `CombatPlayback`）は
崩さない。火傷の解決は `CombatEngine` 内で行い、結果を新種別 `CombatEvent` として記録する。

## 追加ファイル

| ファイル | 役割 |
|---|---|
| `Game/PassiveEffect.h` | パッシブ効果の種別 enum と 1 効果分の構造体。`StatEffect.h` と同じヘッダオンリー方針。 |

`Game/Game.vcxproj` / `.filters` は**変更しない**。データ定義ヘッダ（`StatEffect.h` / `ItemDef.h` /
`CombatEvent.h` / `UnitInstance.h` 等）はいずれも vcxproj に列挙されておらず、include チェーンで
拾われる。`PassiveEffect.h` も同じ扱いにする（.vcxproj を触らないことで他 worktree とのマージ衝突も避ける）。
UTF-8 BOM 付きで保存すること（CP932 環境でのパース崩れ回避。既存ヘッダは全て BOM 付き）。

### `Game/PassiveEffect.h`（新規・全文）

```cpp
#pragma once

/// <summary>
/// アイテムが持つ「戦闘中に発動するパッシブ効果」の種別。
/// 現状はオンヒット火傷のみ。将来 種別を足す場合はここに追加する。
/// </summary>
enum class PassiveEffectType
{
    OnHitBurn, // 通常攻撃を当てたとき、対象へ継続ダメージ(火傷)を付与する。
};

/// <summary>
/// パッシブ効果 1 つ分。パラメータの意味は type によって決まる。
/// OnHitBurn: magnitude = 1 刻みあたりの固定ダメージ / ticks = 刻み回数 / interval = 刻み間隔(秒)。
/// </summary>
struct PassiveEffect
{
    PassiveEffectType type = PassiveEffectType::OnHitBurn;
    int magnitude = 0;
    int ticks = 0;
    float interval = 1.0f;
};
```

## 変更ファイル

### `Game/ItemDef.h`

- `#include "PassiveEffect.h"` を追加。
- `struct ItemDef` に `std::vector<PassiveEffect> passives;` を追加（`effects` の隣）。コメント:
  「装備した対象ユニットに与える、戦闘中に発動するパッシブ効果。」

### `Game/ItemDatabase.h`

`Init()` の完成アイテム定義で、対象アイテムに `passives` を設定する。

- `FuriousEdge`:
  ```cpp
  furiousEdge.passives = { { PassiveEffectType::OnHitBurn, 40, 3, 1.0f } };
  ```
- （2種にする場合）`ArchmageStaff`:
  ```cpp
  archmageStaff.passives = { { PassiveEffectType::OnHitBurn, 60, 3, 1.0f } };
  ```

**未解決（レビューで確定したい）**: 対象を 1 種（`FuriousEdge` のみ）にするか 2 種にするか。
2 種の方が「ビルドが変わる」感は強いが、敵編成にこれらの素材があると敵→プレイヤーにも火傷が飛ぶ
（`Game::BuildEnemyStages` の該当ステージ次第。要確認・必要なら数値を弱める）。

### `Game/UnitInstance.h`

戦闘中だけ使う火傷の状態を追加する。`#include <string>` を追加（`ActiveBurn` が `std::string` を持つため。
現状は `ItemDef.h` 経由で間接的に入っているだけなので明示する）。

```cpp
/// <summary>
/// この UnitInstance に現在かかっている火傷 1 つ分。CombatEngine が戦闘中にのみ使う
/// (SimulateCombat 開始時にクリアされる)。
/// </summary>
struct ActiveBurn
{
    int damagePerTick = 0;
    float nextTickTime = 0.0f; // 次の刻みが発生する戦闘内時刻(秒)。
    int ticksRemaining = 0;
    // 火傷の"出どころ"。CombatEvent の actor 情報として使う(表示専用、解決ロジックには影響しない)。
    std::string sourceOwner;
    std::string sourceName;
    int sourceIndex = -1;
};
```

`struct UnitInstance` に:
```cpp
std::vector<ActiveBurn> activeBurns; // 現在かかっている火傷。戦闘開始時にクリアされる。
```

### `Game/CombatEvent.h`

`enum class CombatEventType` に追加:
```cpp
Burn, // 火傷(継続ダメージ)の 1 刻み。
```
既存フィールドの使い方:
- `actorOwner/actorName/actorIndex` = 火傷の付与元ユニット。
- `targetOwner/targetName/targetIndex` = 火傷を受けているユニット。
- `amount` = この刻みのダメージ、`beforeValue`/`afterValue` = 対象HPの刻み前後。
- `attackType` = 未使用（防御計算を通さないため）。ログ表示では専用ラベルを出す。

### `Game/CombatEngine.h`

#### (1) 戦闘開始時のリセット（`SimulateCombat` 冒頭、`nextActionTime` リセットの隣）

```cpp
for (auto& unit : player.board) { unit.nextActionTime = 0.0f; unit.activeBurns.clear(); }
for (auto& unit : enemy.board)  { unit.nextActionTime = 0.0f; unit.activeBurns.clear(); }
```

#### (2) ウェーブ先頭で「期限が来た火傷」を処理する

`m_currentTime = bestTime;` の直後に呼び出しを追加:
```cpp
ProcessDueBurns(player.board, player.name, enemy.board, enemy.name, outEvents);
```

新規メソッド:
```cpp
/// <summary>
/// 現在時刻(m_currentTime)までに刻みの時刻が来ている火傷を、両陣営の生存ユニットについて処理する。
/// 火傷は防御力で軽減されず、シールドも貫通して currentHP に直接作用する(確定ダメージ)。
/// </summary>
void ProcessDueBurns(
    std::vector<UnitInstance>& playerBoard, const std::string& playerOwner,
    std::vector<UnitInstance>& enemyBoard, const std::string& enemyOwner,
    std::vector<CombatEvent>& outEvents)
{
    TickBurnsForBoard(playerBoard, playerOwner, outEvents);
    TickBurnsForBoard(enemyBoard, enemyOwner, outEvents);
}

void TickBurnsForBoard(std::vector<UnitInstance>& board, const std::string& ownerName,
    std::vector<CombatEvent>& outEvents)
{
    const float kEps = 0.0001f;
    for (size_t i = 0; i < board.size(); ++i)
    {
        UnitInstance& unit = board[i];
        if (unit.currentHP <= 0) { unit.activeBurns.clear(); continue; }

        for (auto& burn : unit.activeBurns)
        {
            // 時刻が飛んでいる場合に備えて、来ている刻みをまとめて消化する。
            while (burn.ticksRemaining > 0 && burn.nextTickTime <= m_currentTime + kEps)
            {
                int beforeHP = unit.currentHP;
                unit.currentHP -= burn.damagePerTick; // 防御・シールドを通さない確定ダメージ。

                CombatEvent e;
                e.type = CombatEventType::Burn;
                e.time = m_currentTime;
                e.actorOwner = burn.sourceOwner;
                e.actorName = burn.sourceName;
                e.actorIndex = burn.sourceIndex;
                e.targetOwner = ownerName;
                e.targetName = unit.def->name;
                e.targetIndex = (int)i;
                e.amount = burn.damagePerTick;
                e.beforeValue = beforeHP;
                e.afterValue = unit.currentHP;
                outEvents.push_back(e);

                burn.ticksRemaining--;
                burn.nextTickTime += burn.interval;

                if (unit.currentHP <= 0)
                {
                    CheckDeath(unit, ownerName, (int)i, outEvents);
                    break; // このユニットの火傷処理は終了。
                }
            }
        }

        // 使い切った火傷を除去する。
        unit.activeBurns.erase(
            std::remove_if(unit.activeBurns.begin(), unit.activeBurns.end(),
                [](const ActiveBurn& b) { return b.ticksRemaining <= 0; }),
            unit.activeBurns.end());
    }
}
```
※ `interval` は `ActiveBurn` にも持たせる（上記 while で使用）。→ `ActiveBurn` に `float interval = 1.0f;` を追加。
※ `<algorithm>`（`std::remove_if`）は `Player.h` 等で既に include 済み。`CombatEngine.h` にも明示追加する。

#### (3) 通常攻撃ヒット時に火傷を付与する

`NormalAttack(...)` の末尾（`CheckDeath` の後）で、対象が生存していれば付与を試みる:
```cpp
if (target.currentHP > 0)
{
    TryApplyOnHitBurn(attacker, attackerOwner, actorIndex, target, targetOwner, targetIndex);
}
```
`NormalAttack` の `attacker` は現在 `const UnitInstance&`。付与処理は `attacker` を変更しないので const のままでよい。
`target` は非 const 参照なので `activeBurns` を触れる。

```cpp
/// <summary>
/// attacker が OnHitBurn パッシブ(アイテム由来)を持っていれば、target に火傷を付与/リフレッシュする。
/// 既に火傷がかかっている場合はスタックせず、残り回数を最大へ戻す(＝最後にヒットしてから ticks 刻み分
/// だけ持続を延長)。damagePerTick は高い方を採用する。
/// **刻みの周期(nextTickTime)はリフレッシュしない** ── 毎秒の刻みを維持したまま持続時間だけ延ばす。
/// (周期までリセットすると、interval 未満の間隔で殴り続けた場合に刻みが永久に先送りされてしまう)
/// </summary>
void TryApplyOnHitBurn(const UnitInstance& attacker, const std::string& attackerOwner, int actorIndex,
    UnitInstance& target)   // targetOwner/targetIndex は受け取らない(マネージャー確定)
{
    for (const ItemDef* item : attacker.items)
    {
        if (item == nullptr) continue;
        for (const PassiveEffect& p : item->passives)
        {
            if (p.type != PassiveEffectType::OnHitBurn) continue;
            if (p.magnitude <= 0 || p.ticks <= 0 || p.interval <= 0.0f) continue; // interval<=0 は Tick が無限ループ

            ActiveBurn* existing = target.activeBurns.empty() ? nullptr : &target.activeBurns.front();
            if (existing != nullptr)
            {
                existing->ticksRemaining = p.ticks; // 残り回数のみリフレッシュ(nextTickTime はそのまま)
                existing->interval = p.interval;
                if (p.magnitude > existing->damagePerTick) existing->damagePerTick = p.magnitude;
                existing->sourceOwner = attackerOwner;
                existing->sourceName = attacker.def->name;
                existing->sourceIndex = actorIndex;
                // 次刻みが過去に取り残されていたら現在時刻基準へ引き上げる(暴走防止の保険。
                // 通常は ProcessDueBurns が毎ウェーブ先頭で消化するので発火しない)。
                if (existing->nextTickTime <= m_currentTime)
                    existing->nextTickTime = m_currentTime + p.interval;
            }
            else
            {
                ActiveBurn b;
                b.damagePerTick = p.magnitude;
                b.interval = p.interval;
                b.nextTickTime = m_currentTime + p.interval;
                b.ticksRemaining = p.ticks;
                b.sourceOwner = attackerOwner;
                b.sourceName = attacker.def->name;
                b.sourceIndex = actorIndex;
                target.activeBurns.push_back(b);
            }
        }
    }
}
```

#### 補足: ループ終了条件との関係

`SimulateCombat` の while は「どちらかの board が全滅するまで」。火傷だけで最後の 1 体を倒すケースでも、
両陣営に生存者がいる限りウェーブは進み続け（各ユニットは毎行動間隔で必ず動く）、その先頭で
`ProcessDueBurns` が走るため、火傷での撃破も必ず記録される。`maxIterations` は行動回数の安全装置で、
火傷刻みはこれを増やさない（`ticks` で上限が決まっているので発散しない）。

### `Game/CombatLogPrinter.h`

`PrintEvent` の `switch` に追加:
```cpp
case CombatEventType::Burn:
    swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs's BURN scorches [%hs] %hs for %d damage! (HP: %d -> %d)\n",
        event.time, event.actorOwner.c_str(), event.actorName.c_str(),
        event.targetOwner.c_str(), event.targetName.c_str(),
        event.amount, event.beforeValue, event.afterValue);
    OutputDebugString(buf);
    break;
```

### `Game/CombatPlayback.cpp`

`ApplyEvent` の `switch` で、`Burn` を既存のダメージイベントと同じ扱いにする:
```cpp
case CombatEventType::NormalAttack:
case CombatEventType::SkillAttack:
case CombatEventType::SplashDamage:
case CombatEventType::Burn:
    if (UnitView* v = ResolveTarget(ev))
    {
        v->displayHP = ev.afterValue < 0 ? 0 : ev.afterValue;
    }
    break;
```
（`Death` は既存どおり `ResolveActor` 側で処理されるため追加不要。）

### `Game/ItemInventoryUIRenderer.cpp`（軽微・実施）

匿名名前空間の `EffectsText(const ItemDef*)` の末尾で、`def->passives` に `OnHitBurn` があれば
要約テキストの末尾に ` 火傷` を付す。FontEngine 制約に触れないよう、追加文字列はかな漢字のみ
（記号・全角矢印は使わない）。`FuriousEdge` の一覧表示が `AT+15 AS+20% 火傷` になる。

## データフロー要約

```
準備フェーズ: ItemSystem::GiveItem で FuriousEdge 等を装備 (passives 付き ItemDef*)
  ↓ 戦闘突入
CombatEngine::SimulateCombat
  - 開始時 activeBurns.clear()
  - NormalAttack 命中 → TryApplyOnHitBurn: target.activeBurns に追加/リフレッシュ
  - 各ウェーブ先頭 ProcessDueBurns: 期限の来た刻みを currentHP に適用し CombatEvent{Burn} を push
  ↓ outEvents
CombatLogPrinter::Print → "... BURN scorches ... for N damage!"
CombatPlayback (実機再生) → displayHP を afterValue へ (HPバーが時間差で減る)
```

## 確認手順

1. `msbuild Game/Game.sln /p:Configuration=Debug /p:Platform=x64` でビルド成功。
2. 実機（F5）→ 準備フェーズで `SwiftCrystal` + `PowerCrystal` を勝利報酬で集め（または開始ゴールド/
   デバッグ手段で）`FuriousEdge` を合成し、物理アタッカー（例: Swordsman / Archer）へ装備 → 戦闘へ。
   - デバッグ出力に `... BURN scorches ...` が、通常攻撃の後に約1秒間隔で最大3回出る。
   - HPバー再生で、対象敵のHPが通常攻撃後もさらに数回カクッと減る。
3. アイテム未装備の編成で戦闘 → `BURN` 行が一切出ない（回帰なし）。
4. `CombatLogPrinter` / `CombatPlayback` の既存イベント表示が変わっていない。

## 確定事項（マネージャー判断済み・2026-08-31）

- **対象アイテムは `FuriousEdge` 1 種のみ**（スコープを絞る。`ArchmageStaff` への拡張は後日検討）。
  → `ItemDatabase.h` の変更は
  `furiousEdge.passives = { { PassiveEffectType::OnHitBurn, 40, 3, 1.0f } };` の 1 行のみ。
- **敵（NightBlade）が round8 で同じ火傷を持つのは許容**（対称ロジック・数値も据え置き）。
- **必殺技ヒットでの火傷付与は無し**（通常攻撃オンヒットのみ）。`UseSkill` は変更しない。
- **`TryApplyOnHitBurn` は `attacker, attackerOwner, actorIndex, target` のみ受け取る**
  （`targetOwner/targetIndex` は付けない。将来必要になったら足す）。

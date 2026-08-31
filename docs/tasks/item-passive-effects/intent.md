# 要件定義: 完成アイテムにパッシブ効果を追加

## 背景・現状

- アイテムの効果は `StatEffect`（ステータス数値の flat/percent 加算）のみ。
  毎ラウンド戦闘直前に `ItemSystem::ApplyItemBonuses` が `bonus*` 系フィールドを再計算するだけで、
  戦闘中に発動する「パッシブ効果」（火傷などの継続ダメージ、条件付き発動、オーラ等）が無い。
- クオリティ比較 `docs/research/tft-vs-hexarena-quality-comparison.md` §6 の指摘:
  「完成品は"素材の合算より少し強い数値"に留まり、質的な差別化が弱い」「完成品にパッシブ効果を
  1〜2個入れる（例: 攻撃時に対象へ数ラウンド継続の火傷 → `CombatEvent` にDoT種別追加）」。
- 戦闘は `CombatEngine::SimulateCombat`（純粋なイベント駆動シミュレーション、`nextActionTime` 内部時計、
  ウェーブ単位）が `std::vector<CombatEvent>` を出力し、`CombatLogPrinter`（テキストログ）と
  `CombatPlayback`（時系列再生・HPバー表示）が別々にそれを読む、という「シミュレーションと表示の分離」設計。
- `CombatEvent` は `actorOwner/actorName/actorIndex` `targetOwner/targetName/targetIndex` `attackType`
  `amount` `beforeValue` `afterValue` を持ち、`CombatPlayback` は `afterValue` で表示用HPを更新する。

## 目的

完成アイテム21種のうち **1〜2種** に、単純な数値加算に加えて **戦闘中に発動するパッシブ効果** を持たせ、
「アイテムでビルドが変わる」感を出す。第一弾として **オンヒット火傷（継続ダメージ / DoT）** を実装する。

## 要求仕様

### パッシブ効果: オンヒット火傷（Burn）

- 対象アイテムを装備したユニットが **通常攻撃を当てた** とき、被弾した敵に「火傷」を付与する。
- 火傷は一定間隔（`interval` 秒）ごとに `magnitude` の固定ダメージを、`ticks` 回与える。
- 火傷ダメージは **防御力で軽減されない**（TFT本家の Burn 同様の"確定ダメージ"扱い。数値スタットとの
  質的な差別化を出す狙い）。シールドも貫通する（`shieldAmount` を減らさず `currentHP` に直接作用）。
- 同じ敵に火傷が再付与された場合は **スタックせず、残り回数を最大値へリフレッシュ**し、
  `magnitude` は高い方を採用する（多段スタックによる青天井を防ぐ）。
- 火傷の刻みは `CombatEngine` のシミュレーション内で解決し、`CombatEvent`（新種別 `Burn`）として記録する。
  `CombatLogPrinter` はテキスト行を、`CombatPlayback` は表示用HPの更新を、それぞれ既存の
  ダメージイベントと同じ枠組みで行う（シミュレーションと表示の分離は崩さない）。
- 火傷の付与元・刻みは敵ユニットが対象アイテムを持っていれば敵→プレイヤー方向にも同様に働く
  （`EnemyFactory` は `ItemSystem` 経由で敵にalso装備しうるため）。

### 対象アイテム

1〜2種を選定する（最終確定は plan.md のレビューで）。第一候補:

- **`FuriousEdge`（激情の刃 / Swift+Power）**: 既存効果 `AttackFlat +15` / `AttackSpeedPercent +20`。
  攻撃速度で手数が多く、火傷のリフレッシュが頻繁に入る物理アタッカー向け。テーマ「激情＝発火」。
- （任意）**`ArchmageStaff`（大魔導の杖 / Wisdom+Wisdom）**: 既存効果 `MagicPowerFlat +30`。
  手数は少ないが1発が重い、魔法アタッカー向け。火傷 `magnitude` を高めに。

数値（初期案・balancingはレビューで調整可）:
- `FuriousEdge`: magnitude 40 / ticks 3 / interval 1.0s
- `ArchmageStaff`: magnitude 60 / ticks 3 / interval 1.0s

## 受け入れ条件

1. `msbuild Game/Game.sln /p:Configuration=Debug /p:Platform=x64` がビルド成功する。
2. 対象アイテムを装備したユニットで戦闘すると、`OutputDebugString` の戦闘ログに
   `Burn`（火傷）ダメージ行が、通常攻撃の後、複数回・時間差で出力される。
3. 実機（F5）で戦闘再生中、火傷を受けた敵のHPバーが通常攻撃の後もさらに数回減っていくのが見える。
4. 対象アイテムを装備していないユニットだけの戦闘では、火傷ログが一切出ない（既存挙動が不変）。
5. `plan.md` の設計と実装が一致している。

## スコープ外 / 割り切り

- 火傷以外のパッシブ（回復、オーラ、条件付き発動、Shred/Sunder 等）は本タスクでは実装しない。
  ただし `PassiveEffect` のデータ構造は将来種別を足せる形にする。
- 必殺技ヒットでの火傷付与は第一弾では行わない（通常攻撃オンヒットのみ）。plan.md で最終判断。
- 火傷の3Dエフェクト表示・アイコン表示はしない（HPバーの数値変化とログのみ）。
- `normalAttackCount` / `receivedAttackCount` が戦闘間でリセットされない既存の挙動には手を付けない。

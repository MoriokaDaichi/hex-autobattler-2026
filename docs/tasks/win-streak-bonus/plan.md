# 連勝ボーナス強化 — plan

intent.md を前提とする。

## 変更ファイル

### 1. `Game/EconomySystem.h`
- 連勝と連敗で表を分離する。共通の `GetStreakBonus` を廃止し、以下の public static 関数に置き換える
  (ツールチップからも参照するため public static)。
  - `GetWinStreakBonus(int winStreak)`: `winStreak >= 2` なら `10 + 2 * (winStreak - 2)`、それ以外 0。上限なし(当初の5×連勝数から+2刻みに変更)。
  - `GetLossStreakBonus(int lossStreak)`: 従来どおり 2で+1、3-4で+2、5以上で+3。
- 定数を追加:
  - `kWinStreakBaseBonus = 10`(2連勝時の額)、`kWinStreakGoldPerWin = 2`(以降1連勝ごとの増分)
  - `kWinStreakBonusMinStreak = 2`(連勝ボーナスが付き始める連勝数)
  - `kWinStreakItemMinStreak = 5`(勝利時に素材アイテムが追加で1つ付く連勝数)
- `GrantRoundIncome` は勝ち→`GetWinStreakBonus`、負け→`GetLossStreakBonus`、引き分け→0。
  収入ログの書式は変更しない。
- `HasWinStreakItemBonus(int winStreak)` を追加(`winStreak >= kWinStreakItemMinStreak`。ツールチップで「次の勝利」の判定にも使うため連勝数を引数に取る)。

### 2. `Game/Game.cpp` `ApplyCombatOutcome`
- `GrantRoundIncome` で `winStreak` が更新された後(既存の呼び出し順のまま)、勝利報酬の素材アイテム付与処理の直後に、
  `EconomySystem::HasWinStreakItemBonus(player.winStreak)` が真ならもう1つ `PickRandomComponent` で付与する。
- ログ: `[Reward] Win streak bonus item: <name> (streak=N, unclaimed total=M)`。
- フィードバック: `連勝ボーナス: <name>`(Success)。
- 既存の勝利報酬ブロックと同じ書き方で並べる(2か所だけなのでヘルパー化はしない)。

### 3. 画面表示の整合
- grep の結果、連勝ボーナスの**数値**を表示している箇所は無かった。
  `TooltipContentBuilder.cpp` の `HudStreakDisplay` ツールチップは「勝利報酬のゴールドが増える」と定性的に書いているだけ。
- 食い違いは無いが、新仕様が分かるようツールチップに行を追加する:
  - 連勝中: 「次の勝利で連勝ボーナス +X G」(X = `GetWinStreakBonus(winStreak + 1)`)、
    `winStreak + 1 >= 5` なら「勝利時に素材アイテム+1」も表示。
  - 連敗中: 「次の敗北で連敗ボーナス +X G」(`GetLossStreakBonus(lossStreak + 1)`)。
  - バッファ長は 48 → 96 に拡張。

## 懸念
- 基本収入5に対し、5連勝で+25、8連勝で+40と非常に大きい。全10ラウンド構成なので最大でも10連勝(+50)程度で収まるが、
  勝ち続けるとゴールドが余りやすくなる点は実機で確認したい。
- 連勝数は引き分けで0リセット(従来どおり)。

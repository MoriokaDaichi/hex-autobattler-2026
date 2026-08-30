# 要件定義: 特性(トレイト)パネルUI

- 種別: クオリティ比較調査(6a担当)で最優先とされた新規タスク(Notion既存タスクではない、ユーザー承認済み)
- 担当: 実装用セッション / 依頼元: マネージャー

## 背景・現状

- `TraitSystem::ApplyTraitBonuses()` は盤面(board)のトレイト構成を毎ラウンド戦闘開始直前に集計し、
  各ユニットへボーナスを適用している。集計結果(`traitCounts`)はこの関数のローカル変数で、
  `LogActiveTraits()` により `OutputDebugString` へログ出力されるのみ。**画面に表示するUIが一切無い。**
- 6aの調査(`docs/research/TFT.md`)によると、これが現状のUI/UX比較で最も大きな欠落。
- 本家TFTでは画面左側に全特性を縦列表示し、**発動中の特性は上部にハイライト、未発動は下部にディム表示**、
  各行に現在人数と次のブレイクポイント閾値を表示する構成(`docs/research/TFT.md` 2節参照)。
- トレイトは `TraitDatabase::GetAllTraitDefs()` で全8種(Monster/Human/Hero/Warrior/Mage/Guardian/
  Assassin/Ranger)を列挙可能。各トレイトの段階(`TraitTier`)は現状すべて2段階
  (HeroのみrequiredCount=1,2、他は2,4)。将来的に段階が増える可能性はあるが、本タスクではUI表示が主眼。
- トレイトはbench(控え)ではなくboard(盤面配置済み)のユニット構成のみで決まる
  (`TraitSystem::ApplyTraitBonuses`が`board`のみを走査している)。

## 目的

準備フェーズ中、現在の盤面構成でどのトレイトが何人発動していて、次の閾値まで何人必要かを
画面上で常に確認できるようにする。

## 要求仕様

1. 全トレイト(8種)を一覧表示する新規UI(`TraitPanelUIRenderer`)。
2. 各行: トレイト名・現在の発動人数(board上のユニット数)・次のブレイクポイント閾値
   (最終段階まで発動済みならその旨)。
3. 発動中のトレイトは上部にまとめてハイライト表示、未発動のトレイトは下部にまとめてディム表示
   (本家の見せ方を踏襲。テキストベースで構わない)。
4. 表示タイミングは準備フェーズ中(戦闘中に表示するかは実装judgement)。
5. 画面はかなり埋まっている: 左上FPS / 左BENCH一覧(準備フェーズ、可変長で下に伸びる) /
   右上GOLD・LV(PlayerStatusUIRenderer) / 右上ROUND・連敗(RoundRecordUIRenderer) /
   右ITEMS一覧(ItemInventoryUIRenderer、準備フェーズ、可変長で下に伸びる) / 下部SHOP。
   本家は左側にトレイトパネルを置くが、BENCHと同じ左側のため配置には工夫が要る。
   実機で確認しながら既存UIと重ならない配置を検討する。

## 既知の制約(FontEngine)

- pivot による中央揃えは機能しない。`kTopLeftPivot` 前提で開始X座標を手動調整する。
- `color.w` による alpha フェードは機能しない。点滅等は描画自体のON/OFF切替で行う。
- ソースファイルは UTF-8 (BOM付き) で保存する(BOM無しだとCP932環境でパースが壊れる)。

## 受け入れ条件

- ビルドが通る(Debug/x64)。
- 実機で、盤面へのユニット配置に応じてトレイトパネルの発動人数・発動中/未発動の区分けが
  正しく更新されることが確認できる。
- 既存UI(FPS / BENCH / GOLD・LV / ROUND・連敗 / ITEMS / SHOP)と表示が重ならない
  (典型的なプレイ状況において。極端に大きいbench/itemsでの完全な非重複は保証しない旨を
  plan.mdのスコープ外に明記する)。

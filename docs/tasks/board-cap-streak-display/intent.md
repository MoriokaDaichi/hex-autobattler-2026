# 要件定義: 盤面使用数/上限・連勝連敗ストリークの常時表示 + トレイト3段階化

- 種別: マネージャー依頼の小タスク3件まとめ(ユーザー承認済み、Notion既存タスクではない)
- 担当: 実装用セッション / 依頼元: マネージャー
- 作業ツリー: main(`c:\my\01_project\02_GameProject\GameTemplate`)

## 背景・現状

- **盤面上限**: `Player::GetMaxBoardSize()`(= `level` をそのまま返す)で「今何体まで盤面に置けるか」が決まり、
  `PlaceUnitOnBoard` が `board.size() >= GetMaxBoardSize()` で配置を弾く。しかし現在の使用数/上限が
  **画面に一切出ていない**(配置失敗フィードバックは ShopUIRenderer に一瞬出るのみ)。
- **ストリーク**: `Player::winStreak` / `Player::lossStreak` を `EconomySystem` が毎ラウンド更新し、
  利子計算に使っている(2つは排他: 片方が増えるともう片方は0)。しかし画面に出ていない。
  なお `RoundRecordUIRenderer` の「連敗 N / M」は別概念(現在の敵に対する敗北数 = ゲームオーバー猶予)で、
  ストリークとは異なる。
- **トレイト段階**: `TraitDatabase::Init` の各トレイトは現状すべて2段階(HeroのみrequiredCount 1,2、他は 2,4)。
  `TraitSystem::FindActiveTier` / `FindNextTier`、`TraitPanelUIRenderer` はいずれも段階数に依存しない実装
  (2段階を前提にした箇所は無い)。

## 目的

- 準備フェーズ中に、盤面の使用数/上限が常に分かるようにする。
- 現在の連勝/連敗ストリークが常に分かるようにする。
- トレイトに3段階目(6体)を追加し、シナジーの伸びしろを増やす。

## 要求仕様

1. **盤面使用数/上限表示**: `PlayerStatusUIRenderer`(GOLD/LV を右上に表示)に「盤面 使用/上限」を追加。
   `board.size()` / `GetMaxBoardSize()`。上限に達しているときは色で分かるようにする。
2. **ストリーク表示**: `RoundRecordUIRenderer`(ROUND/残り/連敗 を右側に表示)に現在のストリークを追加。
   連勝中は連勝数、連敗中は連敗数、どちらでもなければその旨。既存の「連敗 N / M」行と混同しない表記にする。
3. **トレイト3段階目**: `TraitDatabase::Init` で全8トレイトに requiredCount 6 の段階を追加する。
   効果値は既存の1→2段階目の伸び方から自然な範囲(判断は実装者に一任)。

## 既知の制約(FontEngine)

- pivot による中央揃え/右寄せは機能しない。`kTopLeftPivot`(実質左詰め)前提で開始X座標を手動調整する。
- `color.w` による alpha フェードは機能しない。強調は色そのもので行う。
- ソースファイルは UTF-8 (BOM付き) で保存する。

## 受け入れ条件

- ビルドが通る(Debug/x64)。
- 実機で、盤面へのユニット配置数に応じて「使用/上限」が更新され、上限到達時に色が変わることが確認できる。
- 実機で、連勝/連敗に応じてストリーク表示が更新されることが確認できる
  (最低限、初期状態=ストリーク無しの表示と、既存UIと重ならないことを確認)。
- 追加した表示行が既存UI(FPS / BENCH / GOLD・LV / ROUND・残り・連敗 / ITEMS / SHOP / TRAITS)と
  典型的なプレイ状況で重ならない。
- トレイト3段階目を追加してもビルド・既存挙動(2段階目までの発動)が壊れない。
  `TraitPanelUIRenderer` の「次の閾値」表示が 4/6 → 6 MAX と正しく段階を反映する。

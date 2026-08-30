# 設計: プレイヤー向けアイテム取得・装備導線

## 追加ファイル

| ファイル | 役割 |
|---|---|
| `Game/ItemInventoryUIRenderer.h` / `.cpp` | 準備フェーズ、未装備アイテム一覧を画面右側に2D表示する `IRenderer`。`RoundRecordUIRenderer` 踏襲。 |

`Game/Game.vcxproj` と `Game/Game.vcxproj.filters` に上記2ファイルを追加(フィルタ `Core`)。

## 変更ファイル

### `Game/Player.h`

- `std::vector<const ItemDef*> unclaimedItems;` を追加。まだどのユニットにも装備していない入手済みアイテム。

### `Game/CursorSelectionSystem.h` / `.cpp`

- `enum class InputFocus` に `Items` を追加(`Bench` と `Board` の間)。
- Tab / Select の巡回順: `Shop → Bench → Items → Board → Shop`。
- `UpdateListCursor()` の一覧カーソル対象に `Items` を追加(Shop / Bench と同じ扱い)。

### `Game/Game.h`

- `#include "ItemInventoryUIRenderer.h"`、メンバ `ItemInventoryUIRenderer m_itemInventoryUI;` を追加。
- メンバ `int m_heldUnclaimedIndex = -1;` を追加。準備フェーズで「手に持っている」未装備アイテムの
  `players[0].unclaimedItems` 上の index(-1 で無し)。

### `Game/Game.cpp`

- `#include <random>` と、匿名名前空間に `PickRandomComponent(const ItemDatabase&)` ヘルパーを追加
  (`ItemCategory::Component` を集めて `std::mt19937` で1つ抽選。`ShopSystem` と同じRNG方針)。
- `InitializeNewRun()` / 戦闘突入(Bボタン)で `m_heldUnclaimedIndex = -1` にリセット。
- 準備フェーズのカーソルクランプ処理に `Items` フォーカス時の `ClampListCursor(unclaimedItems.size())` を追加。
  併せて `m_heldUnclaimedIndex` が範囲外になっていたら解除。
- **Aボタン処理を再構成**(フォーカスと「アイテムを手に持っているか」で分岐):
  - `focus == Items`: カーソルのアイテムを手に持つ / 同じものをもう一度で手放す。
  - アイテム保持中 かつ `focus == Bench | Board`: 選択中のユニット(bench はカーソル index、
    board はヘックスカーソル位置のユニット)へ `m_itemSystem.GiveItem()` で装備確定。
    成功したら `unclaimedItems` から除去し `m_heldUnclaimedIndex = -1`。満杯等の失敗はフィードバック表示。
  - それ以外: 従来通りユニット購入(挙動は変更しない)。
- **勝利報酬**: `result == CombatResult::Win` の分岐(`=== Round N Clear! ===` ログ直後)で
  `PickRandomComponent()` の結果を `player.unclaimedItems.push_back()`。ログ + `m_shopUI.PushFeedback()`。
- `Game::Render()` の `Phase::Preparation` ブロックで `m_itemInventoryUI.Draw(rc, player, itemsFocused, cursorIndex, m_heldUnclaimedIndex)` を呼ぶ。

## データ構造

- `Player::unclaimedItems : std::vector<const ItemDef*>` … `ItemDatabase` 内の要素へのポインタ
  (`FindItemDefByName` 等と同じくDBのvector要素は寿命が安定している前提)。
- `Game::m_heldUnclaimedIndex : int` … 装備先ユニット選択待ちの一時状態。フェーズをまたがない。

## UI配置(`ItemInventoryUIRenderer`)

- 座標系 UI_SPACE(1920x1080、中央原点・y上向き)。
- `kX = UI_SPACE_WIDTH * 0.27f`(`RoundRecordUIRenderer` と同じ左端x)。
- `kTopY = UI_SPACE_HEIGHT * 0.25f`、`kStepY = 40`。右上の PlayerStatus(y≒454〜500)/
  RoundRecord(y≒312〜388)の下、下部の Shop(y≒-330〜-446)の上に収まる。想定最大(未装備9個)でも重ならない。
- タイトル `ITEMS (n)` + 各行 `<marker><name>  <効果要約>`。
  - marker: 手に持ち = `[持] `、カーソル選択中 = `> `、その他 = 空白。
  - 状態表現は色(選択=黄 / 保持=緑 / 通常=淡灰)とスケール差(選択時 1.08x)。alpha は使わない。
- 準備フェーズのみ描画(`Game::Render()` の該当ブロックからのみ `Draw()` を呼ぶ)。

## 既知の制約への対応

- FontEngine の pivot 中央揃え無効 → `kTopLeftPivot` + 手動X。
- FontEngine の `color.w` フェード無効 → マーカー文字と色/スケールで状態表現。点滅なし。
- ソースは UTF-8 (BOM付き) で保存(CP932環境でのパース崩れ回避)。

## スコープ外 / 既知の割り切り

- 合成(3体 → ★アップ)時に元ユニットの装備アイテムが失われるのは既存仕様。本タスクでは触らない。
- アイテムのショップ購入・ドロップ選択(複数候補から選ぶ)は不採用。勝利報酬でランダム1個固定。

# 合成・売却時の装備アイテム引き継ぎ — plan

要件は `intent.md` を参照。並行作業(ゲームパッド入力の廃止)が `Game/Game.cpp` を大きく書き換えるため、
ロジックは `Player.h` / `ItemSystem.h` / `ItemDatabase.h` に寄せ、`Game.cpp` は通知の表示1か所のみ追加する。

## 現状
- `Player::TryMergeUnits()` は素材3体を erase し、`UnitInstance merged(def)` を新規生成する → 3体の `items` が消える。
- `Player::SellUnitFromBench/SellUnitFromBoard()` は erase するだけ → `items` が消える。
- 売却経路: クリック(Game.cpp 右クリック2回 / 旧LB1)・ドラッグ&ドロップ(DragDropController.cpp DropKind::Sell)。
  いずれも上記2関数を呼ぶので、関数側を直せば全経路に効く。
- `TryMergeUnits()` は `BuyUnit`/`PlaceUnitOnBoard`/`ReturnUnitToBench`/`SwapBenchWithBoard` から引数無しで呼ばれ、
  `ItemDatabase` を受け取れない。素材の自動合成(`ItemSystem::GiveItem`)にはレシピ(`ItemDatabase`)が必要。
- `ShopUIRenderer::PushFeedback` は1行を上書きする方式(キューではない)。

## 設計

### 1. ItemDatabase: 「アクティブなDB」の参照 (`Game/ItemDatabase.h`)
- `static const ItemDatabase* GetActive()` を追加。`Init()` の最後で自身を登録し、デストラクタで(自身なら)解除する。
- 実体は関数内 static(`ActiveSlot()`)。ItemDatabase の実体は `Game::m_itemDatabase` の1つだけなので曖昧さは無い。
- 目的: 引数無しで呼ばれる `TryMergeUnits()` からレシピを引くため。Game.cpp 側に「Player へ DB を渡す」コードを足さずに済む。
- 未登録(nullptr)時は自動合成せず、空き枠への単純追加のみ行う(フォールバック)。

### 2. ItemSystem: 引き継ぎヘルパー (`Game/ItemSystem.h`)
- `static void CarryOverItems(UnitInstance& unit, const std::vector<const ItemDef*>& items, std::vector<const ItemDef*>& overflow, const std::string& ownerName)`
  - `items` を順に装備。DBがあれば `GiveItem`(素材の自動合成+上限3)に従い、無ければ空き枠へ追加のみ。
  - 装備できなかった(枠満杯かつ合成不成立)ものは `overflow` に積む。
- `GiveItem` は非staticメンバだがステートレスなので、内部で `ItemSystem{}` を使う。

### 3. Player (`Game/Player.h`)
- `#include "ItemSystem.h"` を追加(ItemSystem.h → UnitInstance.h/ItemDatabase.h のみで循環しない)。
- 通知用 `std::vector<std::wstring> itemNotices;` を追加。装備の戻し/引き継ぎがあった時に1件積む。Game 側が表示後に clear する。
- `ReturnItemsToUnclaimed(UnitInstance&, const wchar_t* reason)`(private的ヘルパー): 装備を全て `unclaimedItems` 末尾へ移し、
  通知「売却: 装備をアイテム欄に戻しました (A, B)」と OutputDebugString ログを出す。
- `SellUnitFromBench/SellUnitFromBoard`: erase 前に上記を呼ぶ。→ クリック/ドラッグ全経路で同じ挙動。
- `TryMergeUnits`:
  - 合成後に残る1体(survivor)= 盤面にいた素材のうち位置を引き継ぐもの(従来の mergedPos と同じく、3体中最後に見つかった盤面の1体)。
    盤面に1体もいなければ3体中の最初の1体。
  - erase 前に survivor の items をコピー、残り2体の items を素材の並び順で連結して控える。
  - `merged.items = survivor.items`(既存装備はそのまま)→ `ItemSystem::CarryOverItems(merged, others, overflow)`。
  - `overflow` は `unclaimedItems` 末尾へ。
  - 通知: 引き継ぎがあれば「合成★N: 装備を引き継ぎました (…)」、あふれがあれば「… / 枠超過でアイテム欄へ: (…)」。
  - 星2→星3 の連鎖合成でも `while (TryMergeUnits())` の各回で同処理が走るため同様に引き継がれる。
- `ReturnUnitToBench`(盤面→ベンチ)は変更しない(装備は付いたまま)。

### 4. Game.cpp(最小限)
- `Update()` の `m_shopUI.UpdateFeedbackTimer(...)` 直後に、`players[0].itemNotices` が空でなければ " / " で連結して
  `PushFeedback(..., Info)` し clear する、という1ブロックのみ追加。
  前フレームの操作で積まれた通知を次フレーム頭で表示する(1フレーム遅れ。売却/購入の直前メッセージは上書きされるが、
  通知文自体に「売却:」「合成」等の文脈を含める)。入力処理ブロックには触れないので並行作業と衝突しにくい。

### 5. HelpContent.cpp
- アイテムカテゴリの「注意: ユニットを売却・合成すると、そのユニットの装備アイテムは無くなる」を新仕様に書き換える。
  「操作」カテゴリ・パッド表記行には触れない。

## 懸念点
- アクティブDBは暗黙のグローバル参照。ItemDatabase を複数生成する設計に変わったら見直しが必要。
- 合成後のユニットの装備は「survivorの既存装備→他2体」の順で GiveItem されるため、他2体の素材が survivor の素材と
  自動合成されることがある(要件どおり)。
- 未装備アイテム一覧に完成アイテムが戻るようになる(従来は素材のみ)。一覧UI・ツールチップ・セーブは ItemDef 一般を扱うため問題ない想定だが、実機で表示を確認する。

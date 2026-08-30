# 設計: 特性(トレイト)パネルUI

## 追加ファイル

| ファイル | 役割 |
|---|---|
| `Game/TraitPanelUIRenderer.h` / `.cpp` | 準備フェーズ、全トレイトの発動状況を画面左側に2D表示する `IRenderer`。`BoardUIRenderer`のベンチ一覧・`ItemInventoryUIRenderer`踏襲。 |

`Game/Game.vcxproj` と `Game/Game.vcxproj.filters` に上記2ファイルを追加(フィルタ `Core`)。

## 変更ファイル

### `Game/TraitSystem.h`

- 表示専用の非破壊な集計メソッドを追加する(`ApplyTraitBonuses`はボーナス適用+HP全回復という
  副作用を持つため、準備フェーズで毎フレーム呼ぶわけにはいかない)。
  - `std::map<TraitType, int> CountBoardTraits(const std::vector<UnitInstance>& board) const`
    … `ApplyTraitBonuses`冒頭の集計ループを抽出したもの。
  - 既存の`FindActiveTier`を`private`から`public`へ、`const`を付けて移動(表示側からも使う)。
  - `const TraitTier* FindNextTier(const TraitDef& traitDef, int count) const` を追加。
    `requiredCount > count`を満たす最初の`tier`を返す(無ければnullptr=最終段階到達済み)。
- `ApplyTraitBonuses`自体のロジックは変更しない(`CountBoardTraits`を内部で使うようリファクタしても
  外部から見た挙動は同一)。

### `Game/Game.h`

- `#include "TraitPanelUIRenderer.h"`、メンバ `TraitPanelUIRenderer m_traitPanelUI;` を追加。

### `Game/Game.cpp`

- `Game::Render()` の準備フェーズブロックに `m_traitPanelUI.Draw(rc, player.board, m_traitDatabase, m_traitSystem)` を追加。
  戦闘中は表示しない(戦闘中はBoardUIRendererのHPバー表示で画面が既に埋まるため、準備フェーズのみに絞る)。

## データ構造

- `TraitPanelUIRenderer`は`UnitInstance`/`TraitDef`等への参照は保持せず、Draw()の中で表示用の
  文字列(`std::wstring`)へ変換して保持する(他のUIRendererと同じく、Draw()呼び出しとOnRender2D()
  呼び出しがフレーム内で分かれているため)。

## UI配置(`TraitPanelUIRenderer`)

- 座標系 UI_SPACE(1920x1080、中央原点・y上向き)。
- 配置: 画面左側、`BoardUIRenderer`のBENCH一覧(`kBenchX = -910`、`kBenchTopY = 250`から下に伸びる)
  **と同じX列の、さらに下**。本家同様「左側」に置きつつ、BENCHとは縦に住み分ける。
  - `kX = -910`(BENCHと同じ左端)。
  - `kTopY = -70`。典型的なプレイ(bench数体程度)ではBENCH最下行より下に十分な余白がある。
  - `kStepY = 24`(タイトル込み9行 = 216px、下端 ≒ -286。SHOPの`kFeedbackY = -330`まで約44pxの余白)。
  - 極端に大きいbench(8体以上)ではBENCH最下行と本パネルが重なりうる。本タスクでは対応しない
    (スコープ外、下記)。
- タイトル行 `TRAITS`。
- 発動中トレイト → 未発動トレイトの順に並べる(要求仕様通り「上に発動中、下に未発動」)。
  各グループ内は`TraitDatabase::GetAllTraitDefs()`の登録順(出自3種→役割5種)を維持。
- 各行フォーマット: `<マーカー><Name>  <count>/<次の閾値 or "MAX">`
  - マーカー: 発動中 = `"> "`(ShopUIRendererのカーソル`>`と同じ記号を流用、"強調"の意味で一貫)。
  - 未発動 = `"  "`(スペース2つ、インデント揃え)。
  - 最終段階まで到達済みなら閾値の代わりに`"MAX"`。
- 色: 発動中は明るい白寄りの色(既存の見出し色 `(0.9,0.9,0.95,1.0)` を流用)、未発動は
  `(0.5,0.5,0.55,1.0)`程度のディムグレー。alphaは使わず色そのものを変える(既知の制約対応)。
- 準備フェーズのみ描画(`Game::Render()`の該当ブロックからのみ`Draw()`を呼ぶ)。

## 既知の制約への対応

- FontEngine の pivot 中央揃え無効 → `kTopLeftPivot` + 手動X(左詰めなので影響小)。
- FontEngine の `color.w` フェード無効 → 発動中/未発動を色とマーカー文字で区別。点滅なし。
- ソースは UTF-8 (BOM付き) で保存(CP932環境でのパース崩れ回避)。

## スコープ外 / 既知の割り切り

- トレイトの段階が3段階以上に増えた場合の表示(現状全トレイト2段階、`FindNextTier`は
  段階数に依存しない設計のためロジック自体は対応済みだが、UI崩れの実機確認は現状データでのみ行う)。
- 極端に大きいbench(8体以上)でのBENCH一覧との重なり回避(想定外の運用のため今回はスコープ外)。
- 戦闘中(Combatフェーズ)のトレイトパネル表示(要求仕様上任意、今回は準備フェーズのみに絞る。
  画面が最も埋まる戦闘中はBoardUIRendererのHPバー表示を優先)。

# 設計: 盤面ユニット配置を自陣(q0〜2)に制限する

新規ファイルなし。既存3ファイルの変更のみ。

## 変更ファイル

### `Game/HexGridRenderer.h`

- `private:` の盤面範囲定数の並びに、自陣範囲の `public` 定数を追加:
  ```cpp
  // 自陣(プレイヤーがユニットを配置できるゾーン)の axial q 範囲。
  // 盤面全体の区分けは下の kMinQ〜kMaxQ / このクラスの BuildTileFills() を参照
  // (0-2:自陣 / 3-5:中立 / 6-8:敵陣)。
  static constexpr int kAllyZoneMinQ = 0;
  static constexpr int kAllyZoneMaxQ = 2;
  ```
  ※ `IsValidHex` 等と同じく他所から参照しうるので `public`。

### `Game/HexGridRenderer.cpp`

- `BuildTileFills()` の `if (q <= 2)` を `if (q >= kAllyZoneMinQ && q <= kAllyZoneMaxQ)` に変更
  （マジックナンバーの出所を1箇所へ寄せるだけ。塗り分け結果は不変）。

### `Game/Player.h`

- `Player` に自陣範囲の `static constexpr` 定数を追加（`HexGridRenderer` と同値・出所コメント付き。
  循環 include 回避のため `HexGridRenderer.h` は include しない）:
  ```cpp
  // プレイヤーがユニットを配置できる自陣の axial q 範囲。
  // 盤面の区分けの正は HexGridRenderer(kAllyZoneMinQ/MaxQ)。circular include を避けるため同値を再掲。
  static constexpr int kAllyZoneMinQ = 0;
  static constexpr int kAllyZoneMaxQ = 2;
  ```
- `PlaceUnitOnBoard()` の先頭付近（ベンチ index チェックの直後）に自陣範囲チェックを追加:
  ```cpp
  if (targetPos.q < kAllyZoneMinQ || targetPos.q > kAllyZoneMaxQ)
  {
      return false; // 自陣(q0-2)以外には配置できない。
  }
  ```
  ※ r 範囲や q<=8 は既存同様チェックしない（ヘックスカーソルが有効マスを保証しており、
    本バグの対象はゾーン制限のみ）。

### `Game/Game.cpp`

- X ボタン配置処理（現 305〜308 行あたり）の失敗フィードバック文言を、
  自陣制限にも触れる形へ更新:
  `L"配置できません (自陣 q0-2 のみ / 盤面上限 / 空きマス無し)"`
  （分岐を増やさず1行の文言変更に留める。`OutputDebugString` の詳細ログは既に Hex 座標を出力済み）。

## 非変更（意図的に触らない）

- `CursorSelectionSystem` … カーソルは盤面全体を移動可のまま（intent.md の判断参照）。
- `HexGridRenderer::IsValidHex` … 盤面全体の範囲判定という本来の意味を維持。
- `EnemyFactory` … `Player::PlaceUnitOnBoard` を経由しないため影響なし。

## 確認手順

1. `msbuild Game/Game.sln /p:Configuration=Debug /p:Platform=x64` でビルド成功を確認。
2. 実機（F5）→ 準備フェーズでベンチのユニットを選び、
   - 自陣（q0〜2）のマスへ配置 → 成功（盤面に出る / フィードバック「配置: マス(...)」）。
   - 中立（q3〜5）・敵陣（q6〜8）のマスへ配置操作 → `false`、
     フィードバック「配置できません (自陣 q0-2 のみ / ...)」、`OutputDebugString` に
     `Place result: false` が出る。

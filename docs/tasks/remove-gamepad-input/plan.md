# ゲームパッド入力の廃止 — plan

intent.md の要求仕様 1〜5 を満たすための技術設計。編集は `Game/` 配下のみ(k2Engine系は触らない)。

## 0. 調査結果

### 0-1. `g_pad` の使用箇所(Game/ 配下)
| 場所 | 用途 | マウス代替 |
|---|---|---|
| `main.cpp` ゲームループ | A押下で振動(テストコードの残骸) | 不要(削除) |
| `Game.cpp` Title | A=続きから/開始、X=新規開始 | 有り(TitleStartButton / TitleNewGameButton) |
| `Game.cpp` Preparation A | フォーカス依存: 購入 / アイテムを持つ / 装備 | 有り(左クリック専用ブロック・D&D) |
| `Game.cpp` Preparation B | 戦闘開始 | 有り(NextPhaseButton) |
| `Game.cpp` Preparation X | ベンチ→盤面配置 / 盤面内移動 | 有り(左クリック・D&D) |
| `Game.cpp` Preparation Start | ショップロック | 有り(LockButton) |
| `Game.cpp` Preparation Y | リロール | 有り(RerollButton) |
| `Game.cpp` Preparation LB1 | 売却 / ベンチへ戻す | 有り(右クリック2回・D&D) |
| `Game.cpp` Preparation RB1 | XP購入 | 有り(BuyXpButton) |
| `Game.cpp` GameOver/Victory A | タイトルへ戻る | 有り(RestartButton) |
| `CursorSelectionSystem.cpp` | Select=フォーカス巡回、十字=一覧/ヘックスカーソル移動 | パッド操作のための状態そのもの(削除) |
| `HelpUIRenderer.cpp` | LT=開閉、B=閉じる、十字=カテゴリ/ページ | キー直接読み(F1/H/Esc/矢印)と同等のものが既にある |

→ マウスで代わりができない操作は無い。新しいキーボード代替は追加しない。

### 0-2. k2Engine のキーボード→パッド割り当て
`k2EngineLow/HID/GamePad.cpp` に、物理パッド未接続時にキーボードを `g_pad` へ割り当てるフォールバックがある
(J=A, K=B, L=X, I=Y, B=LB1, N=LB2(LT), Enter=Start, Space=Back, テンキー8/2/4/6=十字, WASD/矢印=スティック等)。
エンジン側は変更しない。ゲームが `g_pad` を一切読まなくなれば、これらのキーは自動的に無反応になる。

### 0-3. キーボードを直接読んでいる箇所(残す)
- F5: 準備フェーズのセーブ(`Game.cpp`)
- F1 / H: ヘルプ開閉、Esc: ヘルプを閉じる、↑↓: カテゴリ、←→: ページ(`HelpUIRenderer.cpp`)
- Tab / ←→↑↓(`CursorSelectionSystem.cpp`): パッドと同じフォーカス/カーソル操作のためのもの → **削除**(フォーカス自体を廃止するため)

## 1. 削除するもの

### 1-1. `CursorSelectionSystem`
- `InputFocus` enum、`Update()`、`UpdateFocusSwitch/UpdateListCursor/UpdateHexCursor`、`TryMouseToHex`、
  `GetFocus/GetListCursorIndex/ClampListCursor/GetHexCursor`、メンバ変数、匿名名前空間の方向定数を削除。
- 残すのは `ScreenToUISpace()`(UIInteractionSystem / DragDropController / ツールチップが使用)と
  その内部ヘルパー `GetNormalizedMousePosition()`。クラスは「マウス座標→UI_SPACE変換」の静的ユーティリティとして残す
  (ファイル名・クラス名の変更は vcxproj の編集を伴うため今回は行わない)。
- `HexGridRenderer::TryWorldPositionToHex` は呼び出し元が無くなるが、HexGridRenderer の汎用APIとしてそのまま残す。

### 1-2. `Game`
- `Game.h`: `m_cursorSelection` メンバ、`m_heldBoardHexFromMouse`(フォーカス掃除からマウス発の選択を除外するためだけの
  フラグ。フォーカスが無くなれば不要)を削除。関連コメントをマウス前提に修正。
- `Game.cpp Update()`:
  - `m_cursorSelection.Update()` 呼び出し、ClampListCursor ブロック、「Boardフォーカスから外れたら移動元選択を解除」ブロックを削除。
  - Preparation の A / X / LB1 ブロックを丸ごと削除(マウス左クリック/右クリック/D&D に同等処理あり)。
  - Title(A/X)、B/Start/Y/RB1、GameOver/Victory(A) の `g_pad[0]->IsTrigger(...) ||` 部分を削除し、マウス判定のみにする。
  - ツールチップの「マウスがホバーしていなければフォーカス中要素を即表示」フォールバックと、その際のアンカー計算分岐を削除
    (マウスホバー時のみ表示)。
- `Game.cpp Render()`: 各UIへ渡していた focus / cursorIndex 引数を削除。

### 1-3. 各UI Renderer の選択(フォーカス)表示
- `ShopUIRenderer::Draw`: `shopCursorIndex` / `shopFocused` 引数とメンバを削除。カードの「選択中」金枠・`> `マーカー・
  非選択時の名前の減光を削除(マウスホバー時の水色枠は残す)。名前は常に通常色で表示。
- `BoardUIRenderer::DrawPreparation`: `benchFocused` / `benchCursorIndex` 引数とメンバを削除(ホバー枠は残す)。
- `ItemInventoryUIRenderer::Draw`: `focused` / `cursorIndex` 引数とメンバ、`kSelectedColor`、タイトルの `[Tab]` 表示を削除。
  「手に持っている」表示(`[持]`・緑枠)とホバー枠は残す。

### 1-4. `main.cpp`
- ゲームループ内の「A押下で振動」3行を削除。

## 2. ラベル・文言の置き換え
| 場所 | 旧 | 新 |
|---|---|---|
| ShopUIRenderer 戦闘開始ボタン | `戦闘開始 [B]` | `戦闘開始` |
| TitleUIRenderer(セーブ無し) | `PRESS [A] TO START` | `CLICK TO START` |
| TitleUIRenderer(セーブ有り) | `[A] CONTINUE     [X] NEW GAME` | `CONTINUE     NEW GAME` |
| ResultUIRenderer | `PRESS [A] TO TITLE` | `CLICK TO TITLE` |
| ItemInventoryUIRenderer 操作ガイド | `ベンチ/盤面のユニットを選び [A] で装備` | `ベンチ/盤面のユニットをクリックで装備` |
| Game.cpp フィードバック | `(ユニットを選び[A]で装備)` 等 | A/X ブロックごと削除されるため消える |

- タイトル/結果のクリック矩形と表示開始Xは、文字数が変わるため `UITextUtil::EstimateTextWidth` / 既存の概算式で
  中央揃えになるよう再計算する(実機で要微調整なのは従来どおり)。

## 3. ヘルプ(`HelpContent.cpp`)
並行作業(装備引き継ぎ)とのマージ衝突を避けるため、書き換えるのは以下のみ:
- 「操作」カテゴリ(`BuildControls`): 「ゲームパッド(準備フェーズ)」「パッド未接続時のキーボード」節を削除し、
  「キーボード」節(F5 セーブ、F1/H/Esc/矢印はヘルプ節)に再構成。ヘルプ節からパッド(LT・十字キー・B)を削除。
- 「基本の流れ」の「戦闘開始」ボタン説明の `(パッドはB)` を削除(アイテムカテゴリから離れた行のため衝突リスクは低い)。
- **アイテムカテゴリの「パッド: アイテム欄でA -> …」行は今回は触らない**(並行作業の編集箇所の2行上で衝突しやすいため)。
  マージ後のフォローアップで削除する。

## 4. `HelpUIRenderer`
- `g_pad[0]->IsTrigger(enButtonLB2 / enButtonB / enButtonUp/Down/Left/Right)` の OR 条件を削除。キーボード直接読みはそのまま。
- コメント(.h / .cpp)からパッドの記述を削除。

## 5. リロールコストの定数統一
- `Game.cpp` の `const int kRerollCost = 2;`(Update のリロール処理、Render の ShopUIRenderer::Draw 引数)2か所を削除し、
  `ShopSystem::kRerollCost` を参照する。`ShopSystem.h` の「後日寄せる」コメントを更新。

## 6. 触らないもの
- `Game/Player.h`, `Game/ItemSystem.h`(並行作業の対象)。
- `Game.cpp` の右クリック売却確定行(`SellUnitFromBench` / `ReturnUnitToBench` 呼び出し)。LB1 ブロック内の売却呼び出しは
  ブロックごと消える。
- k2Engine / k2EngineLow / ExEngine。

## 7. 懸念点
- 物理パッドを繋いでいても何も反応しなくなる(仕様どおり)。エンジンの振動・入力更新処理自体は残る。
- 一覧の「選択中」表示が無くなるため、ツールチップはマウスホバー(0.3秒)でのみ出る。
- 既存プレイヤーが慣れていたキーボード代替(J/K/L/I/Tab/矢印)は全て無反応になる。

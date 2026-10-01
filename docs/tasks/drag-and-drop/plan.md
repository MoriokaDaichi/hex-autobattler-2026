# ドラッグ&ドロップ操作 — plan

要件: `docs/tasks/drag-and-drop/intent.md`。準備フェーズのマウス操作に、既存のクリック操作に**追加する形で**
ドラッグ&ドロップを足す。ゲームパッド操作・戦闘中の挙動は変更しない。

## 0. 調査結果(既存の仕組み)

- `UIHotRegion`(`Game/UIHotRegion.h`) … UI_SPACE(1920x1080、中央原点・y上向き)の絶対矩形 + `kind`/`index`/`hex`。
  毎フレーム `Game::Update()` 先頭で各UIRendererの `BuildHotRegions()` と盤面ヘックス(自陣r0-2、
  `BoardUnit`/`BoardEmptyHex`)から `m_hotRegions` を作り直す。
- `UIInteractionSystem` … `m_hotRegions` を後ろから当てて、ホバー/左右クリックを解決する。**クリックは
  押した瞬間(`g_mouse->IsTrigger`)に発火**する。
- 既存のクリック操作(`Game.cpp` の「--- マウス左クリック ---」ブロック):
  - 何も持っていない状態で `BenchUnit` を押す → `m_mouseHeldBenchIndex` に掴む。
  - 何も持っていない状態で `BoardUnit` を押す → `m_heldBoardHex`(+`m_heldBoardHexFromMouse`)に掴む。
  - `UnclaimedItem` を押す → `m_heldUnclaimedIndex` に持つ。
  - その後の行き先クリックで `Player::PlaceUnitOnBoard` / `MoveUnitOnBoard` / `ItemSystem::GiveItem` を呼ぶ。
  - 右クリック: 何か持っていればキャンセル、持っていなければ売却/ベンチ戻しの2段階確認。
- `Player`(`Game/Player.h`) … `PlaceUnitOnBoard`(上限・自陣・空きマスを検査)、`MoveUnitOnBoard`(空きマスのみ)、
  `ReturnUnitToBench`、`SellUnitFromBench`/`SellUnitFromBoard`、`TryMergeUnits`。**入れ替え(swap)処理は無い**。
- ヘックスカーソル(`CursorSelectionSystem::TryMouseToHex`)はゲームパッド/キーボード側のフォーカス用で、
  マウスのクリック判定自体は上記ヒット矩形で行っている。ドラッグもヒット矩形に揃える。

## 1. 方針: 「押下=既存クリック」「ドラッグ=押下で掴んだものを離した場所へ」

クリックが押下で発火する既存仕様を変えずにドラッグを足すため、次のように重ねる。

1. 左ボタン押下フレーム: 既存クリック処理がそのまま走り、`BenchUnit`/`BoardUnit`/`UnclaimedItem` を「掴む」。
   同時に `DragDropController` が、**押下時点で何も持っていなかった場合に限り**、押下位置の領域を
   ドラッグ元候補として記録する(既に何か持っている状態の押下は既存クリック操作の続きなのでドラッグ化しない)。
2. 押したまま押下位置から `kDragThresholdPx`(12px, UI_SPACE)以上動いたらドラッグ開始。
3. 閾値未満で離した場合 → 何もしない(=押下で発火済みの従来のクリック扱い。掴んだ状態もそのまま残る)。
4. ドラッグ中に離した場合 → 離した位置でドロップ先を解決して処理を確定し、`Game` が既存の「掴み」状態
   (`m_mouseHeldBenchIndex`/`m_heldBoardHex*`/`m_heldUnclaimedIndex`/`m_hasPendingSellTarget`)を全て解除する。
   無効な場所なら処理なしで解除のみ(=元に戻る)。
5. ドラッグ中の右クリック → ドラッグをキャンセル(既存の右クリック処理も「持ち物を離す」として働くので整合する)。

この方式なら既存クリック/右クリック/パッド処理には手を入れずに済む。

## 2. ドロップ先の解決と処理

ドロップ判定は「離した位置」で行う。`m_hotRegions` を後ろから当て(`UIInteractionSystem` と同じ優先順)、
該当しなければ以下の「領域」矩形も見る。

- ベンチ領域: `BoardUIRenderer::kBenchX`/`kBenchTopY`/`kBenchPanelBottomY`(public定数)から算出。
- 売却領域(ショップバー): 5枚のカード全体を覆う矩形。`ShopUIRenderer.cpp` の無名名前空間定数
  (`kSlotStartX`/`kSlotStepX`/`kNameY`/`kDetailY`)と同じ値を `DragDropController.cpp` に再掲する
  (並行作業中のヘルプUIとの衝突を避けるため ShopUIRenderer は編集しない)。

| ドラッグ元 | ドロップ先 | 処理 |
|---|---|---|
| ベンチのユニット | 盤面の空きマス | `Player::PlaceUnitOnBoard`(上限等の拒否は既存クリックと同じ文言でフィードバック) |
| ベンチのユニット | 盤面のユニットがいるマス | 新規 `Player::SwapBenchWithBoard`: ベンチのユニットをそのマスへ、盤面のユニットを同じベンチ位置へ(盤面数不変なので上限チェック不要) |
| ベンチのユニット | ベンチ領域 | 何もしない(元に戻る) |
| 盤面のユニット | 盤面の空きマス | `Player::MoveUnitOnBoard` |
| 盤面のユニット | 盤面の別ユニットのマス | 新規 `Player::SwapBoardUnits`(位置・homePositionを交換) |
| 盤面のユニット | 同じマス | 何もしない |
| 盤面のユニット | ベンチ領域/ベンチの行 | `Player::ReturnUnitToBench`(ゴールド増減なし) |
| ベンチ/盤面のユニット | 売却領域(ショップバー) | `Player::SellUnitFromBench` / `SellUnitFromBoard`(既存売却と同じ。装備アイテムは既存同様に失われる) |
| 未装備アイテム | ベンチ/盤面のユニット | `ItemSystem::GiveItem`(合成・上限は既存ルール)→成功時 `unclaimedItems` から削除 |
| 上記以外 | どこでも | 何もしない(元に戻る) |

- 入れ替え・配置・移動・戻しの後は `while (TryMergeUnits()) {}` で合成を走らせる
  (`PlaceUnitOnBoard`/`ReturnUnitToBench` は内部で呼ぶ。新規swap系も同様に呼ぶ)。
- ドロップ時に、ドラッグ元がまだ同じ実体か(ベンチ: indexの`def`/`starLevel`、盤面: そのマスの`def`、
  アイテム: index位置の`ItemDef*`)を検証する。ドラッグ中にパッド操作等で一覧が変わっていたら中止。
- フィードバックは既存同様 `ShopUIRenderer::PushFeedback`、ログは `OutputDebugString`(`[Drag] ...`)。

## 3. 表示

`DragDropController` 自身を `IRenderer`(`OnRender2D`)にし、`Game::Render()` の準備フェーズ分岐の最後
(ツールチップより前)で `Draw()` を呼ぶ。描画はドラッグ中のみ。

- ドロップ候補のハイライト: 候補矩形を半透明の水色で塗り、現在カーソル下の有効ドロップ先は金色の枠+塗り。
  - ユニット: 自陣の全マス(元マスを除く)/ (盤面ユニットのみ)ベンチ領域 / 売却領域(赤系、「売却 +NG」表示)。
  - アイテム: ベンチ・盤面のユニット。
- ゴースト: カーソル右下に小さなパネルでユニット名(★は既存同様 " *2" 表記)/アイテム名を追従表示。
- ドラッグ中はツールチップを出さない(`Game::Update()` 末尾で `m_tooltipVisible=false` にする1行)。

## 4. ファイル変更

- 新規 `Game/DragDropController.h/.cpp` … ドラッグの状態機械・ドロップ先解決・処理実行・描画。
- `Game/Player.h` … `SwapBenchWithBoard(int benchIndex, const HexCoord&)`、`SwapBoardUnits(const HexCoord&, const HexCoord&)` を追加。
- `Game/Game.h` … include と `DragDropController m_dragDrop;` メンバーのみ。
- `Game/Game.cpp`(最小限):
  1. 準備フェーズ分岐の「--- マウス左クリック ---」直前で `m_dragDrop.Update(...)` を呼び、ドラッグ終了フレームなら掴み状態を解除。
  2. `NextPhase` 遷移時に `m_dragDrop.Cancel()`。
  3. ツールチップ抑止1行。
  4. `Render()` の準備フェーズ分岐で `m_dragDrop.Draw(...)`。
- `Game/Game.vcxproj` / `.filters` … 新規ファイル登録。

## 5. 制約・懸念

- 盤面のドロップ判定は既存ヒット矩形(ヘックスを近似した矩形)を使うため、マスの角付近では判定が無い
  (既存クリックと同じ挙動)。
- ベンチの表示上限(8行)を超えたユニットはヒット領域が無いのでドラッグ元にできない(既存クリックと同じ)。
- 押下で既存クリックが「掴んだ: 〜」のフィードバックを出した直後にドラッグになるため、その文言が一瞬出る。
  ドラッグ開始時に「ドラッグ中: 〜」で上書きする。
- 既に何か持っている状態(クリック操作の途中)の押下からはドラッグを開始しない。

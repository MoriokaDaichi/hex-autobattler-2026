# ui-mouse-cards — 要件定義 (intent.md)

## 背景

- TFT本家はマウス操作だけで完全にプレイできる。本プロジェクト(HEX ARENA)も同様にしたい。
- 現状の操作は `Game::Update()` 内で `g_pad[0]->IsTrigger(enButton*)` を直接見るゲームパッド駆動。
  マウスは `CursorSelectionSystem` がヘックスのピッキングとフォーカス巡回(Tab: Shop→Bench→Items→Board)に
  使われているだけで、購入・リロール・XP購入・ロック・フェーズ進行・アイテム装備・売却などの
  「操作の確定」は全てゲームパッドのボタン。マウスだけではゲームを完走できない。
- UIは全て `Font::Draw()` ベース。`docs/tasks/ui-sprite-bars/` で単色スプライトの塗り矩形ヘルパー
  `UIRectRenderer`(`Game/UIRectRenderer.h/.cpp`、`Sprite`プール方式、`Game::Render()`冒頭で`BeginFrame()`)が
  main に入り、HPバー/XPバー/スキルゲージ/Result暗幕が矩形化された。枠・背景パネル・カード化は未着手。
- 参考: [`docs/research/tft-vs-hexarena-quality-comparison.md`](../../research/tft-vs-hexarena-quality-comparison.md)
  §1(単色矩形→カード枠・パネル背景)、§2(ショップのカード形式)、§12(ユニット/トレイト/アイテムの
  ホバー詳細表示、HUDの居場所の再設計)。

## 現状(調査の起点)

- 入力: `Game::Update()` の各フェーズ分岐が `g_pad[0]->IsTrigger(enButtonA/B/X/Y/LB1/RB1/Start)` を直接参照
  (`Game.cpp` L120〜L731 付近に多数)。マウスボタン・ホイールはほぼ未使用。
- `CursorSelectionSystem`(`Game/CursorSelectionSystem.h/.cpp`): マウス座標→ヘックス変換
  (`HexGridRenderer::TryWorldPositionToHex`)、フォーカス領域(Shop/Bench/Items/Board)の巡回、
  一覧内カーソル移動。「どのUI要素の上にマウスがあるか」を矩形単位で解決する仕組みは無い。
- UI Renderer(いずれも `IRenderer`、`OnRender2D` で `Font::Draw` + `UIRectRenderer::DrawRect`):
  `ShopUIRenderer` / `BoardUIRenderer` / `PlayerStatusUIRenderer` / `RoundRecordUIRenderer` /
  `ItemInventoryUIRenderer` / `TraitPanelUIRenderer` / `TitleUIRenderer` / `ResultUIRenderer`。
  各 Renderer は自分の描画レイアウト(座標・サイズ)を内部に持つが、外部にヒット領域として公開していない。
- `Font` に文字列幅計測(MeasureString相当)は無い。中央/右揃えは手動X調整。スプライトフォントは
  等幅なので「文字数 × 係数」で概算幅は出せる。
- `UnitDef` にはスキル説明・射程・攻撃速度・魔力・防御など全ステータスがあるが画面に出ていない。
  `TraitDef` は段階閾値と効果、`ItemDef` はレシピと `StatEffect`/パッシブを持つ。

## 目的

マウスだけで1ゲームを完走できるようにし、UI要素にマウスオーバーすると
「その項目の詳しい説明 + クリックで何が起きるか」をカード状のツールチップで表示する。
あわせて既存分を含む全UIレンダラーを、単色スプライトによる背景パネル + 枠のカード調に統一する。
ゲームパッド操作は併用可能なまま残す(置き換えではなく追加)。

## スコープ(In)

段階納品。フェーズ1→2→3 の順に実装・コミットし、都度レビュー。

### フェーズ1: マウス入力基盤(完全パリティ)

- `Game::Update()` のゲームパッド分岐に対応するマウス操作を追加する。ゲームパッド分岐は残す。
- 画面上に必要なクリック可能ボタン(リロール / XP購入 / ショップロック / 次フェーズへ進む /
  「準備完了」等)を `UIRectRenderer` + `Font` で常設し、クリックで発火。
- 割り当て指針(設計フェーズで最終確定):
  - 左クリック = 主操作(要素の選択・確定、ショップ枠のユニット購入、ベンチ/盤面ユニットの選択、
    アイテムを「手に持つ」、ボタン押下)。
  - 盤面配置 = ベンチのユニットを左クリックで掴む→盤面ヘックスを左クリックで配置(2ステップ)。
    可能ならドラッグ(押下でピック、ドロップで配置)も併用。盤面内再配置・ベンチへ戻すも同様に。
  - 右クリック = キャンセル(掴んでいるユニット/アイテムを離す)、および盤面/ベンチユニットの売却など
    「破壊的操作は確認を挟む」よう設計フェーズで整理(誤クリック対策)。
  - ホイール = 一覧のスクロール(必要な箇所のみ)。
- 受け入れ: マウスだけで「ユニット購入→盤面配置→リロール→XP購入→ショップロック→戦闘へ進む→
  アイテム装備→ユニット売却→次ラウンド→…→GameOver または Victory」まで到達できる。

### フェーズ2: ヒットテスト + ツールチップ

- 各 UI Renderer が「クリック/ホバー可能な矩形リージョン」の一覧を公開する共通の仕組みを作る
  (例: `struct UIHotRegion { RectUI bounds; UIRegionKind kind; int index; }` を Renderer が毎フレーム構築し、
  `Game` or 新 `UITooltipSystem` が現フレームのマウス座標から hovered/clicked を解決)。
  既存の `CursorSelectionSystem` のフォーカス概念と整合させる(設計フェーズで統合方針を決める)。
- 全インタラクティブ要素にツールチップ。少なくとも:
  - ショップの5枠ユニット: 名前・コスト・HP/AT/AP/物防/魔防・攻撃速度・射程・スキル説明・トレイト、
    「クリックで購入(-{cost}G)」。ゴールド不足時はその旨。
  - 盤面/ベンチのユニット: 上記フルステータス + 現在の星・適用中ボーナス、
    「クリックで選択 / 右クリックで売却(+{sell}G)」。
  - アイテム(未装備一覧): 効果(`StatEffect`/パッシブ)の文章化、素材なら「この素材でできる完成品」、
    「クリックで手に持つ→ユニットをクリックで装備」。
  - トレイト(トレイトパネルの各行): 段階閾値・各段階の効果・そのトレイトを持つユニット一覧・現在数、
    発動中かどうか。
  - HUD の各ボタン(リロール/XP購入/ロック/次へ)と、ゴールド/レベル/盤面使用数/ラウンド/連勝連敗:
    数値の意味と「クリックで〜」(ボタンのみ)。
- 表示: カーソル近傍にカード(`UIRectRenderer` の背景+枠 + `Font`)。画面端でクランプ。
  ホバー継続 ~0.2〜0.4秒で表示。ゲームパッドでフォーカス中の要素にも同じツールチップを出してよい。

### フェーズ3: 全UIレンダラーのカード化

- `UIRectRenderer` を使い、以下すべてに背景パネル + 枠(2枚重ね or インセット矩形)を付ける。
  既存の情報配置・文言は原則維持し、下地を敷く。
  - `ShopUIRenderer`: 5枠を個別カード(コスト帯色を枠に反映、選択枠/ホバー枠をハイライト)。
  - `BoardUIRenderer`: ベンチ一覧を縦並びカード、戦闘中の頭上情報も小カード枠に。
  - `PlayerStatusUIRenderer` / `RoundRecordUIRenderer`: 右上HUDを1〜2枚のカードパネルに集約。
  - `ItemInventoryUIRenderer`: アイテム一覧をカードリストに。
  - `TraitPanelUIRenderer`: パネル背景 + 行ごとの区切り、発動中/未発動で色分け。
  - `TitleUIRenderer` / `ResultUIRenderer`: タイトル文字・結果文言をカードパネル上に。
- HUDレイアウト全体を見直し、トレイトパネル(左)とベンチ一覧(左)の衝突など既存の座標手押し回避を解消
  (research §12)。ツールチップカードと HUD が重なって読めなくならないこと。
- 色は `white.dds` + `SetMulColor`。必要なら `spriteData/color` の他色PNGもDDS化して使ってよい
  (使ったものはコミットに含める)。

## スコープ(Out)

- エンジン `Font` への `MeasureString` 追加(真の中央/右揃え) → 今回もやらない。等幅前提の「文字数×係数」
  概算でカード幅・配置を決める。既存テキストの配置ロジックは大きく変えない。
- ユニットポートレート/トレイトアイコン/アイテムアイコンなどの画像素材の導入 → 対象外(枠と色帯まで)。
- 3D戦闘の位置アニメ・エフェクト → 別タスク。
- UI操作音/SE → 対象外。
- ドラッグ中のユニット3Dモデルの追従表示 → 任意(できれば可、必須でない)。
- ネットワーク/リプレイ等 → 無関係。

## 受け入れ条件

- マウスのみ(キーボード・ゲームパッドに一切触れず)で、タイトル→準備→戦闘→…→GameOver/Victory まで
  1ゲーム完走できる。上記フェーズ1の操作列がすべてマウスで通る。
- 既存のゲームパッド操作・キーボード(F5等)が従来どおり動く(回帰なし)。
- 各インタラクティブUI要素にマウスオーバーすると、内容の正しいツールチップ(説明 + クリック時の動作)が
  カードで表示され、画面外にはみ出さない。
- 全フェーズ・全UIレンダラーが背景パネル+枠のカード調で表示され、情報が重なって読めなくなっていない。
- `Debug|x64` ビルドが 0 エラー。新規コード起因の警告なし。
- F5 実機で、各フェーズのスクリーンショットで見た目が破綻していない(デバッグ検証で確認)。

## 制約・注意

- 独自Rendererは既存の `IRenderer` パイプライン(`OnRender2D` 等)に乗せる。`Game::Render()` から
  直接ドローコールを撃たない。
- `UIRectRenderer` はプール方式。1フレームの矩形総数が増える(カード背景+枠 × 全要素 + ツールチップ)。
  `Game::Render()` 冒頭の `BeginFrame()` は維持。`kPrewarmCount`(現160)を必要数に見直す。
- `Sprite` の矩形描画は `Font::Begin()`〜`End()` の外側(前)でまとめて行う(SpriteBatch状態と競合するため)。
- `g_camera2D` はグローバルで全画面合成スプライトと共有。書き換えない(ui-sprite-bars で左右反転の回帰実績)。
- ソースは UTF-8 (BOM付き)。`docs/tasks/ui-mouse-cards/` の Markdown は BOM無しでよい。
- `Game/Assets/modelData/*.dds` は LFS 由来の見かけ上 modified 表示が出るが実害なし。commit も discard もしない。
  main へのマージ時に abort することがある(対処法は Git 作業用が把握済み)。
- 作業は **worktree `feature/ui-mouse-cards`** で。フェーズ1/2/3 を別コミットで積む。完了後 Git 作業用が
  main へマージ。
- フェーズ引き継ぎは `docs/tasks/ui-mouse-cards/` の Markdown で自己完結(intent.md → plan.md → 実装 → レビュー)。

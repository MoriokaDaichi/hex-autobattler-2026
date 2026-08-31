# ui-sprite-bars — 要件定義 (intent.md)

## 背景

- 本プロジェクト(HEX ARENA)の2D UIは、これまでエンジンの `Font::Draw()` だけで描かれてきた。
  矩形塗り・アルファブレンド・整列(中央/右揃え)ができないという制約があり、
  HPバー/XPバーは `[####------]` のASCII表現、`ResultUIRenderer` の暗幕は `#` を敷き詰めた不透明ブロックで代用している。
- 外部調査ドキュメント [`docs/research/tft-vs-hexarena-quality-comparison.md`](../../research/tft-vs-hexarena-quality-comparison.md) の
  **§1「UI描画エンジンの制約」** と **まとめ表** で、
  「**単色矩形が1枚描けるだけで表現力が跳ね上がる(HPバー・カード枠・暗幕・トレイトパネル背景)。
  『1x1白テクスチャのSprite』を1枚追加してスケール描画するだけでも、罫線グリフ問題・アルファ問題・整列問題の多くが緩和される。
  UI全体の底上げになる基盤investmentなので最優先候補**」と指摘されている。
- ユーザーが `Game/Assets/spriteData/color/` に単色PNGを9色追加した(未コミット):
  `black.png / blue.png / Brown.png / gray.png / green.png / purple.png / red.png / white.png / yellow.png`
  (各 数十KB、全体で約76KB)。この素材を使って上記の土台を入れる。

## 現状(調査結果)

- UI Renderer 一覧(すべて `IRenderer` 実装、`Font::Draw()` ベース):
  `BoardUIRenderer` / `ShopUIRenderer` / `PlayerStatusUIRenderer` / `RoundRecordUIRenderer` /
  `ItemInventoryUIRenderer` / `TitleUIRenderer` / `ResultUIRenderer` / `TraitPanelUIRenderer`
- `Game/` 配下に Sprite / SpriteRender を使っている箇所は現状ゼロ(`grep` で確認済み)。
  エンジン(k2Engine / k2EngineLow)の2D Sprite API を使うのは今回が初。
- ASCII バーの具体箇所:
  - `BoardUIRenderer.cpp:11-51` … 戦闘中の頭上HPバー `MakeBar()` (`kBarCells=10`, `[####------]`), `HPColor()` で緑/黄/赤。
  - `PlayerStatusUIRenderer.cpp:21` 付近 … 画面右上の `LV n [####------] xp/next` XPバー。
- 暗幕の代用: `ResultUIRenderer.cpp:56` 付近 … `#` を敷き詰めた不透明ブロック。
- スキル(必殺技)ゲージは内部的には `normalAttackCount + receivedAttackCount` vs `skillThreshold`
  (`CombatEngine.h:177` 付近)。**現状UIに一切出ていない**。

## 目的

エンジンの2D Sprite機能と新規の単色PNG素材を使って、
**「単色矩形/バーを描く共通ヘルパー」を1つ用意し、既存のASCIIバー・#暗幕を本物の塗り矩形に置き換える**。
既存のHUDレイアウト(座標・情報の居場所)は変えない。土台の導入と、その最初の適用までを本タスクのスコープとする。

## スコープ

### やること(In)

1. **UI描画プリミティブの共通ヘルパー新設**
   - Sprite(単色PNG)をスケール描画して「指定UI座標・サイズ・色・アルファの塗り矩形」を1枚描ける関数/クラス。
   - 既存 Renderer が `OnRender2D` / `OnForwardRender` 等の中から呼べる形。
   - 色は素材9色 + 頂点カラー乗算で任意色を作れるなら白1枚で足りる。設計フェーズで確定。
   - アルファブレンドの可否をエンジン側で確認し、半透明が出せるようにする。
2. **HPバー(戦闘中・頭上)を塗り矩形化**
   - `BoardUIRenderer` の `[####------]` を、背景(暗)＋前景(HP割合で緑/黄/赤、既存 `HPColor` 相当)＋必要なら枠 の矩形バーに。
   - シールド量の表現(現状 `(+shield)` テキスト)も可能なら重ねバーに。難しければテキスト併記のまま可。
   - 敵( `isEnemy` )の赤系も踏襲。
3. **XPバーを塗り矩形化**
   - `PlayerStatusUIRenderer` の `LV n [####------] xp/next` を塗り矩形バーに。`(MAX)` 表示は踏襲。
4. **スキルゲージバーの新規表示**
   - 戦闘中、各ユニットのHPバーの下にもう1本、必殺技ゲージ( `gauge / skillThreshold` )の塗り矩形バーを出す。
   - `CombatPlayback` / `CombatEvent` にゲージ情報が乗っていない場合の対応方針は設計フェーズで決める
     (イベント種別追加が必要なら、その要否と最小案を plan.md に書く)。
     ゲージ再生が重い場合は「戦闘開始時点の閾値までの目安表示」など縮小案も可、ただし縮小するなら理由を明記。
5. **ResultUIRenderer の暗幕を半透明矩形化**
   - `#` ブロックを、画面全体を覆う半透明(黒 alpha 0.5前後)の塗り矩形1枚に置換。前面のテキストはそのまま。

### やらないこと(Out)

- エンジン `Font` への文字列幅計測(MeasureString相当)の追加 → **別タスク**。中央/右揃えは今回やらない。
- カード枠・トレイトパネル背景・アイテム欄の枠/背景パネル化 → 今回は土台とバー適用まで。次タスク。
- HUDレイアウト全体の再設計(調査doc §12) → 別タスク。
- 3D戦闘の位置アニメ・エフェクト(調査doc §8) → 別タスク。
- 利子ドット/敵編成プレビュー等のテキスト系追加 → 対象外。
- アイコン素材(コスト枠・トレイトアイコン・ポートレート)の導入 → 対象外。

## 受け入れ条件

- `Debug|x64` でビルドが通る。
- ゲームを実行(F5)して:
  - 戦闘フェーズで、各ユニット頭上のHPバーが塗り矩形になっており、割合・色(緑/黄/赤)・敵味方の差が視認できる。
  - HPバーの下にスキルゲージバーが出て、必殺技発動が近いユニットでゲージが伸びているのが分かる
    (縮小案を採った場合は plan.md 記載の挙動どおり)。
  - 準備/全フェーズで、画面右上のXPバーが塗り矩形になっている。
  - リザルト画面の暗幕が半透明の黒面になっており、背後の盤面がうっすら見える(アルファが効かないと判明した場合は
    plan.md にその旨と代替(暗いグレー不透明面など)を明記した上で不透明でも可)。
- 既存のHUDの他要素(GOLD/LV/ROUND/連敗/ベンチ一覧/ショップ/アイテム欄/トレイトパネル)の位置・表示が
  従来どおりで、新しいバーと重なって読めなくなっていない。
- `Game/Assets/spriteData/color/` の使用したPNGがコミットに含まれる(未使用の色はコミットしてよいが、
  どれを使ったかは plan.md / コミットメッセージに記す)。
- 追加した共通ヘルパーは、次タスク(枠・パネル化)でそのまま流用できる粒度になっている。

## 制約・注意

- 独自Rendererから `Game::Render()` 内で直接ドローコールを撃つと、後段の `RenderingEngine::Execute()` に
  上書きされる。既存の `IRenderer` パイプライン(`OnForwardRender` / `OnTlanslucentRender` / 2D描画フック)に
  乗せる形を守る(CLAUDE.md 記載)。
- スプライトフォントに未収録グリフを描くと `SpriteFont` が例外でアプリごと落ちる。矩形化で ASCII 記号を
  やめる方向なのでむしろ安全になるが、フォールバックでASCIIに戻す実装を残す場合は要注意。
- 作業は **worktree `feature/ui-sprite-bars`** で行う(main では item-passive 等の他タスクが並走中)。
  完了後に Git 作業用セッションが main へマージする。
- フェーズ引き継ぎは本 `docs/tasks/ui-sprite-bars/` 配下の Markdown で自己完結させる(intent.md → plan.md → 実装 → レビュー)。

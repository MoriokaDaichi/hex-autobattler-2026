# font-rendering-bugfixes — 要件定義 (intent.md)

## 背景

Notionの「9月制作_森岡_HEX ARENA」データベースに、優先度「低」として起票されたまま未着手だった
FontEngine関連の不具合調査2件をまとめて対応する。どちらも過去のデバッグ中に見つかった副産物的な
起票で、必須対応ではないが根本原因は判明している。

### 1. Font描画でalphaブレンドが機能していない疑い

- `TitleUIRenderer`の点滅実装で、alpha値を変えても描画結果に変化が無いことがピクセル輝度比較で
  確認済み(2026-08-30、「タイトル画面の実装」デバッグ中に発見)。
- `ShopUIRenderer`の「古いフィードバックを薄く残す」表現(`color.w=0.4f`)も同じ仕組みを使っている
  ため、実際には薄くなっていない可能性がある。

**根本原因(コード調査で特定済み)**: [`k2EngineLow/graphics/font/FontEngine.cpp`](../../../k2EngineLow/graphics/font/FontEngine.cpp)
の`Init()`で`SpriteBatchPipelineStateDescription`をデフォルト(blend引数省略)で構築している。
DirectXTKの`SpriteBatchPipelineStateDescription::s_DefaultBlendDesc`
([`k2EngineLow/DirectXTK/Src/SpriteBatch.cpp`](../../../k2EngineLow/DirectXTK/Src/SpriteBatch.cpp)
222行目)は**プリマルチプライドアルファ**用のブレンド設定(`SrcBlend = D3D12_BLEND_ONE`)になっている。
一方、`Font::Draw`(呼び出し元含む)が渡す`color`はストレートアルファ(RGBはフル値、`w`だけを
透明度として使う)前提で書かれている。`SrcBlend=ONE`だとソースRGBがアルファに関係なく常にフル強度で
加算されるため、`w`(アルファ)をいくら下げても文字の見た目の強さがほぼ変わらない。

## 2. FontEngineの非ASCII記号で未収録グリフクラッシュの可能性

- FontEngineのフィードバック文字列に非ASCII記号(例: 全角矢印「→」)を入れると、未収録グリフで
  クラッシュしうる。「盤面内再配置」実装中に実際に発生し、`->`表記に回避して対処済みだが、根本原因は
  未対応のまま。

**根本原因(コード調査で特定済み)**: [`k2EngineLow/DirectXTK/Src/SpriteFont.cpp`](../../../k2EngineLow/DirectXTK/Src/SpriteFont.cpp)
196-197行目、`SpriteFont::Impl::FindGlyph()`がフォントに収録されていない文字を検出すると
`throw std::exception("Character not in font")`する。`SetDefaultCharacter()`
(200-210行目)で代替グリフを設定しておけば、未収録文字はその代替グリフで描画されクラッシュしなくなる。
[`k2EngineLow/graphics/font/FontEngine.cpp`](../../../k2EngineLow/graphics/font/FontEngine.cpp)の
`Init()`では`SetDefaultCharacter()`が一度も呼ばれていない。

## 目的

上記2件を同時に修正する。どちらもFontEngine初期化まわりの小さな変更で完結し、関連が深いため
まとめて1タスクとして扱う(ユーザー指示)。

## スコープ(In)

1. **アルファブレンド修正**: `FontEngine::Init()`で`SpriteBatchPipelineStateDescription`に、
   ストレートアルファ用のブレンド記述(`SrcBlend`/`SrcBlendAlpha` を `D3D12_BLEND_SRC_ALPHA`
   に変更し、それ以外は`s_DefaultBlendDesc`を踏襲)を明示的に渡すよう変更する。
2. **デフォルトグリフ設定**: `FontEngine::Init()`で`m_spriteFont`生成後に
   `m_spriteFont->SetDefaultCharacter(...)`を呼び、未収録文字での例外送出を防ぐ。代替文字は
   スプライトフォントに確実に含まれるASCII文字(`?`等)を使う。実機確認で未収録文字(例: 全角矢印)を
   実際に描画させても例外が発生しないことを確認する。
3. 上記修正後、`TitleUIRenderer`の点滅・`ShopUIRenderer`のフィードバック薄表示が実際に薄く
   見えることを実機で確認する(既存の点滅ロジック自体は変更しない。表示ロジック側のバグではなく
   描画パイプライン側のバグのため)。

## スコープ(Out)

- FontEngine/Font/SpriteFont以外のリファクタリング。
- 新しいフォントアセットの追加・差し替え。
- 「ASCII縛りをコーディング規約化する」対応(Notion起票の代替案の一つだが、根本修正
  (デフォルトグリフ設定)で置き換えるため今回は不要)。
- 既存の各UIRendererの点滅/フェード演出ロジックそのものの変更(色の透明度計算は現状維持し、
  ブレンド側だけを直す)。

## 受け入れ条件

- `Debug|x64`ビルドが0エラー。
- 実機確認で、alpha値を下げた描画(点滅のフェード、フィードバックの薄表示)が実際に視覚的に
  薄く見えることを確認する。
- 実機確認で、非ASCII文字(全角矢印など)を含む文字列をFontEngineへ描画させてもクラッシュしない
  ことを確認する。
- 既存の文字描画(タイトル、ショップ、盤面UI等)に見た目の regression が無いこと
  (アルファ1.0の完全不透明描画は従来と同じに見える必要がある)。

## 制約・注意

- `k2EngineLow`はエンジン層。`Game/`のみを触る通常のタスクと異なり、エンジン共通コードの変更のため、
  影響範囲(全UIRenderer)を意識して実機確認を行うこと。
- ソースはUTF-8(BOM付き)。
- 作業は**worktree不要**(小規模なエンジン層修正のため、mainブランチ上で直接進めてよい。
  他タスクとのコンフリクトリスクが低いとマネージャー判断済み)。

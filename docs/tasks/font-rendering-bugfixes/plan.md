# font-rendering-bugfixes — 設計 (plan.md)

intent.mdで根本原因は特定済み。実装内容はいずれも
[`k2EngineLow/graphics/font/FontEngine.cpp`](../../../k2EngineLow/graphics/font/FontEngine.cpp)
の`FontEngine::Init()`内の変更のみで完結する。

## 1. アルファブレンド修正

### 現状

```cpp
SpriteBatchPipelineStateDescription sprBatchDesc(renderTargetState);
```

`blend`引数を省略しているため、`SpriteBatchPipelineStateDescription`のデフォルト引数
(`nullptr`)経由で`s_DefaultBlendDesc`(プリマルチプライドアルファ、`SrcBlend=D3D12_BLEND_ONE`)
が使われる。

### 変更後

`Init()`冒頭(ローカル変数`renderTargetState`定義の後、`SpriteBatchPipelineStateDescription`
構築の前)に、ストレートアルファ用の`D3D12_BLEND_DESC`をローカルで定義し、それを渡す。

```cpp
// SpriteBatchのデフォルトブレンド設定(s_DefaultBlendDesc)はプリマルチプライドアルファ
// (SrcBlend=ONE)前提だが、本エンジンのFont::Draw()呼び出し元はストレートアルファ
// (RGBはフル値、colorのwだけを透明度として使う)で色を渡している。SrcBlend=ONEのままだと
// アルファを下げてもソースRGBが常にフル強度で加算され、フェード演出が効かない
// (font-rendering-bugfixesタスクで判明)。ストレートアルファ用のブレンド記述に差し替える。
D3D12_BLEND_DESC straightAlphaBlendDesc = {};
straightAlphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
straightAlphaBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
straightAlphaBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
straightAlphaBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
straightAlphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
straightAlphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
straightAlphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
straightAlphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

SpriteBatchPipelineStateDescription sprBatchDesc(renderTargetState, &straightAlphaBlendDesc);
```

`s_DefaultBlendDesc`は`AlphaToCoverageEnable=FALSE`/`IndependentBlendEnable=FALSE`だが、
`D3D12_BLEND_DESC{}`のゼロ初期化で両方とも`FALSE`相当になるため明示不要。

アルファ1.0(不透明)描画時は`SrcBlend=SRC_ALPHA`でも`Src.rgb*1.0 + Dst.rgb*0.0`となり、
従来の`SrcBlend=ONE`時と同じ結果になる(regressionなし)。

## 2. デフォルトグリフ設定

### 現状

```cpp
m_spriteFont = make_unique<SpriteFont>(
    d3dDevice,
    re,
    L"Assets/font/myfile.spritefont",
    cpuHandle,
    gpuHandle);

re.End(g_graphicsEngine->GetCommandQueue());
```

### 変更後

`m_spriteFont`生成直後、`re.End(...)`より前に1行追加する。

```cpp
// 未収録文字(全角記号等)を描画しようとするとSpriteFont::Impl::FindGlyph()が例外を送出して
// クラッシュする(font-rendering-bugfixesタスクで判明)。ASCIIの'?'はmyfile.spritefontに
// 確実に収録されている基本文字のため、代替グリフとして設定しクラッシュを防ぐ。
m_spriteFont->SetDefaultCharacter(L'?');
```

`'?'`が実際に`myfile.spritefont`に収録されていることは、`SetDefaultCharacter()`内部で
`FindGlyph()`を呼び出し、見つからなければ`defaultGlyph=nullptr`のまま(無効化)になるだけで
例外は出ない([`SpriteFont.cpp`](../../../k2EngineLow/DirectXTK/Src/SpriteFont.cpp)202-210行目)
ため、万一収録されていなくても初期化がクラッシュすることはない。実機確認で全角矢印などの
非ASCII文字を実際に描画させ、クラッシュしないこと(='?'が有効なフォールバックとして機能して
いること)を確認する。

## 実機確認の手順

1. `Debug|x64`ビルドが0エラーであることを確認。
2. タイトル画面の点滅("PRESS [A] TO START")が実際にフェードして見えることを確認
   (アルファ修正の確認)。
3. 適当な操作でShopUIRendererのフィードバック薄表示(古いフィードバックがうっすら残る演出)が
   実際に薄く見えることを確認。
4. 一時的に非ASCII文字(例: `"テスト→矢印"`)を含む文字列をどこかの`PushFeedback`呼び出し等に
   差し込んで実機起動し、クラッシュしないことを確認する(確認後は元に戻す。テスト用の変更は
   コミットしない)。
5. 通常のUI(タイトル/準備/戦闘フェーズのテキスト)が従来通り不透明に表示され、regressionが
   無いことを確認する。

## 未解決の懸念点

- なし。両修正ともFontEngine::Init()内で完結する低リスクな変更。

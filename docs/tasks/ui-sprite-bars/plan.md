# ui-sprite-bars — 設計 (plan.md)

このファイルだけを読めば実装フェーズに着手できるよう、調査結果と具体的な仕様をすべて記す。
背景・要求仕様は [`intent.md`](intent.md) を参照。

## 0. 調査結果サマリ(重要な発見)

1. **PNGはそのままロードできない。** `Texture`クラス(`k2EngineLow/graphics/Texture.h/.cpp`)は
   `InitFromDDSFile`/`InitFromMemory`いずれも内部で`DirectX::CreateDDSTextureFrom*`を使っており、
   **DDS専用**。WICベースのPNGロード経路は存在しない。→ **事前にPNG→DDS変換が必要**(§1参照)。
2. **アルファブレンドは`Sprite`側では機能する。** `Sprite::InitPipelineState`
   (`k2EngineLow/graphics/Sprite.cpp:138`)は`AlphaBlendMode_Trans`指定時に
   `SrcBlend=SRC_ALPHA, DestBlend=INV_SRC_ALPHA`の通常の半透明合成ブレンドステートを作る。
   `Font`(SpriteBatch経由)でアルファが効かないのはFont側の設定の問題であり、**Sprite経由なら
   半透明矩形(暗幕)は素直に実現できる**。intent.mdの「アルファ不可だった場合の代替」は不要。
3. **頂点/乗算カラーで任意色が作れる。** 用意された`Game/Assets/shader/sprite.fx`の`PSMain`は
   `colorTexture.Sample(Sampler, uv) * mulColor`。`Sprite::SetMulColor()`は`Draw()`のたびに
   定数バッファへ書き込まれる(`Sprite::Draw`, `Sprite.cpp:254`)ため、**白1枚のテクスチャ +
   `SetMulColor()`で必要な色をすべて作れる**。9色のPNGは(将来のパネル背景等で使うかもしれないが)
   本タスクでは**`white.png`のみ使用**する。他8色は`受け入れ条件`通りコミットしてよいが未使用。
4. **既存の汎用スプライトシェーダーがそのまま使える。** `Game/Assets/shader/sprite.fx`
   (`VSMain`/`PSMain`、`SpriteInitData`のデフォルトエントリポイント名と一致)。新規シェーダー作成は不要。
5. **[訂正] `Sprite`は1インスタンスを1フレームに複数回Drawできない。** 当初「1個を使い回せる」と
   想定したが誤り。`Sprite::Draw()`は毎回`m_constantBufferGPU.CopyToVRAM()`してから`DrawIndexed`を
   コマンドリストに記録するだけで、`ConstantBuffer::CopyToVRAM`は1フレーム不変の単一領域へmemcpyする
   (描画コールごとのリング確保が無い)。よってフレーム内の全`DrawIndexed`が同じCBVを参照し、
   **最後の`Draw`のMVP/乗算カラーで全矩形が描かれる=実質1枚しか出ない**。
   エンジン側も`RenderingEngine::m_mainSprite`/`m_2DSprite`や各ポストエフェクトは描画1回につき専用
   `Sprite`メンバを1個、`CalcSceneLuminance`は`Sprite m_calcAvgSprites[N]`配列で対応している。
   → `UIRectRenderer`は**`Sprite`のプール**(`std::vector<std::unique_ptr<Sprite>>`)を持ち、
   `DrawRect`呼び出しごとに別インスタンスを割り当てる。`Game::Render()`冒頭で`BeginFrame()`を
   呼んで使用カーソルを0に戻し、プールはフレーム跨ぎで再利用する。`Init()`でピーク見込み(128)を
   事前確保し、描画中の生成を避ける。
6. **`Sprite`の`pivot`はFontと違い本物の正規化ピボット。** `Sprite::Update()`
   (`Sprite.cpp:218`)は`pivot`(0〜1)を`m_size`(初期化時の幅高さ)に対する実際の比率として
   平行移動に変換している。Fontの「生ピクセルオフセットもどき」ピボットとは別物で、
   `(0,0.5)`=左中央アンカー等がそのまま機能する。
7. **座標系はUI_SPACEとほぼ同一。** `Sprite::Draw()`(`Sprite.cpp:246`)は
   `g_camera2D`のビュー行列 × `MakeOrthoProjectionMatrix(viewport.Width, viewport.Height, ...)`。
   `g_camera2D`はデフォルト値(position(0,0,1)、target(0,0,0)、無回転)のままなので、
   `Update()`に渡す`pos.xy`は**UI_SPACE(1920×1080、中央原点、y上向き)とそのまま同じ数値系**で扱える
   (Fontの`Vector2(x,y)`と同じ感覚でよい)。
8. **`Font`のSpriteBatchと同時に使う際の注意。** `Font::Begin()`は内部で
   `m_spriteBatch->Begin(commandList, SpriteSortMode_Deferred, ...)`を呼び、`End()`まで
   自身のパイプライン状態を前提にする。`Sprite::Draw()`は独自に`SetRootSignature`/
   `SetPipelineState`/`SetDescriptorHeap`を都度設定するため、**`Font::Begin()`〜`End()`の
   区間の外側**で呼ぶ必要がある。→ 各`OnRender2D`内では「矩形(Sprite)をすべて描き終えてから
   `Font::Begin()`〜文字描画〜`End()`」の順序を必ず守る(矩形が背景、文字が前面という
   意図する見た目の重なり順にも一致する)。

## 1. アセット変換(実装フェーズの最初の作業、実施・検証済み)

[レビュー是正2] 単色画像に1536×864は不要(元のpng_to_dds.py案だと無圧縮5.3MBになりリポジトリを
肥大化させるため却下)。**`texconv.exe`(`toolsProj/k2SLEditorProj/texconv.exe`)で32×32・
ミップ無しにリサイズしながら直接DDS化**する方針に変更した。以下のコマンドで実施・確認済み:

```powershell
Set-Location "Game\Assets\spriteData\color"
& "..\..\..\..\toolsProj\k2SLEditorProj\texconv.exe" -w 32 -h 32 -m 1 -f R8G8B8A8_UNORM -o . -y "white.png"
```

- 出力: `Game/Assets/spriteData/color/white.dds`(32×32、無圧縮R8G8B8A8、**4,224バイト**)。
  単色矩形の拡大描画に32×32で解像度不足になることは無い(`UIRectRenderer::DrawRect`は
  スケールで目的サイズへ引き伸ばすだけ)。
- 注意: 相対パスを`/`区切りで渡すと`-o`の出力ディレクトリと入力パスが二重結合されて失敗する
  (`texconv`はWindowsパス前提)。上記のように出力先ディレクトリへ`cd`してから
  ファイル名のみ・`-o .`で渡すと問題ない(実際に踏んだ失敗と回避策)。
- png_to_dds.py(Blenderスクリプト)は今回未使用(モデルテクスチャ用にY軸反転の扱い等が
  チューニングされているが、単色画像には無関係かつtexconvの方が一手順で完結するため)。
- 検証: 実機で`white.dds`を読み込んだSpriteが真っ白な矩形として描けることを最初に確認してから
  各UI Rendererの改修に進む(段階的検証、後述の実装ログ参照)。

## 2. 追加ファイル

| ファイル | 役割 |
|---|---|
| `Game/UIRectRenderer.h` / `.cpp` | 単色矩形(塗り/半透明)を描く共通ヘルパー。`Sprite`のプール(`vector<unique_ptr<Sprite>>`)を持ち、`DrawRect`呼び出しごとに1個割り当てる(§0-5訂正参照)。`Game`が1個所有し`Game::Render()`冒頭で`BeginFrame()`、利用する各UIRendererへ参照を渡す。 |

`Game/Game.vcxproj`と`.filters`に追加(フィルタ`Core`)。**IRendererは実装しない**(既存Rendererの
`OnRender2D`内から直接呼ばれるヘルパーであり、自身が`AddRenderObject`されるものではない)。

## 3. `UIRectRenderer` のAPI設計

```cpp
// Game/UIRectRenderer.h
#pragma once

/// <summary>
/// 単色の塗り矩形(半透明可)を描く共通ヘルパー。white.dds 1枚 + 頂点/乗算カラーで
/// 任意の色・アルファを作る。ShopUIRenderer等の既存UIRendererと違い自身はIRendererではなく、
/// 各UIRendererのOnRender2D内から直接呼ばれる薄いラッパー。
///
/// 呼び出し側の注意: Font::Begin()〜End()の区間の外側(前)で呼ぶこと
/// (SpriteBatchの状態と競合するため。詳細はplan.md §0-8)。
/// </summary>
class UIRectRenderer : public Noncopyable
{
public:
	/// <summary>1回だけ呼ぶ(Game::Start())。white.ddsを読み込みSprite初期化を行う。</summary>
	void Init();

	/// <summary>
	/// 矩形を1枚描く。座標系はUI_SPACE(1920x1080、中央原点、y上向き)、Fontと共通。
	/// </summary>
	/// <param name="pos">ピボット位置のUI座標。</param>
	/// <param name="size">幅・高さ(ピクセル)。</param>
	/// <param name="color">RGBA。alphaは0〜1(半透明可、AlphaBlendMode_Trans固定)。</param>
	/// <param name="pivot">0〜1の正規化ピボット。省略時(0.5,0.5)=中心。左端基準で右に伸ばす
	/// バー表現には(0.0,0.5)を指定する。</param>
	void DrawRect(RenderContext& rc, const Vector2& pos, const Vector2& size, const Vector4& color,
		const Vector2& pivot = Vector2(0.5f, 0.5f));

private:
	Sprite m_sprite;
};
```

```cpp
// Game/UIRectRenderer.cpp (要点)
void UIRectRenderer::Init()
{
	SpriteInitData initData;
	// パスは"Assets/..."起点("Game/"無し)。HexGridRenderer::InitShaders()の
	// LoadVS("Assets/shader/hexGrid.fx",...)、FontEngine::Init()のspritefontパスと同じ規約
	// (実行時カレントディレクトリがGame/である前提)。実機確認済み(§1)。
	initData.m_ddsFilePath[0] = "Assets/spriteData/color/white.dds";
	initData.m_fxFilePath = "Assets/shader/sprite.fx";
	initData.m_width = 32;  // white.ddsの実サイズ(texconvで32x32へリサイズ済み、§1参照)。
	initData.m_height = 32; // DrawRect側のsize指定はスケールで別途制御するため、ここは原寸を渡すだけでよい。
	initData.m_alphaBlendMode = AlphaBlendMode_Trans; // 半透明(暗幕)・不透明(HP/XPバー)の両方をこれ1本で賄う(alpha=1なら実質不透明)。
	m_sprite.Init(initData);
}

void UIRectRenderer::DrawRect(RenderContext& rc, const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& pivot)
{
	// size(ピクセル) / テクスチャ原寸(32x32) でスケールを求め、白テクスチャを目的の矩形サイズへ引き伸ばす。
	Vector3 scale(size.x / 32.0f, size.y / 32.0f, 1.0f);
	m_sprite.Update(Vector3(pos.x, pos.y, 0.0f), Quaternion::Identity, scale, pivot);
	m_sprite.SetMulColor(color);
	m_sprite.Draw(rc);
}
```

- ファイルパスは`HexGridRenderer::InitShaders()`(`Assets/shader/hexGrid.fx`)と同じ`"Assets/..."`
  起点(`Game/`無し)で確認済み。それでも実装フェーズの最初に`white.dds`単体の読み込みで
  実機確認してから進める(§1参照、環境差異の最終確認として)。
- `Quaternion::Identity`は`math/Vector.h`に静的メンバとして存在することを確認済み。

## 4. `Game.h` / `Game.cpp` の変更

- `#include "UIRectRenderer.h"`、メンバ`UIRectRenderer m_uiRectRenderer;`を追加。
- `Game::Start()`内、他の一度きりの初期化(`m_hexGridRenderer.Init()`等)と並べて
  `m_uiRectRenderer.Init()`を呼ぶ。
- `BoardUIRenderer::DrawCombat/DrawPreparation`・`PlayerStatusUIRenderer::Draw`・
  `ResultUIRenderer::DrawGameOver/DrawVictory`(暗幕を使う経路)の呼び出しに
  `m_uiRectRenderer`への参照を追加で渡す(各Rendererが`UIRectRenderer*`をメンバに
  キャッシュし、後で呼ばれる`OnRender2D`内で使う。`m_bars`等を`Draw*()`時点でキャッシュする
  既存パターンと同じ)。

## 5. HPバーの塗り矩形化(`BoardUIRenderer`)

対象: `BoardUIRenderer::OnRender2D()`の`Mode::Combat`分岐(現行`BoardUIRenderer.cpp:145-174`)。

- `BarView`に`UIRectRenderer* rectRenderer`は持たせず、`BoardUIRenderer`自身が
  `UIRectRenderer* m_rectRenderer`をメンバとして持ち、`DrawCombat()`の引数で受け取って保持する。
- 描画順(`OnRender2D`内、`Font::Begin()`より前にまとめて実施):
  1. 背景矩形: `m_rectRenderer->DrawRect(rc, {bar.uiPos.x, bar.uiPos.y - 6}, {114, 14}, kBarBgColor, {0.5,0.5})`
     `kBarBgColor = Vector4(0.08f, 0.08f, 0.09f, 0.85f)`(暗い半透明、alphaが効くため縁取り無しでも背景と分離できる)。
  2. HP前景矩形: 左端基準(`pivot=(0.0,0.5)`)で`hpRatio`分だけ幅を伸ばす。
     `Vector2 hpPos = {bar.uiPos.x - 55.0f, bar.uiPos.y - 6.0f};`(背景左端に合わせる、幅114なので半分55を基準にオフセット)
     `Vector2 hpSize = {106.0f * bar.hpRatio, 10.0f};`
     `m_rectRenderer->DrawRect(rc, hpPos, hpSize, HPColor(bar.hpRatio), {0.0f, 0.5f});`
     (`HPColor()`は既存関数をそのまま流用、緑/黄/赤)。
  3. シールド表現: `bar.shieldRatio > 0`のとき、HP前景の右端に隣接して白〜水色の薄い矩形を追加
     (`Vector2 shieldPos = {hpPos.x + hpSize.x, bar.uiPos.y - 6.0f}; Vector2 shieldSize = {106.0f * min(bar.shieldRatio, 1.0f - bar.hpRatio), 10.0f};`
     色`Vector4(0.75f, 0.9f, 1.0f, 0.9f)`)。HP+シールドが106pxを超える分は素直に切り詰める
     (背景幅を超えて描かない)。実装が難しければテキスト併記のままでも受け入れ条件は満たす
     (intent.md「難しければテキスト併記のまま可」)。
  4. 続けて`Font::Begin()`〜: ラベル行(既存のまま、`bar.label`)、値テキストは
     `MakeBar()`のASCII部分を削除し`"%d/%d"`(+shield時`"%d/%d +%d"`)のみに簡略化して
     矩形の下(`bar.uiPos.y - 20`付近、要実機調整)に描く。
- 敵味方の色分けは、ラベルの`sideColor`はそのまま流用(背景/前景の矩形自体はHPColor()の
  緑黄赤で共通、TFT本家もHP色は陣営問わず割合で決まるため方向性一致)。

## 6. スキルゲージバーの新規表示

[レビュー是正1・3] `feature/item-passive-effects`のmainマージ後にrebase済み(衝突なし)。
以下は**rebase後の実ファイルを読み直して確定した**内容(旧版から行番号・詳細を更新)。

### 6-1. `CombatEvent`/`CombatEngine`への最小追加

- `CombatEventType`(`Game/CombatEvent.h`)は現在
  `Move, NormalAttack, SkillAttack, SplashDamage, Heal, Shield, ShieldAbsorb, Burn, Death, Warning`
  (`Burn`はitem-passive-effectsで追加済み)。**`GaugeChange`を`Burn`の直後・`Death`の前に追加**する
  (enumは明示値を持たないため追加位置自体に競合は無いが、意味的なまとまりでこの位置にする)。
- 意味: **自己参照イベント**。「`actorOwner`/`actorName`/`actorIndex`のユニットの現在ゲージ値が
  `afterValue`になった」。新しい構造体フィールドは追加しない(既存フィールドの意味を転用するだけ)。
  `targetOwner`/`targetIndex`等は未使用(Heal/Shield/Warningと同じ扱い)。
- 発行箇所: `Game/CombatEngine.h`の`PerformAction()`(rebase後は**179-207行目**、item-passiveの
  追加分だけ以前の調査時(174-202行目)より5行下にずれている)。
  - `attacker.normalAttackCount++`(202行目、通常攻撃時)の直後 → attacker用`GaugeChange`
    (`afterValue = attacker.normalAttackCount + attacker.receivedAttackCount`)を`outEvents`へpush。
  - `willUseSkill`で`normalAttackCount = 0; receivedAttackCount = 0;`(196-197行目)にリセットした
    直後 → attacker用`GaugeChange`(`afterValue = 0`)をpush(ゲージが空になったことを再生側に伝える)。
  - `target.receivedAttackCount++`(206行目)の直後 → target用`GaugeChange`
    (`afterValue = target.normalAttackCount + target.receivedAttackCount`)をpush。
  - `time`の設定パターンを確定済み: `NormalAttack()`(416行目〜)・`TickBurnsForBoard()`
    (item-passive追加分)いずれも`e.time = m_currentTime;`(`CombatEngine`のメンバ)を使っている。
    `GaugeChange`イベントも同様に`e.time = m_currentTime;`をそのまま使えばよい
    (旧版で「未確認」としていた懸念点はこれで解消)。
- `Game/CombatLogPrinter.h`の`switch(event.type)`に`case CombatEventType::GaugeChange: break;`を
  `case CombatEventType::Burn:`(89行目)と`case CombatEventType::Death:`(97行目)の間に追加
  (頻度が高くログが埋まるため出力はしない)。

### 6-2. `CombatPlayback`への追加

- `UnitView`(`CombatPlayback.h`)に`int displayGauge = 0;`と`int skillThreshold = 1;`を追加。
- `CombatPlayback.cpp`の匿名名前空間`MakeView()`(14-31行目、`displayHP`をmaxHPから初期化している
  箇所と同じ関数)で:
  - `v.displayGauge = unit.normalAttackCount + unit.receivedAttackCount;`
    (**ラウンドをまたいだゲージの持ち越し有無という既存仕様には立ち入らず、渡されたUnitInstanceの
    実際の値をそのまま表示初期値にする**。displayHPをmaxHPから初期化する隣で同様に扱う)。
  - `v.skillThreshold = max(1, unit.def->skillThreshold + unit.bonusSkillThreshold);`
    (`CombatEngine::GetEffectiveSkillThreshold`と同じ式。式自体は2行なので複製で十分、
    CombatEngineへの依存を増やさない)。
- `ApplyEvent()`(113-163行目)の`switch(ev.type)`に`case CombatEventType::GaugeChange:`を追加し、
  `if (UnitView* v = ResolveActor(ev)) { v->displayGauge = ev.afterValue; }`
  (自己参照イベントなので対象はactor側のみ。`NormalAttack`等が`ResolveTarget`を使うブロックとは別に、
  `Heal`/`Shield`と同じ`ResolveActor`パターンに倣う)。

### 6-3. `BoardUIRenderer`側の表示

- HPバーの下、`bar.uiPos.y - 22`付近(HP前景矩形のさらに下、既存の値テキスト位置と衝突しないよう
  実機調整)に、幅114×高さ6程度の小さいバーを追加。
  - 背景: `kBarBgColor`と同色を流用。
  - 前景: `ratio = min(1.0f, (float)v.displayGauge / (float)v.skillThreshold);`
    色は必殺技(マナ)を連想する寒色系`Vector4(0.4f, 0.7f, 1.0f, 1.0f)`固定(TFT本家のマナバー = 青に寄せる)。
  - `ratio >= 1.0f`(次の行動で必殺技発動)のとき、色を少し明るくする/点滅させる等の追加演出は
    任意(必須ではない、既存の点滅手法=描画のON/OFFを流用可)。
- `!bar.alive`のユニットはHPバー同様スキップ(既存の`if (!bar.alive) continue;`と同じ扱い)。

## 7. XPバーの塗り矩形化(`PlayerStatusUIRenderer`)

対象: `PlayerStatusUIRenderer::OnRender2D()`(現行`PlayerStatusUIRenderer.cpp:63-94`)、
`MakeXPBar()`(同26-47行目)は削除する。

- `kX = 560.0f, kLevelY = 454.0f`は変更しない(既存レイアウト維持)。
- `Font::Begin()`より前に:
  - 背景矩形: `pos = {kX + 40.0f, kLevelY - 4.0f}, size = {160, 14}, color = kBarBgColor, pivot = {0.0, 0.5}`
    (`"LV n "`の右にバーを置く想定でX基準をずらす。正確な位置は実機で`"LV %d  "`の文字幅を見て調整)。
  - 前景矩形: `ratio = xp / xpForNextLevel`、`pivot = {0.0, 0.5}`起点、
    `size = {160 * ratio, 10}`、色は既存`kLevelColor`と同系(または金/水色寄りの専用色を新設)。
- `MAX`到達時(`m_xpForNextLevel <= 0`)はバーを描かない(満タン固定の矩形を出すか、
  何も出さず`"LV n (MAX)"`のテキストのみにするかは実装judgement。後者で受け入れ条件は満たす)。
- テキストは`"LV %d"`とバー右側に`"%d/%d"`(または`"(MAX)"`)を残す(ASCII部分のみ削除)。

## 8. `ResultUIRenderer`の暗幕を半透明矩形化

対象: `ResultUIRenderer::OnRender2D()`の`Mode::GameOver`/`Mode::Victory`分岐
(現行`ResultUIRenderer.cpp:117-131`)。`kBackdropBlock`定数と4枚重ね描画のコードは削除する。

- 画面全体を覆う暗幕1枚: `pos = {0, 0}(画面中央), size = {UI_SPACE_WIDTH, UI_SPACE_HEIGHT}, pivot = {0.5, 0.5}`、
  色 `Vector4(0.0f, 0.0f, 0.0f, 0.55f)`(黒、alpha 0.55。intent.mdの「黒 alpha 0.5前後」に合わせる)。
  `#`ブロックのように文字周辺だけを覆うのではなく**画面全体**を覆う設計に変更する
  (本物の半透明が使えるため、"背後の盤面がうっすら見える"という受け入れ条件を画面全体で満たせる。
  文字周辺だけに絞る必要が無くなり、`kBackdropX/Y`等の位置調整コードも不要になる)。
- `Font::Begin()`より前に上記1回のみ`DrawRect`を呼ぶ。既存の`GAME OVER`/`到達ラウンド`/
  `PRESS [A] TO TITLE`のテキスト描画コードはそのまま(位置・内容とも変更しない)。

## 9. 既知の制約への対応(既存と同じ)

- `Font`のpivot中央揃え不可・`color.w`フェード不可という制約は変わらず残る(本タスクのスコープ外、
  テキスト自体の配置ロジックは変更しない)。
- ソースは UTF-8 (BOM付き) で保存する(`Game/`配下の既存規約、CP932環境でのパース崩れ回避)。
- `docs/tasks/ui-sprite-bars/`配下のMarkdownは既存の他タスクと同じくUTF-8(BOM無し)でよい。

## 10. 未解決の懸念点・実装フェーズで確認すべきこと

1. ~~`Sprite::Init`のファイルパス基準ディレクトリ~~ → §1の変換作業と合わせて実装フェーズの
   最初のステップとして実機確認する(white.dds単体を読み込んで白い矩形が出るかで検証)。
2. **HPバー/スキルゲージバーの正確なピクセルサイズ・オフセット**: §5・§6-3の数値は初期値の目安。
   3Dワールド座標をUI射影した`bar.uiPos`はカメラ距離によって画面上の見かけサイズが変わらない
   (UI空間に射影済みのため)ので大枠は安定するはずだが、既存のASCIIバー(現行`kBarValueScale=0.44`
   相当の文字サイズ)と見比べて違和感が無いよう実機で微調整すること。
3. ~~`GaugeChange`イベントの`time`値の厳密な設定方法~~ → §6-1で解消済み(`m_currentTime`を使う)。
4. **`white.dds`の実行時パス配置**: ビルド出力(`Game/x64/Debug/`)からの相対アクセスになるため、
   既存のAssets参照パターン(プロジェクトルート相対で動いている)に倣えば動くはずだが、
   §1の「white.dds単体読み込みの実機確認」で必ず検証すること。
5. **シールド表現の重ねバー**(§5-3)は初回実装で難しければテキスト併記に留めてよい
   (intent.mdで明示的に許容されている)。

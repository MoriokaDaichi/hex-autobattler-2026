# enemy-model-look — 要件定義 / 調査メモ (intent.md)

> board-layout-tuning 作業2 の成果物。**調査のみ。実装方針の決定はユーザー。**
> 発端: board-layout-rework の F5 検証で「敵プレビューモデル(特に Round1 の Slime)が
> フラットなティール単色に見え、プレイヤー機のような陰影・質感が無い」。

## 背景・現状

- 敵盤面(r3-5)のモデルは board-layout-rework で初めて 3D 描画されるようになった
  (`UnitModelDisplay` を board 非依存に一般化し `m_enemyModelDisplay` を追加)。
- **コード上の非対称は無い**: `m_unitModelDisplay`(プレイヤー) と `m_enemyModelDisplay`(敵) は
  同一クラス・同一 `ModelRender::Init(modelPath, animClips, 5)` 経路。マテリアル/ライト/
  シャドウ/スキン/RT登録すべて共通。→ 「敵側だけ設定が抜けている」わけではない。

## 調査で判明したこと(要点)

### 1. シーンのライティング構成 (`Game::Start()`)
```cpp
SetDirectionLight(0, dir(0.35,-0.75,0.55) 正規化, color(1.05,1.0,0.92));
SetAmbient(Vector3(0.35, 0.35, 0.4));
SetSceneMiddleGray(0.03f);   // 露出目標をかなり低く
SetBloomThreshold(10.0f);    // ブルーム発光しきい値を高く
```
- **ディレクショナルライトは 1 本のみ**(`MAX_DIRECTIONAL_LIGHT = 4`、スロット 1〜3 は未使用)。
- **IBL / スカイキューブは未設定**(`SetAmbientByIBLTexture` / `ReInitIBL` を一度も呼んでいない)。
  → シェーダの `light.isIBL` は常に 0。
- フィルライト・リムライトなし。背景はほぼ真っ黒(スカイボックス描画なし)。
- 結果: 全モデルが「1 方向ライト + `ambientLight * albedo` の平坦なアンビエント」だけで陰影付け。
  ベースラインがそもそもフラット。

### 2. この描画パスが実際に使うマテリアルチャンネル
GBuffer 書き込み: `Assets/shader/preProcess/RenderToGBufferFor3DModel.fx`
```
psOut.metaricShadowSmooth = g_spacular.Sample(...);   // metallic/roughness テクスチャの RGBA をそのまま
psOut.metaricShadowSmooth.g = 255 * isShadowReciever; // ★ G チャンネルは影フラグで上書き
```
ディファードライティング: `Assets/shader/DeferredLighting.fx`
```
metaric = tex.r;   // メタリック
smooth  = tex.a;   // スムースネス(粗さの逆)
// .g は影パラメータとしてのみ使用
```
**→ このエンジンは metallic/roughness テクスチャの R(メタリック)と A(スムース)しか見ない。
G チャンネルの roughness は影フラグで上書きされ、一切参照されない。**

### 3. 各ユニットのテクスチャ実測 (DDS の RGBA 平均)
| テクスチャ | R(metallic) | G(roughness/影) | B | A(smooth) |
|---|---|---|---|---|
| `Slime_metallic.dds` | 0 | 0 | 0 | **255** |
| `Warlord_metallic.dds` | 0 | 0 | 0 | **255** |
| `Knight_metallicRoughness.dds` | 0 | 0〜213(変化) | 変化 | **255** |
| `Goblin_metallicRoughness.dds` | 0(一部メタル) | 0〜193(変化) | 0 | **255** |

- **Slime と Knight は、エンジンが実際に読む R と A が同値**(metallic=0, smooth=1.0)。
  Knight の "良さ" を作っている G チャンネルの roughness 変化はエンジンが読んでいない。
- `Slime_albedo.dds` / `Slime_normal.dds` は構造的に正常(値の変化あり、Knight と同様のプロファイル)。
  法線マップも平坦ではない。

### 4. では Slime はなぜフラットに見えるか(結論)
テクスチャの `_metallic` 命名やチャンネル欠落が原因**ではない**(エンジンが roughness を無視するため)。
実際の要因は複合:
- **メッシュがなめらかで一様色のブロブ**。装甲・衣服のような大きな面の切り替わり・色コントラストが無く、
  1 本の斜光 + 平坦アンビエントだと陰影の勾配が乏しい。
- **`smooth = 1.0`(鏡面)** なのに **IBL が無い**ため、鏡面反射が映すものが「ほぼ真っ黒の空間」。
  スペキュラは極小のハイライト点しか出ず、実質 `ambient*albedo + NdotL*albedo` = 平坦なティール。
- 人型ユニットは albedo/normal の高周波ディテールと多色構成でこの弱いライティングでも情報量が出るが、
  無地のスライムには出ない。
- HANDOFF.md の記述どおり Slime は単一ボーンの squash&stretch モデル(`rig_and_animate_simple.py`)で、
  そもそも面の情報量が少ない。

## 対策案

### 案A(推奨・第一候補): フィル/リムライトを追加
`Game::Start()` で未使用のライトスロット 1〜2 を使う。例:
```cpp
// カメラ側からの弱いフィル(影を落とさない)
SetDirectionLight(1, 正規化(-0.2, -0.35, -0.9), Vector3(0.35, 0.37, 0.45));
// 背後/横からのリム(輪郭を起こす)
SetDirectionLight(2, 正規化(-0.5, -0.2,  0.7), Vector3(0.5, 0.45, 0.4));
```
(`castShadow` はデフォルト off のはず。要確認。強度は F5 で調整)
- **メリット**: 変更は `Game::Start()` の数行のみ。シェーダ・アセット・露出設定に触れない。
  単純形状(Slime)に立体感が出る。全ユニット一様に良化(人型も破綻しにくい)。リスク最小。
- **デメリット**: 鏡面反射の情報量は増えない(Slime は "つや無しで立体的" 止まり)。
  ライトが増えるぶん人型の陰影が浅くなる方向なので、メインライトとのバランス調整が要る。
- **影響範囲**: レンダリング全般(全モデル・全フェーズ)。ただし色/強度で可逆的に調整可能。
- **工数感**: 実装 15 分 + F5 調整。

### 案B(第二候補・payoff 大): IBL(環境マップ)を有効化
`Game::Start()` に 1 行:
```cpp
g_renderingEngine->SetAmbientByIBLTexture(L"Assets/modelData/preset/skyCubeMapDay_Toon.dds", <luminance>);
```
プリセットのスカイキューブは `Game/Assets/modelData/preset/skyCubeMap*.dds` に多数同梱済み
(Grass / Day_Toon / Night ほか)。シェーダ側の IBL 経路(`SampleIBLColorFromSkyCube`)は実装済みで、
`light.isIBL` を 1 にするだけで通る。
- **メリット**: 環境反射・環境アンビエントが乗り、`smooth=1.0` の Slime にキューブマップが映り込んで
  一気に立体的・上質になる。シーン全体が本格的な見た目になる。追加アセット作成不要。
- **デメリット**:
  - **露出/ブルームの再調整が必須**。現状 `SetSceneMiddleGray(0.03)` / `SetBloomThreshold(10)` は
    「背景真っ黒」前提の値。環境光が増えると自動露出・ブルームが暴れうる(既存コメントが警告している事象)。
  - シーン全体のトーンが変わるため、以後の F5 スクショの基準がすべて変わる。人型ユニットの
    現状の見た目も変化する(悪化しないかの確認が必要)。
  - `SetAmbientByIBLTexture` が可視スカイボックスも描くのか(盤面の背後に空が出るのか)は
    実装時に要確認。出る場合、意図した見た目か/抑止するかの判断が要る。
- **影響範囲**: レンダリング全般 + 露出・ブルーム設定。
- **工数感**: 実装 5 分 + 露出/ブルーム/ライトの再チューニング 1〜2 時間 + 全フェーズ F5 確認。

### 案C(補助・Slime 単体の微修正): `Slime_metallic.dds` の alpha を下げる
DeferredLighting は `smooth = tex.a` を実際に使う。`Slime_metallic.dds` は現在 A=255(=完全鏡面)。
これを A≈100(smooth≈0.4、やや粗い)にすると、1 本の斜光でもピンポイントでなく広く柔らかい
スペキュラ応答になり「つるっとした平面」感が減る。
- 作り方: `Slime_metallic.dds` を単色 `RGBA=(0,0,0,~100)` の DDS で上書き。
  `png_to_dds.py` は Blender(`bpy`)+ ソース PNG 前提だが、**単色なら DDS ヘッダ + ベタ塗り RGBA を
  直接書く 20 行程度のスタンドアロンスクリプトで足りる**(ヘッダ生成コードは `png_to_dds.py` から流用可、
  Blender 不要)。4×4 の単色 PNG を用意して `png_to_dds.py` に通す手もある。
- **メリット**: 変更が Slime だけに閉じる。人型・シーン・露出に一切影響しない。低リスク。
  IBL(案B)を入れる場合は特に効く(鏡面すぎない反射になる)。
- **デメリット**: 単独では効果は限定的(案A/Bと併用推奨)。`Slime_metallic.dds` は Git LFS 追跡で、
  `modelData/*.dds` は「phantom modified」問題があるため、コミット時に本当に差し替わった 1 ファイルだけを
  意図して add する注意が必要。Warlord も同じ全黒 A=255 なので、揃えるなら Warlord も対象。
- **工数感**: スクリプト + 生成 + F5 確認で 1 時間程度。

### やらない方がよい: roughness データを足す / `_metallicRoughness` 命名へ揃える
このエンジンは metallic/roughness テクスチャの **G チャンネル(roughness)を影フラグで上書きして無視**する。
roughness マップを整備しても描画結果は 1 mm も変わらない。命名の食い違い(`_metallic` vs
`_metallicRoughness`)は tkm の参照ファイル名と実ファイル名が一致していれば実害なし
(Slime は `Slime.tkm` → `Slime_metallic.png` → `Slime_metallic.dds` で一致しているので問題ない)。

## おすすめ

**案A(フィルライト追加)を第一に。** 変更が小さく可逆で、Slime を含む全モデルの立体感が上がり、
リスクが最も低い。これで F5 して不足なら **案C(Slime alpha 下げ)を併用**。
シーン全体をワンランク上の見た目にしたい意思決定があれば **案B(IBL)** に進む(露出再調整の工数を見込む)。

## スコープ / 受け入れ条件(実装タスク化する場合)
- In: `Game::Start()` のライティング設定変更(案A/B)、`Slime_metallic.dds`(必要なら `Warlord_metallic.dds`)の
  単色差し替え(案C)。
- Out: モデル/メッシュの作り直し、albedo/normal マップの再ペイント、シェーダ改修、
  ライティングモデルの変更。
- 受け入れ: 敵盤面の Slime が「平面的なティール塗り」でなく立体的に見える。人型ユニットの見た目が
  悪化しない。全フェーズ(準備/戦闘/結果/タイトル/GameOver)で破綻なし。`Debug|x64` 0 エラー。

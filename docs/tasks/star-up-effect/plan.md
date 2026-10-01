# ★アップ演出 — plan

要件は `intent.md` を参照。エフェクト素材(.efk/.efkefc)はプロジェクトに1つも無く(`Game/Assets` は animData/font/modelData/shader/spriteData のみ、
spriteData は単色 dds のみ)、エンジンの Effekseer ラッパー(`k2EngineLow/graphics/effect/EffectEmitter`)を使うには素材を新規作成する必要がある。
そのため今回は **既存の描画手段だけで自作** する。

## 使う描画手段
| 手段 | 既存の所在 | 用途 |
|---|---|---|
| `UIRectRenderer::DrawRect`(white.dds を乗算カラーで染める半透明矩形、Sprite 1回1個のプール) | `Game/UIRectRenderer.*` | リングの光点・光の柱・粒子・画面フラッシュ。**回転引数を追加**して 45° 回した菱形(キラッとした光点)を描く |
| `BoardUIRenderer::WorldToUI`(カメラVPでワールド→UI空間へ射影) | `Game/BoardUIRenderer.cpp` | 盤面マスの足元の「地面上の円」を3Dで計算して射影する → 遠近の付いた楕円リングになる |
| `UnitModelDisplay` の表示スケール(`GetStarModelScaleMultiplier`) | `Game/UnitModelDisplay.cpp` | ユニットの「ポップ」(一瞬拡大して戻る)。星倍率にさらに乗算 |
| `g_camera3D` の位置/注視点 | `Game/Game.cpp` (Start で固定設定) | ★3のみ、短いカメラ揺れ(位置と注視点を同じだけずらすので向きは変わらない) |

制約: Sprite のブレンドは `AlphaBlendMode_Trans` 固定(加算なし)なので、明るい金色を半透明で重ねる。2D オーバーレイなのでモデルより手前に描かれる
(=柱の芯はモデルを隠しすぎないよう alpha を控えめにする)。Font の alpha は効かない(既知)ため、テキスト演出は使わない。

## 演出の構成(共通の「型」)
全て同じタイムライン t=0 で同時に始まり、同じ色2色(**金色の光 kGlowColor / 白金の芯 kCoreColor**)だけを使う。

1. **光のリング**: 足元の地面上に光点を円周状に並べ、外へ広がりながら薄れる(ease-out)。少しずつ回転する。
2. **光の柱**: 足元から上へ伸びる縦長の帯(外側=金の淡いグロー、内側=白金の芯の2枚重ね)。素早く伸び、細くなりながら消える。
3. **立ち上る粒子**: リング半径内の点から菱形の粒子が上昇して消える(位置・速度は効果ごとのシードで疑似乱数)。
4. **ポップ**: 盤面ユニットのモデルが一瞬拡大して戻る(`1 + amp * sin(π·n·u) * (1-u)`、n=バウンド回数)。
5. (★3のみの +α) **画面フラッシュ**(全画面に白金の半透明矩形、ease-out で消える)と **カメラ揺れ**。

★2と★3は **上記1〜4を同じ関数・同じ色で描き、パラメータ(規模・数・時間・明るさ)だけを変える**。★3専用の+αは5のみ。
パラメータは `StarUpEffectRenderer.cpp` 冒頭の `MakeStar2Params()` / `MakeStar3Params()` にまとめる(後から調整しやすいよう全て名前付き)。

## パラメータ表(単位: 長さ=ワールド単位、時間=秒。kHexSize=50、★1モデル高 ≒ 30)
| 項目 | ★2(控えめ) | ★3(派手) |
|---|---|---|
| リング本数 / 発生間隔 | 1 / - | 3 / 0.14 |
| リング寿命 | 0.45 | 0.70 |
| リング半径(開始→終了) | 10 → 45 | 10 → 72 |
| リング光点数 / 光点サイズ(px) | 16 / 7 | 24 / 10 |
| 柱 寿命 / 高さ / 幅 | 0.40 / 60 / 14 | 0.80 / 140 / 26 |
| 粒子数 / 寿命 / 上昇量 / サイズ(px) | 8 / 0.60 / 45 / 6 | 28 / 0.95 / 95 / 9 |
| 粒子の発生期間 / 広がり半径 | 0.15 / 22 | 0.40 / 34 |
| ポップ 振幅 / 時間 / バウンド | +12% / 0.30 / 1 | +25% / 0.55 / 2 |
| 明るさ(alpha倍率) | 0.75 | 1.0 |
| 画面フラッシュ alpha / 時間 | なし | 0.30 / 0.30 |
| カメラ揺れ 振幅 / 時間 | なし | 4.0 / 0.30 |

一貫性の担保: 色定数は★2/★3で共通の1組のみ・描画関数も共通(`DrawEffect`)で、星ごとに切り替えるのは Params 構造体だけ。
★3は★2の全要素を含み、各値を「大きく・多く・長く・明るく」した上に +α(フラッシュ・揺れ・二度弾むポップ)を足す。

## ベンチの場合
ベンチは2D一覧(モデル無し)なので、同じ `DrawEffect` を「ローカル座標→UI座標」の変換だけ差し替えて描く:
`ui = ベンチ行カード中心 + (x·s, (y + z·kBenchGroundFlatten)·s)`(s=kBenchPxPerWorld=0.7、flatten=0.4)。地面の円が平たい楕円、
柱は上方向に伸びる。ポップ・カメラ揺れは盤面専用(ベンチはモデルが無いため。フラッシュは★3なら共通で出る)。
行が表示上限(`kBenchMaxVisibleRows`)を超える位置なら集約行の位置に出す。

## データの流れ
1. `Player` に `struct MergeEvent { const UnitDef* def; int newStarLevel; bool onBoard; HexCoord pos; }` と
   `std::vector<MergeEvent> mergeEvents` を追加(`itemNotices` と同じ「Playerは積むだけ、Gameが次フレーム頭で消費して clear」方式)。
   `TryMergeUnits()` の合成成立時に1件積む。
2. `Game::Update()` で `players[0].mergeEvents` を消費:
   - **連鎖合成の畳み込み**: 同じバッチ内に同じ UnitDef でより高い星のイベントがあれば低い方は捨てる(1→2→3 は★3演出のみ)。
   - 位置は消費時点の実データから引き直す(盤面: 記録したマスに同 def・同星が居ればそこ/居なければ盤面を検索、ベンチ: 同 def・同星の
     最後の index)。見つからなければ演出なし(同フレームで売却された等)。
   - フィードバック文「合成! <名前> *N」を `itemNotices` の文と連結して `ShopUIRenderer::PushFeedback` に1回で流す
     (PushFeedback は上書き式のため)。星表記はフォントにある ASCII `*`(既存 `NotifyMergeCarryOver` の `★` も `*` に修正。
     ★グリフはスプライトフォント未収録でクラッシュ要因、TooltipContentBuilder.cpp のコメント参照)。
3. `StarUpEffectRenderer`(新規、IRenderer)を Game が1個所有。Preparation 中だけ `Update(dt)`、それ以外は `Clear()`。
   `Game::Render()` の準備フェーズ分岐でドラッグ表示の後(ツールチップ/ヘルプより奥)に `Draw()`。
4. `UnitModelDisplay::Update(board, const StarUpEffectRenderer* = nullptr)` でマスごとのポップ倍率を受け取り星倍率に乗算(足元リフトにも
   掛けるので足は地面に付いたまま上へ伸びる)。敵側は nullptr。
5. カメラ揺れ: Game が Start 時のカメラ位置/注視点を保持し、`GetCameraShakeOffset()` が非ゼロの間(と戻す1フレーム)だけ基準+オフセットを設定。

## 操作を止めない・破綻しないための方針
- 入力処理・フェーズ遷移には一切関与しない(描画とモデルスケールのみ)。
- 同時再生数の上限 `kMaxActiveEffects = 6`(超えたら古い順に捨てる)。
- `UIRectRenderer` の事前確保数を 256 → 384 に引き上げ(★3は1件で最大 ≒ 3×24+28+4+1 = 105 枚)。超えても既存のフォールバックで生成される。

## 変更ファイル
- 新規 `Game/StarUpEffectRenderer.h/.cpp`(+ `Game.vcxproj`/`.filters` 登録)
- `Game/Player.h`(MergeEvent、★→* 修正)
- `Game/UIRectRenderer.h/.cpp`(回転引数、プール数)
- `Game/UnitModelDisplay.h/.cpp`(ポップ倍率)
- `Game/Game.h/.cpp`(所有・消費・描画・カメラ揺れ)

## 懸念点
- 2Dオーバーレイなので、リングの奥側半分もモデルより手前に描かれる(3D深度の前後関係は無い)。
- 演出中に盤面ユニットをドラッグで移動すると、演出は元のマスに残る(ポップも元マス基準)。短時間なので許容。
- カメラ揺れ中は盤面マスのヒット領域が数pxずれる(0.3秒、振幅4)。

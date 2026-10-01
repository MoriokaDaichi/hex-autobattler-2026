# 見やすさ改善: 盤面サイズ・収入内訳 — plan

intent.md (同ディレクトリ) の F・H の技術設計。実機の目視ができない前提のため、F の数値はカメラ投影を
CPU で再計算した結果を根拠にしている(計算式は §F-2)。

> 注: intent.md の「r:0〜8」は旧記述。現行の盤面は board-layout-rework 以降 **q:0-8 × r:0-5 の6行×9列**
> (r0-2=自陣 / r3-5=敵陣、中立ゾーン無し)。ベンチは3D盤面ではなく画面左の2D一覧(BoardUIRenderer)。
> 本設計はこの現行仕様を対象にする。

---

## F. カメラ調整で盤面を大きく表示する

### F-1. 現状

- `Game::Start()`(Game.cpp): `g_camera3D->SetPosition({0, 1230, -975})`, `SetTarget({0, 0, 75})`。
  画角・near/far はエンジン既定(`Camera.h`: 垂直画角 60°, near 1, far 5000)。アスペクトは 1920/1080。
  → 俯角 ≈ 49.5°, ターゲットまでの距離 ≈ 1617。
- マス座標(`HexGridRenderer::CalcTileCenter`): pointy-top, kHexSize=50, 盤面中心がワールド原点。
  x = 50·(√3·(q-4) + √3/2·(r-2.5)), z = 75·(r-2.5)。マスは平行四辺形状に並ぶ(手前行は左寄り、奥行は右寄り)。
- 2D UI(UI空間 1920x1080、中央原点・y上向き)の占有範囲(各Rendererの定数から):

| UI | x範囲 | y範囲 |
|---|---|---|
| ベンチ一覧カード(BoardUIRenderer) | -910 〜 -642 | +250 〜 -110 |
| トレイトパネル(TraitPanelUIRenderer) | -910 〜 -670 | -150 〜 下方 |
| GOLD/LV HUD(PlayerStatusUIRenderer) | +548 〜 最大+952 | +516 〜 +430 |
| ラウンド戦績(RoundRecordUIRenderer) | +506 〜 +900 | +403 〜 +291 |
| 未装備アイテム(ItemInventoryUIRenderer) | +512 〜 | +270 〜 下方 |
| ショップ(フィードバック行〜カード2行目) | -880 〜 +920 | -330 〜 -460 |
| 戦闘開始ボタン | +640 〜 +940 | -269 〜 -341 |

  → 盤面に使える領域は概ね **x: -642 〜 +506, y: -310 〜 +540**(横幅 ≒1150px がボトルネック)。

- 現カメラでの盤面の画面上の範囲(計算値): **X -326〜+278 (604px), Y -157〜+67 (224px)**。
  1マスの横ピッチは手前行 56px / 奥行 48px。HPバー上端は y≈+103。上下左右に大きな余白が余っている。

### F-2. 投影計算の方法

`Camera::Update()` と同じく `XMMatrixLookAtLH(pos, target, up=(0,1,0))` ×
`XMMatrixPerspectiveFovLH(60°, 1920/1080, 1, 5000)` で各マスの6頂点(y=0)と、
HPバー基準点(自陣 y=95、敵陣 y=95+26。BoardUIRenderer の kBarWorldY / kEnemyBarYBonus)を射影し、
`BoardUIRenderer::WorldToUI` と同じく NDC×(960, 540) で UI 座標に変換した。
HPバーは幅114px・上端+7px として範囲に含めた。

探索条件: カメラはターゲットの真後ろ上方(ヨー無し)、俯角 45〜70°、距離 900〜1700、ターゲット x/z をずらして
上表の UI と重ならない範囲で 1マスの画面上の大きさが最大になるものを探した。結果、横幅が拘束条件になり、
俯角を変えても最大倍率はほぼ同じ(手前ピッチ ≈ 98〜102px)。俯角は TFT に近い見下ろし感と縦方向の
読みやすさのバランスで 55° を採用し、UIとの余白を 20px 以上確保するため距離を最大値から約3%引いた。

### F-3. 変更後の値

- `SetTarget({ 10, 0, 50 })`, `SetPosition({ 10, 811, -518 })`
  (俯角 55°, 距離 990。カメラとターゲットの x を同じ +10 にずらす=ヨー無しのまま盤面を画面上で左へ平行移動し、
  左ベンチ列と右HUD列の間の空き領域の中心(x≈-68)に盤面の外接矩形の中心(x≈-67)を合わせる)。
- 画角・near/far は既定のまま(エンジン側は触らない)。

変更後の画面上の範囲(計算値):

| 項目 | 変更前 | 変更後 |
|---|---|---|
| 盤面(タイル)外接矩形 X | -326 〜 +278 | **-566 〜 +433** |
| 盤面(タイル)外接矩形 Y | -157 〜 +67 | **-267 〜 +131** |
| 手前行(r0) | X -326〜+184, Y -157〜-102 | X -566〜+302, Y -267〜-163 |
| 自陣最前列(r2) | X -252〜+225, Y -76〜-28 | X -426〜+362, Y -116〜-30 |
| 敵陣最奥(r5) | X -157〜+278, Y +27〜+67 | X -258〜+433, Y +64〜+131 |
| HPバー込みの範囲 | X -366〜+323, 上端 +103 | X -617〜+486, 上端 +183 |
| 1マスの横ピッチ(手前/奥) | 56 / 48 px | **95 / 76 px (約1.6〜1.7倍)** |

UI とのクリアランス:
- 左: HPバー左端 -617 は y≈-169(q0,r0)で、そこはベンチ(y≥-110)より下・トレイトパネル(x≤-670)の右。
  ベンチカード右端 -642 に対しても 25px 空く。タイル左端は -566。
- 右: HPバー右端 +486(q8,r5 付近, y≈+176)に対し ラウンド戦績/アイテム列の左端 +506 まで 20px。タイル右端 +433。
- 下: タイル下端 -267 に対し、ショップのフィードバック行(-330)まで 63px、戦闘開始ボタン(x≥640)とは x で離れている。
- 上: HPバー上端 +183。画面上端・右上HUD(y≥430)と重ならない。
- 結果表示(ResultUIRenderer)は従来どおり盤面に重ねて表示する仕様のため対象外。

UI の移動は不要(横幅の拘束は UI ではなく平行四辺形の盤面形状による)。

### F-4. マウス判定

- ヘックスカーソル(`CursorSelectionSystem::TryMouseToHex`)は毎フレーム `g_camera3D` の VP 逆行列でレイを飛ばし
  y=0 平面と交差させる方式のため、カメラ変更に自動追従する(変更不要)。
- HPバー位置・UnitModelDisplay・HexGridRenderer も毎フレーム `g_camera3D` の行列を使うので追従する。
- **要修正**: 準備フェーズの盤面ヒット領域(Game.cpp、BoardUnit/BoardEmptyHex の UIHotRegion)は
  UI空間の固定サイズ(半幅35×半高16px)の矩形。拡大後はマスの中心付近しか反応しなくなるため、
  **マス中心からワールド空間で ±39 (x方向) / ±34 (z方向) の点を `BoardUIRenderer::WorldToUI` で射影して
  矩形を決める**方式に変える。マス間ピッチは x 86.6 / z 75 なので隣接マスの矩形同士は重ならず
  (x 隙間 8.6, z 隙間 7)、透視による手前/奥の大きさの差にも自動で追従する。
  変更後カメラでの矩形半サイズの目安: 手前行 43×35px、最前列(r2) 39×29px。

---

## H. ゴールドHUDホバーで次ラウンド収入の内訳ツールチップ

### H-1. 現状

- `PlayerStatusUIRenderer::BuildHotRegions` が GOLD 行に `UIRegionKind::GoldDisplay` の領域を既に登録済み
  (全フェーズで登録)。`TooltipContentBuilder::Build` の GoldDisplay は「所持ゴールド: nG」の1行のみ。
- 収入計算は `EconomySystem::GrantRoundIncome` 内にベタ書き(利子 = gold/10 を上限5でクランプ、
  連勝/連敗ボーナスは UpdateStreak で数を更新した後に GetWin/LossStreakBonus)。
- 実際の結果は Win/Loss のみ(相打ちも Loss 扱い。Draw は enum にあるが未使用)。

### H-2. EconomySystem に計算関数を切り出す(EconomySystem.h)

- `static int CalcInterest(int gold)` — 利子(上限クランプ込み)。
- `static int GoldToNextInterest(int gold)` — 次の利子段階まで必要なゴールド。上限到達済みなら 0。
- `static int CalcStreakBonusFor(const Player& player, CombatResult result)` — 現在の連勝/連敗数に対し、
  次の戦闘が result だった場合に付く連勝/連敗ボーナス(UpdateStreak と同じ規則で次の連数を求めてから
  GetWinStreakBonus/GetLossStreakBonus。Draw は 0)。
- `GrantRoundIncome` も利子は `CalcInterest`、ボーナスは **UpdateStreak より前に** `CalcStreakBonusFor(player, result)`
  で計算するよう置き換え、ツールチップと実収入が文字通り同じ関数を通るようにする(結果は従来と同値)。
  ログ `Income:` の書式は変えない。

### H-3. ツールチップ(TooltipContentBuilder.cpp の GoldDisplay)

表示例(所持 23G、1連勝中):
```
所持ゴールド: 23G                       ← タイトル行(既存)
次ラウンドの収入(戦闘終了時の所持金で計算)
基本収入 +5G
利子 +2G (10Gごとに+1、上限+5)
  あと7Gで利子+3G                      ← 上限到達時は「利子は上限(+5G)に到達」
勝った場合: 連勝ボーナス +10G (2連勝) -> 合計 +17G
負けた場合: 連敗ボーナス +0G (1連敗) -> 合計 +7G
```
- 矢印はフォントのグリフ有無が不明なため既存ツールチップと同じ "->" を使う。
- 基本収入/利子/上限/段階は全て EconomySystem の定数・関数から取る(ハードコードしない)。
- 合計 = kBaseIncome + CalcInterest(gold) + CalcStreakBonusFor(player, Win/Loss) で、GrantRoundIncome の
  totalIncome と同じ式・同じ入力(戦闘中はゴールドが変わらないため player.gold がそのまま使われる)。
- ツールチップ内容は表示中毎フレーム再構築されるため、購入・リロールで所持金が変わると即追従する。

---

## 変更ファイル

- `Game/Game.cpp` — カメラ値(F-3)、盤面ヒット領域の射影化(F-4)。
- `Game/EconomySystem.h` — 計算関数の切り出し(H-2)。
- `Game/TooltipContentBuilder.cpp` — GoldDisplay の内訳(H-3)。
- CombatPlayback.* / 戦闘中ダメージ数字表示まわりは触らない(並行タスク combat-number-overlap との衝突回避)。

## 未解決・実機確認が必要な点

- 計算は Font/モデルの実寸を含まない近似。ユニットモデルの頭部やHPバー左右端が UI に接していないか F5 で確認。
- 手前行のユニットモデルが大きく映るため、モデル同士・HPバー同士の重なり具合が変わる(拡大で相対的には減る方向)。
- ウィンドウのアスペクト比が 16:9 以外の場合は横幅が変わる(従来同様、FRAME_BUFFER 1920x1080 前提)。

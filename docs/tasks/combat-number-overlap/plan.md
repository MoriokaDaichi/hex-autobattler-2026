# 戦闘中の数字表示の重なり解消 — plan

intent.md(同ディレクトリ)の設計。対象は表示のみ。`CombatEngine` / `CombatEvent` / `Game.cpp` は変更しない。

## 1. 現状の仕組み(調査結果)

- 戦闘再生は `CombatPlayback`(Game/CombatPlayback.*)がイベント列を時系列に適用し、ユニットごとの表示用スナップショット `UnitView`(worldPos/displayHP/displayShield/displayGauge 等)を更新する。
- 画面上の数字は **`BoardUIRenderer::DrawCombat` / `OnRender2D` が描く各ユニット頭上のHPブロックだけ** だった:
  - ラベル行「ユニット名 *星」(scale 0.42、bar.uiPos.y+16)
  - HPバー矩形(幅114)+ その上の数値「HP/最大HP(+シールド)」(scale 0.44)
  - スキルゲージ矩形(幅114)
  - 位置はユニットの worldPos を `y+95`(敵は更に +26)した点を `WorldToUI` で射影。
- intent.md が想定していた「ダメージ/回復のポップアップ数字」は **実装されていなかった**(プレイヤーが「ダメージなどの数字」と呼んでいたのは上記HP数値・ラベル)。

## 2. 重なる原因

1. **テキストが中央揃えになっていなかった。** `Font::Draw` の pivot は SpriteFont の origin に生ピクセル値として渡るだけで正規化アンカーとして効かない(TitleUIRenderer/RoundRecordUIRenderer に既知の教訓あり)。HPブロックは `kCenterPivot(0.5,0.5)` を指定していたため、実際は **バー中心から右へ伸びる左詰め** になり、「523/600」(約68px)や名前ラベルが右隣のユニットのブロックへ食い込んでいた。
2. **ブロックが大きすぎる。** hexSize=50(隣接中心間 約87ワールド単位)は現カメラで画面上数十px程度しかないのに、バー幅114px。隣接ユニット同士はほぼ必ず横に重なる。敵バーを+26持ち上げる既存対策は「敵味方が隣接」の1パターンにしか効かない。
3. 同陣営が密集した場合の重なり回避が無い。

## 3. 対策

### 3-1. HPブロックの縮小と正しい中央揃え(BoardUIRenderer.cpp)
- ラベル scale 0.42→0.38、数値 scale 0.44→0.40、バー背景幅 114→90(前景 106→82)。
- テキストは `kTextTopLeftPivot(0,1)` で描き、左端X = 中心X − `UITextUtil::EstimateTextWidth(text, scale)/2` で自前中央揃え(`CenteredTextX`)。
- 表示内容(名前・星・HP/最大HP・シールド)は変えない。

### 3-2. HPブロックの重なり回避(画面空間で積む)— `BoardUIRenderer::DeclutterBars`
- 各ブロックの矩形: 横 = max(バー幅, ラベル概算幅)、縦 = ラベル上端(+16)〜ゲージ下端(-25)。
- 画面下側(手前)のブロックから順に配置し、既配置と重なるものはその上端+3pxまで押し上げる(押し上げ先で別のと当たれば繰り返す、16回上限)。ソートは (uiPos.y, 添字) で安定化。
- 押し上げ量の上限 `kMaxBarStackOffset = 100px`(≒2段)。それ以上の密集では重なりを許容(画面が縦に伸び過ぎるのを防ぐ)。
- 目標オフセットへは `kBarOffsetFollowRate = 12/秒` で指数追従させ、ユニット移動で目標が切り替わってもバーが瞬間移動しないようにする。状態 `m_barOffsetY` は `CombatPlayback::GetBeginSerial()`(Begin毎に+1)が変わったらリセット。

### 3-3. ダメージ/回復ポップアップ数字の追加(intent の要求仕様に沿って新設)
**生成(CombatPlayback.*)** — 表示用のデータだけを持つ。シミュレーション結果には影響しない。
- `DamagePopup { viewIndex, amount, kind, age, sinceLastHit, lethal }` を `m_popups` に保持。寿命・合算は **実時間**(再生速度最大5倍でも読めるように)。
- 種類 `PopupKind`: Physical(物理通常) / Magic(魔法通常・SplashDamage) / Skill(必殺技直撃) / Burn(火傷) / Heal(回復)。ShieldAbsorb は amount が総ダメージに含まれるので出さない(二重表示防止)。
- 寿命: 通常 0.9s、必殺技・とどめ 1.2s。
- 合算: Skill 以外は、同ユニットの最新ポップアップが同種かつ生成から 0.25s 以内ならそこへ加算(新規に出さない)。
- 上限: 1ユニット3個(超えたら同ユニットの最古を削除)、全体48個(超えたら最古を削除)。
- とどめ: Death 反映時、そのユニットの最新ダメージ数字(最後の生成/合算から 0.05s 以内=同フレーム)を `lethal=true` にする。

**描画(BoardUIRenderer::BuildPopupViews / OnRender2D)**
- 出現位置: worldPos の `y+60`(胴体付近、HPバー y+95 より下)を射影し、中央揃え。
- 寿命中に 30px 上昇(ease-out)、末尾 0.3s でフェード(影の alpha も連動。FontEngine はストレートアルファ)。
- 同ユニット内では新しい数字ほど下、古い数字は直前の数字の上端+1px より上に積む(同ユニット内で文字が重ならない)。行の高さは `40px × scale` で概算。
- 色/大きさ: 物理=白系 0.48、魔法=紫 0.48、必殺技=金 0.64、火傷=橙 0.40、回復=緑「+N」0.46、とどめ=赤 ×1.2。必殺技・とどめは出始め 0.12s に +35% から等倍へ縮む「ポン」演出。
- HPブロックの後に描いて最前面にする。

## 4. 変更ファイル
- Game/CombatPlayback.h / .cpp: PopupKind・DamagePopup・GetPopups・GetPopupLifetime・GetBeginSerial、SpawnPopup/UpdatePopups、ApplyEvent からの生成。
- Game/BoardUIRenderer.h / .cpp: 定数調整、CenteredTextX、DeclutterBars、BuildPopupViews、OnRender2D の描画。
- 並行作業中のファイル(Game.cpp のカメラ、ShopUIRenderer.cpp、TooltipContentBuilder.cpp、EconomySystem.h)には触れない。

## 5. 既知の制約・懸念
- テキスト幅・行高は概算(半角22px/全角44px、行40px @scale1)。実機で若干ずれる可能性。
- カメラ設定が並行作業で変わるため、画面上のユニット間隔に依存する値(バー幅90、押し上げ上限100、ポップアップ出現高さ60・上昇30)は F5 での微調整前提の出発値。
- ポップアップは胴体付近から上昇するため、短時間だけ自分のHPブロック下端にかかることがある(最前面描画・影付きで可読性は保つ)。
- 押し上げ上限を超える極端な密集(5体以上が画面上で一塊)では HP ブロックの一部重なりは残り得る。

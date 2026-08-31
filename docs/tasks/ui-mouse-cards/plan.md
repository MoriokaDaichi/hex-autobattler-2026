# ui-mouse-cards — 設計 (plan.md)

このファイルだけを読めば実装フェーズ(1→2→3)に着手できるよう、調査結果と具体的な仕様を記す。
背景・要求仕様・スコープは [`intent.md`](intent.md) を参照。**実装コードはこのフェーズでは書かない**
(このファイル自体が唯一の成果物)。

## 0. 調査結果サマリ(重要な発見・設計判断)

1. **マウス座標→UI_SPACE変換は単純な平行移動でよい。** `k2EngineLowPreCompile.h` で
   `FRAME_BUFFER_W/H` と `UI_SPACE_WIDTH/HEIGHT` はいずれも `1920/1080` で完全に一致している
   (`Font`/`Sprite`のUI空間は実解像度と1:1)。よって
   `uiX = mouseX - UI_SPACE_WIDTH*0.5f; uiY = UI_SPACE_HEIGHT*0.5f - mouseY;`
   だけで変換できる(`CursorSelectionSystem::TryMouseToHex`のNDC変換より単純)。§1参照。
2. **`nsK2EngineLow::Mouse`にホイールは無い。** `k2EngineLow/HID/Mouse.h`は
   `enMouseButtonLeft/Right/Middle`の`IsTrigger`/`IsPress`と`GetPositionX/Y`のみ。
   ホイール delta の取得手段が無い。→ **幸い、このゲームの一覧(ショップ5枠・ベンチ最大9・
   アイテム一覧・トレイト8種)はいずれも画面に収まる件数で、スクロールが必要な箇所が無い**
   (`intent.md`も「必要な箇所のみ」と条件付き)。よって**ホイール対応は不要と判断し、
   エンジン`Mouse`クラスの拡張はしない**(スコープから落とす。将来ベンチ上限撤廃等で
   件数が増えたら再検討)。
3. **`Font`にMeasureStringが無い制約は不変。** 全レンダラー`kTopLeftPivot`(実質左詰め)の
   既存パターンを踏襲する。ボタン/カードのヒット矩形も「文字数×概算幅」ではなく、
   **各レンダラーが元々持っている固定レイアウト定数(kX/kY/kStepY等)から矩形を直接計算する**
   (テキスト幅に依存しない設計にすることで、この制約の影響を受けない)。
4. **`Game::Update()`→`Game::Render()`の順で毎フレーム呼ばれる(標準ループ)。** 各UI Rendererの
   現在のレイアウト計算(`kSlotStartX`等)は`Draw()`/`OnRender2D()`側、つまり**Render()の中**にしか
   存在しない。マウスクリックの解決は**Update()の中**(既存ボタン判定と同じタイミング)で行う必要が
   あるため、**そのフレームのRender()がまだ走っていない=直前フレームのレイアウトを使うと1フレーム
   遅れる**問題がある。→ §2で「ヒット領域はUpdate()の先頭で毎フレーム再計算する」設計にして解消する
   (Render()のDraw()と同じレイアウト定数を、描画を伴わない軽量メソッドとして各Rendererに追加する)。
5. **`UnitDef`にスキルの説明文フィールドは無い。** `skillType`(enum) + `skillHealPercent`/
   `skillShieldAmount`/`skillSplashRadius`/`skillSplashPercent`のパラメータの組でしか
   必殺技効果を表現していない。→ ツールチップ用に**`skillType`から文言を合成するヘルパー関数**が
   必要(既存の`ShopUIRenderer.cpp`内`TraitName()`等、匿名名前空間ヘルパーと同じパターン)。§4-2参照。
6. **`ItemDatabase`にレシピの全件列挙APIが無い。** `FindRecipe(nameA, nameB)`(2つ指定して1件検索)
   のみで、「この素材から作れる完成品一覧」を引く手段が無い。→ `ItemDatabase`に
   `const std::vector<ItemRecipe>& GetAllRecipes() const`を追加する必要がある(§4-2)。
7. **`TraitSystem`に`CountBoardTraits`/`FindActiveTier`/`FindNextTier`が既にpublicで揃っている。**
   トレイトツールチップの「現在数・発動閾値・次段階」はこれをそのまま使える。「このトレイトを
   持つユニット一覧」は`UnitDatabase::GetAllUnitDefs()`を`def->traits`でフィルタすれば作れる
   (ロースター全体から。プレイヤーの所持数ではなく「該当ユニット一覧」という文言のため)。
8. **暗幕・矩形は`UIRectRenderer`(ui-sprite-bars)で解決済み。** カード背景・ツールチップ背景は
   全て`m_uiRectRenderer.DrawRect()`で描ける(本物の半透明alpha込み)。新規のSprite/シェーダー作業は
   不要。ただし`Font::Begin()`〜`End()`の外側(前)で呼ぶ制約は変わらず(§0-8 in ui-sprite-bars/plan.md)。

## 1. 共通基盤: マウス座標変換とヒット領域

### 1-1. スクリーン座標 → UI_SPACE 変換ヘルパー

**[フェーズ1実装で更新・実機検証済み]** 当初案は「UI_SPACE_WIDTH/HEIGHT==FRAME_BUFFER_W/Hなので
単純な平行移動でよい」としていたが、これは**誤りだった**。実機検証(DPIスケーリング環境、
実クライアント矩形が1707x960相当になるケース)で、ウィンドウの実際のクライアント領域のピクセル数は
`FRAME_BUFFER_W/H`(1920x1080)と一致しない場合があると判明。`FRAME_BUFFER_W/H`で直接割ると
マウス操作可能な範囲が画面の一部に縮小してしまう(コミット`d34e107`で修正)。

**さらに、この問題は新設のScreenToUISpaceだけでなく、既存の`TryMouseToHex`(盤面ヘックスpicking)にも
同じ形で存在していた**(FRAME_BUFFER_W/Hで直接NDC化しており、実クライアント矩形を見ていなかった)。
そのため実装では両方を修正した。

新規関数の置き場所: `Game`に持たせず`CursorSelectionSystem`に追加する
(`TryMouseToHex`と同じ「マウス座標系変換」の責務にまとまるため。新規ファイルは作らない)。
`GetClientRect(g_hWnd, ...)`で実際のクライアント矩形サイズを取得し、それを基準に0〜1へ正規化する
共通ヘルパー`GetNormalizedMousePosition()`を、`TryMouseToHex`と`ScreenToUISpace`の両方が使う
(`Game/system/system.h`の`g_hWnd`を利用するため、`CursorSelectionSystem.cpp`に
`#include "system/system.h"`を追加):

```cpp
// CursorSelectionSystem.h(private static)
static bool GetNormalizedMousePosition(float& outU, float& outV);

// CursorSelectionSystem.cpp
bool CursorSelectionSystem::GetNormalizedMousePosition(float& outU, float& outV)
{
    RECT clientRect;
    if (!GetClientRect(g_hWnd, &clientRect)) return false;
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;
    if (clientW <= 0 || clientH <= 0) return false;

    int mx = g_mouse->GetPositionX();
    int my = g_mouse->GetPositionY();
    if (mx < 0 || my < 0 || mx >= clientW || my >= clientH) return false; // クライアント領域外。

    outU = (float)mx / (float)clientW;
    outV = (float)my / (float)clientH;
    return true;
}

// TryMouseToHexは ndcX = u*2-1, ndcY = 1-v*2 として使う(既存のNDC変換ロジックはそのまま)。

// ScreenToUISpace(引数無し。常に現在のマウス位置を読む):
bool CursorSelectionSystem::ScreenToUISpace(Vector2& outUI)
{
    float u, v;
    if (!GetNormalizedMousePosition(u, v)) return false;
    outUI.x = u * (float)UI_SPACE_WIDTH - (float)UI_SPACE_WIDTH * 0.5f;
    outUI.y = (float)UI_SPACE_HEIGHT * 0.5f - v * (float)UI_SPACE_HEIGHT;
    return true;
}
```

`CursorSelectionSystem::TryMouseToHex`と同じ境界チェック(§0-1参照)。UI_SPACE==FRAME_BUFFERなので
NDC経由の変換は不要(3D盤面ピッキングとは別経路のまま、混同しない)。

### 1-2. `UIHotRegion` 共通データ構造

新規ファイル `Game/UIHotRegion.h`(ヘッダオンリー、`StatEffect.h`と同じ方針)。

```cpp
#pragma once
#include <string>

/// <summary>
/// マウスがクリック/ホバー可能なUI要素の種別。ツールチップ内容の分岐にも使う(フェーズ2)。
/// </summary>
enum class UIRegionKind
{
    ShopSlot,        // index = ショップ枠(0-4)
    BenchUnit,       // index = player.bench上のindex
    BoardUnit,       // hex = 盤面上のユニット位置
    BoardEmptyHex,   // hex = 盤面上の空マス(自陣のみ登録。配置/移動先として使う)
    UnclaimedItem,   // index = player.unclaimedItems上のindex
    TraitRow,        // index = TraitPanelUIRenderer::m_rows上のindex
    RerollButton,
    BuyXpButton,
    LockButton,
    NextPhaseButton, // Preparation→Combat(既存Bボタン相当)
    TitleStartButton,     // タイトル: セーブ無し時の「開始」/ セーブ有り時の「続きから」
    TitleNewGameButton,   // タイトル: セーブ有り時のみ「新規開始」
    RestartButton,        // GameOver/Victory: リスタート
    GoldDisplay, HudLevelDisplay, HudBoardCountDisplay, HudRoundDisplay, HudStreakDisplay, // 情報表示のみ(フェーズ2でツールチップだけ付く。クリック無反応)
};

/// <summary>
/// 1つのクリック/ホバー可能矩形。UI_SPACE座標系、左下原点でxy min/maxを持つ(pivotに依存しない
/// 絶対矩形にしておくことで、ヒットテスト側は各RendererのPivot流儀を意識しなくてよい)。
/// </summary>
struct UIHotRegion
{
    UIRegionKind kind = UIRegionKind::ShopSlot;
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    int index = -1;      // kindに応じた添字(ShopSlot/BenchUnit/UnclaimedItem/TraitRow)。無ければ-1。
    HexCoord hex;         // kindがBoardUnit/BoardEmptyHexのときのみ有効。

    bool Contains(const Vector2& uiPos) const
    {
        return uiPos.x >= minX && uiPos.x <= maxX && uiPos.y >= minY && uiPos.y <= maxY;
    }
};

using UIHotRegionList = std::vector<UIHotRegion>;
```

### 1-3. ヒット領域の構築タイミングと所有者(§0-4の結論)

- **各UI Rendererが、自分のレイアウト定数を使ってヒット領域を計算する新規メソッドを持つ**
  (例: `void ShopUIRenderer::BuildHotRegions(const std::vector<const UnitDef*>& shop, UIHotRegionList& out) const`)。
  `Draw()`/`OnRender2D()`とは独立した「描画を伴わない」純粋関数にする(Font/Sprite呼び出し無し)。
  既存の`kSlotStartX`等のレイアウト定数は同じ匿名名前空間から参照するだけなので、
  レイアウトを変えたら両方(Draw用/BuildHotRegions用)に効く=ズレが起きない。
- **`Game`が`UIHotRegionList m_hotRegions;`を1個所有し、`Game::Update()`の**先頭**(既存の
  カーソルクランプ処理より前)で毎フレーム`m_hotRegions.clear()`してから各Rendererの
  `BuildHotRegions()`を呼んで詰め直す**。これにより同一フレーム内で「最新の状態のヒット領域」を
  クリック判定に使える(§0-4の1フレーム遅延問題を回避)。
  `Render()`側の`Draw()`呼び出しは既存のまま変更しない(見た目のレイアウトは今まで通り
  Render()内で決まる。ヒット領域だけUpdate()で"先読み"計算する形)。
- 盤面(`BoardUnit`/`BoardEmptyHex`)のヒット領域は、`HexGridRenderer`のワールド座標→UI変換
  (`BoardUIRenderer::WorldToUI`と同種のVP行列射影)が要るため、`Game::Update()`側で
  `HexCoord`ごとに`HexGridRenderer::CalcTileCenter()` → ワールド座標 → `WorldToUI`相当の変換を
  行い、1マスあたり固定ピクセル半径(例: ヘックス1マス分、`HexGridRenderer::kHexSize`基準で
  実機調整)の矩形として登録する。`BoardUIRenderer::WorldToUI`は`private static`なので
  **`public static`に変更する**(小さな公開範囲変更、既存呼び出し元には影響なし)。

### 1-4. マウスのホバー/クリック解決

新規クラス `Game/UIInteractionSystem.h/.cpp`(`Game`が1個所有、`m_uiInteraction`)。

```cpp
class UIInteractionSystem
{
public:
    /// <summary>毎フレームGame::Update()の先頭、ヒット領域再構築の直後に呼ぶ。</summary>
    void Update(const UIHotRegionList& hotRegions);

    bool GetHovered(UIHotRegion& out) const;   // 現在マウスカーソル下の領域(無ければfalse)。
    bool GetLeftClicked(UIHotRegion& out) const;  // このフレーム左クリックで確定した領域。
    bool GetRightClicked(UIHotRegion& out) const; // このフレーム右クリックで確定した領域。

private:
    UIHotRegion m_hovered; bool m_hasHovered = false;
    // ...IsTrigger相当のエッジ検出用に前フレームの押下状態は g_mouse 自身が持つため、
    // ここでは「クリック時にどの領域の上にいたか」を確定するだけでよい。
};
```

- `Update()`内: `ScreenToUISpace`でマウス位置をUI_SPACE化 → `hotRegions`を**後ろから**線形探索し
  最初にヒットした領域を採用(後に登録された=描画順で手前にあるものを優先。フェーズ2以降で
  ツールチップが他要素の上に重なる場合の誤爆防止にもなる)。
  `g_mouse->IsTrigger(enMouseButtonLeft/Right)`でクリック確定。
- **これはフェーズ1時点では純粋な「今フレームどこがクリックされたか」の解決だけ**。
  実際に「ショップ購入」「装備」等の意味を持たせる分岐は、既存の`Game::Update()`内
  `if (g_pad[0]->IsTrigger(enButtonA)) {...}`ブロック群と**同じ場所に、同じロジックへ合流させる**
  (§2参照。新しい分岐ツリーを別建てしない)。

### 1-5. `Game.vcxproj` / `.filters`

新規追加ファイル(フィルタ`Core`、既存UI Renderer群と同じ扱い):

| ファイル | 区分 |
|---|---|
| `Game/UIHotRegion.h` | ClInclude のみ(ヘッダオンリー、`StatEffect.h`と同様vcxproj追加不要 ※) |
| `Game/UIInteractionSystem.h` / `.cpp` | ClInclude + ClCompile |

※ `UIHotRegion.h`は純データ構造なので、既存の`StatEffect.h`/`PassiveEffect.h`同様vcxprojに
列挙しなくてもビルドは通る(include連鎖で拾われる)。一貫性のため追加してもよいが必須ではない。

## 2. フェーズ1: マウス入力基盤(完全パリティ)

### 2-1. 入力対応表(既存ゲームパッド → 追加するマウス操作)

`Game::Update()`内、フェーズ別に現在使われている`g_pad[0]->IsTrigger(en*)`の全量(調査済み、
`Game.cpp`実際の行番号):

| フェーズ | ボタン | 現在の意味 | 行 |
|---|---|---|---|
| Title | A | 開始 / (セーブ有り時)続きから | L120, L142 |
| Title | X | (セーブ有り時)新規開始 | L136 |
| Preparation | A | 購入 / アイテムを持つ / 装備確定(フォーカスで分岐) | L211 |
| Preparation | B | 戦闘フェーズへ | L322 |
| Preparation | X | ベンチ→盤面配置 / 盤面内再配置(移動元選択・移動先確定) | L339 |
| Preparation | Y | ショップリロール(-2G) | L444 |
| Preparation | Start | ショップロック切り替え | L434 |
| Preparation | LB1 | ベンチユニット売却 / 盤面ユニットをベンチへ戻す | L481 |
| Preparation | RB1 | XP購入 | L543 |
| Preparation | (Tab/Select) | フォーカス巡回(Shop→Bench→Items→Board) ※`CursorSelectionSystem`内 | — |
| Combat | (無し) | 自動再生、入力不要 | — |
| Result | (無し) | 自動タイマーで次フェーズへ | — |
| GameOver/Victory | A | リスタート | L731 |

マウス操作の割り当て(intent.md §フェーズ1の指針を確定させたもの):

| マウス操作 | 対象 | 挙動 |
|---|---|---|
| 左クリック(ShopSlot) | ショップ枠 | **即時購入**(A相当、確認不要。ゴールド不足は既存の失敗フィードバックのみ) |
| 左クリック(UnclaimedItem) | 未装備アイテム | **手に持つ**(A相当、Itemsフォーカス時の挙動を流用) / 既に持っている同じ枠を再クリックで手放す |
| 左クリック(BenchUnit / BoardUnit) | 何も持っていない状態 | アイテムを持っていれば**装備確定**(A相当)。持っていなければ盤面配置/再配置の**「拾う」**(X相当、§2-2) |
| 左クリック(BoardEmptyHex) | 拾った物がある状態 | **配置/移動を確定**(X相当) |
| 左クリック(RerollButton等の画面ボタン) | 常設ボタン | 対応する既存アクションを実行(§2-3) |
| 右クリック | 何か持っている状態 | **キャンセル**(持ち物を離す。B相当ではなくキャンセル専用) |
| 右クリック(BenchUnit / BoardUnit) | 何も持っていない状態 | **売却/ベンチへ戻す確認**(2段階確認、§2-4) |
| ホイール | — | **対応しない**(§0-2、エンジンAPI無し・全一覧が画面内に収まるため不要と判断) |

### 2-2. 「拾う→置く」状態の一般化(設計判断)

既存の`m_heldUnclaimedIndex`(アイテムを手に持つ)と`m_heldBoardHex`(盤面内移動の移動元選択)は、
**どちらも「拾う→対象を選ぶ→確定/キャンセル」という同じ形のステートマシンを、ドメインごとに
別々のメンバ変数で実装している**。ベンチ→盤面配置には、ゲームパッドでは"2本の独立したカーソル
(一覧カーソル+ヘックスカーソル)を同時に構えて片方のボタンを押す"という、マウスの単一ポインタでは
再現しづらい方式が使われている(`X`ボタンが押された瞬間の両カーソル位置を同時に読む)。
intent.mdが要求する「ベンチのユニットを左クリックで掴む→盤面ヘックスを左クリックで配置」という
真の2ステップ操作は、既存の`m_heldBoardHex`(盤面→盤面専用)を流用せず、**マウス専用の新しい
「掴んでいるベンチユニット」状態を1つ追加する**。

`Game.h`に追加:
```cpp
// マウス操作専用: ベンチのユニットを左クリックで「掴んだ」状態。盤面の空きマスを左クリックすると
// 既存 Player::PlaceUnitOnBoard() を呼んで確定する(ゲームパッドXボタンの配置ロジックを流用、
// 新しい配置ルールは増やさない)。右クリックでキャンセル。フォーカス変更・戦闘突入で解除する
// (m_heldUnclaimedIndex/m_heldBoardHexValidと同じ寿命の考え方)。
int m_mouseHeldBenchIndex = -1;
```
盤面ユニットの拾い上げ(左クリックBoardUnit、何も持っていない状態)は**既存の`m_heldBoardHex`/
`m_heldBoardHexValid`をそのまま使う**(ドメインが同じなので新規状態を増やさない。ゲームパッドの
X連打と全く同じ状態を、マウスのクリックが更新するだけ)。

解決ロジック(擬似コード、`Game::Update()`の**A/Xボタン判定ブロックのすぐ後**に追加する新ブロック
としてまとめる。Preparationフェーズ限定):

```cpp
// --- マウス操作: 左クリック ---
UIHotRegion clicked;
if (m_uiInteraction.GetLeftClicked(clicked))
{
    bool holdingItem = (m_heldUnclaimedIndex >= 0 && m_heldUnclaimedIndex < (int)player.unclaimedItems.size());
    bool holdingBoardUnit = m_heldBoardHexValid;
    bool holdingBenchUnit = (m_mouseHeldBenchIndex >= 0 && m_mouseHeldBenchIndex < (int)player.bench.size());

    switch (clicked.kind)
    {
    case UIRegionKind::ShopSlot:
        // 既存A(ショップフォーカス)ロジックをそのまま呼ぶ(clicked.index をshopIndexとして使用)。
        break;
    case UIRegionKind::UnclaimedItem:
        // 既存A(Itemsフォーカス)ロジックをそのまま呼ぶ。
        break;
    case UIRegionKind::BenchUnit:
        if (holdingItem) { /* 既存A(Bench+アイテム保持)の装備ロジックをそのまま呼ぶ */ }
        else if (!holdingBenchUnit && !holdingBoardUnit) { m_mouseHeldBenchIndex = clicked.index; /* フィードバック表示 */ }
        break;
    case UIRegionKind::BoardUnit:
        if (holdingItem) { /* 既存A(Board+アイテム保持)の装備ロジックをそのまま呼ぶ */ }
        else if (!holdingBenchUnit && !holdingBoardUnit) { m_heldBoardHex = clicked.hex; m_heldBoardHexValid = true; }
        break;
    case UIRegionKind::BoardEmptyHex:
        if (holdingBenchUnit) { player.PlaceUnitOnBoard(m_mouseHeldBenchIndex, clicked.hex); m_mouseHeldBenchIndex = -1; }
        else if (holdingBoardUnit) { player.MoveUnitOnBoard(m_heldBoardHex, clicked.hex); m_heldBoardHexValid = false; }
        break;
    case UIRegionKind::RerollButton:    /* 既存Yロジックをそのまま呼ぶ */ break;
    case UIRegionKind::BuyXpButton:     /* 既存RB1ロジックをそのまま呼ぶ */ break;
    case UIRegionKind::LockButton:      /* 既存Startロジックをそのまま呼ぶ */ break;
    case UIRegionKind::NextPhaseButton: /* 既存Bロジックをそのまま呼ぶ */ break;
    default: break;
    }
}
```

**[実装で確定した最終方針・レビュー承認済み]** 上の段落で最初に検討した「`focus`をマウスクリックから
合成し、既存A/X/LB1ブロックの本体(if/else)へそのまま流し込む」案は、**実装時に具体的な破綻ケースが
見つかったため採用しなかった**。既存Aボタンは「Bench/Boardフォーカスだが何も持っていない」場合に
定義された挙動を持たず、`else`(ショップ購入)へ落ちて`shopIndex=0`となり**shop[0]を誤購入する**
(既存のゲームパッドの挙動・今回は変更しない、後述)。マウスでベンチ/盤面ユニットをクリックする
本来の意図(何も持っていなければ配置のために「掴む」)とは相容れないため、focus合成では正しく
動作しない。

代わりに、**A/X/LB1に相当するマウス操作(左クリック/右クリック)は、`UIHotRegion::kind`ごとの
専用switch文として`Game::Update()`に独立して実装し、そこから既存のドメインロジックの
エントリポイント(`Player::BuyUnit` / `ItemSystem::GiveItem` / `Player::PlaceUnitOnBoard` /
`Player::MoveUnitOnBoard` / `Player::SellUnitFromBench` / `Player::ReturnUnitToBench`)を直接呼ぶ**。
「二重実装を作らない」という制約は、購入可否・装備枠の空き・配置ルール等の**ドメインロジック**
(`Player`/`ItemSystem`側)に適用され、これらは常に唯一のエントリポイントを両経路(パッド/マウス)が
呼ぶ。一方「今の操作対象が何か」を解決する**入力glue**は、パッドの暗黙カーソル前提とマウスの
明示クリック対象という前提の違いから、ドメインごとに別実装になるのが自然という判断
(レビュー確認済み)。

- **実際に抽出したエントリポイント**: `Player::FindBoardUnitAt(const HexCoord&)`の**非constオーバーロード**
  (`Game/Player.h`)。既存Aボタンの「Board+アイテム保持」時の対象解決はインラインの
  `for (auto& unit : player.board) if (unit.position==hex) {...}`だったため、これを非const
  `FindBoardUnitAt`へ抽出し、**ゲームパッド経路もマウス経路も同じメソッドを呼ぶ**よう書き換えた
  (`Game.cpp`のAボタンブロック内、旧インラインループを1行の呼び出しへ置換)。
- Y/Start/RB1/B/Title(A/X)/GameOver・Victory(A)は、フォーカスに依存しない単純な発火判定のため、
  `g_pad[0]->IsTrigger(en*) || mouseClicked*` の単純ORで正規化できた(当初案どおり)。
- **既知の既存バグ・今回は未修正**: 上記のゲームパッドAボタンの「Bench/Boardフォーカス+何も
  持っていない時にshop[0]を誤購入する」挙動は、本タスクで見つかったが**ゲームパッドの挙動を
  変えるのはスコープ外**のため修正しない。マウス側は新設の専用switchで正しい挙動(掴む)になる。
- マウス専用の「掴んでいるベンチユニット」インデックス(`m_mouseHeldBenchIndex`)に加えて、
  **盤面ユニットの拾い上げは既存の`m_heldBoardHex`/`m_heldBoardHexValid`をマウスからも
  そのまま更新する**(ドメインが同じため新規状態を増やさない、当初案どおり)。ただし
  `m_cursorSelection.GetFocus() != Board`で自動解除する既存の掃除ロジック(ゲームパッド用)は、
  マウス操作では`m_cursorSelection`のfocusがTab操作なしに変化しない(=常にBoard以外のままになりうる)
  ため、マウス発の拾い上げに適用すると**拾った直後のフレームで即座に解除されてしまう新規バグ**を
  実装中に発見・修正した。`bool m_heldBoardHexFromMouse`を追加し、マウス発(true)の場合は
  この自動解除をスキップする(ゲームパッド発(false)は従来通り)。

### 2-3. 画面上の常設クリックボタン

新規に矩形+ラベルを描画する4つのボタン(Preparationフェーズ中、`ShopUIRenderer`に追加)。
現在ヘッダー行のテキスト`"[Y] Reroll -2G [RB1] BuyXP -4G [Start] Lock"`と同じ情報を、
実際にクリックできる矩形として`kHeaderY`行の**右側**(5枚目のショップ枠の右、x>560+180)に
横並びで配置する(3D盤面・他HUDと重ならない位置。正確な座標は実機調整、目安:
`kButtonStartX = 620.0f, kButtonStepX = 150.0f, kButtonY = kHeaderY, kButtonSize = (130, 30)`)。

- `[Reroll -2G]` `[BuyXP -4G]` `[Lock]`(ロック中はハイライト) を`ShopUIRenderer`に追加。
- `[次の戦闘へ ▶]`(NextPhaseButton、Bボタン相当)は独立ボタンとして`ShopUIRenderer`右下、
  または新規に`BoardUIRenderer`のベンチ一覧付近ではなく**画面右下**(他要素と衝突しない
  空き領域、目安 `x=780, y=-420`)に配置する。
- Title画面: `TitleUIRenderer`の`"PRESS [A] TO START"`/`"[A] CONTINUE [X] NEW GAME"`の
  テキスト領域をそのままクリック可能にする(新規の矩形描画は必須ではないが、intent.mdの
  「クリック可能ボタンを`UIRectRenderer`+`Font`で常設」に合わせ、うっすら枠を敷いてもよい
  →実質フェーズ3のカード化を先取りしても構わない、必須ではない)。
- GameOver/Victory: `"PRESS [A] TO TITLE"`のテキスト領域を同様にクリック可能にする。

いずれも`BuildHotRegions()`で`UIRegionKind::TitleStartButton`等として登録し、`Game::Update()`の
Title/GameOver/Victoryフェーズの既存A/Xボタン判定に、マウスクリックの合流分岐を追加する
(Preparationと同じ「二重実装を作らない」方針)。

### 2-4. 破壊的操作の確認(売却・ベンチへ戻す)

新規状態を`Game.h`に追加:
```cpp
// 右クリックでの売却/ベンチ戻しの2段階確認用。1回目の右クリックで「確認待ち」にし、
// kSellConfirmWindowSec 以内に同じ対象へもう一度右クリックすると確定する。
// タイムアウト・別対象へのクリック・フォーカス変更で自動的に解除する。
UIHotRegion m_pendingSellTarget; bool m_hasPendingSellTarget = false;
float m_sellConfirmTimer = 0.0f;
static constexpr float kSellConfirmWindowSec = 3.0f; // ShopUIRendererのkFeedbackDurationと合わせる。
```
- 1回目の右クリック(BenchUnit/BoardUnit、何も持っていない状態): `m_pendingSellTarget`に記録、
  `m_sellConfirmTimer = kSellConfirmWindowSec`、フィードバック「もう一度右クリックで売却確定」。
- `Game::Update()`毎フレーム、`m_sellConfirmTimer`を減算し0以下で`m_hasPendingSellTarget = false`。
- 2回目の右クリックが**同じ対象**(index/hex一致)なら、既存のLB1ロジック(売却/ベンチ戻し)を
  そのまま呼ぶ。**別の対象**への右クリックなら、新しい対象を1回目として上書き(連続操作を
  妨げない)。
- ゲームパッドのLB1は既存通り即時実行のまま変更しない(intent.mdは「マウスの破壊的操作に確認」
  としており、パッド操作の変更は求めていない)。

### 2-5. 変更ファイル一覧(フェーズ1)

| ファイル | 変更内容 |
|---|---|
| `Game/UIHotRegion.h`(新規) | §1-2 |
| `Game/UIInteractionSystem.h/.cpp`(新規) | §1-4 |
| `Game/CursorSelectionSystem.h/.cpp` | `ScreenToUISpace()`を追加(§1-1)。`TryMouseToHex`と共存、盤面ヘックスの判定は変更しない |
| `Game/ShopUIRenderer.h/.cpp` | `BuildHotRegions()`追加。Reroll/BuyXP/Lock/NextPhaseの4ボタン矩形描画+登録追加 |
| `Game/BoardUIRenderer.h/.cpp` | `BuildHotRegions()`追加(ベンチ一覧)。`WorldToUI`を`public static`に変更 |
| `Game/ItemInventoryUIRenderer.h/.cpp` | `BuildHotRegions()`追加 |
| `Game/TitleUIRenderer.h/.cpp` | `BuildHotRegions()`追加(開始/続きから/新規開始ボタン) |
| `Game/ResultUIRenderer.h/.cpp` | `BuildHotRegions()`追加(リスタートボタン) |
| `Game/Game.h` | `m_mouseHeldBenchIndex`, `m_pendingSellTarget`系, `m_hotRegions`, `m_uiInteraction`メンバ追加 |
| `Game/Game.cpp` | `Update()`先頭でヒット領域再構築。各フェーズの既存ボタン判定にマウス合流分岐を追加(§2-2〜2-4) |
| `Game/HexGridRenderer.h` | 盤面ヘックス→UI射影のため、`CalcTileCenter`は既にpublic staticで利用可(変更不要) |

## 3. フェーズ2: ヒットテスト + ツールチップ

フェーズ1で§1のヒット領域基盤とクリック解決は既に用意済み。フェーズ2は
**(a) ホバー検出をツールチップ表示に使う** **(b) 各`UIRegionKind`の詳細内容を文章化する**
**(c) ツールチップ自体をカードとして描画する** の3点を追加する。

### 3-1. ホバー→ツールチップのトリガー

- `UIInteractionSystem::GetHovered()`が返す領域が**同じまま`kHoverDelaySec`(0.3秒)続いたら**
  ツールチップを表示する(intent.md「~0.2〜0.4秒」)。`Game`に
  `UIHotRegion m_hoverCandidate; float m_hoverTimer = 0.0f; bool m_tooltipVisible = false;`を追加し
  `Update()`内で管理する(領域が変わったらタイマーリセット)。
- **ゲームパッドでフォーカス中の要素にも同じツールチップを出す**(intent.md要求)。
  `m_cursorSelection`の`GetFocus()`/`GetListCursorIndex()`/`GetHexCursor()`から
  対応する`UIHotRegion`(§1-2の構造体)を**逆引き**する小さなヘルパーを`Game::Update()`に追加する
  (`m_hotRegions`から`kind`と`index`/`hex`が一致するものを線形探索するだけ、新規データ構造不要)。
  この場合はホバー遅延を適用しない(パッドはカーソル移動そのものが明示操作のため即表示でよい)。

### 3-2. ツールチップ内容(`UIRegionKind`ごと)

新規ファイル `Game/TooltipContentBuilder.h/.cpp`(`UIRegionKind`+関連データから表示用テキスト行の
`std::vector<std::wstring>`を組み立てる。既存の`ItemInventoryUIRenderer.cpp`の`EffectShortText`/
`ShopUIRenderer.cpp`の`TraitName`と同じ匿名名前空間ヘルパー群のパターンを1ファイルに集約する形)。

- **ShopSlot / BenchUnit / BoardUnit(ユニット系、`UnitDef`+`UnitInstance`があれば併せて)**:
  - タイトル行: 名前(+盤面/ベンチなら`*N`星表記)。
  - `HP{baseHP} AT{baseAttack} AP{magicPower} 物防{physicalDefense} 魔防{magicDefense}`
  - `攻撃速度{attackSpeed}/s  射程{attackRange}(必殺{skillRange})`
  - スキル説明(§3-2-1、`skillType`から合成)。
  - トレイト(フル表示、`TraitName()`を`ShopUIRenderer.cpp`から`Game/TraitNameUtil.h`的な共有ヘルパーへ
    切り出す。現状`ShopUIRenderer.cpp`にしか無いため重複実装を避けるため共通化する)。
  - (盤面/ベンチのユニットのみ)適用中ボーナス: `bonusAttack`等が0でないものだけ列挙
    (例: `+AT 15 (アイテム/トレイト込み)`)。装備アイテム名一覧。
  - 末尾: `ShopSlot`→`"クリックで購入 (-{cost}G)"`(ゴールド不足なら赤字で「ゴールド不足」)。
    `BenchUnit`/`BoardUnit`→`"クリックで選択/移動  右クリックで売却 (+{sellValue}G)"`。
- **UnclaimedItem**:
  - タイトル: アイテム名。
  - 効果: `EffectShortText`の集合(既存`ItemInventoryUIRenderer.cpp`のロジックを流用、
    フル表記に変える(短縮しない)。パッシブがあれば火傷の効果文(ticks/magnitude/interval)も出す。
  - 素材(`ItemCategory::Component`)の場合のみ: 「この素材でできる完成品」を
    `ItemDatabase::GetAllRecipes()`(§0-6で追加)から`componentA/B`が一致するレシピを列挙し、
    結果アイテム名+相手の素材名を表示(例: `+ VitalCrystal → BerserkersAxe`)。
  - 末尾: `"クリックで手に持つ→ユニットをクリックで装備"`。
- **TraitRow**:
  - タイトル: トレイト名 + 現在数。
  - 各段階: `requiredCount`と`effects`(`EffectShortText`流用)を段階ごとに列挙、発動中の段階を強調。
  - 該当ユニット一覧: `UnitDatabase::GetAllUnitDefs()`を`def->traits`でフィルタし名前を列挙
    (§0-7)。
  - 発動中かどうかを色でも表現(タイトル自体を`kActiveColor`/`kInactiveColor`流用)。
- **RerollButton/BuyXpButton/LockButton/NextPhaseButton等のボタン系**:
  - 1行のみ: ボタンの意味 + 効果(例: `"クリックでショップをリロール (-2G)"`)。
- **GoldDisplay/HudLevelDisplay/HudBoardCountDisplay/HudRoundDisplay/HudStreakDisplay(情報表示のみ)**:
  - 1〜2行、数値の意味の説明のみ(intent.md要求通りクリック動作は書かない)。

#### 3-2-1. スキル説明の文章合成(§0-5)

```cpp
// UnitDef::skillType 等から必殺技の説明文を1行合成する(自由記述フィールドが無いため)。
std::wstring BuildSkillDescriptionText(const UnitDef& def)
{
    switch (def.skillType)
    {
    case SkillEffectType::Damage:
        return L"必殺技: 単体に大ダメージ";
    case SkillEffectType::AreaDamage:
        // 例: "必殺技: 範囲ダメージ(半径1, 周囲へ50%)"
        return Format(L"必殺技: 範囲ダメージ(半径%d, 周囲へ%.0f%%)", def.skillSplashRadius, def.skillSplashPercent);
    case SkillEffectType::DamageAndHeal:
        return Format(L"必殺技: ダメージ+自己回復(与ダメージの%.0f%%)", def.skillHealPercent);
    case SkillEffectType::DamageAndShield:
        return Format(L"必殺技: ダメージ+自身にシールド%d", def.skillShieldAmount);
    default:
        return L"必殺技: 不明";
    }
}
```
(`Format`は`swprintf_s`+バッファの簡易ラッパー、既存コードに倣いその場で`swprintf_s`を直書きしてもよい)

### 3-3. ツールチップの描画

新規ファイル `Game/TooltipUIRenderer.h/.cpp`(`IRenderer`、既存UI Rendererと同じ骨格)。

- `Game::Update()`末尾付近(全てのフェーズ判定の後、Preparationに限らずTitle/GameOver等でも
  ボタンツールチップは出したいため)で、`m_tooltipVisible`なら
  `m_tooltipUI.Draw(rc, tooltipContent, cursorUIPos)`を呼ぶ。
- 描画: `UIRectRenderer`で背景パネル(半透明濃灰 `Vector4(0.05,0.05,0.07,0.92)`) + 枠
  (背景より一回り大きい矩形を先に薄い縁色で描き、内側に本体を重ねる「2枚重ね」方式、
  intent.mdフェーズ3の枠表現と同じ手法をここで先取りする)。サイズは行数×行高+パディングで
  計算(`kLineHeight=26, kPaddingX=16, kPaddingY=12`目安、幅は最長行の**文字数**から概算
  ─ MeasureStringが無い制約(§0-3)にここでも従う。全角/半角混在は「全角は2文字分」で概算)。
- 位置: カーソル位置から右下に`(20, -20)`オフセット。**画面端クランプ**必須
  (`tooltipRight = cursorX + 20 + width; if (tooltipRight > UI_SPACE_WIDTH*0.5f) 左側に出す`、
  同様に上下もクランプ)。intent.md受け入れ条件の「画面外にはみ出さない」に対応。
- 文字色はセクションごとに使い分けてよいが、必須ではない(通常テキスト色1色でも受け入れ条件は
  満たす。可読性重視ならタイトル行だけ強調色)。

### 3-4. 変更/追加ファイル一覧(フェーズ2)

| ファイル | 変更内容 |
|---|---|
| `Game/TooltipContentBuilder.h/.cpp`(新規) | §3-2 |
| `Game/TooltipUIRenderer.h/.cpp`(新規) | §3-3 |
| `Game/ItemDatabase.h` | `GetAllRecipes()`アクセサ追加(§0-6) |
| `Game/ItemInventoryUIRenderer.cpp` | `EffectShortText`を`Game/StatEffectTextUtil.h`等へ切り出し、
  `TooltipContentBuilder`と共用(重複実装回避) |
| `Game/ShopUIRenderer.cpp` | `TraitName()`を同様に共有ヘルパーへ切り出し |
| `Game.h/.cpp` | ホバー状態管理・ゲームパッドフォーカス逆引き・`m_tooltipUI`追加 |

## 4. フェーズ3: 全UIレンダラーのカード化

### 4-1. カード表現の共通パターン

全レンダラーで統一する「背景パネル+枠」の描画パターン(§3-3のツールチップと同じ2枚重ね方式を
再利用):

```cpp
void DrawCardPanel(RenderContext& rc, UIRectRenderer& r, const Vector2& pos, const Vector2& size,
    const Vector4& fillColor, const Vector4& borderColor, float borderThickness, const Vector2& pivot)
{
    // 枠(一回り大きい矩形、先に描く) → 内側の塗り(あとから描く、こちらが手前に見える)。
    Vector2 borderSize = size + Vector2(borderThickness * 2.0f, borderThickness * 2.0f);
    r.DrawRect(rc, pos, borderSize, borderColor, pivot);
    r.DrawRect(rc, pos, size, fillColor, pivot);
}
```
共通ヘルパーとして`UIRectRenderer`に`DrawPanel(...)`を追加するか、各Rendererの匿名名前空間に
複製するかは実装フェーズ判断でよい(処理は2行なので複製でも許容範囲。ただし色定数
`kPanelFillColor`/`kPanelBorderColor`は全Renderer共通にし`UIRectRenderer.h`か新規
`Game/UIStyle.h`に集約することを推奨=統一感のため)。

### 4-2. レンダラーごとの適用

- **`ShopUIRenderer`**: 5枠それぞれに個別カード(`slotX`中心、幅`kSlotStepX-20`程度)。
  枠色を`CostTierColor(slot.cost)`で染める(既存関数そのまま流用)。選択枠は枠を太く/明るく。
  ホバー中枠(フェーズ2のホバー状態を流用)も同様にハイライト。
- **`BoardUIRenderer`**: ベンチ一覧の各行を個別カード(縦並び、`kBenchStepY`間隔に合わせて
  高さ調整)。戦闘中の頭上バーは、背景矩形(既存`kBarBgColor`)を「枠付き」に格上げする程度で
  十分(小カード化、過剰装飾はしない─3Dシーン上のオーバーレイなので情報量を増やしすぎない)。
- **`PlayerStatusUIRenderer` / `RoundRecordUIRenderer`**: intent.md/研究§12が指摘する
  「右上HUDの居場所」を1枚のカードパネルに統合する(§4-3のレイアウト再設計と併せて実施)。
- **`ItemInventoryUIRenderer`**: 各アイテム行をカードリスト化(`ShopUIRenderer`の5枠と同じ
  縦版パターン)。
- **`TraitPanelUIRenderer`**: パネル全体を1枚の背景で囲み、行ごとに薄い区切り線(1px相当の
  細い矩形)。発動中/未発動の色分けは既存のまま。
- **`TitleUIRenderer` / `ResultUIRenderer`**: タイトル文字・結果文言の背後に大きめの
  カードパネルを1枚敷く(必須ではないが統一感のため推奨)。

### 4-3. HUDレイアウト全体の再設計(§0でのBENCH/TRAITS衝突の実測)

現状の左側カラム(x≈-910)の縦積みを実測すると:

- `BoardUIRenderer`のBENCHは`kBenchTopY=250`から`kBenchStepY=40`刻みで下へ伸びる
  (ベンチ`N`件なら下端 `y ≈ 250 - 40*(N+1)`)。
- `TraitPanelUIRenderer`は`kTopY=-70`から`kStepY=24`刻みで下へ伸びる(トレイト8種+タイトルで
  下端 `y ≈ -70 - 24*9 = -286`)。
- ベンチが**7件以上**になると下端が`y ≈ -30`以下まで下がり、TraitPanelの開始位置`y=-70`との
  間隔が詰まる(8件で`y=-70`とほぼ同値=衝突)。ベンチ上限が無い(§3 in 研究doc既知の指摘)ため、
  理論上は簡単に衝突しうる。

**再設計方針**: カード化(4-1)で各パネルに背景枠が付く前提で、
1. **BENCHパネルの表示行数に上限を設ける**(例: 最大8行表示、それ以上は「+N件」の1行に集約する
  か、パネル自体をスクロール無しで固定高にしてカード内で詰めて表示する)。ベンチ枚数上限
  そのものの実装(研究§3の別改善案)は本タスクのスコープ外だが、**表示上のクランプ**は
  カード化と同時にやらないとレイアウトが破綻するため本タスクに含める。
  → `BoardUIRenderer`の`kBenchMaxVisibleRows = 8`を新設、超過時は末尾に`"...+{N}件"`。
2. **TraitPanelの開始Yをベンチパネルの固定下端(クランプ後なので予測可能)基準に変更**する。
  例: `kBenchPanelBottomY = 250 - 40*(kBenchMaxVisibleRows+1);`を`BoardUIRenderer`側の
  `public static constexpr`として公開し、`TraitPanelUIRenderer`の`kTopY`をそこから
  さらに余白を空けた位置(`kBenchPanelBottomY - 40.0f`程度)に変更する。
  (循環includeを避けるため、値そのものをコメント付きで再掲する既存パターン
  ─`Player::kAllyZoneMinQ`等─を踏襲してもよいし、`public static`定数を直接参照してもよい。
  後者の方が値のズレが起きないため推奨)。
3. 右上HUD(`PlayerStatusUIRenderer`+`RoundRecordUIRenderer`+`ItemInventoryUIRenderer`)は
  現状既に縦積みで住み分け済み(y≒500→388→270→…)。カード化で枠が付いても相対位置は
  変えなくてよいが、**枠の厚み分だけ隣接パネルとの間隔を広げる**微調整が要る(実機調整)。
- ツールチップ(フェーズ2)は最前面に描く(`Game::Render()`内で全UI Rendererの`AddRenderObject`
  より後に`m_tooltipUI`を登録する順序を維持すれば、既存の描画順管理の範囲で解決する)。
  HUDと重なって読めなくなる問題は、§3-3の画面端クランプに加え、**ツールチップ自体が
  対象要素をなるべく隠さない位置(右下オフセット)に出る設計(§3-3)で基本的に回避**する。

### 4-4. `kPrewarmCount`の見直し

`UIRectRenderer`の現在の事前確保数160(`docs/tasks/ui-sprite-bars/plan.md`より)に対し、
フェーズ3で1フレームに描く矩形の見積もり:

| 要素 | 矩形数/個 | 個数 | 小計 |
|---|---|---|---|
| ショップ5枠カード(枠+塗り) | 2 | 5 | 10 |
| ベンチ最大8行カード | 2 | 8 | 16 |
| アイテム一覧(未装備、最大9想定) | 2 | 9 | 18 |
| トレイトパネル(背景+行区切り8) | 2+8 | 1 | 10 |
| 右上HUDパネル(GOLD/LV/XPバー/ROUND) | 2×3枚+XPバー2 | — | 8 |
| 戦闘中HPバー(背景+HP+シールド+ゲージ背景+ゲージ前景) | 5 | 最大18体(9v9) | 90 |
| ツールチップ(枠+塗り) | 2 | 1 | 2 |
| Result/GameOver暗幕 | 1 | 1 | 1 |
| **合計(目安、戦闘中×準備中は排他だが最大値で見積もる)** | | | **約155** |

現行160はギリギリなので、**`kPrewarmCount`を256程度へ引き上げる**ことを推奨する
(`UIRectRenderer::Init()`内の事前確保数値、`UIRectRenderer.cpp`)。プールは「足りなければ
`AcquireSprite()`が動的に1個生成して足す」設計(§0 in ui-sprite-bars/plan.md)のため超過しても
クラッシュはしないが、フレーム中の動的生成はコストなので事前確保を増やす方が望ましい。

## 5. 既知の制約への対応(既存と同じ・変更なし)

- `Font`のMeasureString無し・pivot実質左詰めの制約は変わらず残る。ツールチップ幅・カード幅は
  すべて「文字数×概算幅」方式で計算する(§3-3)。
- `g_camera2D`は書き換えない(ui-sprite-bars既知の回帰実績)。
- `Sprite`矩形は`Font::Begin()`〜`End()`の外側(前)でまとめて描く制約は全フェーズで維持。
- ソースはUTF-8(BOM付き)。`docs/tasks/ui-mouse-cards/`のMarkdownはBOM無しでよい。
- `Game/Assets/modelData/*.dds`のLFS由来phantom modifiedはcommit/discardしない。

## 6. 未解決の懸念点・実装フェーズで確認すべきこと

1. **`UIHotRegion`の盤面ヘックス→UI矩形サイズ(§6-1)** [フェーズ1実機検証で解決・採用方式決定]:
   当初の等方形(正方形、半径45px固定)は自陣9マス全部にマウス配置できることは確認できたが、
   透視射影のためr(奥行き)方向にヒット矩形が大きく重なることが実機検証で判明した。
   `CalcTileCenter`±隣接マスの中点から動的に算出する完全な方式(当初案)ではなく、
   **異方性の固定ボックス**(X方向半幅35px、Y方向半高16px。奥行き方向は画面上で詰まって
   見えるぶん小さくする)を採用した(`Game::Update()`の盤面ヒット領域構築ブロック、
   `kHexHitHalfWidth`/`kHexHitHalfHeight`)。動的算出より簡易な方式で、レビューでは
   「簡易案として許容、要すれば動的算出方式へ」との判断(承認済み)。**この数値自体はまだ
   実機検証していない**(旧・等方形45px版でF5合格した後の変更のため)。フェーズ2着手時に
   併せて実機確認し、ズレが残るようなら隣接マス中点からの動的算出方式へ切り替える。
2. **右クリックの「持ち物を離す」と「売却確認」の判定順序**: 何かを持っている状態で
   BenchUnit/BoardUnitを右クリックした場合、「キャンセル」と「売却確認」のどちらを優先するか
   は§2-1の表で「何か持っている状態→キャンセル優先」としたが、実機でUXとして自然か確認する
   (「持っている物を離してから、対象を売る」という意図の右クリックと衝突しうる。気になる場合は
   売却確認を「何も持っていない状態限定」で厳密化し、持っている間の右クリックは常にキャンセル、
   という現行案のまま進めればよい)。
3. **ドラッグ操作**(intent.md「可能ならドラッグも併用」)は本plan.mdでは扱っていない
   (クリック2ステップのみ設計)。`IsPress`(押しっぱなし判定)は既に`Mouse`にあるため技術的には
   可能だが、実装コストと相談で任意対応とする(intent.mdスコープでも「できれば可、必須でない」)。
4. **`UnitDatabase::GetAllUnitDefs()`をフェーズ2のツールチップから直接参照するための経路**:
   `Game`は`m_unitDatabase`を持つので`TooltipContentBuilder`へ参照渡しすればよいが、
   関数シグネチャが増える(既存`ItemDatabase`/`TraitDatabase`同様に引数で渡す設計に統一する)。
5. **フェーズ1だけでビルド・実機確認するタイミング**: intent.mdの通りフェーズごとに別コミットで
   積み、都度レビューを挟む。フェーズ1完了時点で「マウスのみで1ゲーム完走」を実機確認できる
   (フェーズ2/3のツールチップ・カード化が無くても機能上は完結するため、受け入れ条件の核心部分は
   フェーズ1で先に検証可能)。

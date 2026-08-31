# ショップのロック機能 — 技術設計

## 変更ファイル

| ファイル | 変更概要 |
|---|---|
| `Game/Game.h` | `bool m_shopLocked = false;` を `m_currentShop` の近くに追加。 |
| `Game/Game.cpp` | (1) `enButtonStart` でロックをトグルするハンドラを追加 (2) `enButtonB` ハンドラで `m_shopLocked` 時は `m_currentShop.clear()` をスキップ (3) `InitializeNewRun()` と セーブロード成功時に `m_shopLocked = false` (4) `m_shopUI.Draw(...)` に `m_shopLocked` を渡す。 |
| `Game/ShopUIRenderer.h` | `Draw()` に `bool shopLocked` 引数を追加。`bool m_shopLocked = false;` メンバを追加。 |
| `Game/ShopUIRenderer.cpp` | `Draw()` で `m_shopLocked` を保存。`OnRender2D()` のヘッダー行に `[LOCKED]` と `[Start] Lock` を追加し、ロック中はヘッダーを金色で描画。 |

## 詳細

### Game.h
```cpp
std::vector<const UnitDef*> m_currentShop;
bool m_shopLocked = false; // ショップのロック。true の間はラウンドを跨いでも m_currentShop を再抽選しない。
```

### Game.cpp

**(1) ロックトグル(`enButtonY` ハンドラの直前あたりに追加)**
```cpp
// Startボタンでショップのロックをトグルする。
// ロック中はラウンドを跨いでもショップの5枠が維持される(下の enButtonB での自動クリアをスキップ)。
// 手動リロール(Y)はロック中も可能で、その結果が新たなロック対象になる。
if (g_pad[0]->IsTrigger(enButtonStart))
{
    m_shopLocked = !m_shopLocked;
    m_shopUI.PushFeedback(
        m_shopLocked ? L"ショップをロックしました (ラウンドを跨いで維持)" : L"ショップのロックを解除しました",
        ShopUIRenderer::FeedbackLevel::Info);
    OutputDebugString(m_shopLocked ? L"[Shop] locked\n" : L"[Shop] unlocked\n");
}
```

**(2) `enButtonB` ハンドラ**
```cpp
if (g_pad[0]->IsTrigger(enButtonB))
{
    if (!m_shopLocked)
    {
        m_currentShop.clear(); // ロック中は維持。次の準備フェーズで空でないため自動リロールされない。
    }
    m_heldUnclaimedIndex = -1;
    m_gameState.currentPhase = Phase::Combat;
}
```

**(3) リセット箇所**
- `InitializeNewRun()`: `m_currentShop.clear();` の直後に `m_shopLocked = false;`
- セーブロード成功ブロック: `m_currentShop.clear();` の直後に `m_shopLocked = false;`

**(4) 描画呼び出し** — `m_shopUI.Draw(...)` の引数末尾に `m_shopLocked` を追加。

### ShopUIRenderer.h
`Draw()` シグネチャの末尾に `bool shopLocked` を追加。`private` に `bool m_shopLocked = false;`。

### ShopUIRenderer.cpp
- `Draw()`: `m_shopFocused = shopFocused;` の近くで `m_shopLocked = shopLocked;`
- `OnRender2D()` ヘッダー行:
```cpp
wchar_t buf[224];
swprintf_s(buf,
    L"SHOP%ls   Gold %d   Lv %d (XP %d/%d)   [Y] Reroll -%dG   [RB1] BuyXP -%dG   [Start] Lock",
    m_shopLocked ? L" [LOCKED]" : L"",
    m_gold, m_level, m_xp, m_xpForNextLevel, m_rerollCost, m_buyXpCost);
Vector4 headerColor = m_shopLocked
    ? Vector4(1.00f, 0.80f, 0.25f, 1.0f)  // ロック中は金色で目立たせる。
    : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
m_font.Draw(buf, Vector2(kLeftX, kHeaderY), headerColor, 0.0f, kHeaderScale, kTopLeftPivot);
```

## 確認観点(レビュー/デバッグ)

- `plan.md` と実装の一致。
- ロックON→ラウンド跨ぎで `--- Shop (Level ...) ---` の再抽選ログが**出ない**こと、5枠の内容が同一であること。
- ロックOFF→ラウンド跨ぎで再抽選ログが出て内容が変わること。
- ロック中のYリロールが機能すること。
- `enButtonStart`(Enter)が準備フェーズの他操作と衝突しないこと。

## 既知の制約 / 補足

- `enButtonStart` のキーボードfallbackは Enter。準備フェーズでは Enter は未使用のため衝突しない(A相当は J / マウス左 / Space系は Select=フォーカス切替)。
- ロック状態はセーブされない。ロード時は必ず解除状態。
- ロック中に枠のユニットを購入しても枠は減らない(既存の `BuyUnit` は `m_currentShop` を変更しない)。本タスクでは変更しない。

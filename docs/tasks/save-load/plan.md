# 設計: セーブ/ロード機能

## 追加ファイル

| ファイル | 役割 |
|---|---|
| `Game/SaveSystem.h` / `.cpp` | ステートレスなサービス。`GameState` のテキスト保存/復元、セーブ有無判定。 |

`Game/Game.vcxproj` と `Game/Game.vcxproj.filters` に上記2ファイルを追加(フィルタ `Core`)。
`.gitignore` に `savedata.txt` を追加。

## `SaveSystem` の API(すべて `const` メソッド、状態を持たない)

```cpp
class SaveSystem {
public:
    static constexpr const char* kSaveFilePath = "savedata.txt";
    bool SaveFileExists() const;
    bool Save(const GameState& state) const;
    bool Load(GameState& state, const UnitDatabase& unitDb, const ItemDatabase& itemDb) const;
};
```

- `Save`: `players[0]` と `roundNumber` / `lossCount` を `kSaveFilePath` へ書き出す。成功で true。
- `Load`: ファイルを読み、`state.players` を作り直して `players[0]` を復元、`roundNumber` / `lossCount` を復元、
  `state.currentPhase = Phase::Preparation` を設定。フォーマット不正・バージョン不一致・ファイル無しは false(state は不変)。
  未知のユニット名/アイテム名はその要素のみスキップして継続。
- `<fstream>` / `<sstream>` のみ使用。外部ライブラリ追加なし。

## セーブファイル形式(テキスト、空白区切り、行志向)

```
HEXARENA_SAVE 1
round <roundNumber>
loss <lossCount>
player <gold> <level> <xp> <winStreak> <lossStreak>
bench <count>
unit <name> <starLevel> <currentHP> <posQ> <posR> <homeQ> <homeR> <itemCount> [<itemName> ...]
   ... (bench の各ユニット)
board <count>
unit ...                              (board の各ユニット)
unclaimed <count> [<itemName> ...]
```

- ユニット名・アイテム名・プレイヤー名は空白を含まない(既存DB確認済み)ため `operator>>` でトークン単位に読める。
- 先頭行のマジック `HEXARENA_SAVE` とバージョン `1` が一致しなければロード失敗扱い。
- `UnitInstance` の `bonus*` / `nextActionTime` / `normalAttackCount` / `receivedAttackCount` / `shieldAmount` は
  戦闘開始時に再計算される揮発値なので保存しない。`currentHP` は完全性のため保存する(ロード後に復元、次戦闘で全回復)。

## 変更ファイル

### `Game/Game.h`

- `#include "SaveSystem.h"`、メンバ `SaveSystem m_saveSystem;` を追加。

### `Game/Game.cpp`

- `#include "SaveSystem.h"` は Game.h 経由。`<...>` の追加不要。
- **`Phase::Title` の入力処理を拡張**(`Game::Update()`):
  - `bool hasSave = m_saveSystem.SaveFileExists();`
  - `hasSave && A`: `m_saveSystem.Load(m_gameState, m_unitDatabase, m_itemDatabase)` 成功時のみ
    `m_currentShop.clear(); m_heldUnclaimedIndex = -1; currentPhase = Preparation;`。失敗時はフィードバックログのみ。
  - `hasSave && X`: ロードせず `currentPhase = Preparation`(新規開始。ディスク上のセーブはそのまま残す)。
  - `!hasSave && A`: 従来どおり `currentPhase = Preparation`。
- **`Phase::Preparation` に F5 セーブを追加**:
  - `if (g_keyboard->IsTrigger(VK_F5)) { bool ok = m_saveSystem.Save(m_gameState); m_shopUI.PushFeedback(ok ? L"セーブしました" : L"セーブに失敗しました", ...); OutputDebugString(...); }`
  - `g_keyboard` は既存(`CursorSelectionSystem.cpp` で使用実績あり)。他の準備フェーズ操作(A/B/X/Y/LB1/RB1)と衝突しない。

### `Game/TitleUIRenderer.h` / `.cpp`

- `Draw(RenderContext& rc, float deltaTime, bool hasSaveData)` に引数追加。
- `hasSaveData` が true のとき、プロンプト文字列を `PRESS [A] TO START` から
  `[A] CONTINUE     [X] NEW GAME` に差し替える(点滅ロジック・座標系はそのまま。開始X座標のみ文言長に合わせ調整)。
- `Game::Render()` の Title 描画呼び出しに `m_saveSystem.SaveFileExists()` を渡す。

## 既知の制約への対応

- FontEngine: pivot 中央揃え不可 → `kTopLeftPivot` + 手動X調整(既存 TitleUIRenderer 踏襲)。
- FontEngine: alpha フェード不可 → 点滅は描画ON/OFF(既存踏襲、変更なし)。
- ソースは UTF-8 (BOM付き) で保存(CP932 環境でのパース崩れ回避。前タスクで踏んだ地雷)。
- ビルドは VS 2026(18系)の MSBuild を使用(v145 ツールセット)。

## 確認手順

1. ビルド(Game.sln Debug/x64)。0 error、警告を増やさない。
2. 実機: 準備フェーズで数ラウンド進める → ユニット購入/配置/装備 → F5。`savedata.txt` 生成を確認。
3. 再起動 → タイトルが `[A] CONTINUE / [X] NEW GAME` 表示 → A → 準備フェーズにセーブ時点の状態が復元。
4. `savedata.txt` を壊す/削除 → タイトルが通常表示に戻り、新規開始できる(クラッシュしない)。

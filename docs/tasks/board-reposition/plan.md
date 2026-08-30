# 設計: 盤面内再配置（盤面上のユニット移動・ベンチへ戻す操作）

新規ファイルなし。既存3ファイルの変更のみ（`Game/Game.vcxproj` 変更なし → worktree マージが容易）。

## 変更ファイル

### `Game/Player.h`

`PlaceUnitOnBoard()` の直後に、盤面ユニットを扱う3メソッドを追加する。
`kAllyZoneMinQ/MaxQ` は既存の同ファイル内定数を再利用する。

```cpp
/// <summary>
/// 盤面の指定マスに居るユニットへのポインタを返す（居なければ nullptr）。
/// Game::Update() が「X で拾うのか従来のベンチ配置なのか」を判定するために使う。
/// </summary>
const UnitInstance* FindBoardUnitAt(const HexCoord& pos) const
{
    for (const auto& unit : board)
    {
        if (unit.position == pos) return &unit;
    }
    return nullptr;
}

/// <summary>
/// 盤面上のユニットを from マスから to マスへ移動する。
/// to は自陣（q0-2）かつ空きマスであること。成功で true。
/// 盤面ユニット数は増減しないため GetMaxBoardSize チェックは行わない。
/// </summary>
bool MoveUnitOnBoard(const HexCoord& from, const HexCoord& to)
{
    if (from == to) return false; // 同じマスへの移動は無効（呼び出し側でキャンセル扱い）。

    if (to.q < kAllyZoneMinQ || to.q > kAllyZoneMaxQ)
    {
        return false; // 自陣(q0-2)以外へは移動できない。
    }

    int fromIdx = -1;
    for (int i = 0; i < (int)board.size(); ++i)
    {
        if (board[i].position == to) return false;        // 移動先が埋まっている。
        if (board[i].position == from) fromIdx = i;
    }
    if (fromIdx < 0) return false; // from に自分のユニットが居ない。

    board[fromIdx].position = to;
    board[fromIdx].homePosition = to; // 毎ラウンド戦闘開始前に戻る先も更新する。
    return true;
}

/// <summary>
/// 盤面上の from マスのユニットをベンチへ戻す。成功で true。
/// ベンチ枚数上限は設けない（BuyUnit と整合）。
/// </summary>
bool ReturnUnitToBench(const HexCoord& from)
{
    for (int i = 0; i < (int)board.size(); ++i)
    {
        if (board[i].position == from)
        {
            bench.push_back(board[i]);
            board.erase(board.begin() + i);
            // 盤面⇔ベンチをまたぐ同一ユニット3体は購入/配置時点で必ず合成済みのため、
            // ここで新たな3体そろいは通常発生しないが、既存 PlaceUnitOnBoard と同じ
            // 防御的呼び出しとして残す。
            while (TryMergeUnits()) {}
            return true;
        }
    }
    return false;
}
```

### `Game/Game.h`

`m_heldUnclaimedIndex` の近くに、盤面内移動の「移動元選択中」状態を追加する。
`HexCoord` は既存 include（`HexGridRenderer.h` → `HexCoord.h`）で解決済み。

```cpp
// 準備フェーズ、盤面内再配置で「移動元」として選択中の盤面マス。
// Board フォーカス中に X で盤面ユニットを指すとセットされ、移動先マスで X（移動確定）/
// LB1（ベンチへ戻す）/ 同じマスで X（キャンセル）/ フォーカスが Board から外れる・戦闘突入で解除。
HexCoord m_heldBoardHex;
bool m_heldBoardHexValid = false;
```

### `Game/Game.cpp`

#### 1. リセット箇所

- `InitializeNewRun()` 内、`m_heldUnclaimedIndex = -1;` と同じ場所で `m_heldBoardHexValid = false;`。
- Preparation の B ボタン処理（`m_heldUnclaimedIndex = -1;` の行）に併せて `m_heldBoardHexValid = false;`。

#### 2. カーソルクランプ／housekeeping ブロック（現 146〜165 行あたり）

フォーカス判定の後に一行追加:

```cpp
// 盤面から離れたら「移動元選択中」は解除する（手持ちアイテムと同じ考え方）。
if (m_cursorSelection.GetFocus() != InputFocus::Board)
{
    m_heldBoardHexValid = false;
}
```

#### 3. X ボタン処理（現 321〜344 行）の再構成

現状は「Bench フォーカスならカーソル index、それ以外は 0 のベンチユニットを、ヘックスカーソルの
マスへ `PlaceUnitOnBoard`」。これを次の分岐で置き換える。**Board フォーカスかつ盤面内再配置に
該当するときだけ新ロジック**、それ以外は**現行の配置ロジックをそのまま実行**する。

```cpp
if (g_pad[0]->IsTrigger(enButtonX))
{
    Player& player = m_gameState.players[0];
    InputFocus focus = m_cursorSelection.GetFocus();

    HexCoord cursorHex;
    bool haveCursor = m_cursorSelection.GetHexCursor(cursorHex);

    bool handledByReposition = false;
    if (focus == InputFocus::Board)
    {
        if (m_heldBoardHexValid)
        {
            // 移動元選択済み → 今回の X は「移動先の確定」。
            handledByReposition = true;
            if (!haveCursor)
            {
                m_shopUI.PushFeedback(L"移動先マスを選んでください", ShopUIRenderer::FeedbackLevel::Failure);
            }
            else if (cursorHex == m_heldBoardHex)
            {
                m_heldBoardHexValid = false;
                m_shopUI.PushFeedback(L"移動をキャンセルしました", ShopUIRenderer::FeedbackLevel::Info);
            }
            else
            {
                bool ok = player.MoveUnitOnBoard(m_heldBoardHex, cursorHex);
                wchar_t buf[192];
                swprintf_s(buf, L"Board move result: %hs, from (%d,%d) to (%d,%d)\n",
                    ok ? "true" : "false", m_heldBoardHex.q, m_heldBoardHex.r, cursorHex.q, cursorHex.r);
                OutputDebugString(buf);
                if (ok)
                {
                    wchar_t fb[128];
                    // 全角矢印(U+2192)はFontEngineのSpriteFontにグリフが無くabortするためASCIIの "->" を使う。
                    swprintf_s(fb, L"移動: (%d,%d) -> (%d,%d)", m_heldBoardHex.q, m_heldBoardHex.r, cursorHex.q, cursorHex.r);
                    m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Success);
                    m_heldBoardHexValid = false;
                }
                else
                {
                    m_shopUI.PushFeedback(L"移動できません (自陣 q0-2 のみ / 空きマス無し)", ShopUIRenderer::FeedbackLevel::Failure);
                }
            }
        }
        else if (haveCursor)
        {
            const UnitInstance* onCell = player.FindBoardUnitAt(cursorHex);
            if (onCell != nullptr)
            {
                // カーソルが盤面ユニットを指している → 「移動元」として選択（拾う）。
                handledByReposition = true;
                m_heldBoardHex = cursorHex;
                m_heldBoardHexValid = true;
                wchar_t fb[160];
                swprintf_s(fb, L"移動元を選択: %hs  (移動先マスで[X] / [LB1]でベンチへ)", onCell->def->name.c_str());
                m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
            }
            // カーソルが空きマス → handledByReposition = false のまま従来の配置ロジックへ。
        }
    }

    if (!handledByReposition)
    {
        // --- 従来のベンチ → 盤面配置（挙動は変更しない） ---
        int benchIndex = (focus == InputFocus::Bench) ? m_cursorSelection.GetListCursorIndex() : 0;
        HexCoord targetHex(0, 0);
        m_cursorSelection.GetHexCursor(targetHex);
        bool success = player.PlaceUnitOnBoard(benchIndex, targetHex);

        wchar_t buf[256];
        swprintf_s(buf, L"Place result: %hs, Bench index: %d, Hex: (%d,%d), Bench count: %d, Board count: %d\n",
            success ? "true" : "false", benchIndex, targetHex.q, targetHex.r, (int)player.bench.size(), (int)player.board.size());
        OutputDebugString(buf);

        if (success)
        {
            wchar_t fb[128];
            swprintf_s(fb, L"配置: マス(%d,%d)  盤面 %d/%d", targetHex.q, targetHex.r, (int)player.board.size(), player.GetMaxBoardSize());
            m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
        }
        else
        {
            m_shopUI.PushFeedback(L"配置できません (自陣 q0-2 のみ / 盤面上限 / 空きマス無し)", ShopUIRenderer::FeedbackLevel::Failure);
        }
    }
}
```

#### 4. LB1 ボタン処理（現 383〜404 行）の再構成

現状は「Bench フォーカスならカーソル、それ以外は 0 のベンチユニットを売却」。
**Board フォーカスかつ対象が盤面ユニットのときだけ「ベンチへ戻す」**、それ以外は現行の売却をそのまま実行。

```cpp
if (g_pad[0]->IsTrigger(enButtonLB1))
{
    Player& player = m_gameState.players[0];
    InputFocus focus = m_cursorSelection.GetFocus();

    // Board フォーカス中は、選択中の移動元（無ければヘックスカーソル）の盤面ユニットをベンチへ戻す。
    bool handledByReturn = false;
    if (focus == InputFocus::Board)
    {
        HexCoord targetHex;
        bool haveTarget = false;
        if (m_heldBoardHexValid) { targetHex = m_heldBoardHex; haveTarget = true; }
        else if (m_cursorSelection.GetHexCursor(targetHex)) { haveTarget = true; }

        if (haveTarget && player.FindBoardUnitAt(targetHex) != nullptr)
        {
            handledByReturn = true;
            bool ok = player.ReturnUnitToBench(targetHex);
            wchar_t buf[192];
            swprintf_s(buf, L"Return to bench result: %hs, hex (%d,%d), Bench count: %d, Board count: %d\n",
                ok ? "true" : "false", targetHex.q, targetHex.r, (int)player.bench.size(), (int)player.board.size());
            OutputDebugString(buf);
            m_heldBoardHexValid = false;
            m_shopUI.PushFeedback(
                ok ? L"ベンチへ戻しました" : L"ベンチへ戻せません",
                ok ? ShopUIRenderer::FeedbackLevel::Info : ShopUIRenderer::FeedbackLevel::Failure);
        }
    }

    if (!handledByReturn)
    {
        // --- 従来のベンチユニット売却（挙動は変更しない） ---
        int benchIndex = (focus == InputFocus::Bench) ? m_cursorSelection.GetListCursorIndex() : 0;
        bool success = player.SellUnitFromBench(benchIndex);

        wchar_t buf[256];
        swprintf_s(buf, L"Sell result: %hs, Bench index: %d, Gold: %d, Bench count: %d\n",
            success ? "true" : "false", benchIndex, player.gold, (int)player.bench.size());
        OutputDebugString(buf);

        if (success)
        {
            wchar_t fb[128];
            swprintf_s(fb, L"売却  所持 %dG", player.gold);
            m_shopUI.PushFeedback(fb, ShopUIRenderer::FeedbackLevel::Info);
        }
        else
        {
            m_shopUI.PushFeedback(L"売却できません (ベンチが空)", ShopUIRenderer::FeedbackLevel::Failure);
        }
    }
}
```

## データ構造

- `Game::m_heldBoardHex : HexCoord` + `m_heldBoardHexValid : bool` … 盤面内移動の「移動元選択待ち」
  一時状態。フェーズをまたがない（`m_heldUnclaimedIndex` と同じ寿命）。
- `Player::board` / `bench` … 既存。移動は `board` 内の要素の `position`/`homePosition` 更新のみ、
  ベンチ戻しは `board` → `bench` への要素移動。

## 操作方法まとめ（Board フォーカス時）

| 状態 | 入力 | 結果 |
|---|---|---|
| 移動元未選択・カーソルが盤面ユニット上 | X | そのユニットを移動元に選択 |
| 移動元未選択・カーソルが空きマス | X | （従来）ベンチ先頭を配置 |
| 移動元選択中・カーソルが空きの自陣マス | X | そのマスへ移動 |
| 移動元選択中・カーソルが移動元と同じマス | X | 選択解除（キャンセル） |
| 移動元選択中・カーソルが埋まった/範囲外マス | X | 失敗フィードバック |
| 移動元選択中（またはカーソルが盤面ユニット上） | LB1 | そのユニットをベンチへ戻す |
| Board フォーカスを抜ける / B で戦闘へ | - | 移動元選択を自動解除 |

## 既知の制約・割り切り

- 移動元選択中であることの盤面上の常時ビジュアル表示は無し（`intent.md` スコープ外参照）。
  `PushFeedback` は数秒で消えるため、選択したまま時間が経つと状態を見失う可能性がある点は許容する。
- `MoveUnitOnBoard` は `IsValidHex`（r 範囲・q≤8）を検証しない。ヘックスカーソルが有効マスのみを
  返す前提で、既存 `PlaceUnitOnBoard` と同じ割り切り。
- セーブ形式は不変（`board`/`bench`/`homePosition` の状態が変わるだけで、`SaveSystem` の
  シリアライズ対象・フォーマットは変更なし）。
- ソースは既存同様 UTF-8（BOM付き）で保存する（CP932環境でのパース崩れ回避）。

## 確認手順

1. `msbuild Game/Game.sln /p:Configuration=Debug /p:Platform=x64` でビルド成功。
2. 実機（F5）→ 準備フェーズ:
   - ベンチから2体以上を自陣に配置。
   - Tab で Board フォーカス。矢印キー/マウスで盤面ユニットを指し **X** → フィードバック「移動元を選択: ...」。
   - 空いている自陣マスへカーソルを移し **X** → 3Dモデルが移動、フィードバック「移動: (q,r) -> (q,r)」。
     `OutputDebugString` に `Board move result: true ...`。
   - 別の盤面ユニットを **X** で選択 → 中立(q3-5)マスへ **X** → 失敗、「自陣 q0-2 のみ / 空きマス無し」。
   - 盤面ユニットを **X** で選択 → 他ユニットが居るマスへ **X** → 失敗。
   - 盤面ユニットを **X** で選択 → **LB1** → BENCH 一覧に戻る、3D表示から消える。
     `OutputDebugString` に `Return to bench result: true ...`。
   - 戻したユニットを Bench フォーカス → **X** で再配置できる。
   - 既存操作（A購入 / Yリロール / Bench選択でLB1売却 / アイテム装備 / B戦闘へ）が従来通り。
3. `plan.md` と実装の一致をレビューで確認。

# 設計: 盤面使用数/上限・ストリーク表示 + トレイト3段階化

新規ファイルは無し。既存3ファイルへの追記のみ。`Game.vcxproj` 変更不要。

## 1. 盤面使用数/上限 — `Game/PlayerStatusUIRenderer.{h,cpp}`

- `Draw(rc, player, xpForNextLevel)` は既に `const Player&` を受け取っている。
  `m_boardCount = (int)player.board.size();`、`m_maxBoardSize = player.GetMaxBoardSize();` を保持
  (メンバ2個追加)。
- 描画は **GOLD行と同じ y(`kGoldY`)に2つ目の `Draw` を x オフセットして並べる**。行を増やさないことで
  下段の `RoundRecordUIRenderer`(y≒389〜313)・`ItemInventoryUIRenderer`(y≒270〜)との縦の取り合いを回避する。
  - `GOLD  <n>` を従来どおり `kX`(560)に描画。
  - `BOARD  <used>/<max>` を `kX + kBoardCol`(= 560 + 250 = 810 付近)に描画。最長 "BOARD  9/9" は
    scale 0.58 で ~150px、右端(x=+960)内に収まる。
  - 色: `used < max` は LV行と同じ淡い青(`kLevelColor`)、`used >= max`(これ以上置けない)は
    警告色 `(1.0, 0.55, 0.30)` のアンバー。alpha は使わない(制約対応)。
- FPS表示は画面左上のため干渉なし。GOLD行の y は変更しない。

## 2. ストリーク — `Game/RoundRecordUIRenderer.{h,cpp}`

- `Draw(rc, gameState)` 内で `gameState.players` が空でなければ
  `m_winStreak = gameState.players[0].winStreak; m_lossStreak = gameState.players[0].lossStreak;`
  を保持(メンバ2個追加)。`GameState.h` は既に include 済みで `Player` 定義も見えている。
- 描画は **「残り N ラウンド」行と同じ y に2つ目の `Draw` を x オフセットして並べる**(行を増やさない。
  4行目を足すと `ItemInventoryUIRenderer` の "ITEMS"(y≒270)と重なるため)。
  - "残り N ラウンド" を従来どおり `kX` に描画。
  - ストリーク文字列を `kX + kStreakCol`(= kX + 210)に描画:
    - `winStreak > 0` → `連勝 <n>` / 色 緑 `(0.45, 0.95, 0.5)`
    - `lossStreak > 0` → `連敗 <n>` / 色 アンバー `(0.98, 0.85, 0.3)`
    - どちらも0 → `連勝連敗なし` / 色 `kNormalColor`
  - 既存の「連敗 N / M」行(ゲームオーバー猶予)とはラベル位置・行が異なり、こちらは「連勝/連敗」を
    併記する短縮表記なので混同しにくい。最長 "残り 10 ラウンド    連勝 9" が scale 0.48 で右端内に収まることを
    実機で確認する。

### 変更しない
- 既存3行(ROUND / 残り / 連敗 N/M)の y・文言・色ロジックはそのまま。

## 3. トレイト3段階目 — `Game/TraitDatabase.h`

各トレイトの `tiers` に requiredCount 6 の段階を1つ追加(既存の2段階の後ろに push_back)。
効果値は「1→2段階目の伸び」を踏襲し、種類は既存段階と同じスタットを一段強くする方針
(新種のスタットは足さない)。

| トレイト | 既存 t1 | 既存 t2 | 追加 t3 (req 6) |
|---|---|---|---|
| Monster  | Atk+10% | Atk+25%, MP+15% | Atk+45%, MP+30% |
| Human    | HP+15%  | HP+35%, MDef+15 | HP+60%, MDef+30 |
| Hero     | Atk+5,HP+20 | Atk+15,HP+50,SkillThr-1 | Atk+30, HP+100, SkillThr-2 |
| Warrior  | Atk+15% | Atk+30%, PDef+15 | Atk+50%, PDef+30 |
| Mage     | MP+20%  | MP+40%, MDef+15 | MP+65%, MDef+30 |
| Guardian | PDef+15,MDef+15 | PDef+35,MDef+35,HP+20% | PDef+55, MDef+55, HP+35% |
| Assassin | SkillThr-1 | SkillThr-2, Atk+20% | SkillThr-3, Atk+40% |
| Ranger   | Atk+15% | Atk+30%, SkillThr-1 | Atk+50%, SkillThr-2 |

- Hero は既存が req 1,2 のため t3 は req 6(3体〜5体の中間段階は設けない。intent通り「6体条件を追加」)。
- `TraitSystem::FindActiveTier` / `FindNextTier`、`TraitPanelUIRenderer` は段階数非依存のため他の変更不要。
  パネルは自動的に `count 4〜5 → "4/6"`、`count 6+ → "6 MAX"` を表示する。

## スコープ外 / 割り切り

- 右側HUDが極端に縦に伸びるケース(ITEMSが6件以上 + …)での完全な非重複は保証しない
  (典型的なプレイ状況で重ならないことのみ確認。既存タスク trait-panel-ui と同じ割り切り)。
- トレイト効果値の厳密なバランス調整(プレイテストによる数値詰め)は本タスクの範囲外。
- 開発機のDPI設定の都合で画面下部(ITEMS下端など)は実機確認で見切れるため、下部の最終確認は
  正規1920x1080環境(デバッグ確認セッション)に委ねる。上部(GOLD/LV/BOARD/ROUND付近)は本機で確認する。

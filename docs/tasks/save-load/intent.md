# 要件定義: セーブ/ロード機能

- Notionタスク: 「セーブ/ロード機能」(期限 2026-09-21)
- 担当: 実装用セッション / 依頼元: マネージャー
- 作業ブランチ: `feature/save-load`(git worktree。`Game/Game.cpp` 等を別タスクが未コミットで触っているため隔離)

## 背景・現状

- ファイルI/O・シリアライズ等のセーブ/ロード関連実装は一切ない。
- 1プレイ分の進行状況は `GameState`(`players[0]` のゴールド/レベル/XP/bench/board、`roundNumber`、`lossCount`、`currentPhase`)に集約されている。
- `UnitInstance::def` / `UnitInstance::items` / `Player::unclaimedItems` は `UnitDatabase` / `ItemDatabase` が持つマスターデータへの生ポインタで、そのままでは永続化できない。名前(`UnitDef::name` / `ItemDef::name`)で引き直す必要がある。
- タイトル画面 `Phase::Title` は既にあり、`PRESS [A] TO START`(A ボタンで `Phase::Preparation` へ)という導線を持つ(`TitleUIRenderer`)。

## 目的

準備フェーズの進行状況をファイルに保存し、次回起動時にタイトル画面から復元して続きから遊べるようにする。

## 要求仕様

1. 保存対象: `players[0]` の gold / level / xp / winStreak / lossStreak / bench / board(各ユニットの名前・スターレベル・currentHP・位置・homePosition・装備アイテム名) / `unclaimedItems`(アイテム名)、`GameState::roundNumber`、`GameState::lossCount`。
2. 復元時、ユニット・アイテムは名前から `UnitDatabase` / `ItemDatabase` を引き直して `def` / `items` ポインタを再結線する。
3. 保存形式: 依存ライブラリを追加しない、シンプルな自前テキスト形式(バージョン番号付き、空白区切り)。
4. セーブタイミング: 準備フェーズ中の手動セーブ(キーボード F5)。
5. ロード導線: タイトル画面に追加する。
   - セーブ無し: 従来どおり `PRESS [A] TO START`(A = 新規開始)。
   - セーブ有り: `[A] CONTINUE   [X] NEW GAME`(A = ロードして準備フェーズへ / X = ロードせず新規開始)。
6. セーブファイル: 実行時の作業ディレクトリに `savedata.txt`(VS 実行時は `Game/` 配下)。`.gitignore` に追加する。

## 受け入れ条件

- ビルドが通る(Debug/x64、VS 2026 の v145 ツールセット、警告を増やさない)。
- 実機で「準備フェーズで F5 セーブ → ゲーム再起動 → タイトルで [A] CONTINUE → セーブ時点の
  ゴールド/レベル/XP/ラウンド数/連敗数/bench/board/装備/未装備アイテムが復元される」ことを確認。
- セーブファイルが壊れている/古い/存在しない場合はロードせず、通常の新規開始にフォールバックする(クラッシュしない)。
- DB に存在しないユニット名/アイテム名があった場合はその要素だけスキップしてロードを継続する。

## スコープ外

- 複数セーブスロット、オートセーブ、戦闘中/リザルト中のセーブ。
- ショップの抽選結果の保存(ロード後は再抽選でよい)。
- 敵編成・戦闘進行・カメラ等、`GameState` 外の状態。

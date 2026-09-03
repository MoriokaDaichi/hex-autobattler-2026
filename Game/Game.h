#pragma once
#include "Level3DRender/LevelRender.h"
#include "GameState.h"
#include "UnitDatabase.h"
#include "ShopSystem.h"
#include "CombatEngine.h"
#include "TraitDatabase.h"
#include "TraitSystem.h"
#include "ItemDatabase.h"
#include "ItemSystem.h"
#include "StarLevelSystem.h"
#include "EconomySystem.h"
#include "LevelSystem.h"
#include "CombatEvent.h"
#include "CombatLogPrinter.h"
#include "CombatPlayback.h"
#include "EnemyStage.h"
#include "EnemyFactory.h"
#include "HexGridRenderer.h"
#include "UnitModelDisplay.h"
#include "ShopUIRenderer.h"
#include "BoardUIRenderer.h"
#include "PlayerStatusUIRenderer.h"
#include "RoundRecordUIRenderer.h"
#include "ItemInventoryUIRenderer.h"
#include "TraitPanelUIRenderer.h"
#include "TitleUIRenderer.h"
#include "ResultUIRenderer.h"
#include "UIRectRenderer.h"
#include "CursorSelectionSystem.h"
#include "SaveSystem.h"
#include "UIHotRegion.h"
#include "UIInteractionSystem.h"
#include "TooltipContentBuilder.h"
#include "TooltipUIRenderer.h"

class Game : public IGameObject
{
public:
	Game() {}
	~Game() {}
	bool Start();
	void Update();
	void Render(RenderContext& rc);

private:
	std::vector<EnemyStage> BuildEnemyStages();

	/// <summary>
	/// 1プレイ分(GameState::players・roundNumber・lossCount等)を初期状態へ戻す。
	/// Start()の初回起動時と、GameOver/Victory画面からのリスタート時の両方から呼ぶ。
	/// UnitDatabase/TraitDatabase等の一度きりの初期化やm_enemyStagesの構築、カメラ・
	/// ライティング設定、Start()末尾のデバッグ用合成テストブロックはここに含めない
	/// (前者は不変のデータ、後者は別タスクで整理予定のためリスタート時は呼ばない)。
	/// </summary>
	void InitializeNewRun();

	GameState m_gameState;
	UnitDatabase m_unitDatabase;
	std::vector<const UnitDef*> m_currentShop;
	bool m_shopLocked = false; // ショップのロック。true の間はラウンドを跨いでも m_currentShop を再抽選しない(手動リロールは可)。
	ShopSystem m_shopSystem;
	CombatEngine m_combatEngine;
	TraitDatabase m_traitDatabase;
	TraitSystem m_traitSystem;
	ItemDatabase m_itemDatabase;
	ItemSystem m_itemSystem;
	StarLevelSystem m_starLevelSystem;
	EconomySystem m_economySystem;
	LevelSystem m_levelSystem;
	CombatLogPrinter m_combatLogPrinter;
	std::vector<CombatEvent> m_combatEvents;
	EnemyFactory m_enemyFactory;
	std::vector<EnemyStage> m_enemyStages;
	HexGridRenderer m_hexGridRenderer;
	UnitModelDisplay m_unitModelDisplay; // players[0].boardのユニットを3D表示する。
	ShopUIRenderer m_shopUI; // 準備フェーズのショップバー(5枠のカード・操作フィードバック)を2D表示する。
	BoardUIRenderer m_boardUI; // 戦闘中のHPバー / 準備フェーズのベンチ一覧を2D表示する。
	PlayerStatusUIRenderer m_playerStatusUI; // 所持ゴールド・レベル/XPゲージをフェーズを問わず常時表示する。
	RoundRecordUIRenderer m_roundRecordUI; // 現在ラウンド数/残りラウンド数・連敗カウントを常時2D表示する。
	ItemInventoryUIRenderer m_itemInventoryUI; // 準備フェーズ、未装備アイテム一覧を画面右側に2D表示する。
	TraitPanelUIRenderer m_traitPanelUI; // 準備フェーズ、全トレイトの発動状況を画面左側に2D表示する。
	TitleUIRenderer m_titleUI; // タイトル画面(Phase::Title)のタイトル文字列・スタート操作ガイドを2D表示する。
	ResultUIRenderer m_resultUI; // ラウンド結果一言・ゲームオーバー/ゲームクリア画面を2D表示する。
	UIRectRenderer m_uiRectRenderer; // 単色矩形(HPバー・XPバー・スキルゲージバー・暗幕)を描く共通ヘルパー。
	CursorSelectionSystem m_cursorSelection; // マウス・キーボード・ゲームパッドを横断するカーソル/選択状態。
	SaveSystem m_saveSystem; // 準備フェーズの進行状況(GameState)をテキストファイルへ保存/復元する。

	// マウス操作基盤(ui-mouse-cards フェーズ1)。毎フレームUpdate()の先頭で、各UI Rendererの
	// BuildHotRegions()を使って"今フレームの"クリック可能矩形一覧へ詰め直す(Render()のDraw()に
	// 依存すると1フレーム遅延するため。docs/tasks/ui-mouse-cards/plan.md §0-4/§1-3)。
	UIHotRegionList m_hotRegions;
	UIInteractionSystem m_uiInteraction; // 上記からホバー/クリックを解決する。

	// ツールチップ(ui-mouse-cards フェーズ2)。マウスホバー(kHoverDelaySec継続)、または
	// ゲームパッド/キーボードでフォーカス中の要素(遅延無し)の詳細をカーソル付近にカード表示する
	// (docs/tasks/ui-mouse-cards/plan.md §3-1)。内容はGame::Update()末尾でTooltipContentBuilderに
	// より毎フレーム組み立て、Game::Render()でm_tooltipUI.Draw()へ渡す。
	TooltipUIRenderer m_tooltipUI;
	UIHotRegion m_hoverCandidate;      // ホバー遅延タイマーの対象として追跡中の領域。
	float m_hoverTimer = 0.0f;         // m_hoverCandidateに留まっている継続時間(秒)。
	bool m_tooltipVisible = false;     // このフレーム、ツールチップを表示するか。
	std::vector<std::wstring> m_tooltipLines; // 表示中の内容(TooltipContentBuilder::Buildの結果)。
	Vector2 m_tooltipAnchor;           // 表示位置の基準点(UI_SPACE座標)。
	static constexpr float kHoverDelaySec = 0.3f; // マウスホバーでツールチップが出るまでの継続時間。

	// マウス操作専用: ベンチのユニットを左クリックで「掴んだ」状態。盤面の空きマスを左クリックすると
	// Player::PlaceUnitOnBoard()を呼んで確定する(ゲームパッドXボタンの配置ロジックを流用、新しい
	// 配置ルールは増やさない)。右クリックでキャンセル。フォーカス変更・戦闘突入で解除する
	// (m_heldUnclaimedIndex/m_heldBoardHexValidと同じ寿命の考え方)。
	int m_mouseHeldBenchIndex = -1;

	// マウス右クリックでの売却/ベンチ戻しの2段階確認用。1回目の右クリックで「確認待ち」にし、
	// kSellConfirmWindowSec以内に同じ対象へもう一度右クリックすると確定する。タイムアウト・
	// 別対象へのクリック・フォーカス変更で自動的に解除する(docs/tasks/ui-mouse-cards/plan.md §2-4)。
	UIHotRegion m_pendingSellTarget;
	bool m_hasPendingSellTarget = false;
	float m_sellConfirmTimer = 0.0f;
	static constexpr float kSellConfirmWindowSec = 3.0f; // ShopUIRendererのkFeedbackDurationと合わせる。

	// 準備フェーズで「手に持っている」未装備アイテムの、players[0].unclaimedItems上のindex(-1で無し)。
	// Itemsフォーカス中にAで持ち、Bench/Boardのユニットを選んでAで装備確定するまでの一時状態。
	int m_heldUnclaimedIndex = -1;

	// 準備フェーズ、盤面内再配置で「移動元」として選択中の盤面マス。
	// Boardフォーカス中にXで盤面ユニットを指すとセットされ、移動先マスでX(移動確定)/
	// LB1(ベンチへ戻す)/同じマスでX(キャンセル)/フォーカスがBoardから外れる・戦闘突入で解除。
	HexCoord m_heldBoardHex;
	bool m_heldBoardHexValid = false;
	// trueなら上記の選択がマウスクリックで拾われたもの(ui-mouse-cardsフェーズ1)。マウス操作は
	// m_cursorSelectionの focus を変更しない(Tab操作をしないため)ので、「focusがBoardでなくなったら
	// 解除する」既存の掃除ロジックをマウス発の選択には適用しない(適用すると拾った直後の
	// フレームで即座に解除されてしまう)。ゲームパッド発の選択(false)には従来通り適用される。
	bool m_heldBoardHexFromMouse = false;

	// 戦闘フェーズの複数フレーム化用。突入時に1回だけシミュレーション+集計を行い(m_combatSimDone=true)、
	// 以降は m_combatPlayback で時系列再生する。再生完了後に m_pendingPhaseAfterCombat へ遷移する。
	CombatPlayback m_combatPlayback;
	bool m_combatSimDone = false;
	Phase m_pendingPhaseAfterCombat = Phase::Result;

	// Phase::Resultの表示・滞在時間管理用。Combatフェーズ解決時にその回の勝敗を控えておき、
	// CombatPlayback再生完了でPhase::Resultへ入った瞬間にタイマーを開始する。
	CombatResult m_lastCombatResult = CombatResult::Win;
	float m_resultPhaseTimer = 0.0f;
};
#pragma once
#include <string>
#include <vector>
#include "UnitDef.h"
#include "UIHotRegion.h"

struct Player;
class UIRectRenderer;

/// <summary>
/// 準備フェーズのショップUI(画面下部のショップバー)を2Dで描画するクラス。
///
/// IRendererを継承し、RenderingEngineの2D描画パス(OnRender2D)で描画する。
/// Game::Render()から直接Font::Draw()を呼ぶと、後段のRenderingEngine::Execute()の
/// 2Dパスがレンダーターゲットをクリアしてしまい消えるため、HexGridRenderer等と同じく
/// 「毎フレームDraw()で状態を更新しAddRenderObject()で登録 → On*Renderで実描画」の形をとる。
///
/// 対象はplayers[0](唯一の人間プレイヤー)のみ。カード枠のスプライト素材やトレイトアイコンは
/// 用意が無いため、今回は文字表示のみで構成する(枠線・アイコンの作り込みはスコープ外)。
/// </summary>
class ShopUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 操作フィードバック1行の意味合い。色分けに使う。
	/// </summary>
	enum class FeedbackLevel
	{
		Info,     // 中立的な通知(リロールした等)。
		Success,  // 操作成功(購入できた等)。
		Failure,  // 操作失敗(ゴールド不足等)。
	};

	/// <summary>
	/// 準備フェーズ中、毎フレームGame::Render()から呼ぶ。表示に必要な現在値をコピーして保持し、
	/// 2D描画パスへの登録(AddRenderObject)を行う。shopが空の場合は何も描画しない。
	/// </summary>
	/// <param name="shopCursorIndex">ショップ一覧上のカーソル位置(CursorSelectionSystem由来)。</param>
	/// <param name="shopFocused">今カーソルのフォーカスがショップに当たっているか(当たっている枠を強調表示する)。</param>
	/// <param name="shopLocked">ショップがロックされているか(ヘッダー行に[LOCKED]を表示し金色にする)。</param>
	/// <param name="hoveredIndex">マウスホバー中のショップ枠index(無ければ-1)。カード枠のハイライトに使う。</param>
	/// <param name="rectRenderer">カード・Reroll/BuyXP/Lock/NextPhaseボタンの背景矩形を描く共通ヘルパー。
	/// OnRender2D内で使うためここで受け取ったポインタをキャッシュする(他UI Rendererと同じパターン)。</param>
	void Draw(
		RenderContext& rc,
		const std::vector<const UnitDef*>& shop,
		const Player& player,
		int xpForNextLevel,
		int rerollCost,
		int buyXpCost,
		int shopCursorIndex,
		bool shopFocused,
		bool shopLocked,
		int hoveredIndex,
		UIRectRenderer& rectRenderer);

	/// <summary>
	/// 直近の操作結果を1行フィードバックとして表示する。一定時間表示したのち自動的に消える。
	/// </summary>
	void PushFeedback(const wchar_t* message, FeedbackLevel level);

	/// <summary>
	/// フィードバックの表示残り時間を進める。毎フレームGame::Update()から呼ぶ。
	/// </summary>
	void UpdateFeedbackTimer(float deltaTime);

	/// <summary>
	/// 現フレームのクリック可能矩形(5枠のショップカード + Reroll/BuyXP/Lock/NextPhaseボタン)を
	/// outへ追加する。描画を伴わない純粋関数。Game::Update()の先頭でDraw()と同じ引数を使って呼ぶ
	/// (レイアウト定数を共有しているためズレない)。shopが空でもボタン4つは常に登録する。
	/// </summary>
	void BuildHotRegions(const std::vector<const UnitDef*>& shop, UIHotRegionList& out) const;

	// IRendererオーバーライド。RenderingEngine::Execute()の2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	/// <summary>
	/// 1枠分の表示内容。UnitDefを直接持たず、描画に必要な値だけをコピーしておく
	/// (Draw()の呼び出しとOnRender2D()の呼び出しがフレーム内で分かれているため)。
	/// </summary>
	struct SlotView
	{
		std::wstring name;
		std::wstring traits;
		int cost = 0;
		int baseHP = 0;
		int baseAttack = 0;
	};

	Font m_font;

	std::vector<SlotView> m_slots;
	int m_gold = 0;
	int m_level = 0;
	int m_xp = 0;
	int m_xpForNextLevel = 0;
	int m_rerollCost = 0;
	int m_buyXpCost = 0;
	int m_cursorIndex = -1;
	bool m_shopFocused = false;
	bool m_shopLocked = false;
	int m_hoveredIndex = -1;
	bool m_hasData = false;

	std::wstring m_feedbackText;
	FeedbackLevel m_feedbackLevel = FeedbackLevel::Info;
	float m_feedbackTimer = 0.0f;   // 残り表示秒数。0以下で非表示。

	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
};

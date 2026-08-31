#include "stdafx.h"
#include "ResultUIRenderer.h"
#include "UIRectRenderer.h"

namespace
{
	// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
	const Vector2 kTopLeftPivot(0.0f, 1.0f); // 実質的な左上アンカー(他のUIRenderer群と同じ扱い)。

	// このFontEngineにMeasureString相当が無いため、TitleUIRenderer実装時に実機で校正した
	// 「1文字あたり概ね23px(スケール1.0基準、半角換算)」を目安に開始X座標を計算し、
	// 見た目上おおよそ中央に来るようにしている(正確な中央揃えではなく近似)。
	float CenteredStartX(int halfWidthCharCount, float scale)
	{
		const float kApproxCharWidth = 23.0f;
		return -(halfWidthCharCount * kApproxCharWidth * scale) * 0.5f;
	}

	// --- ラウンド結果(Phase::Result)一言表示 ---
	const float kRoundResultY = 60.0f;
	const float kRoundResultScale = 1.4f;
	const Vector4 kClearColor(0.45f, 0.95f, 0.5f, 1.0f);  // BoardUIRendererのHP安全域と同系色。
	const Vector4 kDefeatColor(1.0f, 0.4f, 0.35f, 1.0f);  // BoardUIRendererのHP危険域と同系色。
	const wchar_t* kClearText = L"ROUND CLEAR!";
	const wchar_t* kDefeatText = L"DEFEAT...";

	// --- ゲームオーバー/ゲームクリア画面 ---
	const float kBigTitleY = 140.0f;
	const float kBigTitleScale = 2.0f;
	const float kSubY = 20.0f;
	const float kSubScale = 0.6f;
	const float kPromptY = -100.0f;
	const float kPromptScale = 0.66f;

	const Vector4 kGameOverColor(1.0f, 0.32f, 0.28f, 1.0f);
	const Vector4 kVictoryColor(1.00f, 0.85f, 0.35f, 1.0f); // TitleUIRendererのタイトル文字と同系色(金)。
	const Vector4 kSubColor(0.82f, 0.85f, 0.9f, 1.0f);
	const Vector4 kPromptColor(1.0f, 1.0f, 1.0f, 1.0f);

	const wchar_t* kGameOverText = L"GAME OVER";
	const wchar_t* kVictoryText = L"VICTORY!";
	const wchar_t* kPromptText = L"PRESS [A] TO TITLE";

	// アルファブレンドが機能しないため、一定間隔で描画自体をON/OFFして点滅を表現する
	// (TitleUIRendererのPRESS [A] TO STARTと同じ方式)。
	const float kBlinkIntervalSec = 0.5f;

	// --- GameOver/Victory背景の暗幕 ---
	// ui-sprite-barsタスクでUIRectRenderer(Sprite経由、本物のアルファブレンド)が使えるようになったため、
	// 画面全体を覆う半透明の黒矩形1枚に置き換える(旧実装は'#'を敷き詰めた不透明ブロックだった)。
	const Vector4 kBackdropColor(0.0f, 0.0f, 0.0f, 0.55f);
	const Vector2 kCenterPivot(0.5f, 0.5f);
}

void ResultUIRenderer::DrawRoundResult(RenderContext& rc, CombatResult result)
{
	m_mode = Mode::RoundResult;
	m_roundResult = result;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::DrawGameOver(RenderContext& rc, int reachedRound, int totalRounds, float deltaTime, UIRectRenderer& rectRenderer)
{
	if (m_mode != Mode::GameOver) m_elapsedTime = 0.0f; // 新規に突入したフレームで点滅タイマーをリセット。
	m_mode = Mode::GameOver;
	m_reachedRound = reachedRound;
	m_totalRounds = totalRounds;
	m_elapsedTime += deltaTime;
	m_rectRenderer = &rectRenderer;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::DrawVictory(RenderContext& rc, int totalRounds, float deltaTime, UIRectRenderer& rectRenderer)
{
	if (m_mode != Mode::Victory) m_elapsedTime = 0.0f;
	m_mode = Mode::Victory;
	m_totalRounds = totalRounds;
	m_elapsedTime += deltaTime;
	m_rectRenderer = &rectRenderer;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::BuildHotRegions(bool isGameOverOrVictory, UIHotRegionList& out) const
{
	if (!isGameOverOrVictory) return;

	// "PRESS [A] TO TITLE"(半角19文字、kPromptScale)。MeasureString相当が無いため、
	// CenteredStartX()と同じ概算(1文字あたり23px)で幅を見積もる(実機で要微調整)。
	const float kApproxCharWidth = 23.0f;
	float width = 19.0f * kApproxCharWidth * kPromptScale;
	float startX = CenteredStartX(19, kPromptScale);

	UIHotRegion region;
	region.kind = UIRegionKind::RestartButton;
	region.minX = startX - 6.0f;
	region.maxX = startX + width + 6.0f;
	region.minY = kPromptY - 28.0f;
	region.maxY = kPromptY + 16.0f;
	out.push_back(region);
}

void ResultUIRenderer::OnRender2D(RenderContext& rc)
{
	if (m_mode == Mode::None) return;

	// 矩形(Sprite)はFont::Begin()〜End()の外側で先に描き終える(plan.md §0-8)。
	if ((m_mode == Mode::GameOver || m_mode == Mode::Victory) && m_rectRenderer != nullptr)
	{
		// 画面全体を覆う半透明の黒矩形。盤面(色付きヘックスタイル)がうっすら透けて見える。
		m_rectRenderer->DrawRect(rc, Vector2(0.0f, 0.0f),
			Vector2((float)UI_SPACE_WIDTH, (float)UI_SPACE_HEIGHT), kBackdropColor, kCenterPivot);
	}

	m_font.SetShadowParam(true, 3.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	if (m_mode == Mode::RoundResult)
	{
		bool isWin = (m_roundResult == CombatResult::Win);
		const wchar_t* text = isWin ? kClearText : kDefeatText;
		const Vector4& color = isWin ? kClearColor : kDefeatColor;
		int charCount = isWin ? 12 : 9;
		float x = CenteredStartX(charCount, kRoundResultScale);
		m_font.Draw(text, Vector2(x, kRoundResultY), color, 0.0f, kRoundResultScale, kTopLeftPivot);
	}
	else if (m_mode == Mode::GameOver || m_mode == Mode::Victory)
	{
		bool isVictory = (m_mode == Mode::Victory);
		// 暗幕は上でFont::Begin()より前に描画済み。

		const wchar_t* bigText = isVictory ? kVictoryText : kGameOverText;
		const Vector4& bigColor = isVictory ? kVictoryColor : kGameOverColor;
		int bigCharCount = isVictory ? 8 : 9;
		float bigX = CenteredStartX(bigCharCount, kBigTitleScale);
		m_font.Draw(bigText, Vector2(bigX, kBigTitleY), bigColor, 0.0f, kBigTitleScale, kTopLeftPivot);

		wchar_t subLine[64];
		int subCharCount;
		if (isVictory)
		{
			// Victoryは「どこまで進んだか」ではなく全クリアそのものを祝う文言にする
			// (到達ラウンド表記だと常にkTotalRounds/kTotalRoundsで冗長なため)。
			swprintf_s(subLine, L"全%dラウンド制覇!", m_totalRounds);
			subCharCount = 1 /*"全"*/ * 2 + 3 /*"%d"相当*/ + 6 /*"ラウンド"*/ * 2 + 2 /*"制覇"*/ * 2 + 1 /*"!"*/;
		}
		else
		{
			swprintf_s(subLine, L"到達ラウンド %d / %d", m_reachedRound, m_totalRounds);
			// 全角文字は半角の約2倍幅として概算する。
			subCharCount = 6 /*"到達ラウンド"*/ * 2 + 1 /*" "*/ + 5 /*" %d / %d"相当の桁*/;
		}
		float subX = CenteredStartX(subCharCount, kSubScale);
		m_font.Draw(subLine, Vector2(subX, kSubY), kSubColor, 0.0f, kSubScale, kTopLeftPivot);

		// kBlinkIntervalSecごとに表示/非表示を切り替える(アルファブレンドが機能しないため)。
		int blinkPhase = (int)(m_elapsedTime / kBlinkIntervalSec);
		bool showPrompt = (blinkPhase % 2) == 0;
		if (showPrompt)
		{
			float promptX = CenteredStartX(19, kPromptScale); // "PRESS [A] TO TITLE" (半角19文字)。
			m_font.Draw(kPromptText, Vector2(promptX, kPromptY), kPromptColor, 0.0f, kPromptScale, kTopLeftPivot);
		}
	}

	m_font.End(rc);
}

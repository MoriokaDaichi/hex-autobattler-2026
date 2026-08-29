#include "stdafx.h"
#include "ResultUIRenderer.h"

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
}

void ResultUIRenderer::DrawRoundResult(RenderContext& rc, CombatResult result)
{
	m_mode = Mode::RoundResult;
	m_roundResult = result;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::DrawGameOver(RenderContext& rc, int reachedRound, int totalRounds, float deltaTime)
{
	if (m_mode != Mode::GameOver) m_elapsedTime = 0.0f; // 新規に突入したフレームで点滅タイマーをリセット。
	m_mode = Mode::GameOver;
	m_reachedRound = reachedRound;
	m_totalRounds = totalRounds;
	m_elapsedTime += deltaTime;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::DrawVictory(RenderContext& rc, float deltaTime)
{
	if (m_mode != Mode::Victory) m_elapsedTime = 0.0f;
	m_mode = Mode::Victory;
	m_elapsedTime += deltaTime;
	g_renderingEngine->AddRenderObject(this);
}

void ResultUIRenderer::OnRender2D(RenderContext& rc)
{
	if (m_mode == Mode::None) return;

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
		const wchar_t* bigText = isVictory ? kVictoryText : kGameOverText;
		const Vector4& bigColor = isVictory ? kVictoryColor : kGameOverColor;
		int bigCharCount = isVictory ? 8 : 9;
		float bigX = CenteredStartX(bigCharCount, kBigTitleScale);
		m_font.Draw(bigText, Vector2(bigX, kBigTitleY), bigColor, 0.0f, kBigTitleScale, kTopLeftPivot);

		wchar_t subLine[64];
		swprintf_s(subLine, L"到達ラウンド %d / %d", m_reachedRound, m_totalRounds);
		// 全角文字は半角の約2倍幅として概算する。
		int subCharCount = 8 /*"到達ラウンド"*/ * 2 + 1 /*" "*/ + 5 /*" %d / %d"相当の桁*/;
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

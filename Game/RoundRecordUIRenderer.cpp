#include "stdafx.h"
#include "RoundRecordUIRenderer.h"
#include "GameState.h"

namespace
{
	// 画面右上、FPS表示(k2EngineLow::EndFrame、左上 UI_SPACE_WIDTH*-0.48 / UI_SPACE_HEIGHT*0.48)と
	// 同じ余白の取り方で対角に配置する。
	const float kX = (float)UI_SPACE_WIDTH * 0.48f;
	const float kTopY = (float)UI_SPACE_HEIGHT * 0.48f;
	const float kStepY = 38.0f;

	const float kTitleScale = 0.56f;
	const float kLineScale = 0.48f;

	const Vector2 kTopRightPivot(1.0f, 1.0f); // テキストの右上をアンカーにする(右詰め表示)。

	const Vector4 kTitleColor(0.9f, 0.9f, 0.95f, 1.0f);
	const Vector4 kNormalColor(0.82f, 0.85f, 0.9f, 1.0f);

	// 連敗数に応じた警告色。0は通常色、kMaxLosses-1(あと1敗でゲームオーバー)は赤、その間は黄。
	Vector4 LossColor(int lossCount, int maxLosses)
	{
		if (lossCount <= 0) return kNormalColor;
		if (lossCount >= maxLosses - 1) return Vector4(1.0f, 0.4f, 0.35f, 1.0f);  // 赤
		return Vector4(0.98f, 0.85f, 0.3f, 1.0f);                                 // 黄
	}
}

void RoundRecordUIRenderer::Draw(RenderContext& rc, const GameState& gameState)
{
	m_totalRounds = GameState::kTotalRounds;
	// Victory到達時はroundNumberがkTotalRoundsを超えて進んでいるため、表示上はクランプする。
	m_roundNumber = gameState.roundNumber;
	if (m_roundNumber > m_totalRounds) m_roundNumber = m_totalRounds;

	m_lossCount = gameState.lossCount;
	m_maxLosses = GameState::kMaxLossesPerEnemy;
	m_hasData = true;

	g_renderingEngine->AddRenderObject(this);
}

void RoundRecordUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData) return;

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	int remaining = m_totalRounds - m_roundNumber;
	if (remaining < 0) remaining = 0;

	wchar_t roundLine[64];
	swprintf_s(roundLine, L"ROUND %d / %d", m_roundNumber, m_totalRounds);
	m_font.Draw(roundLine, Vector2(kX, kTopY), kTitleColor, 0.0f, kTitleScale, kTopRightPivot);

	wchar_t remainLine[64];
	swprintf_s(remainLine, L"残り %d ラウンド", remaining);
	m_font.Draw(remainLine, Vector2(kX, kTopY - kStepY), kNormalColor, 0.0f, kLineScale, kTopRightPivot);

	wchar_t lossLine[64];
	swprintf_s(lossLine, L"連敗 %d / %d", m_lossCount, m_maxLosses);
	m_font.Draw(lossLine, Vector2(kX, kTopY - kStepY * 2.0f), LossColor(m_lossCount, m_maxLosses), 0.0f, kLineScale, kTopRightPivot);

	m_font.End(rc);
}

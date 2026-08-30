#include "stdafx.h"
#include "TitleUIRenderer.h"

namespace
{
	// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。画面中央に大きくタイトル、
	// その下に操作ガイドを配置する。
	//
	// このFontEngineのpivotは文字列幅で正規化されたアンカーではなく生のピクセルオフセットでしか
	// ないため(MeasureString相当が無い)、(0.5,0.5)指定では真の中央揃えにならない
	// (RoundRecordUIRenderer/ShopUIRenderer/BoardUIRendererでの既知の教訓)。
	// 他のUIと同様kTopLeftPivot前提とし、固定文字列の見た目の長さに合わせて開始X座標を
	// 実機で見た目を確認しながら手動調整している。
	const Vector2 kTopLeftPivot(0.0f, 1.0f);

	const float kTitleY = 120.0f;
	const float kTitleScale = 1.8f;
	const float kTitleStartX = -190.0f; // "HEX ARENA"がこのスケールで中央付近に来るよう調整した値。
	const Vector4 kTitleColor(1.00f, 0.85f, 0.35f, 1.0f); // ショップのコストTier5(金)と同系色。

	const float kPromptY = -120.0f;
	const float kPromptScale = 0.66f;
	const float kPromptStartX = -135.0f; // "PRESS [A] TO START"がこのスケールで中央付近に来るよう調整した値。

	// タイトル名は"HEX ARENA"に正式決定済み(2026-08-30、ユーザー確認済み)。
	const wchar_t* kTitleText = L"HEX ARENA";
	const wchar_t* kPromptText = L"PRESS [A] TO START";

	// このFontEngineの描画パイプラインはアルファブレンドが機能しない(常に不透明)ため、
	// アルファ値でのフェードではなく、一定間隔で描画そのものをスキップする方式で点滅させる。
	const float kBlinkIntervalSec = 0.5f; // この秒数ごとに表示/非表示を切り替える。
}

void TitleUIRenderer::Draw(RenderContext& rc, float deltaTime)
{
	m_elapsedTime += deltaTime;
	g_renderingEngine->AddRenderObject(this);
}

void TitleUIRenderer::OnRender2D(RenderContext& rc)
{
	m_font.SetShadowParam(true, 3.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	m_font.Draw(kTitleText, Vector2(kTitleStartX, kTitleY), kTitleColor, 0.0f, kTitleScale, kTopLeftPivot);

	// kBlinkIntervalSecごとに表示/非表示を切り替える(アルファブレンドが機能しないため)。
	int blinkPhase = (int)(m_elapsedTime / kBlinkIntervalSec);
	bool showPrompt = (blinkPhase % 2) == 0;
	if (showPrompt)
	{
		m_font.Draw(kPromptText, Vector2(kPromptStartX, kPromptY), Vector4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, kPromptScale, kTopLeftPivot);
	}

	m_font.End(rc);
}

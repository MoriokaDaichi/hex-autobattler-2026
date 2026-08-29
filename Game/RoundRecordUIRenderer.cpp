#include "stdafx.h"
#include "RoundRecordUIRenderer.h"
#include "GameState.h"

namespace
{
	// 画面右上に配置する。
	//
	// [重要] Font::Draw/FontEngine::Draw の pivot は、DirectXTK SpriteFont::DrawString の
	// origin(未スケールのテキストローカルピクセル単位のオフセット)へ pivot の値をそのまま
	// 渡しているだけで、テキスト幅で正規化されたアンカー(0〜1の割合)ではない。つまり
	// pivot=(1,1) を指定しても「右上を基準に右詰め」にはならず、ほぼ無視できる1px程度の
	// オフセットにしかならない(このFontEngineにはMeasureString相当の文字列幅取得手段が
	// 無く、真の右寄せは未対応)。そのため実質的には常に左詰め(pivot.x=0相当)として
	// 描画される。当初 pivot=(1,1) かつ右端ギリギリのkXで配置していたところ、テキストが
	// そのまま右へ伸びて画面外へはみ出し、ほぼ全文字が見切れる不具合が起きた
	// (詳細: デバッグ確認セッションでの実機確認により判明)。
	//
	// 対策として、BoardUIRenderer のベンチ一覧(kTopLeftPivot)と同様に実質左詰めとして扱い、
	// 最長行("ROUND 10 / 10"、2桁ラウンド時)でも画面右端(UI空間 x=+960)内に収まるよう、
	// 開始X座標に十分な余白を確保する。
	//
	// Y座標: PlayerStatusUIRenderer(GOLD/LV、y≒454〜500)が画面右上の上段を占有しているため、
	// こちらはその下段に積む。両者が縦に重ならないよう kTopY を下げてある
	// (以前は 0.48f=y≒518 で PlayerStatusUI と座標域が丸かぶりしていた)。
	const float kX = (float)UI_SPACE_WIDTH * 0.27f;
	const float kTopY = (float)UI_SPACE_HEIGHT * 0.36f;
	const float kStepY = 38.0f;

	const float kTitleScale = 0.56f;
	const float kLineScale = 0.48f;

	const Vector2 kTopLeftPivot(0.0f, 1.0f); // 実質的な左上アンカー(BoardUIRendererと同じ扱い)。

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
	m_font.Draw(roundLine, Vector2(kX, kTopY), kTitleColor, 0.0f, kTitleScale, kTopLeftPivot);

	wchar_t remainLine[64];
	swprintf_s(remainLine, L"残り %d ラウンド", remaining);
	m_font.Draw(remainLine, Vector2(kX, kTopY - kStepY), kNormalColor, 0.0f, kLineScale, kTopLeftPivot);

	wchar_t lossLine[64];
	swprintf_s(lossLine, L"連敗 %d / %d", m_lossCount, m_maxLosses);
	m_font.Draw(lossLine, Vector2(kX, kTopY - kStepY * 2.0f), LossColor(m_lossCount, m_maxLosses), 0.0f, kLineScale, kTopLeftPivot);

	m_font.End(rc);
}

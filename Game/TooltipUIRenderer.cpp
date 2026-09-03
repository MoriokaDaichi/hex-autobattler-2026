#include "stdafx.h"
#include "TooltipUIRenderer.h"
#include "UIRectRenderer.h"
#include "UITextUtil.h"

namespace
{
	const float kLineHeight = 26.0f;
	const float kTitleLineHeight = 30.0f; // 1行目(タイトル扱い)はやや行間を広げる。
	const float kPaddingX = 16.0f;
	const float kPaddingY = 12.0f;
	const float kTextScale = 0.46f;
	const float kTitleScale = 0.52f;

	// 行幅は UITextUtil::EstimateTextWidth(myfile.spritefontの実測advance: 半角22px/全角44px)で
	// 正確に見積もる。以前は1字30pxの当て推量で、係数を上げてもGOLD等の短い行がパネルからはみ出して
	// 切れる不具合が残っていた(ui-mouse-cardsフェーズ3フォローアップ: F5フィードバック是正3、要対応3と同根)。
	// 実測ベースに影/カーニングのブレ分の小さめの安全係数だけ掛ける。
	const float kWidthSafetyFactor = 1.06f;
	const float kMinLineWidthPx = 120.0f; // 短い1行でも最低限これだけの幅を確保する。

	const Vector2 kTopLeftPivot(0.0f, 1.0f);
	const Vector2 kCenterPivot(0.5f, 0.5f);

	const Vector4 kBgColor(0.06f, 0.06f, 0.08f, 0.94f);
	const Vector4 kBorderColor(0.55f, 0.58f, 0.65f, 0.95f);
	const float kBorderThickness = 2.0f;
	const Vector4 kTitleColor(1.0f, 0.92f, 0.6f, 1.0f);
	const Vector4 kBodyColor(0.88f, 0.90f, 0.94f, 1.0f);

	// 空でない行はkMinLineWidthPxを下限にする(短い1行の過小見積り対策)。
	float EstimateWidth(const std::wstring& text, float scale)
	{
		if (text.empty()) return 0.0f;

		float width = UITextUtil::EstimateTextWidth(text, scale) * kWidthSafetyFactor;

		float minWidth = kMinLineWidthPx * scale;
		return (width < minWidth) ? minWidth : width;
	}
}

void TooltipUIRenderer::Draw(RenderContext& rc, const std::vector<std::wstring>& lines, const Vector2& anchor, UIRectRenderer& rectRenderer)
{
	m_lines = lines;
	m_anchor = anchor;
	m_rectRenderer = &rectRenderer;
	m_hasData = !m_lines.empty();

	if (!m_hasData)
	{
		return; // 表示すべき内容が無い。2D描画パスにも登録しない。
	}

	g_renderingEngine->AddRenderObject(this);
}

void TooltipUIRenderer::OnRender2D(RenderContext& rc)
{
	if (!m_hasData || m_rectRenderer == nullptr) return;

	// パネルサイズを、行数×行高 + パディングと、最長行の概算幅から決める。
	float maxTextWidth = 0.0f;
	float contentHeight = 0.0f;
	for (size_t i = 0; i < m_lines.size(); ++i)
	{
		float scale = (i == 0) ? kTitleScale : kTextScale;
		float w = EstimateWidth(m_lines[i], scale);
		if (w > maxTextWidth) maxTextWidth = w;
		contentHeight += (i == 0) ? kTitleLineHeight : kLineHeight;
	}

	float panelWidth = maxTextWidth + kPaddingX * 2.0f;
	float panelHeight = contentHeight + kPaddingY * 2.0f;

	// アンカーの右下にオフセットして配置し、画面端(UI_SPACE ±半幅・半高)をはみ出さないようクランプする。
	const float kOffsetX = 20.0f;
	const float kOffsetY = 20.0f;
	float left = m_anchor.x + kOffsetX;
	float top = m_anchor.y - kOffsetY;

	float halfW = (float)UI_SPACE_WIDTH * 0.5f;
	float halfH = (float)UI_SPACE_HEIGHT * 0.5f;

	if (left + panelWidth > halfW)
	{
		left = m_anchor.x - kOffsetX - panelWidth; // 右にはみ出るなら、アンカーの左側に出す。
	}
	if (top - panelHeight < -halfH)
	{
		top = m_anchor.y + kOffsetY; // 下にはみ出るなら、アンカーの上側に出す。
	}

	// フリップ後(あるいはEstimateWidth()の概算誤差)でもまだ画面外に出ている場合に備え、
	// 最後に4辺を無条件でピン留めする(実機検証で判明: フリップだけでは画面下端・右端で
	// パネルが切れるケースが残っていた。左右クランプに揃えて上下も同様に補強する)。
	if (left < -halfW) left = -halfW;
	if (left + panelWidth > halfW) left = halfW - panelWidth;
	if (top > halfH) top = halfH;
	if (top - panelHeight < -halfH) top = -halfH + panelHeight;

	Vector2 panelCenter(left + panelWidth * 0.5f, top - panelHeight * 0.5f);

	// 矩形(Sprite)はFont::Begin()〜End()の外側でまとめて描き終える(SpriteBatchの状態と
	// 競合するため。docs/tasks/ui-sprite-bars/plan.md §0-8)。枠→内側の塗りの順で、枠が縁取りに見える。
	Vector2 borderSize(panelWidth + kBorderThickness * 2.0f, panelHeight + kBorderThickness * 2.0f);
	m_rectRenderer->DrawRect(rc, panelCenter, borderSize, kBorderColor, kCenterPivot);
	m_rectRenderer->DrawRect(rc, panelCenter, Vector2(panelWidth, panelHeight), kBgColor, kCenterPivot);

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	float y = top - kPaddingY;
	for (size_t i = 0; i < m_lines.size(); ++i)
	{
		bool isTitle = (i == 0);
		const Vector4& color = isTitle ? kTitleColor : kBodyColor;
		float scale = isTitle ? kTitleScale : kTextScale;
		m_font.Draw(m_lines[i].c_str(), Vector2(left + kPaddingX, y), color, 0.0f, scale, kTopLeftPivot);
		y -= isTitle ? kTitleLineHeight : kLineHeight;
	}

	m_font.End(rc);
}

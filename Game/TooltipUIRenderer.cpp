#include "stdafx.h"
#include "TooltipUIRenderer.h"
#include "UIRectRenderer.h"

namespace
{
	const float kLineHeight = 26.0f;
	const float kTitleLineHeight = 30.0f; // 1行目(タイトル扱い)はやや行間を広げる。
	const float kPaddingX = 16.0f;
	const float kPaddingY = 12.0f;
	const float kTextScale = 0.46f;
	const float kTitleScale = 0.52f;

	// MeasureString相当が無いため、1文字あたりの見た目幅を概算する。ResultUIRendererの
	// CenteredStartXで校正した「23px(半角換算、スケール1.0基準)」を出発点にしていたが、
	// 実機検証(F5)でGOLD等の短い1行テキストがこの見積りでは実描画幅より狭くなり、
	// パネル右端を画面端にピン留めしてもテキスト自体が画面外へはみ出して切れる不具合が出た。
	// ツールチップは「多少大きく見積もって余白が広がる」方が「切れる」より安全なため、
	// 過大見積り側に倒す(係数を上げる+最小幅フロアを設ける)。
	const float kApproxCharWidthAtScale1 = 30.0f; // 23 -> 30(実機検証結果を受けて引き上げ)。
	const float kMinLineWidthPx = 140.0f;         // 短い1行でも最低限これだけの幅を確保する。

	const Vector2 kTopLeftPivot(0.0f, 1.0f);
	const Vector2 kCenterPivot(0.5f, 0.5f);

	const Vector4 kBgColor(0.06f, 0.06f, 0.08f, 0.94f);
	const Vector4 kBorderColor(0.55f, 0.58f, 0.65f, 0.95f);
	const float kBorderThickness = 2.0f;
	const Vector4 kTitleColor(1.0f, 0.92f, 0.6f, 1.0f);
	const Vector4 kBodyColor(0.88f, 0.90f, 0.94f, 1.0f);

	// 半角=1、全角(0x00A0を超えるほぼ全ての文字)=2として、見た目の文字数を概算する。
	// 空でない行はkMinLineWidthPxを下限にする(短い1行の過小見積り対策)。
	float EstimateWidth(const std::wstring& text, float scale)
	{
		if (text.empty()) return 0.0f;

		float count = 0.0f;
		for (wchar_t ch : text)
		{
			count += (ch > 0x00FF) ? 2.0f : 1.0f;
		}
		float width = count * kApproxCharWidthAtScale1 * scale;

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

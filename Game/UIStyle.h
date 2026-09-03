#pragma once

/// <summary>
/// 全UIレンダラーで統一するカードパネルの色定数(ui-mouse-cardsフェーズ3、plan.md §4-1)。
/// 各Rendererは`UIRectRenderer::DrawPanel()`と組み合わせて使う。個別に色を決めると
/// バラバラな見た目になるため、統一感のためここへ集約した(ヘッダオンリー、`StatEffect.h`と同じ方針)。
/// </summary>
namespace UIStyle
{
	// 通常のカードパネル(ショップ枠・ベンチ行・アイテム行・トレイトパネル・右上HUD等)。
	const Vector4 kPanelFillColor(0.10f, 0.10f, 0.14f, 0.82f);
	const Vector4 kPanelBorderColor(0.42f, 0.45f, 0.52f, 0.9f);
	const float kPanelBorderThickness = 2.0f;

	// 選択中/ホバー中の枠(明るくして強調する)。
	const Vector4 kSelectedBorderColor(1.00f, 0.85f, 0.35f, 1.0f);   // 金色寄り(選択=カーソル)。
	const Vector4 kHoveredBorderColor(0.55f, 0.85f, 1.00f, 1.0f);    // 水色寄り(マウスホバー)。
	const float kSelectedBorderThickness = 3.0f;

	// 行の区切り線(トレイトパネル等、薄い1px相当の矩形)。
	const Vector4 kDividerColor(0.35f, 0.37f, 0.42f, 0.6f);
}

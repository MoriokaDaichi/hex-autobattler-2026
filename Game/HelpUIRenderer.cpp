#include "stdafx.h"
#include "HelpUIRenderer.h"
#include "UIRectRenderer.h"
#include "UIInteractionSystem.h"
#include "UITextUtil.h"
#include "UIStyle.h"

namespace
{
	// --- 座標系: UI_SPACE(1920x1080、中央原点・y上向き)。配置の根拠は docs/tasks/help-panel/plan.md §2 ---

	const Vector2 kTopLeftPivot(0.0f, 1.0f);
	const Vector2 kCenterPivot(0.5f, 0.5f);

	// ラベルを矩形の縦中央に載せるためのオフセット(ShopUIRendererと同じ考え方:
	// myfile.spritefontのcap height ≒ 33px(scale1.0)の半分だけ、テキスト上端を中心より上に置く)。
	float LabelYOffset(float scale) { return 33.0f * 0.5f * scale; }

	// --- ヘルプボタン(左上隅。Debug時のFPS表示(y≒+518〜+478)の下、BENCH見出し(y=+250)の上) ---
	const Vector2 kHelpButtonCenter(-850.0f, 436.0f);
	const Vector2 kHelpButtonSize(200.0f, 44.0f);
	const float kHelpButtonLabelScale = 0.6f;
	const wchar_t* const kHelpButtonLabel = L"ヘルプ [F1]";
	const Vector4 kHelpButtonColor(0.16f, 0.30f, 0.46f, 0.92f);
	const Vector4 kHelpButtonOpenColor(0.30f, 0.48f, 0.70f, 0.95f);

	// --- 暗幕・パネル本体 ---
	const Vector4 kDimColor(0.0f, 0.0f, 0.0f, 0.6f);
	const Vector2 kPanelCenter(0.0f, 10.0f);
	const Vector2 kPanelSize(1440.0f, 860.0f); // x:-720〜+720, y:-420〜+440
	const Vector4 kPanelFillColor(0.07f, 0.08f, 0.11f, 0.97f);
	const float kPanelBorderThickness = 3.0f;

	// 見出し・操作ヒント
	const Vector2 kTitlePos(-690.0f, 430.0f);
	const float kTitleScale = 0.8f;
	const Vector2 kHintPos(-520.0f, 414.0f);
	const float kHintScale = 0.42f;
	const wchar_t* const kHintText = L"上下: 項目   左右: ページ   Esc / F1 / H: 閉じる";

	// 閉じるボタン(右上)
	const Vector2 kCloseButtonCenter(620.0f, 400.0f);
	const Vector2 kCloseButtonSize(160.0f, 44.0f);
	const float kCloseLabelScale = 0.5f;
	const wchar_t* const kCloseLabel = L"閉じる [Esc]";

	// カテゴリタブ(左列、縦並び)
	const float kTabCenterX = -590.0f;
	const float kTabTopCenterY = 330.0f;
	const float kTabStepY = 68.0f;
	const Vector2 kTabSize(230.0f, 56.0f);
	const float kTabLabelScale = 0.6f;
	const Vector4 kTabColor(0.16f, 0.17f, 0.22f, 0.95f);
	const Vector4 kTabSelectedColor(0.34f, 0.29f, 0.12f, 0.97f);

	// タブと本文の区切り線
	const float kDividerX = -460.0f;
	const float kDividerTopY = 360.0f;
	const float kDividerBottomY = -350.0f;

	// 本文
	const float kContentLeftX = -430.0f;
	const float kContentRightX = 700.0f;
	const float kContentMaxWidth = kContentRightX - kContentLeftX - 10.0f; // 折り返し幅。
	const float kCategoryTitleY = 384.0f;
	const float kCategoryTitleScale = 0.66f;
	const float kLineTopY = 330.0f;
	const float kLineStepY = 32.0f;
	const float kLineScale = 0.5f;
	const int kLinesPerPage = 20; // 最終行の上端 y = 330 - 19*32 = -278(ページ送り行 y≒-360より上)。
	const int kHeadingKeepWithNext = 2; // 見出しの後に最低この行数が同じページに入らなければ次ページへ送る。

	// ページ送り(本文エリアの中央 x≒135 を基準に左右へ)
	const float kPagerY = -382.0f;
	const float kPrevButtonX = -60.0f;
	const float kNextButtonX = 330.0f;
	const float kPageLabelX = 135.0f;
	const Vector2 kPagerButtonSize(160.0f, 44.0f);
	const float kPagerLabelScale = 0.5f;

	// ボタン共通色
	const Vector4 kButtonColor(0.28f, 0.28f, 0.34f, 0.95f);
	const Vector4 kButtonDisabledColor(0.16f, 0.16f, 0.19f, 0.9f);
	const Vector4 kButtonLabelColor(0.92f, 0.92f, 0.95f, 1.0f);
	const Vector4 kButtonLabelDisabledColor(0.45f, 0.45f, 0.50f, 1.0f);

	// 文字色
	const Vector4 kTitleColor(1.0f, 0.92f, 0.6f, 1.0f);
	const Vector4 kHintColor(0.70f, 0.73f, 0.80f, 1.0f);
	const Vector4 kHeadingColor(0.55f, 0.85f, 1.00f, 1.0f);
	const Vector4 kBodyColor(0.90f, 0.91f, 0.94f, 1.0f);

	Vector2 TabCenter(int index)
	{
		return Vector2(kTabCenterX, kTabTopCenterY - kTabStepY * (float)index);
	}

	UIHotRegion MakeRegion(UIRegionKind kind, const Vector2& center, const Vector2& size, int index = -1)
	{
		UIHotRegion region;
		region.kind = kind;
		region.index = index;
		region.minX = center.x - size.x * 0.5f;
		region.maxX = center.x + size.x * 0.5f;
		region.minY = center.y - size.y * 0.5f;
		region.maxY = center.y + size.y * 0.5f;
		return region;
	}

	bool IsAsciiWordChar(wchar_t ch)
	{
		return (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z');
	}
}

void HelpUIRenderer::Init(const TraitDatabase& traitDatabase, const ItemDatabase& itemDatabase)
{
	m_categories.clear();

	std::vector<HelpContent::Category> categories = HelpContent::Build(traitDatabase, itemDatabase);
	for (const auto& category : categories)
	{
		// 1) 本文幅で折り返す。
		std::vector<HelpContent::Line> wrapped;
		for (const auto& line : category.lines)
		{
			WrapLine(line, wrapped);
		}

		// 2) ページに分割する。ページ先頭の空行は捨て、見出しがページ末尾に取り残される場合は次ページへ送る。
		CategoryPages pages;
		pages.title = category.title;
		pages.pages.emplace_back();
		for (const auto& line : wrapped)
		{
			auto* page = &pages.pages.back();
			int remaining = kLinesPerPage - (int)page->size();
			bool needNewPage = (remaining <= 0)
				|| (line.heading && !page->empty() && remaining <= kHeadingKeepWithNext);
			if (needNewPage)
			{
				pages.pages.emplace_back();
				page = &pages.pages.back();
			}
			if (page->empty() && line.text.empty())
			{
				continue; // ページ先頭の空行。
			}
			page->push_back(line);
		}
		// 末尾の空行だけのページが残らないよう、空ページは(1ページ目以外)取り除く。
		while (pages.pages.size() > 1 && pages.pages.back().empty())
		{
			pages.pages.pop_back();
		}
		m_categories.push_back(std::move(pages));
	}

	m_category = 0;
	m_page = 0;
}

void HelpUIRenderer::WrapLine(const HelpContent::Line& line, std::vector<HelpContent::Line>& out)
{
	if (UITextUtil::EstimateTextWidth(line.text, kLineScale) <= kContentMaxWidth)
	{
		out.push_back(line);
		return;
	}

	// 続き行の字下げ: 元の行頭の半角スペース + 箇条書き("・")なら全角1字ぶん(半角2個)。
	size_t leadingSpaces = 0;
	while (leadingSpaces < line.text.size() && line.text[leadingSpaces] == L' ') ++leadingSpaces;
	std::wstring indent(leadingSpaces, L' ');
	if (leadingSpaces < line.text.size() && line.text[leadingSpaces] == L'・')
	{
		indent += L"  ";
	}

	std::wstring rest = line.text;
	bool first = true;
	while (!rest.empty())
	{
		std::wstring prefix = first ? std::wstring() : indent;
		float width = UITextUtil::EstimateTextWidth(prefix, kLineScale);

		// 収まる最大の文字数を求める。
		size_t count = 0;
		while (count < rest.size())
		{
			float w = ((rest[count] > 0x00FF) ? 44.0f : 22.0f) * kLineScale;
			if (width + w > kContentMaxWidth) break;
			width += w;
			++count;
		}

		if (count < rest.size())
		{
			if (count == 0) count = 1; // 念のため(1文字も入らない幅にはならない)。

			// 英単語・数字の途中で切らないよう、直前の区切りまで戻す(戻りすぎる場合はそのまま切る)。
			if (IsAsciiWordChar(rest[count]) && IsAsciiWordChar(rest[count - 1]))
			{
				size_t back = count;
				while (back > 0 && IsAsciiWordChar(rest[back - 1])) --back;
				if (back > count / 2) count = back;
			}
		}

		HelpContent::Line piece;
		piece.heading = line.heading;
		piece.text = prefix + rest.substr(0, count);
		out.push_back(piece);

		rest = rest.substr(count);
		// 続き行の先頭の半角スペースは詰める(字下げはindentで付ける)。
		size_t trim = 0;
		while (trim < rest.size() && rest[trim] == L' ') ++trim;
		rest = rest.substr(trim);
		first = false;
	}
}

int HelpUIRenderer::PageCount() const
{
	if (m_category < 0 || m_category >= (int)m_categories.size()) return 1;
	return (int)m_categories[m_category].pages.size();
}

void HelpUIRenderer::SetCategory(int category)
{
	if (m_categories.empty()) return;
	int count = (int)m_categories.size();
	m_category = ((category % count) + count) % count; // 上下キーでの循環に対応。
	m_page = 0;
}

void HelpUIRenderer::ChangePage(int delta)
{
	int next = m_page + delta;
	if (next < 0 || next >= PageCount()) return;
	m_page = next;
}

bool HelpUIRenderer::IsHovered(UIRegionKind kind, int index) const
{
	return m_hasHovered && m_hovered.kind == kind && (index < 0 || m_hovered.index == index);
}

void HelpUIRenderer::BuildHotRegions(UIHotRegionList& out) const
{
	if (!m_open)
	{
		// 末尾に追加する(UIInteractionSystemは後ろから探索するため、他の領域と重なってもこちらが優先)。
		out.push_back(MakeRegion(UIRegionKind::HelpButton, kHelpButtonCenter, kHelpButtonSize));
		return;
	}

	// パネル表示中は盤面・ショップ等の領域を全て外し、ヘルプ用の領域だけにする。
	out.clear();

	// 全画面のブロッカー(最初に積む=最も低い優先度)。パネル外のクリックはこれに吸われて何も起きない。
	out.push_back(MakeRegion(UIRegionKind::HelpBlocker, Vector2(0.0f, 0.0f),
		Vector2((float)UI_SPACE_WIDTH, (float)UI_SPACE_HEIGHT)));

	out.push_back(MakeRegion(UIRegionKind::HelpButton, kHelpButtonCenter, kHelpButtonSize));
	out.push_back(MakeRegion(UIRegionKind::HelpClose, kCloseButtonCenter, kCloseButtonSize));
	for (int i = 0; i < (int)m_categories.size(); ++i)
	{
		out.push_back(MakeRegion(UIRegionKind::HelpCategoryTab, TabCenter(i), kTabSize, i));
	}
	out.push_back(MakeRegion(UIRegionKind::HelpPrevPage, Vector2(kPrevButtonX, kPagerY), kPagerButtonSize));
	out.push_back(MakeRegion(UIRegionKind::HelpNextPage, Vector2(kNextButtonX, kPagerY), kPagerButtonSize));
}

void HelpUIRenderer::UpdateInput(const UIInteractionSystem& uiInteraction)
{
	m_hasHovered = uiInteraction.GetHovered(m_hovered);

	UIHotRegion clicked;
	bool hasClick = uiInteraction.GetLeftClicked(clicked);

	// 開閉キー: F1 / H(キーボードを直接読む)。
	bool toggleKey = g_keyboard->IsTrigger(VK_F1) || g_keyboard->IsTrigger('H');

	if (!m_open)
	{
		if (toggleKey || (hasClick && clicked.kind == UIRegionKind::HelpButton))
		{
			m_open = true;
		}
		return;
	}

	// --- 以下、パネル表示中 ---
	bool closeKey = toggleKey || g_keyboard->IsTrigger(VK_ESCAPE);
	if (closeKey)
	{
		m_open = false;
		return;
	}

	if (hasClick)
	{
		switch (clicked.kind)
		{
		case UIRegionKind::HelpButton:
		case UIRegionKind::HelpClose:
			m_open = false;
			return;
		case UIRegionKind::HelpCategoryTab:
			SetCategory(clicked.index);
			break;
		case UIRegionKind::HelpPrevPage:
			ChangePage(-1);
			break;
		case UIRegionKind::HelpNextPage:
			ChangePage(+1);
			break;
		default:
			break; // HelpBlocker等: 何もしない(パネル外クリックで閉じたり盤面を操作したりしない)。
		}
	}

	// 上下: カテゴリ、左右: ページ(キーボードの矢印キー)。
	if (g_keyboard->IsTrigger(VK_UP))
	{
		SetCategory(m_category - 1);
	}
	else if (g_keyboard->IsTrigger(VK_DOWN))
	{
		SetCategory(m_category + 1);
	}
	if (g_keyboard->IsTrigger(VK_LEFT))
	{
		ChangePage(-1);
	}
	else if (g_keyboard->IsTrigger(VK_RIGHT))
	{
		ChangePage(+1);
	}
}

void HelpUIRenderer::Draw(RenderContext& rc, UIRectRenderer& rectRenderer)
{
	m_rectRenderer = &rectRenderer;
	g_renderingEngine->AddRenderObject(this);
}

void HelpUIRenderer::OnRender2D(RenderContext& rc)
{
	if (m_rectRenderer == nullptr) return;

	const bool hasCategory = (m_category >= 0 && m_category < (int)m_categories.size());
	const int pageCount = PageCount();
	const bool canPrev = m_page > 0;
	const bool canNext = m_page + 1 < pageCount;

	// --- 矩形(Font::Begin()より前にまとめて描く。UIRectRenderer.hの注意参照) ---
	if (m_open)
	{
		m_rectRenderer->DrawRect(rc, Vector2(0.0f, 0.0f), Vector2((float)UI_SPACE_WIDTH, (float)UI_SPACE_HEIGHT), kDimColor, kCenterPivot);
		m_rectRenderer->DrawPanel(rc, kPanelCenter, kPanelSize, kPanelFillColor,
			UIStyle::kPanelBorderColor, kPanelBorderThickness, kCenterPivot);

		// カテゴリタブ
		for (int i = 0; i < (int)m_categories.size(); ++i)
		{
			bool selected = (i == m_category);
			bool hovered = IsHovered(UIRegionKind::HelpCategoryTab, i);
			Vector4 border = selected ? UIStyle::kSelectedBorderColor
				: hovered ? UIStyle::kHoveredBorderColor : UIStyle::kPanelBorderColor;
			float thickness = selected ? UIStyle::kSelectedBorderThickness : UIStyle::kPanelBorderThickness;
			m_rectRenderer->DrawPanel(rc, TabCenter(i), kTabSize, selected ? kTabSelectedColor : kTabColor,
				border, thickness, kCenterPivot);
		}

		// 区切り線
		m_rectRenderer->DrawRect(rc, Vector2(kDividerX, (kDividerTopY + kDividerBottomY) * 0.5f),
			Vector2(2.0f, kDividerTopY - kDividerBottomY), UIStyle::kDividerColor, kCenterPivot);

		// 閉じる・ページ送りボタン
		auto drawButton = [&](UIRegionKind kind, const Vector2& center, const Vector2& size, bool enabled)
		{
			Vector4 border = (enabled && IsHovered(kind)) ? UIStyle::kHoveredBorderColor : UIStyle::kPanelBorderColor;
			m_rectRenderer->DrawPanel(rc, center, size, enabled ? kButtonColor : kButtonDisabledColor,
				border, UIStyle::kPanelBorderThickness, kCenterPivot);
		};
		drawButton(UIRegionKind::HelpClose, kCloseButtonCenter, kCloseButtonSize, true);
		drawButton(UIRegionKind::HelpPrevPage, Vector2(kPrevButtonX, kPagerY), kPagerButtonSize, canPrev);
		drawButton(UIRegionKind::HelpNextPage, Vector2(kNextButtonX, kPagerY), kPagerButtonSize, canNext);
	}

	// ヘルプボタン(常時。開いている間は明るい色で「開いている」ことを示す)
	{
		bool hovered = IsHovered(UIRegionKind::HelpButton);
		m_rectRenderer->DrawPanel(rc, kHelpButtonCenter, kHelpButtonSize,
			m_open ? kHelpButtonOpenColor : kHelpButtonColor,
			hovered ? UIStyle::kHoveredBorderColor : UIStyle::kPanelBorderColor,
			UIStyle::kPanelBorderThickness, kCenterPivot);
	}

	// --- 文字 ---
	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	auto drawCenteredLabel = [&](const wchar_t* text, const Vector2& center, float scale, const Vector4& color)
	{
		float width = UITextUtil::EstimateTextWidth(text, scale);
		m_font.Draw(text, Vector2(center.x - width * 0.5f, center.y + LabelYOffset(scale)), color, 0.0f, scale, kTopLeftPivot);
	};

	drawCenteredLabel(kHelpButtonLabel, kHelpButtonCenter, kHelpButtonLabelScale, kButtonLabelColor);

	if (m_open)
	{
		m_font.Draw(L"ヘルプ", kTitlePos, kTitleColor, 0.0f, kTitleScale, kTopLeftPivot);
		m_font.Draw(kHintText, kHintPos, kHintColor, 0.0f, kHintScale, kTopLeftPivot);
		drawCenteredLabel(kCloseLabel, kCloseButtonCenter, kCloseLabelScale, kButtonLabelColor);

		for (int i = 0; i < (int)m_categories.size(); ++i)
		{
			bool selected = (i == m_category);
			drawCenteredLabel(m_categories[i].title.c_str(), TabCenter(i), kTabLabelScale,
				selected ? kTitleColor : kButtonLabelColor);
		}

		if (hasCategory)
		{
			const CategoryPages& category = m_categories[m_category];
			m_font.Draw(category.title.c_str(), Vector2(kContentLeftX, kCategoryTitleY), kTitleColor, 0.0f,
				kCategoryTitleScale, kTopLeftPivot);

			if (m_page >= 0 && m_page < (int)category.pages.size())
			{
				const auto& page = category.pages[m_page];
				for (size_t i = 0; i < page.size(); ++i)
				{
					const HelpContent::Line& line = page[i];
					if (line.text.empty()) continue;
					m_font.Draw(line.text.c_str(), Vector2(kContentLeftX, kLineTopY - kLineStepY * (float)i),
						line.heading ? kHeadingColor : kBodyColor, 0.0f, kLineScale, kTopLeftPivot);
				}
			}
		}

		drawCenteredLabel(L"< 前へ", Vector2(kPrevButtonX, kPagerY), kPagerLabelScale,
			canPrev ? kButtonLabelColor : kButtonLabelDisabledColor);
		drawCenteredLabel(L"次へ >", Vector2(kNextButtonX, kPagerY), kPagerLabelScale,
			canNext ? kButtonLabelColor : kButtonLabelDisabledColor);

		wchar_t pageLabel[32];
		swprintf_s(pageLabel, L"%d / %d", m_page + 1, pageCount);
		drawCenteredLabel(pageLabel, Vector2(kPageLabelX, kPagerY), kPagerLabelScale, kHintColor);
	}

	m_font.End(rc);
}

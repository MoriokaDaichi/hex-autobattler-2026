#pragma once
#include <string>
#include "StatEffect.h"
#include "TraitType.h"

/// <summary>
/// UI表示用の短いテキストへ変換する共通ヘルパー群。元々ShopUIRenderer.cpp(TraitName)/
/// ItemInventoryUIRenderer.cpp(EffectShortText)に個別実装されていたが、フェーズ2の
/// TooltipContentBuilderからも同じ変換が必要になったため、重複実装を避けてここへ集約した
/// (docs/tasks/ui-mouse-cards/plan.md §3-2・§3-4)。
/// </summary>
namespace UITextUtil
{
	/// <summary>StatEffect 1つを "AT+10" / "HP+20%" のような短いテキストにする。</summary>
	std::wstring EffectShortText(const StatEffect& e);

	/// <summary>トレイトの日本語名を返す。未知の値は"?"。</summary>
	const wchar_t* TraitName(TraitType type);

	/// <summary>
	/// テキストの描画幅(px)を概算する。このFontEngineにはMeasureString相当が無いため、
	/// 同梱のmyfile.spritefontを実測した固定advance ―― 半角=22px / 全角=44px(scale=1.0基準)――
	/// から算出する(k2EngineLow/DirectXTK/Src/SpriteFont.cpp のForEachGlyphで、このフォントは
	/// XOffset+rectWidth+XAdvance が全ASCII字で一律22px、全CJK字で一律44pxになっている)。
	/// カード/ツールチップのパネル幅を「中身に追従」させる用途。全角判定は ch > 0x00FF。
	/// </summary>
	float EstimateTextWidth(const std::wstring& text, float scale);
}

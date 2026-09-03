#include "stdafx.h"
#include "UITextUtil.h"

namespace UITextUtil
{
	std::wstring EffectShortText(const StatEffect& e)
	{
		const wchar_t* label = L"?";
		bool percent = false;
		switch (e.stat)
		{
		case StatEffectType::AttackFlat:             label = L"AT"; break;
		case StatEffectType::AttackPercent:          label = L"AT"; percent = true; break;
		case StatEffectType::MagicPowerFlat:         label = L"AP"; break;
		case StatEffectType::MagicPowerPercent:      label = L"AP"; percent = true; break;
		case StatEffectType::MaxHPFlat:              label = L"HP"; break;
		case StatEffectType::MaxHPPercent:           label = L"HP"; percent = true; break;
		case StatEffectType::PhysicalDefenseFlat:    label = L"物防"; break;
		case StatEffectType::PhysicalDefensePercent: label = L"物防"; percent = true; break;
		case StatEffectType::MagicDefenseFlat:       label = L"魔防"; break;
		case StatEffectType::MagicDefensePercent:    label = L"魔防"; percent = true; break;
		case StatEffectType::SkillThresholdFlat:     label = L"技"; break;
		case StatEffectType::AttackSpeedFlat:        label = L"AS"; break;
		case StatEffectType::AttackSpeedPercent:     label = L"AS"; percent = true; break;
		}

		wchar_t buf[32];
		swprintf_s(buf, L"%ls+%d%ls", label, (int)e.value, percent ? L"%" : L"");
		return buf;
	}

	const wchar_t* TraitName(TraitType type)
	{
		switch (type)
		{
		case TraitType::Monster:  return L"魔物";
		case TraitType::Human:    return L"人間";
		case TraitType::Hero:     return L"英雄";
		case TraitType::Warrior:  return L"戦士";
		case TraitType::Mage:     return L"魔道士";
		case TraitType::Guardian: return L"守護者";
		case TraitType::Assassin: return L"暗殺者";
		case TraitType::Ranger:   return L"狩人";
		default:                  return L"?";
		}
	}

	float EstimateTextWidth(const std::wstring& text, float scale)
	{
		float widthAtScale1 = 0.0f;
		for (wchar_t ch : text)
		{
			widthAtScale1 += (ch > 0x00FF) ? 44.0f : 22.0f;
		}
		return widthAtScale1 * scale;
	}
}

#pragma once
#include <vector>
#include "CombatEvent.h"

/// <summary>
/// CombatEventの列を、デバッグ出力(OutputDebugString)向けのテキストログに変換して表示するクラス。
/// 戦闘シミュレーション(CombatEngine)そのものとは切り離してあるので、将来ここを別クラスに
/// 差し替えれば、同じイベント列から画面描画やリプレイなど別の表現を作れる(Step5以降の想定)。
///
/// [将来メモ] このクラスはテキストログなので1行ずつ順番に出力しているが、画面描画版に差し替える際は、
/// 同じCombatEvent::time値を持つイベント同士は同時(同フレーム)に再生すること(CombatEvent.h参照)。
/// </summary>
class CombatLogPrinter
{
public:
	void Print(const std::vector<CombatEvent>& events)
	{
		for (const auto& event : events)
		{
			PrintEvent(event);
		}
	}

private:
	const wchar_t* GetAttackTypeLabel(AttackType attackType)
	{
		return (attackType == AttackType::Physical) ? L"PHYSICAL" : L"MAGIC";
	}

	void PrintEvent(const CombatEvent& event)
	{
		wchar_t buf[256];

		switch (event.type)
		{
		case CombatEventType::Move:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs moves toward [%hs] %hs (distance %d -> %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(),
				event.targetOwner.c_str(), event.targetName.c_str(),
				event.beforeValue, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::NormalAttack:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs attacks(%ls) [%hs] %hs for %d damage! (HP: %d -> %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(), GetAttackTypeLabel(event.attackType),
				event.targetOwner.c_str(), event.targetName.c_str(),
				event.amount, event.beforeValue, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::SkillAttack:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs uses SKILL(%ls) on [%hs] %hs for %d damage! (HP: %d -> %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(), GetAttackTypeLabel(event.attackType),
				event.targetOwner.c_str(), event.targetName.c_str(),
				event.amount, event.beforeValue, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::SplashDamage:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs's SKILL(%ls) splashes onto [%hs] %hs for %d damage! (HP: %d -> %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(), GetAttackTypeLabel(event.attackType),
				event.targetOwner.c_str(), event.targetName.c_str(),
				event.amount, event.beforeValue, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::Heal:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs drains %d HP! (HP: %d -> %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(),
				event.amount, event.beforeValue, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::Shield:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs gains a shield of %d! (Shield: %d)\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str(),
				event.amount, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::ShieldAbsorb:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs's shield absorbs %d damage! (Shield remaining: %d)\n",
				event.time, event.targetOwner.c_str(), event.targetName.c_str(),
				event.amount, event.afterValue);
			OutputDebugString(buf);
			break;

		case CombatEventType::Death:
			swprintf_s(buf, L"[T=%6.2fs] [%hs] %hs has died!\n",
				event.time, event.actorOwner.c_str(), event.actorName.c_str());
			OutputDebugString(buf);
			break;

		case CombatEventType::Warning:
			swprintf_s(buf, L"[T=%6.2fs] [Warning] %hs\n", event.time, event.message.c_str());
			OutputDebugString(buf);
			break;
		}
	}
};

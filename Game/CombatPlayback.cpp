#include "stdafx.h"
#include "CombatPlayback.h"
#include "UnitInstance.h"
#include "HexGridRenderer.h"

const float CombatPlayback::kTargetPlaybackSeconds = 6.0f;
const float CombatPlayback::kMinSpeed = 1.0f;
const float CombatPlayback::kMaxSpeed = 5.0f;
const float CombatPlayback::kTailSeconds = 1.0f;

namespace
{
	// board 1体分を UnitView へ変換する。表示用HPは戦闘開始時(=全回復済み)の値から始める。
	CombatPlayback::UnitView MakeView(const UnitInstance& unit, bool isEnemy)
	{
		CombatPlayback::UnitView v;
		wchar_t nameBuf[64];
		swprintf_s(nameBuf, L"%hs", unit.def->name.c_str());
		v.name = nameBuf;
		v.starLevel = unit.starLevel;
		// HPバーは戦闘開始時の配置(homePosition)に据える。SimulateCombat後のposition(最終位置)だと
		// 1v1等で両者が同じマスに寄り、バーが重なって読めなくなるため。位置はアニメーションしない前提。
		v.worldPos = HexGridRenderer::CalcTileCenter(unit.homePosition.q, unit.homePosition.r);
		v.maxHP = unit.def->baseHP + unit.bonusMaxHP;
		if (v.maxHP < 1) v.maxHP = 1;
		v.displayHP = v.maxHP;   // 戦闘開始時は各Apply*Bonusesで全回復済み。
		v.displayShield = 0;     // シールドは戦闘中に必殺技で付与される。開始時は0。
		v.alive = true;
		v.isEnemy = isEnemy;
		return v;
	}
}

void CombatPlayback::Begin(
	const std::vector<UnitInstance>& playerBoard, const std::string& playerOwner,
	const std::vector<UnitInstance>& enemyBoard, const std::string& enemyOwner,
	const std::vector<CombatEvent>& events)
{
	m_views.clear();
	m_views.reserve(playerBoard.size() + enemyBoard.size());
	for (const auto& u : playerBoard) m_views.push_back(MakeView(u, false));
	for (const auto& u : enemyBoard)  m_views.push_back(MakeView(u, true));
	m_playerCount = playerBoard.size();
	m_playerOwner = playerOwner;
	m_enemyOwner = enemyOwner;

	m_events = events;
	m_nextIndex = 0;
	m_clock = 0.0f;
	m_tailTimer = kTailSeconds;

	// 総尺が kTargetPlaybackSeconds に収まるよう再生速度を決める(下限1.0倍=スローにはしない)。
	float total = m_events.empty() ? 0.0f : m_events.back().time;
	if (total > 0.1f)
	{
		m_speed = total / kTargetPlaybackSeconds;
		if (m_speed < kMinSpeed) m_speed = kMinSpeed;
		if (m_speed > kMaxSpeed) m_speed = kMaxSpeed;
	}
	else
	{
		m_speed = kMinSpeed;
	}

	m_active = true;
	m_begun = true;
}

void CombatPlayback::Update(float deltaTime)
{
	if (!m_active) return;

	m_clock += deltaTime * m_speed;

	// クロックが到達したイベントをまとめて反映する。while条件が time <= clock なので、
	// 同じ time 値のイベントは必ず同じフレームでまとめて処理される。
	const float kEps = 1e-4f;
	while (m_nextIndex < m_events.size() && m_events[m_nextIndex].time <= m_clock + kEps)
	{
		ApplyEvent(m_events[m_nextIndex]);
		++m_nextIndex;
	}

	if (m_nextIndex >= m_events.size())
	{
		// 全イベント消化。最終状態を少し見せてから終了する。
		m_tailTimer -= deltaTime;
		if (m_tailTimer <= 0.0f)
		{
			m_active = false;
		}
	}
}

CombatPlayback::UnitView* CombatPlayback::ResolveActor(const CombatEvent& ev)
{
	if (ev.actorIndex < 0) return nullptr;
	size_t base = (ev.actorOwner == m_playerOwner) ? 0 : m_playerCount;
	size_t idx = base + (size_t)ev.actorIndex;
	if (idx >= m_views.size()) return nullptr;
	return &m_views[idx];
}

CombatPlayback::UnitView* CombatPlayback::ResolveTarget(const CombatEvent& ev)
{
	if (ev.targetIndex < 0) return nullptr;
	size_t base = (ev.targetOwner == m_playerOwner) ? 0 : m_playerCount;
	size_t idx = base + (size_t)ev.targetIndex;
	if (idx >= m_views.size()) return nullptr;
	return &m_views[idx];
}

void CombatPlayback::ApplyEvent(const CombatEvent& ev)
{
	switch (ev.type)
	{
	case CombatEventType::NormalAttack:
	case CombatEventType::SkillAttack:
	case CombatEventType::SplashDamage:
	case CombatEventType::Burn:
		if (UnitView* v = ResolveTarget(ev))
		{
			v->displayHP = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::ShieldAbsorb:
		if (UnitView* v = ResolveTarget(ev))
		{
			v->displayShield = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::Heal:
		if (UnitView* v = ResolveActor(ev))
		{
			v->displayHP = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::Shield:
		if (UnitView* v = ResolveActor(ev))
		{
			v->displayShield = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::Death:
		if (UnitView* v = ResolveActor(ev))
		{
			v->alive = false;
			v->displayHP = 0;
			v->displayShield = 0;
		}
		break;

	case CombatEventType::Move:
	case CombatEventType::Warning:
	default:
		// 位置再生はスコープ外。警告は表示に影響しない。
		break;
	}
}

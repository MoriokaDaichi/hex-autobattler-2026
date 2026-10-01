#include "stdafx.h"
#include "CombatPlayback.h"
#include "UnitInstance.h"
#include "HexGridRenderer.h"
#include <algorithm>

const float CombatPlayback::kTargetPlaybackSeconds = 6.0f;
const float CombatPlayback::kMinSpeed = 1.0f;
const float CombatPlayback::kMaxSpeed = 5.0f;
const float CombatPlayback::kTailSeconds = 1.0f;

namespace
{
	// --- 位置補間の所要(combat内時刻・秒。F5 微調整前提の出発値) ---
	const float kMoveDurMin = 0.12f;        // 最速でも見える下限。
	const float kMoveDurMax = 0.9f;         // 次アクション(≒1/attackSpeed≒0.7〜1.4s)前に到着し切るための上限。
	const float kMoveDurFallback = 0.35f;   // そのユニットの最後の Move で次イベントが無い場合。

	// --- combat-number-overlap: ダメージ/回復ポップアップ(すべて実時間・秒) ---
	const float kPopupLifetime = 0.9f;       // 通常の数字の寿命。
	const float kSkillPopupLifetime = 1.2f;  // 必殺技直撃・とどめは長めに残す。
	// 同じユニットへの同種ダメージがこの時間内に続いたら、新しい数字を出さず直前の数字へ合算する
	// (再生速度最大5倍時に1体が複数から殴られると実時間で十数回/秒になるため)。
	const float kPopupMergeWindow = 0.25f;
	const size_t kMaxPopupsPerUnit = 3;      // 1ユニットに同時に出す上限(超えたら古いものから消す)。
	const size_t kMaxPopupsTotal = 48;       // 画面全体の上限。
	// Death を受けた時、最後の生成/合算からこの時間以内の数字を「とどめ」とみなして強調する
	// (Death は致死ダメージと同じ time=同フレームで反映されるので実質0)。
	const float kLethalMarkMaxAge = 0.05f;

	float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

	// XZ平面での単位ベクトル化(y は 0 にする)。長さがほぼ 0 なら fallback を返す。
	Vector3 NormalizeXZ(const Vector3& d, const Vector3& fallback)
	{
		float len = sqrtf(d.x * d.x + d.z * d.z);
		if (len < 1e-4f) return fallback;
		return Vector3(d.x / len, 0.0f, d.z / len);
	}

	Vector3 LerpVec(const Vector3& a, const Vector3& b, float t)
	{
		return Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
	}

	// board 1体分を UnitView へ変換する。表示用HPは戦闘開始時(=全回復済み)の値から始める。
	CombatPlayback::UnitView MakeView(const UnitInstance& unit, bool isEnemy)
	{
		CombatPlayback::UnitView v;
		wchar_t nameBuf[64];
		swprintf_s(nameBuf, L"%hs", unit.def->name.c_str());
		v.name = nameBuf;
		v.starLevel = unit.starLevel;
		v.def = unit.def;
		// 初期表示位置は戦闘開始時の配置(homePosition)。以降 Move イベントで補間して動く。
		v.worldPos = HexGridRenderer::CalcTileCenter(unit.homePosition.q, unit.homePosition.r);
		v.homeWorldPos = v.worldPos;
		v.moveFromPos = v.worldPos;
		v.moveToPos = v.worldPos;
		// 敵はプレイヤー側(-Z)、プレイヤーは敵側(+Z)を向いて開始する。
		v.facingDir = isEnemy ? Vector3(0.0f, 0.0f, -1.0f) : Vector3(0.0f, 0.0f, 1.0f);
		v.maxHP = unit.def->baseHP + unit.bonusMaxHP;
		if (v.maxHP < 1) v.maxHP = 1;
		v.displayHP = v.maxHP;   // 戦闘開始時は各Apply*Bonusesで全回復済み。
		v.displayShield = 0;     // シールドは戦闘中に必殺技で付与される。開始時は0。
		// ゲージはラウンドをまたいだ持ち越し有無という既存仕様には立ち入らず、渡されたUnitInstanceの
		// 実際の値をそのまま表示初期値にする(displayHPをmaxHPから初期化するのと同じ扱い)。
		v.displayGauge = unit.normalAttackCount + unit.receivedAttackCount;
		v.skillThreshold = unit.def->skillThreshold + unit.bonusSkillThreshold;
		if (v.skillThreshold < 1) v.skillThreshold = 1; // CombatEngine::GetEffectiveSkillThresholdと同じ式。
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
	m_popups.clear();
	++m_beginSerial;

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

	// 既存ポップアップの加齢・寿命切れ削除は、このフレームのイベント反映(新規生成)より先に行う。
	UpdatePopups(deltaTime);

	// クロックが到達したイベントをまとめて反映する。while条件が time <= clock なので、
	// 同じ time 値のイベントは必ず同じフレームでまとめて処理される。
	const float kEps = 1e-4f;
	while (m_nextIndex < m_events.size() && m_events[m_nextIndex].time <= m_clock + kEps)
	{
		ApplyEvent(m_events[m_nextIndex]);
		++m_nextIndex;
	}

	// 各ユニットの位置補間を毎フレーム評価する(m_clock 基準。イベント消化の後段)。
	for (auto& v : m_views)
	{
		if (!v.alive) { v.isMoving = false; continue; }

		if (v.moveStartClock < 0.0f)
		{
			v.isMoving = false;
			continue;
		}

		if (m_clock >= v.moveEndClock)
		{
			v.worldPos = v.moveToPos;
			v.moveStartClock = -1.0f;
			v.isMoving = false;
		}
		else
		{
			float span = v.moveEndClock - v.moveStartClock;
			float t = (span > 1e-4f) ? (m_clock - v.moveStartClock) / span : 1.0f;
			if (t < 0.0f) t = 0.0f;
			t = t * t * (3.0f - 2.0f * t); // smoothstep(加減速)。
			v.worldPos = LerpVec(v.moveFromPos, v.moveToPos, t);
			v.isMoving = true;
			Vector3 dir = Vector3(v.moveToPos.x - v.moveFromPos.x, 0.0f, v.moveToPos.z - v.moveFromPos.z);
			v.facingDir = NormalizeXZ(dir, v.facingDir);
		}
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

float CombatPlayback::GetPopupLifetime(const DamagePopup& p)
{
	return (p.kind == PopupKind::Skill || p.lethal) ? kSkillPopupLifetime : kPopupLifetime;
}

void CombatPlayback::UpdatePopups(float deltaTime)
{
	for (auto& p : m_popups) { p.age += deltaTime; p.sinceLastHit += deltaTime; }
	m_popups.erase(
		std::remove_if(m_popups.begin(), m_popups.end(),
			[](const DamagePopup& p) { return p.age >= GetPopupLifetime(p); }),
		m_popups.end());
}

void CombatPlayback::SpawnPopup(const UnitView* v, int amount, PopupKind kind)
{
	if (v == nullptr || amount <= 0 || m_views.empty()) return;
	size_t viewIndex = (size_t)(v - &m_views[0]);

	// 必殺技以外は、直前に出た同種の数字へ合算する(短時間の連続ダメージをまとめる)。
	if (kind != PopupKind::Skill)
	{
		for (auto it = m_popups.rbegin(); it != m_popups.rend(); ++it)
		{
			if (it->viewIndex != viewIndex) continue;
			if (it->kind == kind && !it->lethal && it->age < kPopupMergeWindow)
			{
				it->amount += amount;
				it->sinceLastHit = 0.0f;
				return;
			}
			break; // 同ユニットの最新が別種なら合算しない(並び順の見た目を保つ)。
		}
	}

	// 同ユニット分が上限なら最も古いものを消す。
	size_t count = 0;
	for (const auto& p : m_popups) if (p.viewIndex == viewIndex) ++count;
	if (count >= kMaxPopupsPerUnit)
	{
		for (auto it = m_popups.begin(); it != m_popups.end(); ++it)
		{
			if (it->viewIndex == viewIndex) { m_popups.erase(it); break; }
		}
	}
	if (m_popups.size() >= kMaxPopupsTotal) m_popups.erase(m_popups.begin());

	DamagePopup p;
	p.viewIndex = viewIndex;
	p.amount = amount;
	p.kind = kind;
	m_popups.push_back(p);
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

			// combat-number-overlap: ダメージ数字。amount はシールド吸収分を含む総ダメージ
			// (ShieldAbsorb は別途数字を出さない=二重表示しない)。
			PopupKind kind = PopupKind::Physical;
			if (ev.type == CombatEventType::SkillAttack) kind = PopupKind::Skill;
			else if (ev.type == CombatEventType::Burn) kind = PopupKind::Burn;
			else if (ev.attackType == AttackType::Magic) kind = PopupKind::Magic;
			SpawnPopup(v, ev.amount, kind);
		}
		// 攻撃モーションは通常攻撃・必殺技の直撃イベントでのみトリガーする
		// (SplashDamage は同じ必殺技の巻き込み分なので二重発火させない。Burn は継続ダメージ)。
		if (ev.type == CombatEventType::NormalAttack || ev.type == CombatEventType::SkillAttack)
		{
			if (UnitView* a = ResolveActor(ev))
			{
				a->attackAnimSeq++;
				a->attackAnimIsSkill = (ev.type == CombatEventType::SkillAttack);
				if (UnitView* t = ResolveTarget(ev))
				{
					Vector3 d(t->worldPos.x - a->worldPos.x, 0.0f, t->worldPos.z - a->worldPos.z);
					a->facingDir = NormalizeXZ(d, a->facingDir);
				}
			}
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
			SpawnPopup(v, ev.amount, PopupKind::Heal);
		}
		break;

	case CombatEventType::Shield:
		if (UnitView* v = ResolveActor(ev))
		{
			v->displayShield = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::GaugeChange:
		// 自己参照イベント(actor自身のゲージがafterValueになった)。
		if (UnitView* v = ResolveActor(ev))
		{
			v->displayGauge = ev.afterValue < 0 ? 0 : ev.afterValue;
		}
		break;

	case CombatEventType::Death:
		if (UnitView* v = ResolveActor(ev))
		{
			v->alive = false;
			v->displayHP = 0;
			v->displayShield = 0;
			v->deathAnimTriggered = true;
			// combat-number-overlap: 同フレームに出たこのユニットへの最新ダメージ数字を「とどめ」として強調する。
			size_t viewIndex = (size_t)(v - &m_views[0]);
			for (auto it = m_popups.rbegin(); it != m_popups.rend(); ++it)
			{
				if (it->viewIndex != viewIndex) continue;
				if (it->kind != PopupKind::Heal && it->sinceLastHit <= kLethalMarkMaxAge) it->lethal = true;
				break;
			}
			// 同一フレーム内でMove直後にDeathが来た場合(moveStartClock==m_clockでまだ補間ループを
			// 1回も通っていない)、worldPosが移動元のまま取り残されてしまう。シミュレーション上は
			// MoveTowardsの時点で既にattacker.positionが移動先へ更新済みなので、moveToPosへ
			// スナップしてground truthに合わせる。
			if (v->moveStartClock >= 0.0f && v->moveStartClock == m_clock)
			{
				v->worldPos = v->moveToPos;
			}
			v->moveStartClock = -1.0f; // 補間中でも撃破されたらその場で崩れる。
		}
		break;

	case CombatEventType::Move:
		if (UnitView* v = ResolveActor(ev))
		{
			// 移動先が移動元と同じ(周囲が塞がって動けなかった)なら何もしない。
			if (ev.moveFromQ != ev.moveToQ || ev.moveFromR != ev.moveToR)
			{
				Vector3 to = HexGridRenderer::CalcTileCenter(ev.moveToQ, ev.moveToR);
				v->moveFromPos = v->worldPos; // 現在の表示位置から繋ぐ(補間の連続性)。
				v->moveToPos = to;
				v->moveStartClock = m_clock;
				// 次アクションまでの実効間隔(CombatEngineが生成時点で計算済み)の0.8倍を clamp する。
				float dur = (ev.moveDurationHint > 0.0f)
					? Clampf(ev.moveDurationHint * 0.8f, kMoveDurMin, kMoveDurMax)
					: kMoveDurFallback;
				v->moveEndClock = m_clock + dur;
			}
		}
		break;

	case CombatEventType::Warning:
	default:
		// 警告は表示に影響しない。
		break;
	}
}

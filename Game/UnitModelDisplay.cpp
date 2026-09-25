#include "stdafx.h"
#include "UnitModelDisplay.h"
#include "HexGridRenderer.h"

namespace
{
	// ★1基準の表示スケール。board-layout-rework で「1体が概ね1マスに収まり隣と重ならない」よう
	// 10.0 → 4.0 に縮小(F5是正2で 5→4。さらに 3.5〜4.5 を微調整する出発値)。頭上バーの
	// Yオフセット(BoardUIRenderer::kBarWorldY)もこれに追随して調整している。
	const Vector3 kUnitModelScale(4.0f, 4.0f, 4.0f);

	// モデル原点が中心にあり、そのまま y=0 に置くと下半分がヘックス平面へめり込む。
	// 足元が平面に乗るよう、モデル高の概ね半分だけ Y へ持ち上げる。
	// 実効スケール(kUnitModelScale × 星倍率)に比例させる。scale=1 相当での半モデル高の目安値で、
	// F5 反復前提の出発値(埋まる/浮くなら増減する。scale 4 で +40 相当)。
	const float kUnitModelHalfHeightAtScale1 = 10.0f;

	// combat-movement-playback: 再生駆動でのモデルの向き。
	// モデルの前方軸が tkm によって +Z / -Z のどちらか分からないため、F5 で 0 または PI を確定する出発値。
	const float kModelYawOffsetRad = 0.0f;
	// 向きを目標へ寄せる速さ(1フレームの Slerp 係数 = この値 * dt、1.0 で頭打ち)。
	const float kTurnRatePerSec = 12.0f;

	// アニメクリップの index(UnitModelDisplay::GetOrLoadAnimClips のロード順と一致させる)。
	const int kClipIdle = 0;
	const int kClipMove = 1;
	const int kClipNormalAttack = 2;
	const int kClipSkill = 3;
	const int kClipDeath = 4;

	// 星レベルに応じた表示スケール倍率。★が上がるほど一回り大きく見せて盤面上で区別できるようにする。
	// StarLevelSystem::GetStarMultiplier(★1比 約1.8倍/★)はステータス用で、そのまま使うと
	// ★3が基準の約3.24倍(=スケール32)になり1マスに収まらないため、見た目専用の控えめな値を独自に定義する。
	// エフェクト・パーティクル等による更なる差別化は今回スコープ外(スケール変化のみで十分と判断)。
	float GetStarModelScaleMultiplier(int starLevel)
	{
		switch (starLevel)
		{
		case 2:  return 1.15f;
		case 3:  return 1.32f;
		default: return 1.0f; // ★1(および想定外の値)は基準サイズ。
		}
	}
}

void UnitModelDisplay::Update(const std::vector<UnitInstance>& board)
{
	RebuildIfBoardChanged(board);

	// 表示位置はHexGridRendererのグリッド線・ゾーン塗りと同じ座標系(盤面中心が原点)に合わせる。
	// モデルの再ロードは伴わない軽い処理なので、位置同期とアニメーション更新は毎フレーム行う。
	// Windows.hのmin/maxマクロとの衝突を避けるため、std::minは使わずに手書きする。
	size_t numDisplayed = m_displayEntries.size();
	if (board.size() < numDisplayed)
	{
		numDisplayed = board.size();
	}
	for (size_t i = 0; i < numDisplayed; ++i)
	{
		const UnitInstance& unit = board[i];
		DisplayEntry& e = m_displayEntries[i];
		ModelRender& modelRender = *e.modelRender;

		// 再生駆動(UpdateFromPlayback)で死亡/攻撃クリップに入ったエントリが、board 駆動へ戻っても
		// そのまま(死亡ポーズ最終フレームで静止)残る回帰を防ぐ。編成据え置きのリトライでは
		// RebuildIfBoardChanged が不発なので、ここで idle へ戻す(idle はループ設定済み)。
		if (e.curClip != kClipIdle)
		{
			modelRender.PlayAnimation(kClipIdle, 0.15f);
			e.curClip = kClipIdle;
			e.seenAttackSeq = 0;
		}
		// 再生駆動用の向き補間状態(lastRot)は board 駆動では使わない。次回の再生開始時に古い値を
		// 持ち越さないよう、待機のみで再生を終えたエントリも含めて毎フレームここでリセットしておく。
		e.lastRot = Quaternion::Identity;

		Vector3 worldPos = HexGridRenderer::CalcTileCenter(unit.position.q, unit.position.r);

		// 星レベルに応じてモデルを一回り大きくする(★1=基準/★2=やや大/★3=更に大)。
		float starScale = GetStarModelScaleMultiplier(unit.starLevel);
		Vector3 modelScale = kUnitModelScale;
		modelScale.Scale(starScale);

		// 足元がヘックス平面(y=0)に乗るよう、実効スケールに比例して持ち上げる(めり込み対策)。
		worldPos.y += kUnitModelHalfHeightAtScale1 * kUnitModelScale.x * starScale;

		modelRender.SetTRS(worldPos, Quaternion::Identity, modelScale);
		modelRender.SetAnimationSpeed(1.0f); // 再生駆動時に変更された速度が残らないよう、board駆動では常に等倍に戻す。
		modelRender.Update();
	}
}

void UnitModelDisplay::Draw(RenderContext& rc)
{
	for (auto& entry : m_displayEntries)
	{
		entry.modelRender->Draw(rc);
	}
}

void UnitModelDisplay::Clear()
{
	m_displayEntries.clear();
	m_lastBoardSignature.clear();
}

void UnitModelDisplay::RebuildIfViewsChanged(const CombatPlayback::UnitView* views, size_t count)
{
	std::vector<std::pair<const UnitDef*, int>> currentSignature;
	currentSignature.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		currentSignature.push_back({ views[i].def, views[i].starLevel });
	}

	if (currentSignature == m_lastBoardSignature)
	{
		return; // 構成に変化なし。再構築不要。
	}
	m_lastBoardSignature = currentSignature;

	m_displayEntries.clear();
	m_displayEntries.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		DisplayEntry entry;
		entry.modelRender = std::make_unique<ModelRender>();
		std::array<AnimationClip, 5>& animClips = GetOrLoadAnimClips(views[i].def);
		entry.modelRender->Init(views[i].def->modelPath.c_str(), animClips.data(), 5);
		m_displayEntries.push_back(std::move(entry));
	}
}

void UnitModelDisplay::UpdateFromPlayback(const CombatPlayback::UnitView* views, size_t count, float playbackSpeed)
{
	RebuildIfViewsChanged(views, count);

	float dt = g_gameTime->GetFrameDeltaTime();
	float turnT = kTurnRatePerSec * dt;
	if (turnT > 1.0f) turnT = 1.0f;

	size_t numDisplayed = m_displayEntries.size();
	if (count < numDisplayed) numDisplayed = count;

	for (size_t i = 0; i < numDisplayed; ++i)
	{
		const CombatPlayback::UnitView& view = views[i];
		DisplayEntry& e = m_displayEntries[i];
		ModelRender& modelRender = *e.modelRender;

		// --- 再生セッション跨ぎの状態リセット(RebuildIfViewsChanged が発火しない敗北リトライ等) ---
		if (!view.deathAnimTriggered && e.curClip == kClipDeath) { e.curClip = -1; }
		if (view.attackAnimSeq < e.seenAttackSeq) e.seenAttackSeq = view.attackAnimSeq;

		// --- TRS ---
		float starScale = GetStarModelScaleMultiplier(view.starLevel);
		Vector3 modelScale = kUnitModelScale;
		modelScale.Scale(starScale);

		Vector3 worldPos = view.worldPos;
		worldPos.y += kUnitModelHalfHeightAtScale1 * kUnitModelScale.x * starScale;

		Quaternion targetRot;
		targetRot.SetRotationY(atan2f(view.facingDir.x, view.facingDir.z) + kModelYawOffsetRad);
		Quaternion rot;
		rot.Slerp(turnT, e.lastRot, targetRot);
		e.lastRot = rot;

		modelRender.SetTRS(worldPos, rot, modelScale);

		// --- アニメ状態機(死亡 > 攻撃再生中 > 攻撃トリガ > 移動 > idle) ---
		if (view.deathAnimTriggered && e.curClip != kClipDeath)
		{
			modelRender.PlayAnimation(kClipDeath, 0.15f);
			e.curClip = kClipDeath;
		}
		else if (view.attackAnimSeq != e.seenAttackSeq)
		{
			int clip = view.attackAnimIsSkill ? kClipSkill : kClipNormalAttack;
			modelRender.PlayAnimation(clip, 0.1f);
			e.seenAttackSeq = view.attackAnimSeq;
			e.curClip = clip;
		}
		else if ((e.curClip == kClipNormalAttack || e.curClip == kClipSkill) && modelRender.IsPlayingAnimation())
		{
			// 攻撃モーション再生中は割り込まない。
		}
		else if (e.curClip == kClipDeath)
		{
			// death クリップの最終フレームで静止。
		}
		else
		{
			int desired = view.isMoving ? kClipMove : kClipIdle;
			if (e.curClip != desired)
			{
				modelRender.PlayAnimation(desired, 0.15f);
				e.curClip = desired;
			}
		}

		modelRender.SetAnimationSpeed(playbackSpeed); // 位置補間の速さ(m_speed)にアニメ再生速度を揃える。
		modelRender.Update();
	}
}

void UnitModelDisplay::RebuildIfBoardChanged(const std::vector<UnitInstance>& board)
{
	// UnitDef*だけでなくstarLevelも含める。合成で星が上がってもUnitDef*は変わらないため、
	// UnitDef*のみの比較では星レベルに応じた表示スケールの更新契機を取りこぼす。
	std::vector<std::pair<const UnitDef*, int>> currentSignature;
	currentSignature.reserve(board.size());
	for (const auto& unit : board)
	{
		currentSignature.push_back({ unit.def, unit.starLevel });
	}

	if (currentSignature == m_lastBoardSignature)
	{
		return; // 盤面構成に変化なし。再構築不要。
	}
	m_lastBoardSignature = currentSignature;

	m_displayEntries.clear();
	m_displayEntries.reserve(board.size());

	for (const auto& unit : board)
	{
		DisplayEntry entry;
		entry.modelRender = std::make_unique<ModelRender>();

		std::array<AnimationClip, 5>& animClips = GetOrLoadAnimClips(unit.def);
		entry.modelRender->Init(unit.def->modelPath.c_str(), animClips.data(), 5);

		m_displayEntries.push_back(std::move(entry));
	}
}

std::array<AnimationClip, 5>& UnitModelDisplay::GetOrLoadAnimClips(const UnitDef* def)
{
	auto it = m_animClipCache.find(def->name);
	if (it != m_animClipCache.end())
	{
		return it->second;
	}

	std::array<AnimationClip, 5>& clips = m_animClipCache[def->name];
	clips[0].Load(def->idleAnimPath.c_str());
	clips[1].Load(def->moveAnimPath.c_str());
	clips[2].Load(def->normalAttackAnimPath.c_str());
	clips[3].Load(def->skillAnimPath.c_str());
	clips[4].Load(def->deathAnimPath.c_str());
	// idle / move はループ、attack / skill / death は単発(最終フレームで止まる)。
	// board 駆動(準備/結果)は PlayAnimation を呼ばないのでこのフラグは影響しない。
	clips[0].SetLoopFlag(true);
	clips[1].SetLoopFlag(true);
	return clips;
}

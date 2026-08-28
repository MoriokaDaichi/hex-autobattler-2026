#include "stdafx.h"
#include "UnitModelDisplay.h"
#include "HexGridRenderer.h"

namespace
{
	// 検証用コードで確認済みの基準スケール(★1)。1ヘックスマスに収まる適正値。
	const Vector3 kUnitModelScale(10.0f, 10.0f, 10.0f);

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

void UnitModelDisplay::Update(const Player& player)
{
	RebuildIfBoardChanged(player);

	// 表示位置はHexGridRendererのグリッド線・ゾーン塗りと同じ座標系(盤面中心が原点)に合わせる。
	// モデルの再ロードは伴わない軽い処理なので、位置同期とアニメーション更新は毎フレーム行う。
	// Windows.hのmin/maxマクロとの衝突を避けるため、std::minは使わずに手書きする。
	size_t numDisplayed = m_displayEntries.size();
	if (player.board.size() < numDisplayed)
	{
		numDisplayed = player.board.size();
	}
	for (size_t i = 0; i < numDisplayed; ++i)
	{
		const UnitInstance& unit = player.board[i];
		ModelRender& modelRender = *m_displayEntries[i].modelRender;

		Vector3 worldPos = HexGridRenderer::CalcTileCenter(unit.position.q, unit.position.r);

		// 星レベルに応じてモデルを一回り大きくする(★1=基準/★2=やや大/★3=更に大)。
		float starScale = GetStarModelScaleMultiplier(unit.starLevel);
		Vector3 modelScale = kUnitModelScale;
		modelScale.Scale(starScale);

		modelRender.SetTRS(worldPos, Quaternion::Identity, modelScale);
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

void UnitModelDisplay::RebuildIfBoardChanged(const Player& player)
{
	// UnitDef*だけでなくstarLevelも含める。合成で星が上がってもUnitDef*は変わらないため、
	// UnitDef*のみの比較では星レベルに応じた表示スケールの更新契機を取りこぼす。
	std::vector<std::pair<const UnitDef*, int>> currentSignature;
	currentSignature.reserve(player.board.size());
	for (const auto& unit : player.board)
	{
		currentSignature.push_back({ unit.def, unit.starLevel });
	}

	if (currentSignature == m_lastBoardSignature)
	{
		return; // 盤面構成に変化なし。再構築不要。
	}
	m_lastBoardSignature = currentSignature;

	m_displayEntries.clear();
	m_displayEntries.reserve(player.board.size());

	for (const auto& unit : player.board)
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
	return clips;
}

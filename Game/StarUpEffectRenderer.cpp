#include "stdafx.h"
#include "StarUpEffectRenderer.h"
#include "UIRectRenderer.h"
#include "BoardUIRenderer.h"
#include "HexGridRenderer.h"
#include <cmath>

namespace
{
	const float kPi = 3.14159265f;

	/// <summary>
	/// 星ごとの演出パラメータ。★2と★3は「同じ要素を同じ描き方で」使い、この値だけを変える
	/// (一貫性の担保。docs/tasks/star-up-effect/plan.md のパラメータ表と対応)。
	/// 長さはワールド単位(kHexSize=50、★1モデル高≒30)、時間は秒、サイズ(px)はUI空間ピクセル。
	/// </summary>
	struct StarUpParams
	{
		// 光のリング(足元から外へ広がる光点の輪)。
		int ringCount = 1;            // 本数(発生間隔ごとに1本ずつ出る)。
		float ringInterval = 0.0f;    // 2本目以降の発生間隔。
		float ringLife = 0.45f;       // 1本の寿命。
		float ringStartRadius = 10.0f;
		float ringEndRadius = 45.0f;
		int ringDotCount = 16;        // 円周上の光点の数。
		float ringDotSize = 7.0f;     // 光点(菱形)の大きさ(px)。

		// 光の柱(足元から上へ伸びる縦帯)。
		float pillarLife = 0.40f;
		float pillarHeight = 60.0f;
		float pillarWidth = 14.0f;

		// 立ち上る粒子。
		int particleCount = 8;
		float particleLife = 0.60f;
		float particleRise = 45.0f;        // 寿命の間に上昇する量(個体差 0.6〜1.4倍)。
		float particleSize = 6.0f;         // px(個体差 0.7〜1.3倍)。
		float particleSpawnWindow = 0.15f; // 粒子の発生タイミングをこの秒数の中にばらす。
		float particleSpreadRadius = 22.0f;// 発生位置の半径(足元の円内)。

		// ユニットのポップ(盤面のみ)。倍率 = 1 + amp * sin(π·bounces·u) * (1-u)。
		float popAmplitude = 0.12f;
		float popDuration = 0.30f;
		int popBounces = 1;

		// 明るさ(全要素のalphaに掛ける倍率)。
		float brightness = 0.75f;

		// ★3のみの +α(0で無効)。
		float screenFlashAlpha = 0.0f;
		float screenFlashDuration = 0.0f;
		float shakeAmplitude = 0.0f;  // カメラ揺れの振幅(ワールド単位)。
		float shakeDuration = 0.0f;
	};

	// ★2: 控えめ(小さく短く1回)。
	StarUpParams MakeStar2Params()
	{
		StarUpParams p;
		p.ringCount = 1;
		p.ringInterval = 0.0f;
		p.ringLife = 0.45f;
		p.ringStartRadius = 10.0f;
		p.ringEndRadius = 45.0f;
		p.ringDotCount = 16;
		p.ringDotSize = 7.0f;
		p.pillarLife = 0.40f;
		p.pillarHeight = 60.0f;
		p.pillarWidth = 14.0f;
		p.particleCount = 8;
		p.particleLife = 0.60f;
		p.particleRise = 45.0f;
		p.particleSize = 6.0f;
		p.particleSpawnWindow = 0.15f;
		p.particleSpreadRadius = 22.0f;
		p.popAmplitude = 0.12f;
		p.popDuration = 0.30f;
		p.popBounces = 1;
		p.brightness = 0.75f;
		p.screenFlashAlpha = 0.0f;
		p.screenFlashDuration = 0.0f;
		p.shakeAmplitude = 0.0f;
		p.shakeDuration = 0.0f;
		return p;
	}

	// ★3: 派手(★2と同じ要素を大きく・多く・長く・明るく + 画面フラッシュ・カメラ揺れ・二度弾むポップ)。
	StarUpParams MakeStar3Params()
	{
		StarUpParams p;
		p.ringCount = 3;
		p.ringInterval = 0.14f;
		p.ringLife = 0.70f;
		p.ringStartRadius = 10.0f;
		p.ringEndRadius = 72.0f;
		p.ringDotCount = 24;
		p.ringDotSize = 10.0f;
		p.pillarLife = 0.80f;
		p.pillarHeight = 140.0f;
		p.pillarWidth = 26.0f;
		p.particleCount = 28;
		p.particleLife = 0.95f;
		p.particleRise = 95.0f;
		p.particleSize = 9.0f;
		p.particleSpawnWindow = 0.40f;
		p.particleSpreadRadius = 34.0f;
		p.popAmplitude = 0.25f;
		p.popDuration = 0.55f;
		p.popBounces = 2;
		p.brightness = 1.0f;
		p.screenFlashAlpha = 0.30f;
		p.screenFlashDuration = 0.30f;
		p.shakeAmplitude = 4.0f;
		p.shakeDuration = 0.30f;
		return p;
	}

	const StarUpParams kStar2Params = MakeStar2Params();
	const StarUpParams kStar3Params = MakeStar3Params();

	const StarUpParams& GetParams(int starLevel)
	{
		return (starLevel >= 3) ? kStar3Params : kStar2Params;
	}

	// --- ★2/★3共通の見た目(色系統・描き方の型) ---
	// 色は「金色の光」と「白金の芯」の2色だけを使う(★の金色から連想。星で色は変えない)。
	const Vector4 kGlowColor(1.00f, 0.80f, 0.28f, 1.0f);
	const Vector4 kCoreColor(1.00f, 0.97f, 0.80f, 1.0f);

	const float kRingGroundY = 1.5f;         // リング・粒子の発生高さ(ヘックス平面 y=0 のわずか上)。
	const float kRingSpinRadPerSec = 0.8f;   // リングの光点が円周方向へ回る速さ。
	const float kRingNthDim = 0.85f;         // 2本目以降のリングの明るさ減衰(本数ごとに乗算)。
	const int kRingCoreDotEvery = 4;         // 光点のうちN個に1個を白金の芯色にする(きらめき)。
	const float kPillarGrowPortion = 0.25f;  // 柱が最大高さまで伸びるのに使う、寿命中の割合。
	const float kPillarGlowAlpha = 0.30f;    // 柱の外側(金色グロー)の不透明度。
	const float kPillarCoreAlpha = 0.55f;    // 柱の芯(白金)の不透明度。モデルを隠しすぎない程度。
	const float kPillarCoreWidthRatio = 0.35f; // 芯の幅(グロー幅に対する比)。
	const float kParticleFadeInPortion = 0.15f; // 粒子が出始めにフェードインする寿命中の割合。
	const float kDiamondRad = kPi * 0.25f;   // 光点を45°回して菱形にする。
	const float kShakeFrequencyHz = 18.0f;   // カメラ揺れの周波数。

	// ベンチ(2D一覧でモデル無し)用の代用投影。ローカル1ワールド単位を何pxにするかと、
	// 地面(z方向)を縦へ潰す比率(盤面を斜め上から見たときの楕円感に寄せる)。
	const float kBenchPxPerWorld = 0.7f;
	const float kBenchGroundFlatten = 0.4f;

	// 同時に再生する演出数の上限。超えたら古い順に捨てる(矩形プールの膨張防止)。
	const size_t kMaxActiveEffects = 6;

	float EaseOut(float u)
	{
		float inv = 1.0f - u;
		return 1.0f - inv * inv;
	}

	float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	/// <summary>粒子の個体差用の疑似乱数(0〜1)。seed・粒子番号・用途(salt)から決定的に求める。</summary>
	float Hash01(unsigned int seed, unsigned int index, unsigned int salt)
	{
		unsigned int h = seed * 0x9E3779B1u ^ (index + 1u) * 0x85EBCA77u ^ (salt + 1u) * 0xC2B2AE3Du;
		h ^= h >> 15;
		h *= 0x2C1B3C6Du;
		h ^= h >> 12;
		h *= 0x297A2D39u;
		h ^= h >> 15;
		return (float)(h & 0xFFFFFFu) / 16777216.0f;
	}

	/// <summary>この演出が完全に終わるまでの秒数(全要素の寿命の最大値)。</summary>
	float CalcTotalDuration(const StarUpParams& p)
	{
		float total = p.ringInterval * (float)(p.ringCount - 1) + p.ringLife;
		float candidates[] = {
			p.pillarLife,
			p.particleSpawnWindow + p.particleLife,
			p.popDuration,
			p.screenFlashDuration,
			p.shakeDuration,
		};
		for (float c : candidates)
		{
			if (c > total) total = c;
		}
		return total;
	}

	Vector4 WithAlpha(const Vector4& color, float alpha)
	{
		return Vector4(color.x, color.y, color.z, Clamp01(alpha));
	}
}

void StarUpEffectRenderer::Spawn(int newStarLevel, bool onBoard, const HexCoord& boardPos, int benchIndex)
{
	// 同じ場所で再生中の演出は置き換える(連鎖合成で★2→★3と続いた場合に★3だけが見えるように)。
	for (size_t i = 0; i < m_effects.size();)
	{
		const ActiveEffect& e = m_effects[i];
		bool sameSpot = (e.onBoard == onBoard) && (onBoard ? (e.boardPos == boardPos) : (e.benchIndex == benchIndex));
		if (sameSpot)
		{
			m_effects.erase(m_effects.begin() + i);
		}
		else
		{
			++i;
		}
	}
	if (m_effects.size() >= kMaxActiveEffects)
	{
		m_effects.erase(m_effects.begin()); // 一番古いものを捨てる。
	}

	ActiveEffect e;
	e.starLevel = newStarLevel;
	e.onBoard = onBoard;
	e.boardPos = boardPos;
	e.benchIndex = benchIndex;
	e.elapsed = 0.0f;
	e.seed = m_nextSeed++;
	m_effects.push_back(e);
}

void StarUpEffectRenderer::Update(float deltaTime)
{
	for (size_t i = 0; i < m_effects.size();)
	{
		m_effects[i].elapsed += deltaTime;
		if (m_effects[i].elapsed >= CalcTotalDuration(GetParams(m_effects[i].starLevel)))
		{
			m_effects.erase(m_effects.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

void StarUpEffectRenderer::Clear()
{
	m_effects.clear();
}

void StarUpEffectRenderer::Draw(RenderContext& rc, UIRectRenderer& rectRenderer)
{
	if (m_effects.empty()) return;

	m_rectRenderer = &rectRenderer;
	g_renderingEngine->AddRenderObject(this);
}

float StarUpEffectRenderer::GetModelPopScale(const HexCoord& pos) const
{
	float scale = 1.0f;
	for (const auto& e : m_effects)
	{
		if (!e.onBoard || e.boardPos != pos) continue;

		const StarUpParams& p = GetParams(e.starLevel);
		if (e.elapsed >= p.popDuration || p.popDuration <= 0.0f) continue;

		// 一瞬大きくなって戻る(bounces=2なら「大きく→少し縮む→戻る」のぷるん、と弾む)。
		float u = e.elapsed / p.popDuration;
		scale *= 1.0f + p.popAmplitude * sinf(kPi * (float)p.popBounces * u) * (1.0f - u);
	}
	return scale;
}

Vector3 StarUpEffectRenderer::GetCameraShakeOffset() const
{
	Vector3 offset(0.0f, 0.0f, 0.0f);
	for (const auto& e : m_effects)
	{
		const StarUpParams& p = GetParams(e.starLevel);
		if (p.shakeAmplitude <= 0.0f || e.elapsed >= p.shakeDuration) continue;

		// 振幅は二乗で減衰させ、出始めだけガツンと揺らしてすぐ収める。X/Zで周波数をずらして直線的な往復を避ける。
		float u = e.elapsed / p.shakeDuration;
		float amp = p.shakeAmplitude * (1.0f - u) * (1.0f - u);
		float phase = 2.0f * kPi * kShakeFrequencyHz * e.elapsed + (float)e.seed;
		offset.x += amp * sinf(phase);
		offset.z += amp * cosf(phase * 1.3f);
	}
	return offset;
}

bool StarUpEffectRenderer::Project(const ActiveEffect& e, const Vector3& local, Vector2& outUI) const
{
	if (e.onBoard)
	{
		Vector3 world = HexGridRenderer::CalcTileCenter(e.boardPos.q, e.boardPos.r);
		world.Add(local);
		return BoardUIRenderer::WorldToUI(world, outUI);
	}

	// ベンチ: 行カードの中心を足元とみなす(BoardUIRenderer::OnRender2Dのカード配置と同じ式)。
	// 表示上限を超える行は集約行("...+N件")の位置に出す。
	int row = e.benchIndex;
	if (row > BoardUIRenderer::kBenchMaxVisibleRows) row = BoardUIRenderer::kBenchMaxVisibleRows;
	if (row < 0) row = 0;
	float rowY = BoardUIRenderer::kBenchTopY - BoardUIRenderer::kBenchStepY * (float)(row + 1);
	Vector2 cardCenter(BoardUIRenderer::kBenchX + 130.0f, rowY - BoardUIRenderer::kBenchStepY * 0.5f + 6.0f);

	outUI.x = cardCenter.x + local.x * kBenchPxPerWorld;
	outUI.y = cardCenter.y + (local.y + local.z * kBenchGroundFlatten) * kBenchPxPerWorld;
	return true;
}

void StarUpEffectRenderer::DrawEffect(RenderContext& rc, const ActiveEffect& e)
{
	const StarUpParams& p = GetParams(e.starLevel);
	const float t = e.elapsed;
	const Vector2 kCenter(0.5f, 0.5f);

	// --- 光の柱(奥に描くため最初) ---
	Vector2 foot;
	if (t < p.pillarLife && Project(e, Vector3(0.0f, 0.0f, 0.0f), foot))
	{
		float u = t / p.pillarLife;
		float grow = EaseOut(Clamp01(u / kPillarGrowPortion)); // 素早く伸びる。
		float thin = 1.0f - u;                                  // 細くなりながら消える。
		float alpha = (1.0f - u) * p.brightness;

		Vector2 top, side;
		if (Project(e, Vector3(0.0f, p.pillarHeight * grow, 0.0f), top)
			&& Project(e, Vector3(p.pillarWidth, 0.0f, 0.0f), side))
		{
			float height = top.y - foot.y;
			float widthPx = fabsf(side.x - foot.x) * thin;
			if (height > 1.0f && widthPx > 0.5f)
			{
				Vector2 mid((foot.x + top.x) * 0.5f, (foot.y + top.y) * 0.5f);
				m_rectRenderer->DrawRect(rc, mid, Vector2(widthPx, height), WithAlpha(kGlowColor, alpha * kPillarGlowAlpha), kCenter);
				m_rectRenderer->DrawRect(rc, mid, Vector2(widthPx * kPillarCoreWidthRatio, height), WithAlpha(kCoreColor, alpha * kPillarCoreAlpha), kCenter);
			}
		}
	}

	// --- 光のリング(足元の地面上の円。外へ広がりながら薄れる) ---
	for (int k = 0; k < p.ringCount; ++k)
	{
		float rt = t - p.ringInterval * (float)k;
		if (rt < 0.0f || rt >= p.ringLife) continue;

		float u = rt / p.ringLife;
		float radius = p.ringStartRadius + (p.ringEndRadius - p.ringStartRadius) * EaseOut(u);
		float alpha = (1.0f - u) * (1.0f - u) * p.brightness * powf(kRingNthDim, (float)k);
		float dotSize = p.ringDotSize * (1.0f - 0.5f * u);
		float spin = 0.4f * (float)k + kRingSpinRadPerSec * rt;

		for (int j = 0; j < p.ringDotCount; ++j)
		{
			float ang = 2.0f * kPi * (float)j / (float)p.ringDotCount + spin;
			Vector2 ui;
			if (!Project(e, Vector3(cosf(ang) * radius, kRingGroundY, sinf(ang) * radius), ui)) continue;

			const Vector4& color = (j % kRingCoreDotEvery == 0) ? kCoreColor : kGlowColor;
			m_rectRenderer->DrawRect(rc, ui, Vector2(dotSize, dotSize), WithAlpha(color, alpha), kCenter, kDiamondRad);
		}
	}

	// --- 立ち上る粒子 ---
	for (int i = 0; i < p.particleCount; ++i)
	{
		unsigned int idx = (unsigned int)i;
		float delay = Hash01(e.seed, idx, 0) * p.particleSpawnWindow;
		float pt = t - delay;
		if (pt < 0.0f || pt >= p.particleLife) continue;

		float u = pt / p.particleLife;
		float ang = Hash01(e.seed, idx, 1) * 2.0f * kPi;
		float rad = sqrtf(Hash01(e.seed, idx, 2)) * p.particleSpreadRadius; // 円内に一様に散らす。
		float rise = p.particleRise * (0.6f + 0.8f * Hash01(e.seed, idx, 3)) * EaseOut(u);

		Vector2 ui;
		if (!Project(e, Vector3(cosf(ang) * rad, kRingGroundY + rise, sinf(ang) * rad), ui)) continue;

		float size = p.particleSize * (0.7f + 0.6f * Hash01(e.seed, idx, 4)) * (1.0f - 0.5f * u);
		float alpha = (1.0f - u) * Clamp01(u / kParticleFadeInPortion) * p.brightness;
		float rot = kDiamondRad + (Hash01(e.seed, idx, 5) - 0.5f) * 2.0f * kPi * u; // ゆっくり回りながら昇る。
		const Vector4& color = (i % 3 == 0) ? kCoreColor : kGlowColor;
		m_rectRenderer->DrawRect(rc, ui, Vector2(size, size), WithAlpha(color, alpha), kCenter, rot);
	}

	// --- 画面フラッシュ(★3のみ。白金の半透明矩形を全画面に1枚) ---
	if (p.screenFlashAlpha > 0.0f && t < p.screenFlashDuration)
	{
		float u = t / p.screenFlashDuration;
		float alpha = p.screenFlashAlpha * (1.0f - u) * (1.0f - u);
		m_rectRenderer->DrawRect(rc, Vector2(0.0f, 0.0f), Vector2((float)UI_SPACE_WIDTH, (float)UI_SPACE_HEIGHT),
			WithAlpha(kCoreColor, alpha), kCenter);
	}
}

void StarUpEffectRenderer::OnRender2D(RenderContext& rc)
{
	if (m_rectRenderer == nullptr) return;

	for (const auto& e : m_effects)
	{
		DrawEffect(rc, e);
	}
}

#include "stdafx.h"
#include "BoardUIRenderer.h"
#include "CombatPlayback.h"
#include "Player.h"

namespace
{
	// HPバーを浮かべる高さ(ユニットのワールド座標からの+Yオフセット)。モデルは約10倍スケールで大きい。
	const float kBarWorldY = 150.0f;

	const int kBarCells = 10;                 // HPバーのマス数。
	// フォント(myfile.spritefont)に矩形やブロック罫線が収録されていないため、
	// HPバーはASCII記号で表現する(未収録グリフを描くとSpriteFontが例外を投げアプリごと落ちる)。
	const wchar_t kFilledCell = L'#';
	const wchar_t kEmptyCell = L'-';

	const Vector2 kCenterPivot(0.5f, 0.5f);   // テキストの中心をアンカーにする。
	const Vector2 kTopLeftPivot(0.0f, 1.0f);  // ベンチ一覧用(左上アンカー、FPS表示と同じ)。

	const float kBarLabelScale = 0.42f;
	const float kBarValueScale = 0.44f;

	// ベンチ一覧のレイアウト(UI空間、中央原点・y上向き)。左端に縦並び。
	const float kBenchX = -910.0f;
	const float kBenchTopY = 250.0f;
	const float kBenchStepY = 40.0f;
	const float kBenchTitleScale = 0.6f;
	const float kBenchItemScale = 0.52f;

	Vector4 HPColor(float ratio)
	{
		if (ratio > 0.5f)  return Vector4(0.45f, 0.95f, 0.5f, 1.0f);  // 緑
		if (ratio > 0.25f) return Vector4(0.98f, 0.85f, 0.3f, 1.0f);  // 黄
		return Vector4(1.0f, 0.4f, 0.35f, 1.0f);                      // 赤
	}

	std::wstring MakeBar(float ratio)
	{
		if (ratio < 0.0f) ratio = 0.0f;
		if (ratio > 1.0f) ratio = 1.0f;
		int filled = (int)(ratio * kBarCells + 0.5f);
		if (filled <= 0 && ratio > 0.0f) filled = 1; // わずかでも残っていれば1マスは点ける。
		if (filled > kBarCells) filled = kBarCells;

		std::wstring s;
		s.reserve(kBarCells + 2);
		s += L'[';
		for (int i = 0; i < kBarCells; ++i) s += (i < filled) ? kFilledCell : kEmptyCell;
		s += L']';
		return s;
	}

	std::wstring StarSuffix(int starLevel)
	{
		if (starLevel <= 1) return std::wstring();
		wchar_t buf[8];
		swprintf_s(buf, L" *%d", starLevel); // " *2"
		return buf;
	}
}

bool BoardUIRenderer::WorldToUI(const Vector3& world, Vector2& outUI)
{
	const Matrix& vp = g_camera3D->GetViewProjectionMatrix();
	Vector4 h(world.x, world.y, world.z, 1.0f);
	vp.Apply(h);
	if (h.w <= 0.0001f) return false; // カメラ背後。

	float ndcX = h.x / h.w;
	float ndcY = h.y / h.w;
	if (ndcX < -1.3f || ndcX > 1.3f || ndcY < -1.3f || ndcY > 1.3f) return false; // 大きく画面外。

	outUI = Vector2(ndcX * ((float)UI_SPACE_WIDTH * 0.5f), ndcY * ((float)UI_SPACE_HEIGHT * 0.5f));
	return true;
}

void BoardUIRenderer::DrawPreparation(RenderContext& rc, const Player& player)
{
	m_bench.clear();
	m_bench.reserve(player.bench.size());
	for (const auto& unit : player.bench)
	{
		BenchView bv;
		wchar_t buf[80];
		if (unit.starLevel > 1)
			swprintf_s(buf, L"*%d %hs", unit.starLevel, unit.def->name.c_str()); // "*2 Knight"
		else
			swprintf_s(buf, L"%hs", unit.def->name.c_str());
		bv.text = buf;
		m_bench.push_back(std::move(bv));
	}

	m_mode = Mode::Preparation;
	g_renderingEngine->AddRenderObject(this);
}

void BoardUIRenderer::DrawCombat(RenderContext& rc, const CombatPlayback& playback)
{
	m_bars.clear();
	const auto& views = playback.GetUnitViews();
	m_bars.reserve(views.size());

	for (const auto& v : views)
	{
		BarView bar;
		bar.isEnemy = v.isEnemy;
		bar.alive = v.alive;
		bar.hp = v.displayHP;
		bar.maxHP = v.maxHP;
		bar.shield = v.displayShield;
		bar.hpRatio = (v.maxHP > 0) ? (float)v.displayHP / (float)v.maxHP : 0.0f;
		bar.shieldRatio = (v.maxHP > 0) ? (float)v.displayShield / (float)v.maxHP : 0.0f;
		bar.label = v.name + StarSuffix(v.starLevel);

		Vector3 world = v.worldPos;
		world.y += kBarWorldY;
		bar.onScreen = WorldToUI(world, bar.uiPos);

		m_bars.push_back(std::move(bar));
	}

	m_mode = Mode::Combat;
	g_renderingEngine->AddRenderObject(this);
}

void BoardUIRenderer::OnRender2D(RenderContext& rc)
{
	if (m_mode == Mode::None) return;

	m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	m_font.Begin(rc);

	if (m_mode == Mode::Preparation)
	{
		wchar_t title[64];
		swprintf_s(title, L"BENCH (%d)", (int)m_bench.size());
		m_font.Draw(title, Vector2(kBenchX, kBenchTopY), Vector4(0.9f, 0.9f, 0.95f, 1.0f), 0.0f, kBenchTitleScale, kTopLeftPivot);

		for (size_t i = 0; i < m_bench.size(); ++i)
		{
			float y = kBenchTopY - kBenchStepY * (float)(i + 1);
			m_font.Draw(m_bench[i].text.c_str(), Vector2(kBenchX, y), Vector4(0.82f, 0.85f, 0.9f, 1.0f), 0.0f, kBenchItemScale, kTopLeftPivot);
		}
	}
	else if (m_mode == Mode::Combat)
	{
		for (const auto& bar : m_bars)
		{
			if (!bar.onScreen) continue;
			if (!bar.alive) continue; // 撃破済みはバーを消す。

			Vector4 sideColor = bar.isEnemy
				? Vector4(1.0f, 0.72f, 0.72f, 1.0f)
				: Vector4(0.75f, 0.9f, 1.0f, 1.0f);

			// ラベル行(ユニット名 + 星)。
			m_font.Draw(bar.label.c_str(), Vector2(bar.uiPos.x, bar.uiPos.y + 16.0f),
				sideColor, 0.0f, kBarLabelScale, kCenterPivot);

			// HPバー行。
			wchar_t valueLine[96];
			if (bar.shield > 0)
			{
				swprintf_s(valueLine, L"%ls %d/%d +%d",
					MakeBar(bar.hpRatio).c_str(), bar.hp, bar.maxHP, bar.shield);
			}
			else
			{
				swprintf_s(valueLine, L"%ls %d/%d",
					MakeBar(bar.hpRatio).c_str(), bar.hp, bar.maxHP);
			}
			m_font.Draw(valueLine, Vector2(bar.uiPos.x, bar.uiPos.y - 8.0f),
				HPColor(bar.hpRatio), 0.0f, kBarValueScale, kCenterPivot);
		}
	}

	m_font.End(rc);
}

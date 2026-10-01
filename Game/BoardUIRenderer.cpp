#include "stdafx.h"
#include "BoardUIRenderer.h"
#include "CombatPlayback.h"
#include "Player.h"
#include "UIRectRenderer.h"
#include "UIStyle.h"
#include "UITextUtil.h"
#include <algorithm>

namespace
{
	// HPバーを浮かべる高さ(ユニットのワールド座標からの+Yオフセット)。
	// board-layout-rework: モデルスケール縮小(10→4)＋足元を平面に乗せるYリフト(約+40)に追随。
	// リフト分＋モデル高の頭上に来るよう 95(F5是正2。実機で頭とバーの隙間を見て微調整する出発値)。
	const float kBarWorldY = 95.0f;
	// combat-movement-playback: ユニットが移動して見えるようになり、近接戦で両陣営が隣接マスに
	// 寄るとHPバーが横に重なる。敵側のバーだけ一段上へずらして両方読めるようにする(F5微調整前提)。
	const float kEnemyBarYBonus = 26.0f;

	const Vector2 kCenterPivot(0.5f, 0.5f);   // テキストの中心をアンカーにする。
	const Vector2 kTopLeftPivot(0.0f, 1.0f);  // ベンチ一覧用(左上アンカー、FPS表示と同じ)。
	const Vector2 kLeftMidPivot(0.0f, 0.5f);  // バーの前景矩形用(左端基準で右に伸ばす)。

	// combat-number-overlap: 隣接ヘックスのユニット間隔は画面上で数十px程度しかないため、
	// HPブロック(名前+バー+数値)を一回り小さくする(旧: ラベル0.42/数値0.44/バー幅114)。
	const float kBarLabelScale = 0.38f;
	const float kBarValueScale = 0.40f;
	const float kBarLabelY = 16.0f;     // bar.uiPosからのラベル上端のYオフセット。

	// HPバー(塗り矩形)のレイアウト。中心x=bar.uiPos.x、背景の左端を基準に前景を伸ばす。
	const float kBarBgWidth = 90.0f;
	const float kBarBgHeight = 14.0f;
	const float kBarFgWidth = 82.0f;    // 背景の内側(左右4pxずつ余白)。
	const float kBarFgHeight = 10.0f;
	const float kBarY = -6.0f;          // bar.uiPosからのYオフセット(旧ASCIIバーの位置を踏襲)。
	const Vector4 kBarBgColor(0.22f, 0.22f, 0.26f, 0.9f); // 暗いスレート色の半透明背景(黒背景に対しても視認できる明るさ)。

	// スキルゲージバー(塗り矩形)。HPバーの下に幅は揃えてやや薄く配置する。
	const float kGaugeBgHeight = 6.0f;
	const float kGaugeFgHeight = 4.0f;
	const float kGaugeY = -22.0f;       // bar.uiPosからのYオフセット。
	const Vector4 kGaugeColor(0.4f, 0.7f, 1.0f, 1.0f); // 必殺技(マナ)を連想する寒色系。

	// --- combat-number-overlap: HPブロックの重なり回避(UI空間px) ---
	// ブロックの縦範囲(bar.uiPos基準): 上端=ラベル上端、下端=ゲージ背景の下端。
	const float kBlockTop = kBarLabelY;
	const float kBlockBottom = kGaugeY - kGaugeBgHeight * 0.5f;
	const float kBlockGap = 3.0f;            // 積んだブロック同士の隙間。
	const float kMaxBarStackOffset = 100.0f; // 上へずらす量の上限(≒2段)。超える密集時は重なりを許容する。
	const float kBarOffsetFollowRate = 12.0f;// 目標オフセットへの追従速度(1/秒)。移動に伴うガタつき抑制。

	// --- combat-number-overlap: ダメージ/回復ポップアップ ---
	const float kPopupWorldY = 60.0f;        // ユニットの胴体付近(HPバー kBarWorldY=95 より下)から出す。
	const float kPopupRise = 30.0f;          // 寿命の間に上へ流れる量(px、ease-out)。
	const float kPopupFadeSeconds = 0.3f;    // 寿命末尾のフェードアウト時間。
	const float kPopupStackGap = 1.0f;       // 同ユニットで縦に積む時の行間。
	const float kPopupPunchSeconds = 0.12f;  // 必殺技/とどめの「ポン」と大きく出る演出の長さ。
	const float kPopupPunchScale = 0.35f;    // 出始めの拡大率(+35%から等倍へ)。
	// フォントの行の高さ(scale=1.0基準)。MeasureString相当が無いため概算(半角advance=22pxのフォント)。
	const float kFontLineHeight = 40.0f;
	const Vector2 kTextTopLeftPivot(0.0f, 1.0f);

	// テキストを中央揃えで描くための左端X。pivotは正規化アンカーとして効かない(TitleUIRenderer等の
	// 既知の教訓)ため、UITextUtil::EstimateTextWidth の概算幅で自前で中央に寄せる。
	float CenteredTextX(float centerX, const std::wstring& text, float scale)
	{
		return centerX - UITextUtil::EstimateTextWidth(text, scale) * 0.5f;
	}

	// ベンチ一覧のレイアウト定数(kBenchX/kBenchTopY/kBenchStepY)はBoardUIRenderer.hへpublic化した
	// (TraitPanelUIRendererが自分の開始位置を算出するために参照するため。plan.md §4-3)。
	const float kBenchTitleScale = 0.6f;
	const float kBenchItemScale = 0.52f;
	const float kBenchCardPaddingX = 8.0f; // カード内側の左余白(テキストがカード枠に接しないように)。

	Vector4 HPColor(float ratio)
	{
		if (ratio > 0.5f)  return Vector4(0.45f, 0.95f, 0.5f, 1.0f);  // 緑
		if (ratio > 0.25f) return Vector4(0.98f, 0.85f, 0.3f, 1.0f);  // 黄
		return Vector4(1.0f, 0.4f, 0.35f, 1.0f);                      // 赤
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

void BoardUIRenderer::DrawPreparation(RenderContext& rc, const Player& player, bool benchFocused, int benchCursorIndex, int hoveredIndex, UIRectRenderer& rectRenderer)
{
	m_bench.clear();
	// kBenchMaxVisibleRows件まで個別表示し、それを超える分は末尾の集約行にまとめる
	// (ベンチ枚数上限が無いため、カード化しても表示が崩れないようにする。plan.md §4-3)。
	size_t visibleCount = player.bench.size();
	bool hasSummaryRow = visibleCount > (size_t)kBenchMaxVisibleRows;
	if (hasSummaryRow) visibleCount = (size_t)kBenchMaxVisibleRows;

	m_bench.reserve(visibleCount + (hasSummaryRow ? 1 : 0));
	for (size_t i = 0; i < visibleCount; ++i)
	{
		const auto& unit = player.bench[i];
		BenchView bv;
		wchar_t buf[80];
		if (unit.starLevel > 1)
			swprintf_s(buf, L"*%d %hs", unit.starLevel, unit.def->name.c_str()); // "*2 Knight"
		else
			swprintf_s(buf, L"%hs", unit.def->name.c_str());
		bv.text = buf;
		m_bench.push_back(std::move(bv));
	}
	if (hasSummaryRow)
	{
		BenchView bv;
		wchar_t buf[32];
		swprintf_s(buf, L"...+%d件", (int)(player.bench.size() - visibleCount));
		bv.text = buf;
		bv.isSummaryRow = true;
		m_bench.push_back(std::move(bv));
	}

	m_benchFocused = benchFocused;
	m_benchCursorIndex = benchCursorIndex;
	m_benchHoveredIndex = hoveredIndex;
	m_rectRenderer = &rectRenderer;
	m_mode = Mode::Preparation;
	g_renderingEngine->AddRenderObject(this);
}

void BoardUIRenderer::BuildHotRegions(const Player& player, UIHotRegionList& out) const
{
	// ベンチ一覧の各行(kBenchX起点、kBenchStepY間隔で下へ伸びる。DrawPreparation()と同じ定数)。
	// kBenchMaxVisibleRowsを超える分は集約行になり個別のクリック対象が無いため登録しない。
	size_t visibleCount = player.bench.size();
	if (visibleCount > (size_t)kBenchMaxVisibleRows) visibleCount = (size_t)kBenchMaxVisibleRows;

	for (size_t i = 0; i < visibleCount; ++i)
	{
		float y = kBenchTopY - kBenchStepY * (float)(i + 1);

		UIHotRegion region;
		region.kind = UIRegionKind::BenchUnit;
		region.index = (int)i;
		region.minX = kBenchX - 4.0f;
		region.maxX = kBenchX + 260.0f; // ベンチ行の想定最大幅(実機で要微調整)。
		region.maxY = y + 4.0f;
		region.minY = y - kBenchStepY + 8.0f;
		out.push_back(region);
	}
}

void BoardUIRenderer::DrawCombat(RenderContext& rc, const CombatPlayback& playback, UIRectRenderer& rectRenderer)
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
		bar.gaugeRatio = (v.skillThreshold > 0) ? (float)v.displayGauge / (float)v.skillThreshold : 0.0f;
		if (bar.gaugeRatio > 1.0f) bar.gaugeRatio = 1.0f;
		bar.label = v.name + StarSuffix(v.starLevel);

		Vector3 world = v.worldPos;
		world.y += kBarWorldY + (v.isEnemy ? kEnemyBarYBonus : 0.0f);
		bar.onScreen = WorldToUI(world, bar.uiPos);

		m_bars.push_back(std::move(bar));
	}

	// combat-number-overlap: 戦闘が切り替わったら(またはユニット数が変わったら)ずらし量をリセットする。
	if (m_playbackSerial != playback.GetBeginSerial() || m_barOffsetY.size() != m_bars.size())
	{
		m_playbackSerial = playback.GetBeginSerial();
		m_barOffsetY.assign(m_bars.size(), 0.0f);
	}
	DeclutterBars(g_gameTime->GetFrameDeltaTime());
	BuildPopupViews(playback);

	m_rectRenderer = &rectRenderer;
	m_mode = Mode::Combat;
	g_renderingEngine->AddRenderObject(this);
}

void BoardUIRenderer::DeclutterBars(float deltaTime)
{
	struct BlockRect { float minX, maxX, minY, maxY; };

	// 手前(画面下側=uiPos.yが小さい)のブロックから順に置き、後から置くものが既存と重なれば
	// その上端の上へ押し上げる。並びは添字で安定させ、同値でフレーム毎に順序が入れ替わらないようにする。
	std::vector<size_t> order;
	order.reserve(m_bars.size());
	for (size_t i = 0; i < m_bars.size(); ++i)
	{
		if (m_bars[i].onScreen && m_bars[i].alive) order.push_back(i);
	}
	std::sort(order.begin(), order.end(), [this](size_t a, size_t b) {
		if (m_bars[a].uiPos.y != m_bars[b].uiPos.y) return m_bars[a].uiPos.y < m_bars[b].uiPos.y;
		return a < b;
	});

	std::vector<float> target(m_bars.size(), 0.0f);
	std::vector<BlockRect> placed;
	placed.reserve(order.size());
	for (size_t i : order)
	{
		const BarView& bar = m_bars[i];
		float labelWidth = UITextUtil::EstimateTextWidth(bar.label, kBarLabelScale);
		float halfW = ((labelWidth > kBarBgWidth) ? labelWidth : kBarBgWidth) * 0.5f;
		float baseMinY = bar.uiPos.y + kBlockBottom;
		float height = kBlockTop - kBlockBottom;

		float offset = 0.0f;
		// 押し上げた先で別のブロックに当たることがあるので、当たらなくなるまで繰り返す(上限付き)。
		for (int iter = 0; iter < 16; ++iter)
		{
			bool moved = false;
			float minY = baseMinY + offset;
			float maxY = minY + height;
			for (const auto& r : placed)
			{
				bool overlapX = (bar.uiPos.x - halfW) < r.maxX && (bar.uiPos.x + halfW) > r.minX;
				bool overlapY = (minY - kBlockGap) < r.maxY && (maxY + kBlockGap) > r.minY;
				if (overlapX && overlapY)
				{
					offset = r.maxY + kBlockGap - baseMinY;
					moved = true;
					break;
				}
			}
			if (!moved || offset > kMaxBarStackOffset) break;
		}
		if (offset > kMaxBarStackOffset) offset = kMaxBarStackOffset;
		if (offset < 0.0f) offset = 0.0f;

		target[i] = offset;
		placed.push_back({ bar.uiPos.x - halfW, bar.uiPos.x + halfW, baseMinY + offset, baseMinY + offset + height });
	}

	// 目標へ滑らかに追従させる(ユニットの移動で目標が切り替わってもバーが瞬間移動しない)。
	float t = deltaTime * kBarOffsetFollowRate;
	if (t > 1.0f) t = 1.0f;
	for (size_t i = 0; i < m_bars.size(); ++i)
	{
		m_barOffsetY[i] += (target[i] - m_barOffsetY[i]) * t;
		m_bars[i].uiPos.y += m_barOffsetY[i];
	}
}

void BoardUIRenderer::BuildPopupViews(const CombatPlayback& playback)
{
	m_popupViews.clear();
	const auto& views = playback.GetUnitViews();
	const auto& popups = playback.GetPopups();
	if (popups.empty()) return;
	m_popupViews.reserve(popups.size());

	// ユニットごとに、新しい数字ほど下(出現位置付近)、古い数字ほど上に積む。
	// popups は生成順(古い→新しい)なので、後ろから走査して同ユニット分の直前の上端を覚えておく。
	std::vector<float> prevTop(views.size(), 0.0f);
	std::vector<bool> hasPrev(views.size(), false);
	std::vector<Vector2> anchor(views.size());
	std::vector<int> anchorState(views.size(), 0); // 0=未計算, 1=画面内, -1=画面外。

	for (auto it = popups.rbegin(); it != popups.rend(); ++it)
	{
		const auto& p = *it;
		if (p.viewIndex >= views.size()) continue;

		if (anchorState[p.viewIndex] == 0)
		{
			Vector3 world = views[p.viewIndex].worldPos;
			world.y += kPopupWorldY;
			anchorState[p.viewIndex] = WorldToUI(world, anchor[p.viewIndex]) ? 1 : -1;
		}
		if (anchorState[p.viewIndex] < 0) continue;

		float life = CombatPlayback::GetPopupLifetime(p);
		float t = (life > 0.0f) ? p.age / life : 1.0f;
		if (t > 1.0f) t = 1.0f;

		// 種類ごとの色・大きさ。重要な数字(必殺技直撃・とどめ)ほど大きく目立たせる。
		PopupView pv;
		Vector4 color;
		float scale = 0.5f;
		bool punch = false;
		wchar_t buf[32];
		swprintf_s(buf, L"%d", p.amount);
		switch (p.kind)
		{
		case CombatPlayback::PopupKind::Physical: color = Vector4(1.0f, 0.95f, 0.85f, 1.0f); scale = 0.48f; break;
		case CombatPlayback::PopupKind::Magic:    color = Vector4(0.82f, 0.62f, 1.0f, 1.0f); scale = 0.48f; break;
		case CombatPlayback::PopupKind::Skill:    color = Vector4(1.0f, 0.82f, 0.2f, 1.0f);  scale = 0.64f; punch = true; break;
		case CombatPlayback::PopupKind::Burn:     color = Vector4(1.0f, 0.55f, 0.2f, 1.0f);  scale = 0.40f; break;
		case CombatPlayback::PopupKind::Heal:
			color = Vector4(0.45f, 1.0f, 0.5f, 1.0f); scale = 0.46f;
			swprintf_s(buf, L"+%d", p.amount);
			break;
		}
		if (p.lethal)
		{
			color = Vector4(1.0f, 0.32f, 0.25f, 1.0f);
			scale *= 1.2f;
			punch = true;
		}
		if (punch && p.age < kPopupPunchSeconds)
		{
			scale *= 1.0f + kPopupPunchScale * (1.0f - p.age / kPopupPunchSeconds);
		}

		float remaining = life - p.age;
		color.w = (remaining < kPopupFadeSeconds) ? (remaining / kPopupFadeSeconds) : 1.0f;
		if (color.w < 0.0f) color.w = 0.0f;

		pv.text = buf;
		pv.color = color;
		pv.scale = scale;

		// 上へ流れる(ease-out)。テキストは左上基準なので、下端=anchor+rise になるよう上端を求める。
		float rise = kPopupRise * (1.0f - (1.0f - t) * (1.0f - t));
		float height = kFontLineHeight * scale;
		float top = anchor[p.viewIndex].y + rise + height;
		// 同ユニットで直前(より新しい)の数字と重ならないよう、その上に積む。
		if (hasPrev[p.viewIndex])
		{
			float minTop = prevTop[p.viewIndex] + kPopupStackGap + height;
			if (top < minTop) top = minTop;
		}
		prevTop[p.viewIndex] = top;
		hasPrev[p.viewIndex] = true;

		pv.topLeft = Vector2(CenteredTextX(anchor[p.viewIndex].x, pv.text, scale), top);
		m_popupViews.push_back(std::move(pv));
	}
}

void BoardUIRenderer::OnRender2D(RenderContext& rc)
{
	if (m_mode == Mode::None) return;

	// 矩形(Sprite)はFont::Begin()〜End()の外側でまとめて描き終える(SpriteBatchの状態と
	// 競合するため。docs/tasks/ui-sprite-bars/plan.md §0-8)。背景→前景の順で描くことで、
	// あとから描くFontのテキストが最前面に来る。
	if (m_mode == Mode::Preparation && m_rectRenderer != nullptr)
	{
		// ベンチ一覧をカードリスト化する(ui-mouse-cardsフェーズ3、plan.md §4-2)。集約行
		// ("...+N件")にはカード枠を付けない(クリック対象ではないため)。
		for (size_t i = 0; i < m_bench.size(); ++i)
		{
			if (m_bench[i].isSummaryRow) continue;

			float y = kBenchTopY - kBenchStepY * (float)(i + 1);
			Vector2 cardCenter(kBenchX + 130.0f, y - kBenchStepY * 0.5f + 6.0f);
			Vector2 cardSize(268.0f, kBenchStepY - 4.0f);

			bool selected = m_benchFocused && ((int)i == m_benchCursorIndex);
			bool hovered = ((int)i == m_benchHoveredIndex);
			Vector4 borderColor = selected ? UIStyle::kSelectedBorderColor
				: hovered ? UIStyle::kHoveredBorderColor : UIStyle::kPanelBorderColor;
			float borderThickness = selected ? UIStyle::kSelectedBorderThickness : UIStyle::kPanelBorderThickness;

			m_rectRenderer->DrawPanel(rc, cardCenter, cardSize, UIStyle::kPanelFillColor, borderColor, borderThickness, kCenterPivot);
		}
	}
	else if (m_mode == Mode::Combat && m_rectRenderer != nullptr)
	{
		for (const auto& bar : m_bars)
		{
			if (!bar.onScreen || !bar.alive) continue;

			// HPバー: 背景(小カード枠付き) → HP前景(左詰め) → シールド前景(HP前景の右に隣接)。
			// 3Dシーン上のオーバーレイなので過剰装飾はしない(plan.md §4-2、薄い枠を足す程度)。
			Vector2 bgPos(bar.uiPos.x, bar.uiPos.y + kBarY);
			m_rectRenderer->DrawPanel(rc, bgPos, Vector2(kBarBgWidth, kBarBgHeight), kBarBgColor, UIStyle::kPanelBorderColor, 1.0f, kCenterPivot);

			Vector2 hpPos(bar.uiPos.x - kBarFgWidth * 0.5f, bar.uiPos.y + kBarY);
			float hpWidth = kBarFgWidth * bar.hpRatio;
			m_rectRenderer->DrawRect(rc, hpPos, Vector2(hpWidth, kBarFgHeight), HPColor(bar.hpRatio), kLeftMidPivot);

			if (bar.shieldRatio > 0.0f)
			{
				float remaining = 1.0f - bar.hpRatio;
				float shieldRatio = (bar.shieldRatio < remaining) ? bar.shieldRatio : remaining; // 背景幅を超えて描かない。
				if (shieldRatio > 0.0f)
				{
					Vector2 shieldPos(hpPos.x + hpWidth, bar.uiPos.y + kBarY);
					m_rectRenderer->DrawRect(rc, shieldPos, Vector2(kBarFgWidth * shieldRatio, kBarFgHeight),
						Vector4(0.75f, 0.9f, 1.0f, 0.9f), kLeftMidPivot);
				}
			}

			// スキルゲージバー: HPバーの下に、背景 → 前景(左詰め)。
			Vector2 gaugeBgPos(bar.uiPos.x, bar.uiPos.y + kGaugeY);
			m_rectRenderer->DrawRect(rc, gaugeBgPos, Vector2(kBarBgWidth, kGaugeBgHeight), kBarBgColor, kCenterPivot);

			Vector2 gaugePos(bar.uiPos.x - kBarFgWidth * 0.5f, bar.uiPos.y + kGaugeY);
			m_rectRenderer->DrawRect(rc, gaugePos, Vector2(kBarFgWidth * bar.gaugeRatio, kGaugeFgHeight), kGaugeColor, kLeftMidPivot);
		}
	}

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

			// ラベル行(ユニット名 + 星)。combat-number-overlap: 旧実装は kCenterPivot 指定だったが
			// このFontEngineのpivotは正規化アンカーとして効かず、実際はバー中心から右へ伸びる左詰めに
			// なっていて隣のユニットのブロックへはみ出していた。概算幅で左端を求めて中央揃えにする。
			m_font.Draw(bar.label.c_str(),
				Vector2(CenteredTextX(bar.uiPos.x, bar.label, kBarLabelScale), bar.uiPos.y + kBarLabelY),
				sideColor, 0.0f, kBarLabelScale, kTextTopLeftPivot);

			// HP数値行(バー本体は矩形で既に描画済み、ここは数値のみ)。
			wchar_t valueLine[64];
			if (bar.shield > 0)
			{
				swprintf_s(valueLine, L"%d/%d +%d", bar.hp, bar.maxHP, bar.shield);
			}
			else
			{
				swprintf_s(valueLine, L"%d/%d", bar.hp, bar.maxHP);
			}
			std::wstring valueText(valueLine);
			m_font.Draw(valueLine,
				Vector2(CenteredTextX(bar.uiPos.x, valueText, kBarValueScale), bar.uiPos.y + kBarY),
				HPColor(bar.hpRatio), 0.0f, kBarValueScale, kTextTopLeftPivot);
		}

		// combat-number-overlap: ダメージ/回復ポップアップ。HPブロックより後に描いて最前面に出す。
		// フェードに合わせて影の濃さも落とす(文字だけ消えて黒い影が残らないように)。
		for (const auto& pv : m_popupViews)
		{
			if (pv.color.w <= 0.0f) continue;
			m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, pv.color.w));
			m_font.Draw(pv.text.c_str(), pv.topLeft, pv.color, 0.0f, pv.scale, kTextTopLeftPivot);
		}
		m_font.SetShadowParam(true, 2.0f, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	}

	m_font.End(rc);
}

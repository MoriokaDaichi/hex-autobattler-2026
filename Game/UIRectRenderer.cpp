#include "stdafx.h"
#include "UIRectRenderer.h"

namespace
{
	// 実機で観測されたピーク枚数(戦闘フェーズ: 盤面のユニット数 × ユニットあたり
	// 背景/HP前景/シールド/ゲージ背景/ゲージ前景 の最大5枚 + XPバー2枚 + 暗幕1枚)を
	// 見込んだ事前確保数。描画中(OnRender2D)の生成を避けるため多めに取る。
	// これを超えた場合は AcquireSprite() が実行時に生成する(フォールバック)。
	// 最大盤面(両陣営ほぼ満杯 ≒ 20体 × 最大5枚 + XPバー2枚 + 暗幕1枚 ≒ 103枚)に余裕を持たせた値。
	const size_t kPrewarmCount = 160;

	void InitOneSprite(Sprite& sprite)
	{
		SpriteInitData initData;
		// パスは"Assets/..."起点("Game/"無し)。HexGridRenderer::InitShaders()の
		// LoadVS("Assets/shader/hexGrid.fx",...)、FontEngine のspritefontパスと同じ規約
		// (実行時カレントディレクトリがGame/である前提)。
		initData.m_ddsFilePath[0] = "Assets/spriteData/color/white.dds";
		initData.m_fxFilePath = "Assets/shader/sprite.fx";
		initData.m_width = 32;  // white.ddsの実サイズ(texconvで32x32へリサイズ済み)。
		initData.m_height = 32; // DrawRect側のsize指定はスケールで別途制御するため、ここは原寸を渡すだけでよい。
		initData.m_alphaBlendMode = AlphaBlendMode_Trans; // 半透明(暗幕)・不透明(HP/XPバー)の両方をこれ1本で賄う(alpha=1なら実質不透明)。
		sprite.Init(initData);
	}
}

void UIRectRenderer::Init()
{
	// [触らないこと] g_camera2D はグローバルで、RenderingEngine の全画面合成スプライト
	// (m_mainSprite / m_2DSprite)と共有されている。位置/ターゲットを書き換えると3Dシーン
	// 合成ごと左右反転する回帰が出る。far面クリップ対策は DrawRect() の Update() に渡す Z で行う。
	m_pool.reserve(kPrewarmCount);
	for (size_t i = 0; i < kPrewarmCount; ++i)
	{
		m_pool.push_back(std::make_unique<Sprite>());
		InitOneSprite(*m_pool.back());
	}
	m_used = 0;
}

void UIRectRenderer::BeginFrame()
{
	m_used = 0;
}

Sprite& UIRectRenderer::AcquireSprite()
{
	if (m_used >= m_pool.size())
	{
		// 事前確保を超えた。描画中の生成になるが、通常フレームでは起きない想定。
		m_pool.push_back(std::make_unique<Sprite>());
		InitOneSprite(*m_pool.back());
	}
	return *m_pool[m_used++];
}

void UIRectRenderer::DrawRect(RenderContext& rc, const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& pivot)
{
	Sprite& sprite = AcquireSprite();

	// size(ピクセル) / テクスチャ原寸(32x32) でスケールを求め、白テクスチャを目的の矩形サイズへ引き伸ばす。
	Vector3 scale(size.x / 32.0f, size.y / 32.0f, 1.0f);
	// Z は -0.5f。既定 g_camera2D は eye z=-1 / target z=0(forward +Z)、Sprite::Draw() の
	// 直交射影は near0.1/far1.0。world z=0 だと far 面ちょうどでクリップされ得るため、
	// eye とターゲットの間(near/far 内・ミラーなし)に置く。g_camera2D 自体は書き換えない。
	sprite.Update(Vector3(pos.x, pos.y, -0.5f), Quaternion::Identity, scale, pivot);
	sprite.SetMulColor(color);
	sprite.Draw(rc);
}

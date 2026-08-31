#include "stdafx.h"
#include "UIRectRenderer.h"

void UIRectRenderer::Init()
{
	// 注意: g_camera2D はグローバルで、RenderingEngine の全画面合成スプライト
	// (m_mainSprite / m_2DSprite) と共有されている。ここで位置/ターゲットを書き換えると
	// 3Dシーン合成ごと左右反転する回帰が出る。触らないこと。
	// Z=0 の far 面クリップ対策は DrawRect() 側で Update() に渡す Z で行う(下記参照)。

	SpriteInitData initData;
	// パスは"Assets/..."起点("Game/"無し)。HexGridRenderer::InitShaders()の
	// LoadVS("Assets/shader/hexGrid.fx",...)、FontEngine::Init()のspritefontパスと同じ規約
	// (実行時カレントディレクトリがGame/である前提)。
	initData.m_ddsFilePath[0] = "Assets/spriteData/color/white.dds";
	initData.m_fxFilePath = "Assets/shader/sprite.fx";
	initData.m_width = 32;  // white.ddsの実サイズ(texconvで32x32へリサイズ済み)。
	initData.m_height = 32; // DrawRect側のsize指定はスケールで別途制御するため、ここは原寸を渡すだけでよい。
	initData.m_alphaBlendMode = AlphaBlendMode_Trans; // 半透明(暗幕)・不透明(HP/XPバー)の両方をこれ1本で賄う(alpha=1なら実質不透明)。
	m_sprite.Init(initData);
}

void UIRectRenderer::DrawRect(RenderContext& rc, const Vector2& pos, const Vector2& size, const Vector4& color, const Vector2& pivot)
{
	// size(ピクセル) / テクスチャ原寸(32x32) でスケールを求め、白テクスチャを目的の矩形サイズへ引き伸ばす。
	Vector3 scale(size.x / 32.0f, size.y / 32.0f, 1.0f);
	// Z は -0.5f。既定 g_camera2D は eye z=-1 / target z=0(forward +Z)、Sprite::Draw() の
	// 直交射影は near0.1/far1.0。world z=0 だと far 面ちょうどでクリップされ得るため、
	// eye とターゲットの間(near/far 内・ミラーなし)に置く。g_camera2D 自体は書き換えない。
	m_sprite.Update(Vector3(pos.x, pos.y, -0.5f), Quaternion::Identity, scale, pivot);
	m_sprite.SetMulColor(color);
	m_sprite.Draw(rc);
}

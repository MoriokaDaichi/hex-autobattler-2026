#include "stdafx.h"
#include "UIRectRenderer.h"

void UIRectRenderer::Init()
{
	// Sprite::Draw()はg_camera2Dのビュー行列と、0.1〜1.0という狭いnear/far(ハードコードされた
	// 直交射影)でMVPを組む。g_camera2Dはデフォルトのposition(0,0,1)/target(0,0,0)のままだと、
	// Update()に渡すZ=0のスプライトがちょうどfar面(カメラからの距離1.0)上に乗ってしまい、
	// 浮動小数の誤差でクリップされて何も描画されない(Game/配下でSprite未使用だったため
	// 気づかれていなかった問題)。near/farの中間(距離0.5)にsprite群が収まるよう、
	// ここでg_camera2Dを明示的に設定する(g_camera2DはSprite専用でFont等は使わないため、
	// 他の描画への影響は無い)。
	g_camera2D->SetPosition(Vector3(0.0f, 0.0f, 0.5f));
	g_camera2D->SetTarget(Vector3(0.0f, 0.0f, 0.0f));

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
	m_sprite.Update(Vector3(pos.x, pos.y, 0.0f), Quaternion::Identity, scale, pivot);
	m_sprite.SetMulColor(color);
	m_sprite.Draw(rc);
}

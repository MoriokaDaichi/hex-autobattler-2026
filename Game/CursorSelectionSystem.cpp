#include "stdafx.h"
#include "CursorSelectionSystem.h"
#include "HexGridRenderer.h"

namespace
{
	// 盤面フォーカス時、矢印キー/D-padの1回のトリガー入力で隣接マスへ移動するための
	// axial方向オフセット(HexCoord::GetNeighborsの方向定義のうち上下左右に対応する4つ)。
	struct Direction { int dq; int dr; };
	const Direction kLeft{ -1, 0 };
	const Direction kRight{ 1, 0 };
	const Direction kUp{ 0, -1 };
	const Direction kDown{ 0, 1 };

	// まだヘックスカーソルが未確定の状態でキー入力があった場合に開始点とする盤面中央マス。
	const HexCoord kBoardCenter(4, 1);
}

void CursorSelectionSystem::Update()
{
	UpdateFocusSwitch();
	UpdateListCursor();
	UpdateHexCursor();
}

void CursorSelectionSystem::UpdateFocusSwitch()
{
	// Tabキー、またはゲームパッドのSelectボタンでフォーカスを巡回させる。
	bool switchTriggered = g_keyboard->IsTrigger(VK_TAB) || g_pad[0]->IsTrigger(enButtonSelect);
	if (!switchTriggered) {
		return;
	}

	switch (m_focus)
	{
	case InputFocus::Shop:
		m_focus = InputFocus::Bench;
		break;
	case InputFocus::Bench:
		m_focus = InputFocus::Items;
		break;
	case InputFocus::Items:
		m_focus = InputFocus::Board;
		break;
	case InputFocus::Board:
		m_focus = InputFocus::Shop;
		break;
	}
	// 一覧⇔盤面でカーソルの意味が変わるため、フォーカスが変わるたびに一覧カーソルは先頭に戻す。
	m_listCursorIndex = 0;
}

void CursorSelectionSystem::UpdateListCursor()
{
	if (m_focus != InputFocus::Shop && m_focus != InputFocus::Bench && m_focus != InputFocus::Items) {
		return;
	}

	if (g_keyboard->IsTrigger(VK_LEFT) || g_pad[0]->IsTrigger(enButtonLeft)) {
		m_listCursorIndex--;
	}
	else if (g_keyboard->IsTrigger(VK_RIGHT) || g_pad[0]->IsTrigger(enButtonRight)) {
		m_listCursorIndex++;
	}
	// 負値だけここで防ぎ、上限はClampListCursor(一覧の実際の要素数を知っている呼び出し側)に委ねる。
	if (m_listCursorIndex < 0) {
		m_listCursorIndex = 0;
	}
}

void CursorSelectionSystem::ClampListCursor(int count)
{
	if (count <= 0) {
		m_listCursorIndex = 0;
		return;
	}
	if (m_listCursorIndex >= count) {
		m_listCursorIndex = count - 1;
	}
}

void CursorSelectionSystem::UpdateHexCursor()
{
	if (m_focus != InputFocus::Board) {
		return;
	}

	// マウスホバーは継続的な入力なので、毎フレーム有効な盤面上の位置を指していれば追従する。
	HexCoord mouseHex;
	if (TryMouseToHex(mouseHex)) {
		m_hexCursor = mouseHex;
		m_hexCursorValid = true;
	}

	// 矢印キー/D-padでの単発移動。マウスより後に適用することで、押されたフレームは
	// キー操作を優先しつつ、それ以外のフレームはマウスに追従する。
	const Direction* dir = nullptr;
	if (g_keyboard->IsTrigger(VK_LEFT) || g_pad[0]->IsTrigger(enButtonLeft)) {
		dir = &kLeft;
	}
	else if (g_keyboard->IsTrigger(VK_RIGHT) || g_pad[0]->IsTrigger(enButtonRight)) {
		dir = &kRight;
	}
	else if (g_keyboard->IsTrigger(VK_UP) || g_pad[0]->IsTrigger(enButtonUp)) {
		dir = &kUp;
	}
	else if (g_keyboard->IsTrigger(VK_DOWN) || g_pad[0]->IsTrigger(enButtonDown)) {
		dir = &kDown;
	}

	if (dir != nullptr) {
		HexCoord base = m_hexCursorValid ? m_hexCursor : kBoardCenter;
		HexCoord candidate(base.q + dir->dq, base.r + dir->dr);
		if (HexGridRenderer::IsValidHex(candidate)) {
			m_hexCursor = candidate;
			m_hexCursorValid = true;
		}
		else if (!m_hexCursorValid && HexGridRenderer::IsValidHex(base)) {
			// 盤面端で移動できなくても、初回は中央マスを選択状態にする。
			m_hexCursor = base;
			m_hexCursorValid = true;
		}
	}
}

bool CursorSelectionSystem::TryMouseToHex(HexCoord& outHex) const
{
	int mx = g_mouse->GetPositionX();
	int my = g_mouse->GetPositionY();

	if (mx < 0 || my < 0 || mx >= static_cast<int>(FRAME_BUFFER_W) || my >= static_cast<int>(FRAME_BUFFER_H)) {
		// ウィンドウのクライアント領域外。
		return false;
	}

	// スクリーン座標(左上原点、ピクセル) -> NDC(-1〜1、Yは上向き正)。
	float ndcX = (static_cast<float>(mx) / FRAME_BUFFER_W) * 2.0f - 1.0f;
	float ndcY = 1.0f - (static_cast<float>(my) / FRAME_BUFFER_H) * 2.0f;

	// このプロジェクトの数学ライブラリのMatrixには、パースペクティブ除算込みでVector3を
	// 変換するヘルパーが無いため、Vector4で変換してwで除算する(TransformCoord相当)。
	Matrix vpInv = g_camera3D->GetViewProjectionMatrixInv();
	Vector4 nearH(ndcX, ndcY, 0.0f, 1.0f);
	vpInv.Apply(nearH);
	Vector4 farH(ndcX, ndcY, 1.0f, 1.0f);
	vpInv.Apply(farH);

	if (fabsf(nearH.w) < 0.0001f || fabsf(farH.w) < 0.0001f) {
		return false;
	}

	Vector3 nearPos(nearH.x / nearH.w, nearH.y / nearH.w, nearH.z / nearH.w);
	Vector3 farPos(farH.x / farH.w, farH.y / farH.w, farH.z / farH.w);
	Vector3 dir = farPos - nearPos;

	if (fabsf(dir.y) < 0.0001f) {
		// 視線がほぼ水平で、盤面の平面(y=0)と交差しない。
		return false;
	}

	float t = -nearPos.y / dir.y;
	if (t < 0.0f) {
		// 盤面の平面がカメラの後方にある。
		return false;
	}

	Vector3 worldPos = nearPos + dir * t;
	return HexGridRenderer::TryWorldPositionToHex(worldPos, outHex);
}

bool CursorSelectionSystem::GetHexCursor(HexCoord& outHex) const
{
	if (!m_hexCursorValid) {
		return false;
	}
	outHex = m_hexCursor;
	return true;
}

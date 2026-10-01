#include "stdafx.h"
#include "CursorSelectionSystem.h"
#include "system/system.h" // g_hWnd(実際のウィンドウクライアント矩形を取得するため)。

bool CursorSelectionSystem::GetNormalizedMousePosition(float& outU, float& outV)
{
	RECT clientRect;
	if (!GetClientRect(g_hWnd, &clientRect)) {
		return false;
	}
	int clientW = clientRect.right - clientRect.left;
	int clientH = clientRect.bottom - clientRect.top;
	if (clientW <= 0 || clientH <= 0) {
		return false;
	}

	int mx = g_mouse->GetPositionX();
	int my = g_mouse->GetPositionY();
	if (mx < 0 || my < 0 || mx >= clientW || my >= clientH) {
		// ウィンドウのクライアント領域外。
		return false;
	}

	outU = static_cast<float>(mx) / static_cast<float>(clientW);
	outV = static_cast<float>(my) / static_cast<float>(clientH);
	return true;
}

bool CursorSelectionSystem::ScreenToUISpace(Vector2& outUI)
{
	// 実際のウィンドウクライアント矩形(GetClientRect)基準で正規化することで、DPIスケーリング等で
	// クライアント領域の実ピクセル数がUI_SPACE_WIDTH/HEIGHT(1920x1080)と一致しない環境でも
	// UI_SPACEへ正しく写像できる(以前はFRAME_BUFFER_W/Hで直接割っており、クライアント領域が
	// それと異なるサイズになる環境ではマウス操作可能な範囲が画面の一部に縮小/拡大してしまっていた)。
	float u, v;
	if (!GetNormalizedMousePosition(u, v)) {
		return false;
	}

	outUI.x = u * static_cast<float>(UI_SPACE_WIDTH) - static_cast<float>(UI_SPACE_WIDTH) * 0.5f;
	outUI.y = static_cast<float>(UI_SPACE_HEIGHT) * 0.5f - v * static_cast<float>(UI_SPACE_HEIGHT);
	return true;
}

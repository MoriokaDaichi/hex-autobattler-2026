/*!
*@brief	マウス。
*/
#include "k2EngineLowPreCompile.h"
#include "Mouse.h"

namespace nsK2EngineLow {
	Mouse* g_mouse = nullptr;

	namespace {
		const int vKeyTable[enMouseButtonNum] = {
			VK_LBUTTON,
			VK_RBUTTON,
			VK_MBUTTON,
		};
	}

	Mouse::Mouse()
	{
		memset(m_trigger, 0, sizeof(m_trigger));
		memset(m_press, 0, sizeof(m_press));
	}
	Mouse::~Mouse()
	{
	}
	void Mouse::Update()
	{
		for (int button = 0; button < enMouseButtonNum; button++) {
			if (GetAsyncKeyState(vKeyTable[button]) & 0x8000) {
				m_trigger[button] = 1 ^ m_press[button];
				m_press[button] = 1;
			}
			else {
				m_trigger[button] = 0;
				m_press[button] = 0;
			}
		}

		POINT point;
		GetCursorPos(&point);
		if (m_hWnd) {
			ScreenToClient(m_hWnd, &point);
		}
		m_posX = point.x;
		m_posY = point.y;
	}
}

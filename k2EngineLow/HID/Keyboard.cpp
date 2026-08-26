/*!
*@brief	キーボード。
*/
#include "k2EngineLowPreCompile.h"
#include "Keyboard.h"

namespace nsK2EngineLow {
	Keyboard* g_keyboard = nullptr;

	Keyboard::Keyboard()
	{
		memset(m_trigger, 0, sizeof(m_trigger));
		memset(m_press, 0, sizeof(m_press));
	}
	Keyboard::~Keyboard()
	{
	}
	void Keyboard::Update()
	{
		// 仮想キーコード0は未定義のため1から走査する。
		for (int vKey = 1; vKey < kKeyCodeNum; vKey++) {
			if (GetAsyncKeyState(vKey) & 0x8000) {
				m_trigger[vKey] = 1 ^ m_press[vKey];
				m_press[vKey] = 1;
			}
			else {
				m_trigger[vKey] = 0;
				m_press[vKey] = 0;
			}
		}
	}
}

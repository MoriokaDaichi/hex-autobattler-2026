#pragma once

namespace nsK2EngineLow {
	/// <summary>
	/// キーボードクラス。
	/// 仮想キーコード(VK_*、'A'?'Z'/'0'?'9'含む)をそのまま添字として使い、
	/// GetAsyncKeyStateによるポーリングでトリガー/プレス状態を管理する。
	/// GamePadのキーボードフォールバック処理と同じ方式。
	/// </summary>
	class Keyboard : public Noncopyable {
	public:
		static const int kKeyCodeNum = 256;	// 仮想キーコードの範囲は0?255。

		Keyboard();
		~Keyboard();

		/// <summary>
		/// キーボード状態の更新。
		/// </summary>
		void Update();

		/// <summary>
		/// キーのトリガー入力判定。
		/// </summary>
		/// <param name="vKey">判定したい仮想キーコード(VK_*)</param>
		/// <returns>trueが返ってきたらトリガー入力されている</returns>
		bool IsTrigger(int vKey) const
		{
			return m_trigger[vKey] != 0;
		}

		/// <summary>
		/// キーが押されているか判定。
		/// </summary>
		/// <param name="vKey">判定したい仮想キーコード(VK_*)</param>
		/// <returns>trueが返ってきたら押されている。</returns>
		bool IsPress(int vKey) const
		{
			return m_press[vKey] != 0;
		}

	private:
		int m_trigger[kKeyCodeNum];	// トリガー入力のフラグ。
		int m_press[kKeyCodeNum];		// press入力のフラグ。
	};

	extern Keyboard* g_keyboard;
}

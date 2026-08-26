#pragma once

namespace nsK2EngineLow {
	/// <summary>
	/// マウスボタン定義。
	/// </summary>
	enum EnMouseButton {
		enMouseButtonLeft,		//!<左ボタン。
		enMouseButtonRight,		//!<右ボタン。
		enMouseButtonMiddle,	//!<中ボタン。
		enMouseButtonNum,		//!<ボタンの数。
	};

	/// <summary>
	/// マウスクラス。
	/// GamePadのキーボードフォールバック処理と同じく、GetAsyncKeyState/GetCursorPosに
	/// よるポーリング方式で状態を取得する(ウィンドウメッセージのフックは行わない)。
	/// </summary>
	class Mouse : public Noncopyable {
	public:
		Mouse();
		~Mouse();

		/// <summary>
		/// 初期化。
		/// </summary>
		/// <param name="hWnd">座標をクライアント座標系に変換するために使うウィンドウハンドル。</param>
		void Init(HWND hWnd)
		{
			m_hWnd = hWnd;
		}

		/// <summary>
		/// マウス状態の更新。
		/// </summary>
		void Update();

		/// <summary>
		/// ボタンのトリガー入力判定。
		/// </summary>
		/// <param name="button">判定したいボタン</param>
		/// <returns>trueが返ってきたらトリガー入力されている</returns>
		bool IsTrigger(EnMouseButton button) const
		{
			return m_trigger[button] != 0;
		}

		/// <summary>
		/// ボタンが押されているか判定。
		/// </summary>
		/// <param name="button">判定したいボタン</param>
		/// <returns>trueが返ってきたら押されている。</returns>
		bool IsPress(EnMouseButton button) const
		{
			return m_press[button] != 0;
		}

		/// <summary>
		/// マウスカーソルのX座標を取得(ウィンドウのクライアント座標系、左上原点、単位ピクセル)。
		/// </summary>
		int GetPositionX() const
		{
			return m_posX;
		}

		/// <summary>
		/// マウスカーソルのY座標を取得(ウィンドウのクライアント座標系、左上原点、単位ピクセル)。
		/// </summary>
		int GetPositionY() const
		{
			return m_posY;
		}

	private:
		HWND m_hWnd = nullptr;
		int m_trigger[enMouseButtonNum];	// トリガー入力のフラグ。
		int m_press[enMouseButtonNum];		// press入力のフラグ。
		int m_posX = 0;
		int m_posY = 0;
	};

	extern Mouse* g_mouse;
}

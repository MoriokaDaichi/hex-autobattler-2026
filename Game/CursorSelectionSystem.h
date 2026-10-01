#pragma once

/// <summary>
/// マウスカーソル位置の座標変換ユーティリティ。
///
/// 以前はマウス・キーボード・ゲームパッドを横断するフォーカス/一覧カーソル/ヘックスカーソルを
/// 持っていたが、ゲームパッド入力の廃止(docs/tasks/remove-gamepad-input)でそれらは削除した。
/// 現在は、マウス位置をUI_SPACE座標へ変換する静的関数だけを提供する
/// (UIInteractionSystem・DragDropController・ツールチップのアンカー計算が使用)。
/// </summary>
class CursorSelectionSystem
{
public:
	/// <summary>
	/// 現在のマウス位置をUI_SPACE座標(1920x1080、中央原点・y上向き。Font/UIRectRendererと共通)へ
	/// 変換する。実際のウィンドウクライアント矩形(GetClientRect)を基準に正規化するため、
	/// DPIスケーリング等でクライアント領域の実ピクセル数がUI_SPACE_WIDTH/HEIGHT(1920x1080)と
	/// 一致しない環境でも正しく動く。ウィンドウのクライアント領域外ならfalseを返す。
	/// </summary>
	static bool ScreenToUISpace(Vector2& outUI);

private:
	/// <summary>
	/// 現在のマウス位置を、実際のウィンドウクライアント矩形(GetClientRect)基準で
	/// 0〜1に正規化して返す(左上原点、右方向・下方向が正)。クライアント領域外や
	/// GetClientRect失敗時はfalseを返す。
	/// </summary>
	static bool GetNormalizedMousePosition(float& outU, float& outV);
};

#pragma once
#include "HexCoord.h"

/// <summary>
/// 現在どの一覧/盤面を選択操作の対象にしているか。
/// </summary>
enum class InputFocus
{
	Shop,	// ショップのユニット一覧。
	Bench,	// ベンチのユニット一覧。
	Items,	// 未装備アイテム一覧。
	Board,	// 盤面(ヘックスグリッド)。
};

/// <summary>
/// マウス・キーボード・ゲームパッドを横断する共通のカーソル・選択システム。
///
/// 「ショップ/ベンチのような一覧から1つ選ぶ」操作と「盤面(ヘックスグリッド)上で1マス選ぶ」
/// 操作を、同じフォーカス切り替えの語彙で扱う。実際の決定操作(購入/売却/配置など)のボタンは
/// 従来通りGame::Update()側のゲームパッド判定(A/B/X/Y/LB1/RB1)がそのまま担い続け、
/// このシステムは「今どのインデックス/マスを対象にしているか」だけを提供する
/// (既存のゲームパッド操作体系を置き換えるのではなく、その対象選択部分を拡張する形)。
///
/// カーソル移動自体は、ゲームパッドのD-pad(既存のenButtonUp/Down/Left/Right。物理パッド未接続時は
/// GamePad内部のキーボードフォールバックでも動く)に加えて、矢印キー(VK_LEFT等、物理パッド接続中でも
/// 常時有効な独立したキーボード入力)、盤面についてはさらにマウスホバー/クリックでも操作できる。
/// </summary>
class CursorSelectionSystem
{
public:
	/// <summary>
	/// 毎フレーム呼び出す。フォーカス切り替え、一覧カーソル、盤面ヘックスカーソルを更新する。
	/// </summary>
	void Update();

	/// <summary>
	/// 現在の入力フォーカス。
	/// </summary>
	InputFocus GetFocus() const { return m_focus; }

	/// <summary>
	/// ショップ/ベンチのような一覧選択用のカーソル位置(0始まり)。
	/// </summary>
	int GetListCursorIndex() const { return m_listCursorIndex; }

	/// <summary>
	/// 一覧の要素数が変わったとき(ショップ再抽選やベンチの増減)に呼び、
	/// カーソルが範囲外を指さないようにする。countが0の場合は0のままにする。
	/// </summary>
	void ClampListCursor(int count);

	/// <summary>
	/// 現在のヘックスカーソル位置を取得する。まだ一度も有効なマスを指していない場合はfalseを返す。
	/// </summary>
	bool GetHexCursor(HexCoord& outHex) const;

	/// <summary>
	/// 現在のマウス位置をUI_SPACE座標(1920x1080、中央原点・y上向き。Font/UIRectRendererと共通)へ
	/// 変換する。実際のウィンドウクライアント矩形(GetClientRect)を基準に正規化するため、
	/// DPIスケーリング等でクライアント領域の実ピクセル数がUI_SPACE_WIDTH/HEIGHT(1920x1080)と
	/// 一致しない環境でも正しく動く(TryMouseToHexと同じ考え方。3D射影を経由しない点のみ異なる)。
	/// ウィンドウのクライアント領域外ならfalseを返す。
	/// </summary>
	static bool ScreenToUISpace(Vector2& outUI);

private:
	void UpdateFocusSwitch();
	void UpdateListCursor();
	void UpdateHexCursor();
	bool TryMouseToHex(HexCoord& outHex) const;

	/// <summary>
	/// 現在のマウス位置を、実際のウィンドウクライアント矩形(GetClientRect)基準で
	/// 0〜1に正規化して返す(左上原点、右方向・下方向が正)。クライアント領域外や
	/// GetClientRect失敗時はfalseを返す。TryMouseToHex/ScreenToUISpaceの共通処理。
	/// </summary>
	static bool GetNormalizedMousePosition(float& outU, float& outV);

	InputFocus m_focus = InputFocus::Shop;
	int m_listCursorIndex = 0;
	HexCoord m_hexCursor;
	bool m_hexCursorValid = false;
};

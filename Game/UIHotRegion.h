#pragma once
#include <vector>
#include "HexCoord.h"

/// <summary>
/// マウスがクリック/ホバー可能なUI要素の種別。ツールチップ内容の分岐にも使う予定(フェーズ2)。
/// </summary>
enum class UIRegionKind
{
	ShopSlot,        // index = ショップ枠(0-4)。
	BenchUnit,       // index = player.bench上のindex。
	BoardUnit,       // hex = 盤面上のユニット位置。
	BoardEmptyHex,   // hex = 盤面上の空マス(自陣のみ登録。配置/移動先として使う)。
	UnclaimedItem,   // index = player.unclaimedItems上のindex。
	RerollButton,
	BuyXpButton,
	LockButton,
	NextPhaseButton,     // Preparation→Combat(既存Bボタン相当)。
	TitleStartButton,    // タイトル: セーブ無し時の「開始」/ セーブ有り時の「続きから」。
	TitleNewGameButton,  // タイトル: セーブ有り時のみ「新規開始」。
	RestartButton,       // GameOver/Victory: リスタート。
};

/// <summary>
/// 1つのクリック/ホバー可能矩形。UI_SPACE座標系(1920x1080、中央原点・y上向き)、
/// pivotに依存しない絶対矩形(min/max)として持つ(ヒットテスト側が各Rendererのpivot流儀を
/// 意識しなくて済むようにするため)。
/// </summary>
struct UIHotRegion
{
	UIRegionKind kind = UIRegionKind::ShopSlot;
	float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
	int index = -1; // kindに応じた添字(ShopSlot/BenchUnit/UnclaimedItem)。無ければ-1。
	HexCoord hex;    // kindがBoardUnit/BoardEmptyHexのときのみ有効。

	bool Contains(const Vector2& uiPos) const
	{
		return uiPos.x >= minX && uiPos.x <= maxX && uiPos.y >= minY && uiPos.y <= maxY;
	}
};

using UIHotRegionList = std::vector<UIHotRegion>;

#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"

struct Player;
class UIRectRenderer;

/// <summary>
/// 準備フェーズに、まだどのユニットにも装備していない入手済みアイテム(Player::unclaimedItems)を
/// 画面右側に縦一覧で表示するHUD。ShopUIRenderer/RoundRecordUIRenderer と同じ方式: IRenderer を
/// 継承し OnRender2D で描画、毎フレーム Game::Render() から Draw() で状態を受け取り
/// g_renderingEngine->AddRenderObject() で当該フレームの描画に登録する。
/// 座標系は UI_SPACE(1920x1080、中央原点・y上向き)。準備フェーズ以外では表示しない
/// (アイテムの装備操作は準備フェーズでのみ行うため)。
///
/// FontEngineの制約に合わせ、pivotによる中央揃え・color.wによるフェードは使わず、
/// 手に持っている枠は "[持] " マーカーと色で表現する(ホバー中はカード枠を水色にする)。
/// </summary>
class ItemInventoryUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 準備フェーズ中、毎フレームGame::Render()から呼ぶ。表示に必要な現在値をコピーして保持し、
	/// 2D描画パスへの登録(AddRenderObject)を行う。
	/// </summary>
	/// <param name="heldIndex">「手に持っている」アイテムのindex(-1で無し)。装備先ユニット選択待ちの状態。</param>
	/// <param name="hoveredIndex">マウスホバー中のアイテムindex(無ければ-1)。カード枠のハイライトに使う。</param>
	/// <param name="rectRenderer">カード背景の塗り矩形を描く共通ヘルパー。OnRender2D用に保持する。</param>
	void Draw(RenderContext& rc, const Player& player, int heldIndex, int hoveredIndex, UIRectRenderer& rectRenderer);

	/// <summary>
	/// 現フレームの未装備アイテム一覧のクリック可能矩形をoutへ追加する。描画を伴わない純粋関数。
	/// Game::Update()の先頭でDraw()と同じplayerを使って呼ぶ(レイアウト定数を共有しているためズレない)。
	/// </summary>
	void BuildHotRegions(const Player& player, UIHotRegionList& out) const;

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	/// <summary>アイテム1つ分の表示テキスト(名前 + 効果の要約)。</summary>
	struct ItemView
	{
		std::wstring name;
		std::wstring effects;
	};

	Font m_font;

	std::vector<ItemView> m_items;
	int m_heldIndex = -1;
	int m_hoveredIndex = -1;
	UIRectRenderer* m_rectRenderer = nullptr; // Draw()で渡されたものをOnRender2D用に保持する。
	bool m_hasData = false;
};

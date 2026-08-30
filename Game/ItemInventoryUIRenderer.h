#pragma once
#include <string>
#include <vector>

struct Player;

/// <summary>
/// 準備フェーズに、まだどのユニットにも装備していない入手済みアイテム(Player::unclaimedItems)を
/// 画面右側に縦一覧で表示するHUD。ShopUIRenderer/RoundRecordUIRenderer と同じ方式: IRenderer を
/// 継承し OnRender2D で描画、毎フレーム Game::Render() から Draw() で状態を受け取り
/// g_renderingEngine->AddRenderObject() で当該フレームの描画に登録する。
/// 座標系は UI_SPACE(1920x1080、中央原点・y上向き)。準備フェーズ以外では表示しない
/// (アイテムの装備操作は準備フェーズでのみ行うため)。
///
/// FontEngineの制約に合わせ、pivotによる中央揃え・color.wによるフェードは使わず、
/// 選択枠は "> " マーカーと色/スケール差で、手に持っている枠は "[持] " マーカーで表現する。
/// </summary>
class ItemInventoryUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 準備フェーズ中、毎フレームGame::Render()から呼ぶ。表示に必要な現在値をコピーして保持し、
	/// 2D描画パスへの登録(AddRenderObject)を行う。
	/// </summary>
	/// <param name="focused">今カーソルのフォーカスがアイテム一覧に当たっているか。</param>
	/// <param name="cursorIndex">アイテム一覧上のカーソル位置(CursorSelectionSystem由来)。</param>
	/// <param name="heldIndex">「手に持っている」アイテムのindex(-1で無し)。装備先ユニット選択待ちの状態。</param>
	void Draw(RenderContext& rc, const Player& player, bool focused, int cursorIndex, int heldIndex);

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
	bool m_focused = false;
	int m_cursorIndex = -1;
	int m_heldIndex = -1;
	bool m_hasData = false;
};

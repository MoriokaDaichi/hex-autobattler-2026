#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"

struct Player;
struct UnitDef;
struct ItemDef;
class ItemSystem;
class ItemDatabase;
class ShopUIRenderer;
class UIRectRenderer;

/// <summary>
/// 準備フェーズのマウスによるドラッグ&ドロップ操作(drag-and-drop)。
///  - ベンチ/盤面のユニット → 盤面のマス(配置・移動・入れ替え) / ベンチ領域(ベンチへ戻す) / ショップバー(売却)
///  - 未装備アイテム → ベンチ/盤面のユニット(装備)
///
/// 既存のクリック操作(押下で発火、Game.cppの「--- マウス左クリック ---」ブロック)はそのまま残し、
/// 「何も持っていない状態で押下 → 閾値以上動かす → 離す」の流れだけをここで扱う。押下フレームでは
/// 既存クリックが対象を「掴む」が、ドラッグとして確定(ドロップ/キャンセル)したフレームで Update() が
/// true を返すので、呼び出し側(Game)が既存の掴み状態を解除する(docs/tasks/drag-and-drop/plan.md §1)。
///
/// 描画はShopUIRenderer等と同じ方式: IRendererを継承しOnRender2Dで描画、毎フレームGame::Render()から
/// Draw()で状態を受け取りg_renderingEngine->AddRenderObject()で当該フレームの描画に登録する(ドラッグ中のみ)。
/// 座標系はUI_SPACE(1920x1080、中央原点・y上向き)。
/// </summary>
class DragDropController : public IRenderer, public Noncopyable
{
public:
	/// <summary>
	/// 準備フェーズ中、毎フレーム、既存の左クリック処理より前に呼ぶ。
	/// </summary>
	/// <param name="hotRegions">今フレームのヒット領域一覧(Game::m_hotRegions)。</param>
	/// <param name="holdingSomething">押下時点で既存のクリック操作により何か掴んでいるか。trueの間の押下は
	/// 既存クリック操作の続きとみなし、ドラッグを開始しない。</param>
	/// <returns>このフレームでドラッグが終了(離してドロップ確定 or 右クリックで取り消し)したらtrue。
	/// 呼び出し側は既存の掴み状態(m_mouseHeldBenchIndex等)を解除し、同フレームの既存右クリック処理
	/// (売却/ベンチ戻しの確認)を走らせないこと。</returns>
	bool Update(
		const UIHotRegionList& hotRegions,
		bool holdingSomething,
		Player& player,
		ItemSystem& itemSystem,
		const ItemDatabase& itemDatabase,
		ShopUIRenderer& feedback);

	/// <summary>ドラッグ(押下中の候補を含む)を破棄する。準備フェーズを抜けるときに呼ぶ。</summary>
	void Cancel();

	/// <summary>閾値を超えてドラッグ中か(ツールチップ抑止・ハイライト表示用)。</summary>
	bool IsDragging() const { return m_state == State::Dragging; }

	/// <summary>
	/// 準備フェーズ中、毎フレームGame::Render()から呼ぶ。ドラッグ中のみ、ドロップ候補のハイライトと
	/// カーソルに追従する掴んだもの(名前)の表示を2D描画パスへ登録する。他のUIより後(ツールチップより前)に呼ぶこと。
	/// </summary>
	void Draw(RenderContext& rc, const UIHotRegionList& hotRegions, const Player& player, UIRectRenderer& rectRenderer);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	enum class State
	{
		Idle,     // 何もしていない。
		Pressed,  // ドラッグ元の上で左ボタンを押した(まだ閾値以内。離せば従来のクリック扱い)。
		Dragging, // 閾値を超えて動かした。離した場所でドロップを確定する。
	};

	/// <summary>ドロップ先の種別。</summary>
	enum class DropKind
	{
		None,       // 無効(離しても何も起こらず元に戻る)。
		BoardHex,   // 盤面のマス(空きなら配置/移動、ユニットが居れば入れ替え)。
		Bench,      // ベンチ領域(盤面ユニットをベンチへ戻す)。
		Sell,       // ショップバー(売却)。
		EquipBench, // ベンチのユニットへアイテム装備。
		EquipBoard, // 盤面のユニットへアイテム装備。
	};

	struct DropTarget
	{
		DropKind kind = DropKind::None;
		HexCoord hex;       // BoardHex/EquipBoard。
		int benchIndex = -1; // EquipBench。
		UIHotRegion region; // ハイライト用の矩形(BoardHex/EquipBench/EquipBoard)。
	};

	/// <summary>描画用の矩形1枚(Draw()で確定し、OnRender2Dで描く)。</summary>
	struct RectView
	{
		Vector2 center;
		Vector2 size;
		Vector4 fill;
		Vector4 border;
		float borderThickness = 2.0f;
	};

	/// <summary>描画用のテキスト1行。</summary>
	struct TextView
	{
		Vector2 topLeft;
		std::wstring text;
		Vector4 color;
		float scale = 0.5f;
	};

	/// <summary>uiPosにある領域を、UIInteractionSystemと同じく後ろから探す。無ければnullptr。</summary>
	static const UIHotRegion* FindRegionAt(const UIHotRegionList& hotRegions, const Vector2& uiPos);

	/// <summary>ドラッグ元(m_source)と位置から、ドロップ先を解決する。</summary>
	DropTarget ResolveDrop(const UIHotRegionList& hotRegions, const Vector2& uiPos) const;

	/// <summary>ドラッグ元がまだ押下時と同じ実体か(ドラッグ中に他の処理で一覧が変わっていないか)。</summary>
	bool IsSourceStillValid(const Player& player) const;

	/// <summary>ドロップを確定し、Player/ItemSystemの既存エントリポイントを呼ぶ。</summary>
	void ExecuteDrop(const DropTarget& target, Player& player, ItemSystem& itemSystem,
		const ItemDatabase& itemDatabase, ShopUIRenderer& feedback);

	/// <summary>ドラッグ元の表示名("Knight *2" / アイテム名)。</summary>
	std::wstring SourceLabel(const Player& player) const;

	/// <summary>ベンチ領域・売却領域(ショップバー)の矩形。</summary>
	static UIHotRegion BenchArea();
	static UIHotRegion SellArea();

	State m_state = State::Idle;
	UIHotRegion m_source;       // ドラッグ元(BenchUnit/BoardUnit/UnclaimedItem)。
	const UnitDef* m_sourceDef = nullptr;  // ドラッグ元ユニットの検証用。
	int m_sourceStar = 0;
	const ItemDef* m_sourceItem = nullptr; // ドラッグ元アイテムの検証用。
	Vector2 m_pressPos;         // 押下位置(UI_SPACE)。閾値判定用。
	Vector2 m_cursorPos;        // 最新のカーソル位置(UI_SPACE)。

	// 押下位置からこの距離(UI_SPACE px)以上動かしたらドラッグ開始。未満で離せば従来のクリック扱い。
	static constexpr float kDragThresholdPx = 12.0f;

	// --- 描画 ---
	Font m_font;
	std::vector<RectView> m_rects;
	std::vector<TextView> m_texts;
	UIRectRenderer* m_rectRenderer = nullptr;
	bool m_hasData = false;
};

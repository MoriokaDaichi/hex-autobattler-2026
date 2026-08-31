#pragma once
#include <string>
#include <vector>
#include "UIHotRegion.h"

struct Player;
class CombatPlayback;
class UIRectRenderer;

/// <summary>
/// 盤面まわりの2D UI を描画するクラス。
///  - 戦闘フェーズ: 各ユニットの頭上にHPバー(+シールドゲージ)。値はCombatPlaybackの表示用HP。
///  - 準備フェーズ: プレイヤーのベンチ一覧(名前+星レベル)。
///
/// ShopUIRenderer と同じ方式: IRenderer を継承し OnRender2D で描画、毎フレーム Game::Render() から
/// Draw系メソッドで状態を受け取り g_renderingEngine->AddRenderObject() で当該フレームの描画に登録する。
/// 座標系は UI_SPACE(1920x1080、中央原点・y上向き)。矩形描画手段が無いため、HPバーはブロック文字で表現する。
/// </summary>
class BoardUIRenderer : public IRenderer, public Noncopyable
{
public:
	/// <summary>準備フェーズ中に毎フレーム呼ぶ。ベンチ一覧を表示対象にする。</summary>
	void DrawPreparation(RenderContext& rc, const Player& player);

	/// <summary>戦闘の再生中に毎フレーム呼ぶ。playbackの各ユニットのHPバーを表示対象にする。</summary>
	/// <param name="rectRenderer">HPバー/スキルゲージバーの塗り矩形を描く共通ヘルパー。OnRender2D内で
	/// 使うため、ここで受け取ったポインタをキャッシュしておく(Fontと違い毎フレームGameから渡される)。</param>
	void DrawCombat(RenderContext& rc, const CombatPlayback& playback, UIRectRenderer& rectRenderer);

	/// <summary>
	/// 現フレームのベンチ一覧のクリック可能矩形をoutへ追加する。描画を伴わない純粋関数。
	/// Game::Update()の先頭でDrawPreparation()と同じplayerを使って呼ぶ(レイアウト定数を
	/// 共有しているためズレない)。
	/// </summary>
	void BuildHotRegions(const Player& player, UIHotRegionList& out) const;

	/// <summary>
	/// ワールド座標を、カメラのVP行列でUI空間座標へ射影する。カメラ背後や大きく画面外ならfalse。
	/// 盤面ヘックスのヒット領域計算(Game::Update())からも使うためpublicに公開している。
	/// </summary>
	static bool WorldToUI(const Vector3& world, Vector2& outUI);

	// IRendererオーバーライド。RenderingEngineの2D描画パスから呼ばれる。
	void OnRender2D(RenderContext& rc) override;

private:
	enum class Mode { None, Preparation, Combat };

	/// <summary>HPバー1本分の描画データ(Draw時に確定させ、OnRender2Dで描く)。</summary>
	struct BarView
	{
		Vector2 uiPos;            // バー中心のUI空間座標。
		bool onScreen = false;    // カメラ視錐台内に射影できたか。
		float hpRatio = 0.0f;     // 0〜1。
		float shieldRatio = 0.0f; // 0〜1(maxHP基準)。
		float gaugeRatio = 0.0f;  // 0〜1(1で必殺技発動可)。
		int hp = 0;
		int maxHP = 1;
		int shield = 0;
		std::wstring label;       // "Knight ★2" 等。
		bool isEnemy = false;
		bool alive = true;
	};

	/// <summary>ベンチ1体分の表示テキスト。</summary>
	struct BenchView
	{
		std::wstring text;
	};

	Font m_font;
	Mode m_mode = Mode::None;
	std::vector<BarView> m_bars;
	std::vector<BenchView> m_bench;
	UIRectRenderer* m_rectRenderer = nullptr; // DrawCombat()で渡されたものをOnRender2D用に保持する。
};

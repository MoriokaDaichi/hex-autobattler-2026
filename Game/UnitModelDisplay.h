#pragma once
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Player.h"

/// <summary>
/// ヘックス盤面に配置されたユニット(UnitInstanceの並び)の3Dモデルを表示するクラス。
/// ModelRenderは既にIRendererを継承しており、Game::Update()/Game::Render()から
/// このクラスのUpdate()/Draw()を直接呼んでもらう想定のため、新たにIRendererは実装しない。
///
/// board-layout-rework: 元はプレイヤー盤面(Player::board)専用だったが、敵盤面(r3-5)の
/// プレビュー表示にも使えるよう「UnitInstanceのvector」を受け取る形へ一般化した。
/// Gameがプレイヤー用・敵用の2インスタンスを持つ。
/// </summary>
class UnitModelDisplay
{
public:
	/// <summary>
	/// 毎フレーム呼ぶ。盤面構成(board)が前フレームから変化していれば表示用モデルを再構築し、
	/// 変化の有無に関わらず各モデルの位置とアニメーション状態を更新する。
	/// </summary>
	void Update(const std::vector<UnitInstance>& board);

	/// <summary>
	/// 毎フレーム呼ぶ。現在保持している全モデルを描画キューに登録する。
	/// </summary>
	void Draw(RenderContext& rc);

	/// <summary>
	/// 保持中の表示モデルを全て破棄する。Title/GameOver/Victory へ遷移する際に呼び、
	/// 前プレイのユニットモデルが背景に残る(ゴースト)のを防ぐ(board-layout-rework §D)。
	/// </summary>
	void Clear();

private:
	/// <summary>
	/// 盤面1体分の表示エンティティ。ModelRenderはコピー不可な内部状態を持つため、
	/// vector内での再配置に困らないようunique_ptrで保持する。
	/// </summary>
	struct DisplayEntry
	{
		std::unique_ptr<ModelRender> modelRender;
	};

	/// <summary>
	/// boardの構成(サイズ+各要素の「UnitDef*とstarLevelの組」の並び)が前回のUpdate()時点から
	/// 変化しているか確認し、変化していれば表示エンティティ一式を作り直す。ModelRender::Init()はtkmを
	/// ディスクから再ロードするため、変化が無いフレームでは何もしない(呼ばない)。
	/// starLevelもシグネチャに含めるのは、合成で同じユニットの星が上がった場合(UnitDef*は不変)にも
	/// 表示スケールを更新する必要があるため。
	/// </summary>
	void RebuildIfBoardChanged(const std::vector<UnitInstance>& board);

	/// <summary>
	/// ユニット種別(UnitDef::name)ごとのAnimationClip[5]を取得する。未ロードならここで初めてロードし、
	/// 以後は同じ種別の複数体で共有する(ModelRender::Initに渡すポインタは保持されるだけでコピーされない)。
	/// </summary>
	std::array<AnimationClip, 5>& GetOrLoadAnimClips(const UnitDef* def);

	std::map<std::string, std::array<AnimationClip, 5>> m_animClipCache;
	std::vector<DisplayEntry> m_displayEntries;      // boardと同じ並び順で対応する表示用モデル一式。
	// 前回のUpdate()時点でのboard構成(変化検出用)。各要素は{UnitDef*, starLevel}の組。
	std::vector<std::pair<const UnitDef*, int>> m_lastBoardSignature;
};

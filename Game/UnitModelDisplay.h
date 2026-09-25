#pragma once
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Player.h"
#include "CombatPlayback.h"

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
	/// (準備/結果フェーズ用。位置は board[i].position、向きは無し、アニメは再生しない=バインドポーズ。)
	/// </summary>
	void Update(const std::vector<UnitInstance>& board);

	/// <summary>
	/// 戦闘再生中に毎フレーム呼ぶ。CombatPlayback のユニットビュー配列(プレイヤー slice または
	/// 敵 slice のいずれか)から、モデルの TRS(補間後ワールド座標 + Yリフト / 進行方向 or 対象方向の
	/// yaw / スケール)と再生アニメ(idle/move/attack/skill/death)を更新する。
	/// ビュー構成((UnitDef*,star) の並び)が変わったらモデルを再構築する。
	/// playbackSpeedはCombatPlayback::GetPlaybackSpeed()の値で、アニメ再生速度を位置補間の速さに揃える。
	/// (combat-movement-playback)
	/// </summary>
	void UpdateFromPlayback(const CombatPlayback::UnitView* views, size_t count, float playbackSpeed);

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

		// combat-movement-playback: 再生駆動(UpdateFromPlayback)用の状態。board 駆動では未使用。
		Quaternion lastRot = Quaternion::Identity; // 向きの Slerp 用。
		int seenAttackSeq = 0;   // 直近で反映した UnitView::attackAnimSeq。
		int curClip = -1;        // 現在再生中(または最後に指示した)クリップ index。-1 = 未指定。
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
	/// RebuildIfBoardChanged の再生ビュー版。(UnitDef*, starLevel) の並びで判定する。
	/// </summary>
	void RebuildIfViewsChanged(const CombatPlayback::UnitView* views, size_t count);

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

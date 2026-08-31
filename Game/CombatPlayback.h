#pragma once
#include <string>
#include <vector>
#include "CombatEvent.h"

struct UnitInstance;

/// <summary>
/// CombatEngine::SimulateCombat(1フレームで瞬時解決)の結果を、複数フレームに渡って
/// 時系列で「再生」するクラス。各ユニットの表示用HP/シールドを戦闘開始時の値から保持し、
/// 再生クロックがCombatEvent::timeに到達するたびに更新する。
///
/// 承認済み設計:
///  - SimulateCombat自体は変更しない(瞬時解決のまま)。ここは結果イベント列を読むだけ。
///  - 同じtime値を持つイベントは同フレームでまとめて反映する(CombatEvent.h/CombatEngine.hのメモ)。
///  - 位置はアニメーションしない。HPバーは戦闘終了時の最終位置に固定し、HP/シールドのみ時間再生する
///    (3Dモデルの移動アニメーションは別タスク・スコープ外)。
/// </summary>
class CombatPlayback
{
public:
	/// <summary>
	/// 盤面UI(HPバー描画)から参照する、1ユニット分の表示用スナップショット。
	/// </summary>
	struct UnitView
	{
		std::wstring name;
		int starLevel = 1;
		Vector3 worldPos;        // HPバーを浮かべる盤面上のワールド座標(戦闘終了時の位置、y=0)。
		int displayHP = 0;       // 再生クロックに応じて増減する表示用HP。
		int maxHP = 1;           // バーの割合計算用。
		int displayShield = 0;   // 表示用シールド量。
		int displayGauge = 0;    // 再生クロックに応じて増減する表示用の必殺技ゲージ。
		int skillThreshold = 1;  // ゲージが満ちる閾値(バーの割合計算用、戦闘中は不変)。
		bool alive = true;
		bool isEnemy = false;
	};

	/// <summary>
	/// SimulateCombat直後に1回だけ呼ぶ。両陣営のboard(シミュレーション後の状態)と
	/// 陣営名、イベント列を受け取り、表示用HPを戦闘開始時の値(=実効最大HP)へ初期化する。
	/// eventsはtime昇順である前提(CombatEngineが時系列で追記するため)。
	/// </summary>
	void Begin(
		const std::vector<UnitInstance>& playerBoard, const std::string& playerOwner,
		const std::vector<UnitInstance>& enemyBoard, const std::string& enemyOwner,
		const std::vector<CombatEvent>& events);

	/// <summary>
	/// 毎フレーム呼ぶ。再生クロックを (deltaTime * 再生速度) だけ進め、到達したイベントを
	/// (同じtime値のものは同フレームでまとめて)表示用HP/シールドへ反映する。
	/// 全イベント消化後は余韻(約1秒)を実時間で数え、経過したらIsFinished()がtrueになる。
	/// </summary>
	void Update(float deltaTime);

	/// <summary>Begin済みで、まだ再生(＋余韻)が終わっていない。</summary>
	bool IsActive() const { return m_active; }

	/// <summary>Begin済みで、全イベント再生＋余韻が完了した。</summary>
	bool IsFinished() const { return m_begun && !m_active; }

	const std::vector<UnitView>& GetUnitViews() const { return m_views; }

private:
	void ApplyEvent(const CombatEvent& ev);
	UnitView* ResolveActor(const CombatEvent& ev);
	UnitView* ResolveTarget(const CombatEvent& ev);

	static const float kTargetPlaybackSeconds; // 総再生尺の目安(これに収まるよう速度を決める)。
	static const float kMinSpeed;
	static const float kMaxSpeed;
	static const float kTailSeconds;           // 最終イベント後の余韻。

	std::vector<UnitView> m_views;   // [0..m_playerCount) がプレイヤーboard、以降が敵board。
	size_t m_playerCount = 0;
	std::string m_playerOwner;
	std::string m_enemyOwner;

	std::vector<CombatEvent> m_events;
	size_t m_nextIndex = 0;
	float m_clock = 0.0f;
	float m_speed = 1.0f;
	float m_tailTimer = 0.0f;
	bool m_active = false;
	bool m_begun = false;
};

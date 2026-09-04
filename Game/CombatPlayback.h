#pragma once
#include <string>
#include <vector>
#include "CombatEvent.h"

struct UnitInstance;
struct UnitDef;

/// <summary>
/// CombatEngine::SimulateCombat(1フレームで瞬時解決)の結果を、複数フレームに渡って
/// 時系列で「再生」するクラス。各ユニットの表示用HP/シールドを戦闘開始時の値から保持し、
/// 再生クロックがCombatEvent::timeに到達するたびに更新する。
///
/// 承認済み設計:
///  - SimulateCombat自体は変更しない(瞬時解決のまま)。ここは結果イベント列を読むだけ。
///  - 同じtime値を持つイベントは同フレームでまとめて反映する(CombatEvent.h/CombatEngine.hのメモ)。
///  - combat-movement-playback: Move イベントに移動先が入るようになり、UnitView の worldPos を
///    m_clock 基準で補間する。向き・アニメ状態のヒントも UnitView に持たせ、UnitModelDisplay が
///    再生駆動でモデルを動かす。
/// </summary>
class CombatPlayback
{
public:
	/// <summary>
	/// 盤面UI(HPバー描画)/ 戦闘再生中のモデル表示(UnitModelDisplay)から参照する、
	/// 1ユニット分の表示用スナップショット。
	/// </summary>
	struct UnitView
	{
		std::wstring name;
		int starLevel = 1;
		const UnitDef* def = nullptr; // モデル/アニメのロード・対応付け用(combat-movement-playback)。
		Vector3 worldPos;        // 現在の表示ワールド座標(y=0)。毎 Update() で補間結果に更新。
		int displayHP = 0;       // 再生クロックに応じて増減する表示用HP。
		int maxHP = 1;           // バーの割合計算用。
		int displayShield = 0;   // 表示用シールド量。
		int displayGauge = 0;    // 再生クロックに応じて増減する表示用の必殺技ゲージ。
		int skillThreshold = 1;  // ゲージが満ちる閾値(バーの割合計算用、戦闘中は不変)。
		bool alive = true;
		bool isEnemy = false;

		// --- 位置補間(すべて m_clock = 戦闘内時刻 基準) ---
		Vector3 homeWorldPos;          // homePosition の CalcTileCenter(初期値・フォールバック)。
		Vector3 moveFromPos;           // 補間区間の始点(y=0)。
		Vector3 moveToPos;             // 補間区間の終点(y=0)。
		float moveStartClock = -1.0f;  // 補間開始時の m_clock。< 0 で「補間なし(静止)」。
		float moveEndClock = -1.0f;    // 補間終了時の m_clock。

		// --- 向き / アニメ状態ヒント(UnitModelDisplay が読む) ---
		Vector3 facingDir;             // XZ 単位ベクトル。初期: 敵 = (0,0,-1)、味方 = (0,0,+1)。
		bool isMoving = false;         // このフレーム補間中か。
		int attackAnimSeq = 0;         // 攻撃(通常/必殺)を出すたび +1。表示側が前回値と比較して発火検出。
		bool attackAnimIsSkill = false;// 直近 attackAnimSeq 更新が必殺技か。
		bool deathAnimTriggered = false; // Death イベントで true(以後不変)。
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

	/// <summary>
	/// m_views の先頭 [0, GetPlayerViewCount()) がプレイヤー board、以降が敵 board。
	/// 戦闘再生中のモデル表示で、プレイヤー分 / 敵分を別々の UnitModelDisplay に渡すために使う。
	/// </summary>
	size_t GetPlayerViewCount() const { return m_playerCount; }

	/// <summary>再生速度(Begin で決定、combat秒/実秒)。アニメ速度合わせに使う。</summary>
	float GetPlaybackSpeed() const { return m_speed; }

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
	// m_events と同添字。Move イベントの位置補間所要(combat秒)。Begin() で 1 パス前計算。
	// Move 以外の添字の値は未使用。
	std::vector<float> m_moveDur;
	size_t m_nextIndex = 0;
	float m_clock = 0.0f;
	float m_speed = 1.0f;
	float m_tailTimer = 0.0f;
	bool m_active = false;
	bool m_begun = false;
};

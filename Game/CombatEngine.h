#pragma once
#include <climits>
#include <vector>
#include <algorithm>
#include "Player.h"
#include "AttackType.h"
#include "CombatEvent.h"

/// <summary>
/// 戦闘シミュレーションを行うクラス。
/// 戦闘中に起きたことはOutputDebugStringへ直接出力せず、CombatEventとしてoutEventsに記録する。
/// 表示はCombatLogPrinter等、別のクラスの役目にする(シミュレーションと表示を分離するため)。
/// </summary>
class CombatEngine
{
public:
	/// <summary>
	/// attackerから見て、defenderの盤面にいる中で一番近い敵ユニットのインデックスを返す。
	/// </summary>
	int FindNearestEnemy(const UnitInstance& attacker, const std::vector<UnitInstance>& enemyBoard)
	{
		int nearestIndex = -1;
		int nearestDist = INT_MAX;

		for (size_t i = 0; i < enemyBoard.size(); ++i)
		{
			if (enemyBoard[i].currentHP <= 0)
			{
				continue;
			}

			int dist = attacker.position.Distance(enemyBoard[i].position);
			if (dist < nearestDist)
			{
				nearestDist = dist;
				nearestIndex = (int)i;
			}
		}

		return nearestIndex;
	}

	/// <summary>
	/// 指定した盤面に、生存しているユニットが1体もいなければtrueを返す。
	/// </summary>
	bool IsBoardWiped(const std::vector<UnitInstance>& board)
	{
		for (const auto& unit : board)
		{
			if (unit.currentHP > 0)
			{
				return false;
			}
		}
		return true;
	}

	/// <summary>
	/// 指定した盤面で、生存しているユニットの数を返す。
	/// </summary>
	int CountAliveUnits(const std::vector<UnitInstance>& board)
	{
		int count = 0;
		for (const auto& unit : board)
		{
			if (unit.currentHP > 0)
			{
				++count;
			}
		}
		return count;
	}

	/// <summary>
	/// 片方のプレイヤーの盤面全体、もう片方のプレイヤーの盤面全体を使って、
	/// どちらかが全滅するまで戦闘をシミュレーションする。発生した出来事はoutEventsに追記する。
	///
	/// 「誰が何番目に動くか」は固定の順番(配列順)ではなく、内部時計(m_currentTime)で管理する。
	/// 各ユニットはnextActionTimeという「次に行動する予定時刻」を持ち、両陣営を通じて
	/// 最も早いnextActionTimeを「ウェーブ」としてまとめ、そのウェーブに該当するユニット全員を
	/// (プレイヤー1体目→敵1体目→プレイヤー2体目→敵2体目…という順で交互に)行動させる。
	/// 戦闘開始時は全ユニットのnextActionTimeが0なので、両陣営の全ユニットが同じ時刻(T=0)の
	/// 1つのウェーブとしてまとめて動き出す。片方の陣営が丸ごと先に動き終えてからもう片方が動く、
	/// ということがないようにするための処理(全ユニットの実効攻撃速度が同じ間は、これが毎ラウンド起こる)。
	///
	/// [TODO/将来メモ] 同じウェーブ(=同じCombatEvent::time)内でのプレイヤー→敵の交互処理は、あくまで
	/// テキストログを1行ずつ書き出すための便宜的な順序であり、「同時に起きたこと」を表現しているわけではない。
	/// 将来、デバッグログの代わりに実際のゲーム画面(アニメーション/エフェクト表示)を実装する際は、
	/// 同じtime値を持つCombatEventはこの処理順ではなく、本当に同時(同フレーム)に再生されるようにすること。
	/// </summary>
	void SimulateCombat(Player& player, Player& enemy, std::vector<CombatEvent>& outEvents)
	{
		const int maxIterations = 4000; // 1回の行動を1イテレーションとして数える安全装置。
		const float kTimeEpsilon = 0.0001f; // 浮動小数点の誤差を吸収するための微小値。

		// 前回の戦闘の値が残らないよう、内部時計と各ユニットの行動予定時刻・火傷をリセットする。
		m_currentTime = 0.0f;
		for (auto& unit : player.board) { unit.nextActionTime = 0.0f; unit.activeBurns.clear(); }
		for (auto& unit : enemy.board) { unit.nextActionTime = 0.0f; unit.activeBurns.clear(); }

		int iteration = 0;
		while (!IsBoardWiped(player.board) && !IsBoardWiped(enemy.board) && iteration < maxIterations)
		{
			// 両陣営の生存ユニットの中から、最も早いnextActionTime(=次のウェーブの時刻)を探す。
			float bestTime = 1e9f;
			for (const auto& unit : player.board)
			{
				if (unit.currentHP <= 0) continue;
				if (unit.nextActionTime < bestTime) bestTime = unit.nextActionTime;
			}
			for (const auto& unit : enemy.board)
			{
				if (unit.currentHP <= 0) continue;
				if (unit.nextActionTime < bestTime) bestTime = unit.nextActionTime;
			}

			if (bestTime >= 1e9f) break; // 通常はIsBoardWipedで先に抜けるはずだが、念のため。

			m_currentTime = bestTime;

			// このウェーブの時刻までに刻みが来ている火傷(継続ダメージ)を先に処理する。
			ProcessDueBurns(player.board, player.name, enemy.board, enemy.name, outEvents);
			if (IsBoardWiped(player.board) || IsBoardWiped(enemy.board)) break; // 火傷で決着したらここで抜ける。

			// このウェーブ(=bestTime)に行動予定のユニットを、両陣営から1体ずつ交互に行動させる。
			size_t maxBoardSize = (player.board.size() > enemy.board.size()) ? player.board.size() : enemy.board.size();
			for (size_t i = 0; i < maxBoardSize && iteration < maxIterations; ++i)
			{
				if (i < player.board.size())
				{
					UnitInstance& unit = player.board[i];
					if (unit.currentHP > 0 && unit.nextActionTime <= bestTime + kTimeEpsilon)
					{
						int targetIndex = FindNearestEnemy(unit, enemy.board);
						if (targetIndex >= 0)
						{
							PerformAction(unit, player.name, (int)i, enemy.board[targetIndex], enemy.name, targetIndex, player.board, enemy.board, outEvents);
						}
						unit.nextActionTime = m_currentTime + GetEffectiveAttackInterval(unit);
						++iteration;
					}
				}

				if (i < enemy.board.size())
				{
					UnitInstance& unit = enemy.board[i];
					if (unit.currentHP > 0 && unit.nextActionTime <= bestTime + kTimeEpsilon)
					{
						int targetIndex = FindNearestEnemy(unit, player.board);
						if (targetIndex >= 0)
						{
							PerformAction(unit, enemy.name, (int)i, player.board[targetIndex], player.name, targetIndex, enemy.board, player.board, outEvents);
						}
						unit.nextActionTime = m_currentTime + GetEffectiveAttackInterval(unit);
						++iteration;
					}
				}
			}
		}

		if (iteration >= maxIterations)
		{
			CombatEvent e;
			e.type = CombatEventType::Warning;
			e.time = m_currentTime;
			e.message = "Combat reached max iterations without a winner.";
			outEvents.push_back(e);
		}
	}

private:
	float m_currentTime = 0.0f; // シミュレーション中の内部時計(秒)。SimulateCombat開始時に0へリセットされる。

	/// <summary>
	/// attackerが1回行動する。ゲージが溜まっていれば必殺技、そうでなければ通常攻撃。
	/// ただしターゲットがその技の射程外にいる場合は、射程内に入るまで移動する(このターンは攻撃しない)。
	/// attackerOwner/targetOwnerは陣営名(Player::name)で、イベントの表示に使う。
	/// </summary>
	void PerformAction(UnitInstance& attacker, const std::string& attackerOwner, int actorIndex, UnitInstance& target, const std::string& targetOwner, int targetIndex,
		const std::vector<UnitInstance>& allyBoard, std::vector<UnitInstance>& enemyBoard, std::vector<CombatEvent>& outEvents)
	{
		int gauge = attacker.normalAttackCount + attacker.receivedAttackCount;
		bool willUseSkill = gauge >= GetEffectiveSkillThreshold(attacker);
		int requiredRange = willUseSkill ? attacker.def->skillRange : attacker.def->attackRange;

		int distance = attacker.position.Distance(target.position);
		if (distance > requiredRange)
		{
			MoveTowards(attacker, target, requiredRange, attackerOwner, actorIndex, targetOwner, targetIndex, distance, allyBoard, enemyBoard, outEvents);
			return; // このターンは移動のみ。攻撃・ゲージ加算は行わない。
		}

		if (willUseSkill)
		{
			UseSkill(attacker, attackerOwner, actorIndex, target, targetOwner, targetIndex, enemyBoard, outEvents);
			attacker.normalAttackCount = 0;
			attacker.receivedAttackCount = 0;
		}
		else
		{
			NormalAttack(attacker, attackerOwner, actorIndex, target, targetOwner, targetIndex, outEvents);
			attacker.normalAttackCount++;
		}

		// 攻撃を受けた側は、被弾回数を+1する(通常攻撃・必殺技どちらでも)。
		target.receivedAttackCount++;
	}

	/// <summary>
	/// attackerを、targetに向かって射程内に入るか、この行動で移動できる歩数を使い切るまで移動させる。
	/// 簡易実装: 毎歩、隣接6マスの中で(他の生存ユニットに占有されていないマスのみ対象に)
	/// targetに最も近づけるマスを選ぶ。真の経路探索(障害物の回り込みなど)は行わない。
	/// ただし、近づけるマスが全て塞がっている場合は、距離が変わらない空きマスへの横移動(迂回)を
	/// 許可する。これにより、密集地帯で完全に立ち往生し続けることを避けられる
	/// (それでも空きマスが1つもなければ、その場に留まる)。
	/// </summary>
	void MoveTowards(UnitInstance& attacker, const UnitInstance& target, int requiredRange,
		const std::string& attackerOwner, int actorIndex, const std::string& targetOwner, int targetIndex, int startDistance,
		const std::vector<UnitInstance>& allyBoard, const std::vector<UnitInstance>& enemyBoard,
		std::vector<CombatEvent>& outEvents)
	{
		int maxSteps = GetEffectiveMoveSteps(attacker);
		for (int step = 0; step < maxSteps; ++step)
		{
			int currentDist = attacker.position.Distance(target.position);
			if (currentDist <= requiredRange)
			{
				break;
			}

			HexCoord bestNext = attacker.position;
			int bestDist = currentDist;
			for (const auto& neighbor : attacker.position.GetNeighbors())
			{
				if (IsPositionOccupied(neighbor, attacker, allyBoard, enemyBoard))
				{
					continue;
				}

				int dist = neighbor.Distance(target.position);
				if (dist < bestDist)
				{
					bestDist = dist;
					bestNext = neighbor;
				}
			}

			if (bestNext == attacker.position)
			{
				// 近づけるマスが空いていなかった場合、距離が変わらない空きマスへの横移動で妥協する
				// (渋滞で完全に足止めされるのを避けるための迂回)。
				for (const auto& neighbor : attacker.position.GetNeighbors())
				{
					if (IsPositionOccupied(neighbor, attacker, allyBoard, enemyBoard))
					{
						continue;
					}

					if (neighbor.Distance(target.position) == currentDist)
					{
						bestNext = neighbor;
						break;
					}
				}
			}

			if (bestNext == attacker.position)
			{
				break; // 周囲が完全に塞がっていて、近づくことも迂回することもできない。
			}

			attacker.position = bestNext;
		}

		CombatEvent e;
		e.type = CombatEventType::Move;
		e.time = m_currentTime;
		e.actorOwner = attackerOwner;
		e.actorName = attacker.def->name;
		e.actorIndex = actorIndex;
		e.targetOwner = targetOwner;
		e.targetName = target.def->name;
		e.targetIndex = targetIndex;
		e.beforeValue = startDistance;
		e.afterValue = attacker.position.Distance(target.position);
		outEvents.push_back(e);
	}

	/// <summary>
	/// posに、mover自身以外の生存しているユニットがいればtrueを返す。
	/// </summary>
	bool IsPositionOccupied(const HexCoord& pos, const UnitInstance& mover,
		const std::vector<UnitInstance>& allyBoard, const std::vector<UnitInstance>& enemyBoard)
	{
		for (const auto& unit : allyBoard)
		{
			if (&unit == &mover) continue;
			if (unit.currentHP <= 0) continue;
			if (unit.position == pos) return true;
		}

		for (const auto& unit : enemyBoard)
		{
			if (unit.currentHP <= 0) continue;
			if (unit.position == pos) return true;
		}

		return false;
	}

	/// <summary>
	/// 防御力による軽減を適用した最終ダメージを返す(TFT等でおなじみの逓減式)。
	/// 防御力0なら等倍、100ならダメージ半減、200なら1/3、というように緩やかに頭打ちさせる。
	/// 最低1ダメージは保証する(防御力がどれだけ高くても無敵にはしない)。
	/// </summary>
	int ApplyDefense(int rawDamage, int defense)
	{
		float multiplier = 100.0f / (100.0f + (float)defense);
		int reduced = (int)(rawDamage * multiplier);
		return reduced < 1 ? 1 : reduced;
	}

	/// <summary>
	/// attackTypeに応じて、攻撃側が参照すべき攻撃力(物理=攻撃力/魔法=魔力)を、
	/// トレイト・アイテムによるボーナスを含めた実効値で返す。
	/// </summary>
	int GetPower(const UnitInstance& unit, AttackType attackType)
	{
		return (attackType == AttackType::Physical)
			? (unit.def->baseAttack + unit.bonusAttack)
			: (unit.def->magicPower + unit.bonusMagicPower);
	}

	/// <summary>
	/// attackTypeに応じて、防御側が参照すべき防御力(物理/魔法)を、
	/// トレイト・アイテムによるボーナスを含めた実効値で返す。
	/// </summary>
	int GetDefenseFor(const UnitInstance& unit, AttackType attackType)
	{
		return (attackType == AttackType::Physical)
			? (unit.def->physicalDefense + unit.bonusPhysicalDefense)
			: (unit.def->magicDefense + unit.bonusMagicDefense);
	}

	/// <summary>
	/// トレイト・アイテムによるボーナスを含めた、必殺技発動に必要なゲージ値を返す(最低1)。
	/// </summary>
	int GetEffectiveSkillThreshold(const UnitInstance& unit)
	{
		int threshold = unit.def->skillThreshold + unit.bonusSkillThreshold;
		return threshold < 1 ? 1 : threshold;
	}

	/// <summary>
	/// トレイト・アイテムによるボーナスを含めた、実効攻撃速度(1秒あたりの行動回数)を返す。
	/// 0や負の値になって行動間隔が発散しない(0除算にならない)よう下限を設けている。
	/// </summary>
	float GetEffectiveAttackSpeed(const UnitInstance& unit)
	{
		const float kMinAttackSpeed = 0.1f;
		float speed = unit.def->attackSpeed + unit.bonusAttackSpeed;
		return speed < kMinAttackSpeed ? kMinAttackSpeed : speed;
	}

	/// <summary>
	/// 実効攻撃速度から、次に行動するまでの間隔(秒)を計算する。
	/// </summary>
	float GetEffectiveAttackInterval(const UnitInstance& unit)
	{
		return 1.0f / GetEffectiveAttackSpeed(unit);
	}

	/// <summary>
	/// unit.def->moveSpeed(1秒あたりに移動できるヘックス数)と実効行動間隔(秒)から、
	/// この1回の行動で実際に移動できる歩数を計算する。四捨五入し、最低1歩は保証する
	/// (行動する以上は必ず前進できるようにするため)。
	/// </summary>
	int GetEffectiveMoveSteps(const UnitInstance& unit)
	{
		float steps = unit.def->moveSpeed * GetEffectiveAttackInterval(unit);
		int rounded = (int)(steps + 0.5f);
		return rounded < 1 ? 1 : rounded;
	}

	/// <summary>
	/// targetにdamage分のダメージを適用する。シールドが残っていれば、まずそちらから減らす。
	/// シールドで吸収した量を返す(0なら吸収なし)。
	/// </summary>
	int ApplyDamageToTarget(UnitInstance& target, int damage)
	{
		int shieldAbsorbed = (target.shieldAmount < damage) ? target.shieldAmount : damage;
		target.shieldAmount -= shieldAbsorbed;
		target.currentHP -= (damage - shieldAbsorbed);
		return shieldAbsorbed;
	}

	/// <summary>
	/// シールドが実際にダメージを吸収していれば、その旨をイベントとして記録する。
	/// </summary>
	void RecordShieldAbsorb(const UnitInstance& target, const std::string& targetOwner, int targetIndex, int shieldAbsorbed,
		std::vector<CombatEvent>& outEvents)
	{
		if (shieldAbsorbed <= 0) return;

		CombatEvent e;
		e.type = CombatEventType::ShieldAbsorb;
		e.time = m_currentTime;
		e.targetOwner = targetOwner;
		e.targetName = target.def->name;
		e.targetIndex = targetIndex;
		e.amount = shieldAbsorbed;
		e.afterValue = target.shieldAmount;
		outEvents.push_back(e);
	}

	void NormalAttack(const UnitInstance& attacker, const std::string& attackerOwner, int actorIndex, UnitInstance& target, const std::string& targetOwner, int targetIndex,
		std::vector<CombatEvent>& outEvents)
	{
		AttackType attackType = attacker.def->attackType;
		int rawDamage = GetPower(attacker, attackType);
		int damage = ApplyDefense(rawDamage, GetDefenseFor(target, attackType));

		int beforeHP = target.currentHP;
		int shieldAbsorbed = ApplyDamageToTarget(target, damage);

		CombatEvent e;
		e.type = CombatEventType::NormalAttack;
		e.time = m_currentTime;
		e.actorOwner = attackerOwner;
		e.actorName = attacker.def->name;
		e.actorIndex = actorIndex;
		e.targetOwner = targetOwner;
		e.targetName = target.def->name;
		e.targetIndex = targetIndex;
		e.attackType = attackType;
		e.amount = damage;
		e.beforeValue = beforeHP;
		e.afterValue = target.currentHP;
		outEvents.push_back(e);

		RecordShieldAbsorb(target, targetOwner, targetIndex, shieldAbsorbed, outEvents);
		CheckDeath(target, targetOwner, targetIndex, outEvents);

		// 通常攻撃がヒットし対象が生存していれば、attackerのオンヒットパッシブ(火傷等)を適用する。
		if (target.currentHP > 0)
		{
			TryApplyOnHitBurn(attacker, attackerOwner, actorIndex, target);
		}
	}

	/// <summary>
	/// attackerがOnHitBurnパッシブ(アイテム由来)を持っていれば、targetに火傷を付与/リフレッシュする。
	/// 既に火傷がかかっている場合はスタックせず、残り回数を最大へ戻し(＝最後にヒットしてから
	/// ticks刻み分だけ延長)、damagePerTickは高い方を採用する。
	/// 刻みの周期(nextTickTime)はリフレッシュしない ── 毎秒の刻みを維持したまま持続時間だけ延ばす。
	/// (周期までリセットすると、interval未満の間隔で殴り続けた場合に刻みが永久に先送りされてしまう)
	/// </summary>
	void TryApplyOnHitBurn(const UnitInstance& attacker, const std::string& attackerOwner, int actorIndex,
		UnitInstance& target)
	{
		for (const ItemDef* item : attacker.items)
		{
			if (item == nullptr) continue;
			for (const PassiveEffect& p : item->passives)
			{
				if (p.type != PassiveEffectType::OnHitBurn) continue;
				if (p.magnitude <= 0 || p.ticks <= 0 || p.interval <= 0.0f) continue; // interval<=0はTick処理が無限ループするため弾く。

				// 火傷は対象につき1インスタンスのみ(スタックさせない)。
				ActiveBurn* existing = target.activeBurns.empty() ? nullptr : &target.activeBurns.front();
				if (existing != nullptr)
				{
					existing->ticksRemaining = p.ticks; // 残り回数をリフレッシュ(周期nextTickTimeはそのまま)。
					existing->interval = p.interval;
					if (p.magnitude > existing->damagePerTick) existing->damagePerTick = p.magnitude;
					existing->sourceOwner = attackerOwner;
					existing->sourceName = attacker.def->name;
					existing->sourceIndex = actorIndex;
					// 何らかの理由で次刻みが過去に取り残されていたら、現在時刻基準へ引き上げる(暴走防止)。
					if (existing->nextTickTime <= m_currentTime)
					{
						existing->nextTickTime = m_currentTime + p.interval;
					}
				}
				else
				{
					ActiveBurn b;
					b.damagePerTick = p.magnitude;
					b.interval = p.interval;
					b.nextTickTime = m_currentTime + p.interval;
					b.ticksRemaining = p.ticks;
					b.sourceOwner = attackerOwner;
					b.sourceName = attacker.def->name;
					b.sourceIndex = actorIndex;
					target.activeBurns.push_back(b);
				}
			}
		}
	}

	/// <summary>
	/// 現在時刻(m_currentTime)までに刻みの時刻が来ている火傷を、両陣営の生存ユニットについて処理する。
	/// 火傷は防御力で軽減されず、シールドも貫通してcurrentHPに直接作用する(確定ダメージ)。
	/// </summary>
	void ProcessDueBurns(
		std::vector<UnitInstance>& playerBoard, const std::string& playerOwner,
		std::vector<UnitInstance>& enemyBoard, const std::string& enemyOwner,
		std::vector<CombatEvent>& outEvents)
	{
		TickBurnsForBoard(playerBoard, playerOwner, outEvents);
		TickBurnsForBoard(enemyBoard, enemyOwner, outEvents);
	}

	void TickBurnsForBoard(std::vector<UnitInstance>& board, const std::string& ownerName,
		std::vector<CombatEvent>& outEvents)
	{
		const float kEps = 0.0001f;
		for (size_t i = 0; i < board.size(); ++i)
		{
			UnitInstance& unit = board[i];
			if (unit.currentHP <= 0)
			{
				unit.activeBurns.clear();
				continue;
			}

			for (auto& burn : unit.activeBurns)
			{
				// 時刻が飛んでいる場合に備えて、来ている刻みをまとめて消化する。
				while (burn.ticksRemaining > 0 && burn.nextTickTime <= m_currentTime + kEps)
				{
					int beforeHP = unit.currentHP;
					unit.currentHP -= burn.damagePerTick; // 防御・シールドを通さない確定ダメージ。

					CombatEvent e;
					e.type = CombatEventType::Burn;
					e.time = m_currentTime;
					e.actorOwner = burn.sourceOwner;
					e.actorName = burn.sourceName;
					e.actorIndex = burn.sourceIndex;
					e.targetOwner = ownerName;
					e.targetName = unit.def->name;
					e.targetIndex = (int)i;
					e.amount = burn.damagePerTick;
					e.beforeValue = beforeHP;
					e.afterValue = unit.currentHP;
					outEvents.push_back(e);

					burn.ticksRemaining--;
					burn.nextTickTime += burn.interval;

					if (unit.currentHP <= 0)
					{
						CheckDeath(unit, ownerName, (int)i, outEvents);
						break; // このユニットの火傷処理は終了。
					}
				}
			}

			// 使い切った火傷を除去する。
			unit.activeBurns.erase(
				std::remove_if(unit.activeBurns.begin(), unit.activeBurns.end(),
					[](const ActiveBurn& b) { return b.ticksRemaining <= 0; }),
				unit.activeBurns.end());
		}
	}

	/// <summary>
	/// attackerの必殺技をtargetに対して発動する。skillTypeに応じて、単体ダメージに加え
	/// 範囲ダメージ・自己回復(ドレイン)・シールド付与のいずれかの追加効果を適用する。
	/// enemyBoardはtargetが所属する盤面全体で、AreaDamageの巻き込み判定に使う。
	/// </summary>
	void UseSkill(UnitInstance& attacker, const std::string& attackerOwner, int actorIndex, UnitInstance& target, const std::string& targetOwner, int targetIndex,
		std::vector<UnitInstance>& enemyBoard, std::vector<CombatEvent>& outEvents)
	{
		AttackType attackType = attacker.def->skillAttackType;
		int rawDamage = GetPower(attacker, attackType);
		int damage = ApplyDefense(rawDamage, GetDefenseFor(target, attackType));

		// 本来の対象への直撃ダメージは、どの効果タイプでも共通して発生する。
		int beforeHP = target.currentHP;
		int shieldAbsorbed = ApplyDamageToTarget(target, damage);

		CombatEvent hitEvent;
		hitEvent.type = CombatEventType::SkillAttack;
		hitEvent.time = m_currentTime;
		hitEvent.actorOwner = attackerOwner;
		hitEvent.actorName = attacker.def->name;
		hitEvent.actorIndex = actorIndex;
		hitEvent.targetOwner = targetOwner;
		hitEvent.targetName = target.def->name;
		hitEvent.targetIndex = targetIndex;
		hitEvent.attackType = attackType;
		hitEvent.amount = damage;
		hitEvent.beforeValue = beforeHP;
		hitEvent.afterValue = target.currentHP;
		outEvents.push_back(hitEvent);

		RecordShieldAbsorb(target, targetOwner, targetIndex, shieldAbsorbed, outEvents);
		CheckDeath(target, targetOwner, targetIndex, outEvents);

		switch (attacker.def->skillType)
		{
		case SkillEffectType::AreaDamage:
			ApplySplashDamage(attacker, attackerOwner, actorIndex, target, targetOwner, rawDamage, attackType, enemyBoard, outEvents);
			break;

		case SkillEffectType::DamageAndHeal:
		{
			int healAmount = (int)(damage * attacker.def->skillHealPercent / 100.0f);
			int effectiveMaxHP = attacker.def->baseHP + attacker.bonusMaxHP;
			int beforeAttackerHP = attacker.currentHP;

			attacker.currentHP += healAmount;
			if (attacker.currentHP > effectiveMaxHP) attacker.currentHP = effectiveMaxHP;

			CombatEvent healEvent;
			healEvent.type = CombatEventType::Heal;
			healEvent.time = m_currentTime;
			healEvent.actorOwner = attackerOwner;
			healEvent.actorName = attacker.def->name;
			healEvent.actorIndex = actorIndex;
			healEvent.amount = attacker.currentHP - beforeAttackerHP;
			healEvent.beforeValue = beforeAttackerHP;
			healEvent.afterValue = attacker.currentHP;
			outEvents.push_back(healEvent);
		}
		break;

		case SkillEffectType::DamageAndShield:
		{
			attacker.shieldAmount += attacker.def->skillShieldAmount;

			CombatEvent shieldEvent;
			shieldEvent.type = CombatEventType::Shield;
			shieldEvent.time = m_currentTime;
			shieldEvent.actorOwner = attackerOwner;
			shieldEvent.actorName = attacker.def->name;
			shieldEvent.actorIndex = actorIndex;
			shieldEvent.amount = attacker.def->skillShieldAmount;
			shieldEvent.afterValue = attacker.shieldAmount;
			outEvents.push_back(shieldEvent);
		}
		break;

		case SkillEffectType::Damage:
		default:
			break; // 直撃のみ。追加効果なし。
		}
	}

	/// <summary>
	/// AreaDamage用。primaryTargetの周囲(skillSplashRadius以内)にいる生存中の敵にも、
	/// rawDamageのskillSplashPercent%を(個別に防御計算した上で)ダメージとして与える。
	/// </summary>
	void ApplySplashDamage(const UnitInstance& attacker, const std::string& attackerOwner, int actorIndex,
		const UnitInstance& primaryTarget, const std::string& targetOwner,
		int rawDamage, AttackType attackType, std::vector<UnitInstance>& enemyBoard,
		std::vector<CombatEvent>& outEvents)
	{
		for (size_t k = 0; k < enemyBoard.size(); ++k)
		{
			UnitInstance& other = enemyBoard[k];
			if (&other == &primaryTarget) continue;
			if (other.currentHP <= 0) continue;
			if (primaryTarget.position.Distance(other.position) > attacker.def->skillSplashRadius) continue;

			int splashRawDamage = (int)(rawDamage * attacker.def->skillSplashPercent / 100.0f);
			int splashDamage = ApplyDefense(splashRawDamage, GetDefenseFor(other, attackType));

			int beforeHP = other.currentHP;
			int shieldAbsorbed = ApplyDamageToTarget(other, splashDamage);

			CombatEvent e;
			e.type = CombatEventType::SplashDamage;
			e.time = m_currentTime;
			e.actorOwner = attackerOwner;
			e.actorName = attacker.def->name;
			e.actorIndex = actorIndex;
			e.targetOwner = targetOwner;
			e.targetName = other.def->name;
			e.targetIndex = (int)k;
			e.attackType = attackType;
			e.amount = splashDamage;
			e.beforeValue = beforeHP;
			e.afterValue = other.currentHP;
			outEvents.push_back(e);

			RecordShieldAbsorb(other, targetOwner, (int)k, shieldAbsorbed, outEvents);
			CheckDeath(other, targetOwner, (int)k, outEvents);
		}
	}

	void CheckDeath(const UnitInstance& unit, const std::string& ownerName, int unitIndex, std::vector<CombatEvent>& outEvents)
	{
		if (unit.currentHP <= 0)
		{
			CombatEvent e;
			e.type = CombatEventType::Death;
			e.time = m_currentTime;
			e.actorOwner = ownerName;
			e.actorName = unit.def->name;
			e.actorIndex = unitIndex;
			outEvents.push_back(e);
		}
	}
};

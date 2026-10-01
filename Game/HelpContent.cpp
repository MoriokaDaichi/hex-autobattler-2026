#include "stdafx.h"
#include "HelpContent.h"
#include "GameState.h"
#include "EconomySystem.h"
#include "LevelSystem.h"
#include "ShopSystem.h"
#include "ItemSystem.h"
#include "StarLevelSystem.h"
#include "TraitDatabase.h"
#include "ItemDatabase.h"
#include "UITextUtil.h"

namespace
{
	using HelpContent::Line;
	using HelpContent::Category;

	/// <summary>見出し行を追加する。</summary>
	void H(std::vector<Line>& out, const std::wstring& text)
	{
		Line line;
		line.text = text;
		line.heading = true;
		out.push_back(line);
	}

	/// <summary>本文行を追加する(空文字なら空行)。</summary>
	void T(std::vector<Line>& out, const std::wstring& text)
	{
		Line line;
		line.text = text;
		out.push_back(line);
	}

	/// <summary>printf形式で本文行を追加する。</summary>
	template <typename... Args>
	void F(std::vector<Line>& out, const wchar_t* format, Args... args)
	{
		wchar_t buf[512];
		swprintf_s(buf, format, args...);
		T(out, buf);
	}

	std::wstring Widen(const std::string& s)
	{
		wchar_t buf[128];
		swprintf_s(buf, L"%hs", s.c_str());
		return buf;
	}

	/// <summary>
	/// StatEffect 1つをヘルプ用の日本語テキストにする("攻撃力+10%" / "必殺技の必要ゲージ-1")。
	/// UITextUtil::EffectShortTextはマイナス値が"技+-1"になり、略語(AT/AP)も初見では分かりにくいため、
	/// ヘルプでは正式名+符号付きで書く。
	/// </summary>
	std::wstring EffectText(const StatEffect& e)
	{
		const wchar_t* label = L"?";
		bool percent = false;
		bool fractional = false;
		switch (e.stat)
		{
		case StatEffectType::AttackFlat:             label = L"攻撃力"; break;
		case StatEffectType::AttackPercent:          label = L"攻撃力"; percent = true; break;
		case StatEffectType::MagicPowerFlat:         label = L"魔力"; break;
		case StatEffectType::MagicPowerPercent:      label = L"魔力"; percent = true; break;
		case StatEffectType::MaxHPFlat:              label = L"最大HP"; break;
		case StatEffectType::MaxHPPercent:           label = L"最大HP"; percent = true; break;
		case StatEffectType::PhysicalDefenseFlat:    label = L"物理防御"; break;
		case StatEffectType::PhysicalDefensePercent: label = L"物理防御"; percent = true; break;
		case StatEffectType::MagicDefenseFlat:       label = L"魔法防御"; break;
		case StatEffectType::MagicDefensePercent:    label = L"魔法防御"; percent = true; break;
		case StatEffectType::SkillThresholdFlat:     label = L"必殺技の必要ゲージ"; break;
		case StatEffectType::AttackSpeedFlat:        label = L"攻撃速度"; fractional = true; break;
		case StatEffectType::AttackSpeedPercent:     label = L"攻撃速度"; percent = true; break;
		}

		wchar_t buf[64];
		if (fractional)
		{
			swprintf_s(buf, L"%ls%+.2f", label, e.value);
		}
		else
		{
			swprintf_s(buf, L"%ls%+d%ls", label, (int)e.value, percent ? L"%" : L"");
		}
		return buf;
	}

	std::wstring EffectsText(const std::vector<StatEffect>& effects)
	{
		std::wstring s;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			if (i > 0) s += L"、";
			s += EffectText(effects[i]);
		}
		return s;
	}

	/// <summary>アイテム1つの効果(ステータス+パッシブ)を1行テキストにする。</summary>
	std::wstring ItemEffectText(const ItemDef& def)
	{
		std::wstring s = EffectsText(def.effects);
		for (const auto& p : def.passives)
		{
			if (p.type == PassiveEffectType::OnHitBurn)
			{
				wchar_t buf[96];
				swprintf_s(buf, L"、通常攻撃で火傷(%dダメージx%d回)", p.magnitude, p.ticks);
				s += buf;
			}
		}
		return s;
	}

	// ------------------------------------------------------------------
	// 1. 基本の流れ
	// ------------------------------------------------------------------
	Category BuildFlow()
	{
		Category c;
		c.title = L"基本の流れ";
		auto& o = c.lines;

		H(o, L"ゲームの目的");
		F(o, L"・ユニットを集めて盤面に並べ、全%dラウンドの敵をすべて倒すとゲームクリア。", GameState::kTotalRounds);
		T(o, L"・戦闘はユニットが自動で行う。プレイヤーは準備フェーズで編成を整える。");
		T(o, L"");
		H(o, L"1ラウンドの流れ");
		T(o, L"・準備フェーズ: ショップでユニットを買い、盤面に配置し、アイテムを装備する。");
		T(o, L"  準備ができたら右下の「戦闘開始」ボタン(パッドはB)を押す。");
		T(o, L"・戦闘フェーズ: 味方と敵が自動で戦う。操作は不要。");
		T(o, L"・結果: 勝てば次のラウンドへ進む。負けたら同じ敵ともう一度戦う。");
		T(o, L"・戦闘のあとは勝ち負けに関係なく、ゴールドと経験値がもらえる(「経済」参照)。");
		T(o, L"");
		H(o, L"勝ち負けとゲームオーバー");
		T(o, L"・敵を全滅させれば勝ち。味方が全滅したら負け。相打ち(両方全滅)も負け扱い。");
		F(o, L"・同じ敵に%d回負けるとゲームオーバー。勝てば負けた回数は0に戻る。", GameState::kMaxLossesPerEnemy);
		T(o, L"・敵の編成はラウンドごとに決まっていて、後のラウンドほど数が多く強い。");
		T(o, L"・次に戦う敵は、準備フェーズ中も盤面の奥側(敵陣)に表示されている。");
		T(o, L"・画面右上で、今のラウンド・負けた回数・連勝/連敗を確認できる。");
		T(o, L"");
		H(o, L"セーブ");
		T(o, L"・準備フェーズ中にF5キーでセーブ。タイトル画面の「続きから」で再開できる。");
		return c;
	}

	// ------------------------------------------------------------------
	// 2. 操作
	// ------------------------------------------------------------------
	Category BuildControls()
	{
		Category c;
		c.title = L"操作";
		auto& o = c.lines;

		H(o, L"マウス(準備フェーズ)");
		T(o, L"・ショップのカードを左クリック: ユニットを購入");
		T(o, L"・ベンチのユニット -> 盤面の空きマスの順に左クリック: 配置");
		T(o, L"・盤面のユニット -> 空きマスの順に左クリック: 移動");
		T(o, L"・アイテム -> ユニットの順に左クリック: 装備");
		T(o, L"・ベンチのユニットを右クリック2回: 売却");
		T(o, L"・盤面のユニットを右クリック2回: ベンチへ戻す");
		T(o, L"・何かを選んでいる時に右クリック: 選択を解除");
		T(o, L"・ドラッグ&ドロップでも、配置・移動・装備・売却などができる");
		T(o, L"・Reroll / BuyXP / Lock / 戦闘開始 の各ボタンをクリック");
		T(o, L"・カーソルを乗せると、詳しい説明(ツールチップ)が出る");
		T(o, L"");
		H(o, L"ゲームパッド(準備フェーズ)");
		T(o, L"・Back(キーボードはTab): 操作する場所を切り替え");
		T(o, L"  ショップ -> ベンチ -> アイテム -> 盤面 の順に切り替わる");
		T(o, L"・十字キー(キーボードは矢印キー): カーソル移動");
		T(o, L"・A: ユニット購入 / アイテムを持つ / 持ったアイテムを装備");
		T(o, L"・X: ベンチのユニットを配置 / 盤面のユニットを選んで移動");
		T(o, L"・LB: ベンチのユニットを売却 / 盤面のユニットをベンチへ");
		T(o, L"・Y: リロール   RB: 経験値購入   Start: ショップのロック");
		T(o, L"・B: 戦闘開始");
		T(o, L"");
		H(o, L"ヘルプ");
		T(o, L"・ヘルプボタン / F1 / Hキー / パッドのLT: ヘルプを開く・閉じる");
		T(o, L"・上下キー(十字キー)で項目、左右キーでページを切り替え。Esc / B で閉じる");
		T(o, L"・ヘルプを開いている間は盤面などを操作できない(戦闘は止まらない)");
		T(o, L"");
		H(o, L"パッド未接続時のキーボード");
		T(o, L"・J=A  K=B  L=X  I=Y  B=LB  7=RB  N=LT  Enter=Start  Space=Back");
		return c;
	}

	// ------------------------------------------------------------------
	// 3. 経済
	// ------------------------------------------------------------------
	Category BuildEconomy()
	{
		Category c;
		c.title = L"経済";
		auto& o = c.lines;

		H(o, L"戦闘後の収入(勝っても負けてももらえる)");
		F(o, L"・基本収入: 毎ラウンド +%dG", EconomySystem::kBaseIncome);
		F(o, L"・利子: 所持金%dGごとに+1G。最大+%dG(%dG以上持っていても+%dG)",
			EconomySystem::kGoldPerInterest, EconomySystem::kMaxInterest,
			EconomySystem::kGoldPerInterest * EconomySystem::kMaxInterest, EconomySystem::kMaxInterest);
		T(o, L"  利子は戦闘が終わった時点の所持金で計算される");

		// 連勝ボーナス: 付き始め〜+3連勝ぶんの例を生成する(上限なしで伸び続ける)。
		{
			std::wstring s = L"・連勝ボーナス: ";
			int first = EconomySystem::kWinStreakBonusMinStreak;
			for (int n = first; n <= first + 3; ++n)
			{
				wchar_t buf[48];
				swprintf_s(buf, L"%d連勝 +%dG、", n, EconomySystem::GetWinStreakBonus(n));
				s += buf;
			}
			s += L"...";
			T(o, s);
			F(o, L"  以降も1連勝ごとに+%dGずつ増える(上限なし)", EconomySystem::kWinStreakGoldPerWin);
		}

		// 連敗ボーナス: GetLossStreakBonusの値が同じ連敗数をまとめて表にする。
		{
			const int kMaxShown = 10;
			std::wstring s = L"・連敗ボーナス: ";
			int n = 1;
			while (n <= kMaxShown && EconomySystem::GetLossStreakBonus(n) == 0) ++n;
			bool firstGroup = true;
			while (n <= kMaxShown)
			{
				int value = EconomySystem::GetLossStreakBonus(n);
				int end = n;
				while (end + 1 <= kMaxShown && EconomySystem::GetLossStreakBonus(end + 1) == value) ++end;

				wchar_t buf[48];
				if (end >= kMaxShown)
				{
					swprintf_s(buf, L"%d連敗以上 +%dG", n, value);
				}
				else if (end > n)
				{
					swprintf_s(buf, L"%d-%d連敗 +%dG", n, end, value);
				}
				else
				{
					swprintf_s(buf, L"%d連敗 +%dG", n, value);
				}
				if (!firstGroup) s += L" / ";
				s += buf;
				firstGroup = false;
				n = end + 1;
			}
			T(o, s);
		}
		F(o, L"・%d連勝以上になると、勝つたびに素材アイテムがもう1つもらえる", EconomySystem::kWinStreakItemMinStreak);
		T(o, L"・引き分け(相打ち)は負けとして数える");
		T(o, L"");
		H(o, L"ゴールドの使い道");
		F(o, L"・ユニット購入: コストと同じG(%d-%dG)", ShopSystem::kMinUnitCost, ShopSystem::kMaxUnitCost);
		F(o, L"・リロール: %dGでショップの5枠を引き直す", ShopSystem::kRerollCost);
		F(o, L"・経験値購入(BuyXP): %dGで経験値+%d", LevelSystem::kBuyXPCost, LevelSystem::kBuyXPAmount);
		T(o, L"・売却: 星1はコスト分、星2はコストx3、星3はコストx9のGが戻る");
		T(o, L"");
		H(o, L"ショップのロック");
		T(o, L"・Lock中は、次のラウンドになってもショップの中身がそのまま残る");
		T(o, L"・ロック中でもリロールはできる(引き直した中身がロックされる)");
		T(o, L"");
		H(o, L"ヒント");
		F(o, L"・%dGを保つと利子が最大になる。使いすぎず、貯めすぎず。",
			EconomySystem::kGoldPerInterest * EconomySystem::kMaxInterest);
		T(o, L"・ゴールド表示にカーソルを乗せると、次のラウンドの収入の内訳が見られる");
		return c;
	}

	// ------------------------------------------------------------------
	// 4. ユニットと星
	// ------------------------------------------------------------------
	Category BuildUnits()
	{
		Category c;
		c.title = L"ユニットと星";
		auto& o = c.lines;

		H(o, L"ユニットの入手");
		T(o, L"・ショップには5体が並び、購入するとベンチ(画面左)に入る");
		T(o, L"・買った枠は空になり、リロールか次のラウンドで補充される");
		T(o, L"");
		H(o, L"合成(星)");
		T(o, L"・同じユニット・同じ星を3体そろえると、自動で1体の上の星に合成される");
		T(o, L"  星1 x3 -> 星2、 星2 x3 -> 星3(ベンチと盤面のどちらにいてもよい)");
		F(o, L"・星が1つ上がるごとに、最大HP・攻撃力・魔力・防御が%.1f倍(星3は約%.2f倍)",
			StarLevelSystem::kStarMultiplierStep, StarLevelSystem::GetStarMultiplier(3));
		T(o, L"・画面では、星2は「*2」、星3は「*3」と表示される");
		T(o, L"・注意: 合成すると、素材になったユニットの装備アイテムは無くなる");
		T(o, L"");
		H(o, L"レベルと盤面の上限");
		F(o, L"・盤面に置ける数 = プレイヤーのレベル(Lv1なら1体、最大Lv%dで%d体)",
			LevelSystem::kMaxLevel, LevelSystem::kMaxLevel);
		T(o, L"・配置できるのは自陣(手前3行)の空きマスだけ");
		F(o, L"・経験値: 毎ラウンド+%d。BuyXPで%dGごとに+%d",
			LevelSystem::kPassiveXPPerRound, LevelSystem::kBuyXPCost, LevelSystem::kBuyXPAmount);
		T(o, L"・レベルが上がると、ショップに高コストのユニットが出やすくなる");

		// 次のレベルまでの必要経験値(LevelSystemの表から生成。4レベル分ずつ1行)。
		{
			LevelSystem levelSystem;
			T(o, L"・次のレベルまでの必要経験値:");
			std::wstring s = L"  ";
			int perLine = 0;
			for (int lv = 1; lv < LevelSystem::kMaxLevel; ++lv)
			{
				wchar_t buf[48];
				swprintf_s(buf, L"Lv%d->%d: %-3d  ", lv, lv + 1, levelSystem.XPForNextLevel(lv));
				s += buf;
				if (++perLine == 4)
				{
					T(o, s);
					s = L"  ";
					perLine = 0;
				}
			}
			if (perLine > 0) T(o, s);
		}
		T(o, L"");

		// レベル別のショップ出現率(ShopSystemの表から生成)。
		H(o, L"ショップの出現率(コスト1 / 2 / 3 / 4 / 5、%)");
		for (int lv = 1; lv <= LevelSystem::kMaxLevel; ++lv)
		{
			const int* odds = ShopSystem::GetCostOdds(lv);
			F(o, L"  Lv%d:  %3d / %3d / %3d / %3d / %3d", lv, odds[0], odds[1], odds[2], odds[3], odds[4]);
		}
		return c;
	}

	// ------------------------------------------------------------------
	// 5. 戦闘
	// ------------------------------------------------------------------
	Category BuildCombat()
	{
		Category c;
		c.title = L"戦闘";
		auto& o = c.lines;

		H(o, L"自動戦闘");
		T(o, L"・戦闘はすべて自動。各ユニットは一番近い敵をねらう");
		T(o, L"・敵が射程の外なら近づき、射程に入ったら攻撃する");
		T(o, L"・攻撃速度が高いユニットほど、ひんぱんに行動する");
		T(o, L"・戦闘の前に、全ユニットのHPは毎回満タンに戻る");
		T(o, L"");
		H(o, L"必殺技ゲージ");
		T(o, L"・通常攻撃を1回するか、攻撃を1回受けるたびにゲージ+1");
		T(o, L"・ゲージが必要数(ユニットごとに違う)に達すると、次の行動で必殺技を使う");
		T(o, L"・必殺技を使うとゲージは0に戻る。必殺技の射程は通常攻撃と違うことがある");
		T(o, L"・必殺技の種類: 単体ダメージ / 範囲ダメージ / ダメージ+自分を回復 /");
		T(o, L"  ダメージ+自分にシールド");
		T(o, L"・シールドは先にダメージを受け止めて、HPを守る");
		T(o, L"");
		H(o, L"物理と魔法");
		T(o, L"・物理攻撃: 攻撃力(AT)でダメージが決まり、物理防御で減らされる");
		T(o, L"・魔法攻撃: 魔力(AP)でダメージが決まり、魔法防御で減らされる");
		T(o, L"・通常攻撃と必殺技がそれぞれ物理か魔法かは、ユニットごとに決まっている");
		T(o, L"・防御の効果: ダメージ x 100/(100+防御)。防御100で半分、200で3分の1");
		T(o, L"  どれだけ防御が高くても、最低1ダメージは受ける");
		T(o, L"・火傷: 一部のアイテムの効果。防御とシールドを無視して少しずつダメージ");
		T(o, L"");
		H(o, L"その他");
		T(o, L"・トレイトの効果は戦闘開始時の盤面で決まり、戦闘中に味方が倒れても変わらない");
		T(o, L"・ツールチップの略語: AT=攻撃力 AP=魔力 AS=攻撃速度 物防/魔防=物理/魔法防御");
		T(o, L"・ユニットにカーソルを乗せると、射程・攻撃速度・必殺技の内容が見られる");
		return c;
	}

	// ------------------------------------------------------------------
	// 6. アイテム
	// ------------------------------------------------------------------
	Category BuildItems(const ItemDatabase& itemDatabase)
	{
		Category c;
		c.title = L"アイテム";
		auto& o = c.lines;

		H(o, L"入手");
		F(o, L"・ラウンドに勝つと、素材アイテムを1つもらえる(%d連勝以上ならさらに+1つ)",
			EconomySystem::kWinStreakItemMinStreak);
		T(o, L"・もらったアイテムは、画面右の ITEMS 一覧に入る");
		T(o, L"");
		H(o, L"装備");
		T(o, L"・アイテムをクリック -> ユニットをクリックで装備");
		T(o, L"  パッド: アイテム欄でA -> ベンチか盤面のユニットを選んでA");
		F(o, L"・ベンチのユニットにも装備できる。1体に%dつまで", ItemSystem::kMaxItemSlots);
		T(o, L"・一度装備したアイテムは外せない");
		T(o, L"・ユニットを売却すると、装備していたアイテムは ITEMS 一覧に戻る");
		F(o, L"・合成すると素材3体の装備を引き継ぐ(%dつを超えた分は ITEMS 一覧へ)",
			ItemSystem::kMaxItemSlots);
		T(o, L"・盤面からベンチへ戻しても、装備は付いたまま");
		T(o, L"");
		H(o, L"合成");
		T(o, L"・素材を持っているユニットに別の素材を装備すると、2つが自動で完成アイテムになる");
		T(o, L"・完成アイテムは素材2つより強く、使う枠は1つだけ");
		T(o, L"・枠が全部埋まっていても、合成できる素材なら装備できる");
		T(o, L"");

		H(o, L"素材アイテム");
		for (const auto& def : itemDatabase.GetAllItemDefs())
		{
			if (def.category != ItemCategory::Component) continue;
			T(o, L"・" + Widen(def.name) + L": " + ItemEffectText(def));
		}
		T(o, L"");

		H(o, L"合成レシピ(素材 + 素材 -> 完成アイテム: 効果)");
		for (const auto& recipe : itemDatabase.GetAllRecipes())
		{
			const ItemDef* result = itemDatabase.FindItemDefByName(recipe.resultName);
			std::wstring line = L"・" + Widen(recipe.componentA) + L" + " + Widen(recipe.componentB)
				+ L" -> " + Widen(recipe.resultName);
			if (result != nullptr)
			{
				line += L": " + ItemEffectText(*result);
			}
			T(o, line);
		}
		return c;
	}

	// ------------------------------------------------------------------
	// 7. トレイト
	// ------------------------------------------------------------------
	Category BuildTraits(const TraitDatabase& traitDatabase)
	{
		Category c;
		c.title = L"トレイト";
		auto& o = c.lines;

		H(o, L"しくみ");
		T(o, L"・ユニットはそれぞれ、種族や役割を表すトレイトを持っている");
		T(o, L"・盤面に同じトレイトのユニットが決まった数いると、そのトレイトが発動する");
		T(o, L"・効果を受けるのは、そのトレイトを持つユニットだけ");
		T(o, L"・数が増えると段階が上がり、効果が強くなる(一番高い段階だけが有効)");
		T(o, L"・ベンチのユニットは数えない。同じユニットが2体いれば2体として数える");
		T(o, L"・画面左のトレイト一覧で、今の数と次の段階を確認できる");
		T(o, L"・%は、そのユニットの基礎値に対する割合");
		T(o, L"");

		for (const auto& def : traitDatabase.GetAllTraitDefs())
		{
			H(o, std::wstring(UITextUtil::TraitName(def.type)) + L" (" + Widen(def.name) + L")");
			for (const auto& tier : def.tiers)
			{
				wchar_t head[32];
				swprintf_s(head, L"  %d体: ", tier.requiredCount);
				T(o, head + EffectsText(tier.effects));
			}
		}
		return c;
	}
}

namespace HelpContent
{
	std::vector<Category> Build(const TraitDatabase& traitDatabase, const ItemDatabase& itemDatabase)
	{
		std::vector<Category> categories;
		categories.push_back(BuildFlow());
		categories.push_back(BuildControls());
		categories.push_back(BuildEconomy());
		categories.push_back(BuildUnits());
		categories.push_back(BuildCombat());
		categories.push_back(BuildItems(itemDatabase));
		categories.push_back(BuildTraits(traitDatabase));
		return categories;
	}
}

# ユニット3DモデルAI生成プロンプト & 必要アニメーション

MeshyAIでのテキスト→3D生成用。対象は `Game/UnitDatabase.cpp` に定義済みの全18ユニット。
既存サンプル資産(unityChan等)がトゥーン調のため、全ユニットで以下のスタイル接頭辞を共通で使う想定。

**共通スタイル接頭辞(全プロンプトの先頭に付ける):**
`stylized toon-shaded fantasy game character, cel-shaded, vibrant colors, clean silhouette, low-poly stylized 3D model, T-pose, game-ready, isolated on plain background,`

## 必要アニメーションについて

`CombatEvent.h` の `CombatEventType` で可視化が必要なのは実質 `Move` / `NormalAttack` / `SkillAttack`(+`SplashDamage`) / `Death` の4種のみ。
`Heal` / `Shield` / `ShieldAbsorb` は専用モーションを持たず、Skillアニメーションの上に光るエフェクト(パーティクル/グロー)を重ねるだけで表現できるため、追加アニメーションは不要と想定。
よって基本セットは **Idle / Move / NormalAttack / Skill / Death** の5クリップで統一。

**★1/★2/★3(星レベル)の見た目差別について:** 18ユニット×3星レベル=54モデルは制作コストが大きすぎるため、モデルは1ユニット1種類のみ生成し、星レベルの差はランタイム側(色味のティント変更・スケール拡大・オーラエフェクト追加など)で表現することを推奨。`StarLevelSystem` も現状ステータスのみ再計算する設計なので、見た目もマテリアル/エフェクト側で対応するのが自然。

**Meshy自動リギングの制約について:** MeshyのAuto-Rig/プリセットアニメーションは二足歩行の人型を前提としている。以下のユニットは四足・非人型のため、自動リグや標準プリセットがそのまま使えない可能性が高い: Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake。これらは手動リギング(Blender等)か、簡略化したカスタムアニメーション(スライム=スケールの伸縮のみ、四足獣=既存の四足プリセットを流用等)を検討する。

---

## 通常ユニット(コスト1〜2)

### Slime (★1, Monster/Warrior)
- **Prompt:** `...a small gelatinous blob monster, translucent teal-green slime body, simple cute-but-menacing face embedded inside, rounded amorphous teardrop shape, subtle glowing core, no limbs, slow sluggish creature design`
- **Animations:** Idle(呼吸のように上下に伸縮) / Move(にじり寄るような滑り移動) / NormalAttack(体当たり) / Skill=Damage(大きく伸びてからの体当たり) / Death(溶けて潰れる)
- 補足: 四肢なし。骨格リグ不要、ブレンドシェイプ/スケールアニメのみで足りる可能性が高い。

### Goblin (★1, Monster/Assassin)
- **Prompt:** `...a small green-skinned goblin humanoid, wiry agile build, tattered leather armor, wielding a curved dagger, pointed ears, sharp teeth, sneaky crouched posture`
- **Animations:** Idle / Move(小走り) / NormalAttack(短剣で突く) / Skill=Damage(素早い二連突き) / Death(倒れ込む)

### Archer (★1, Human/Ranger)
- **Prompt:** `...a human archer in a hooded green ranger cloak, holding a longbow with a quiver on the back, lean athletic build, alert stance`
- **Animations:** Idle / Move / NormalAttack(弓を引いて射る) / Skill=Damage(大きく引き絞っての強射) / Death(崩れ落ちる)

### Knight (★1, Human/Guardian)
- **Prompt:** `...a human knight in simple heavy iron armor, carrying a tower shield and a mace, sturdy stocky build, plain solid defensive design`
- **Animations:** Idle(盾構え) / Move / NormalAttack(メイスで殴打) / Skill=DamageAndShield(打撃+自分に盾の光エフェクト) / Death(倒れ込む)

### Swordsman (★2, Human/Warrior)
- **Prompt:** `...a human swordsman in polished steel armor, wielding a one-handed sword and a small round shield, balanced heroic stance`
- **Animations:** Idle / Move / NormalAttack(剣で斬りつけ) / Skill=Damage(強い二段斬り) / Death(崩れ落ちる)

### Priest (★2, Human/Mage)
- **Prompt:** `...a human priest in white and gold flowing robes, holding an ornate staff with a glowing holy symbol, serene expression`
- **Animations:** Idle / Move / NormalAttack(杖から光弾を放つ) / Skill=DamageAndHeal(詠唱+自分に回復の光) / Death(膝から崩れる)

### Cultist (★2, Human/Mage)
- **Prompt:** `...a human dark cultist in tattered purple hooded robes, holding a cursed grimoire and a bone staff, glowing purple runic markings, eerie hunched posture`
- **Animations:** Idle / Move / NormalAttack(闇の弾を放つ) / Skill=AreaDamage(範囲に闇の波動を放つ) / Death(崩れ落ちる)

### Direwolf (★2, Monster/Assassin)
- **Prompt:** `...a large direwolf, dark gray fur, glowing yellow eyes, lean muscular quadruped build, bared fangs, aggressive crouched hunting stance`
- **Animations:** Idle / Move(疾走) / NormalAttack(噛みつき) / Skill=DamageAndHeal(噛みつき+吸血の赤い光) / Death(倒れ込む)
- 補足: 四足獣。自動リグ非対応の可能性あり。

### ShadowStalker (★2, Human/Assassin)
- **Prompt:** `...a human shadow assassin in a fitted dark hooded outfit, wielding twin curved daggers, face partially covered by a mask, crouched stealthy stance`
- **Animations:** Idle / Move / NormalAttack(短剣で斬る) / Skill=Damage(連続斬りの乱舞) / Death(崩れ落ちる)

---

## 中コストユニット(コスト3)

### OrcBerserker (★3, Monster/Hero/Warrior)
- **Prompt:** `...a muscular orc berserker, green-gray skin with battle scars, spiked leather and fur armor, wielding a large double-bladed axe, fierce tusked face, aggressive stance`
- **Animations:** Idle / Move / NormalAttack(斧を振り下ろす) / Skill=DamageAndHeal(斧攻撃+赤い吸血エフェクト) / Death(崩れ落ちる)

### Paladin (★3, Human/Hero/Guardian)
- **Prompt:** `...a human paladin in heavy golden plate armor, wielding a broadsword and a large kite shield engraved with holy symbols, radiant cape, noble heroic pose`
- **Animations:** Idle / Move / NormalAttack(剣+盾での一撃) / Skill=DamageAndShield(一撃+自分に聖なる盾の光) / Death(崩れ落ちる)

### Griffin (★3, Monster/Ranger)
- **Prompt:** `...a majestic griffin, eagle head and wings combined with a lion body, golden feathers and tawny fur, sharp talons, wings spread mid-flight pose`
- **Animations:** Idle(旋回待機) / Move(飛行移動) / NormalAttack(急降下の鉤爪攻撃) / Skill=Damage(甲高い咆哮+強襲) / Death(落下)
- 補足: 飛行する四足+翼の複合構造。自動リグ非対応の可能性が高く、手動リグ推奨。

### Behemoth (★3, Monster/Guardian)
- **Prompt:** `...a massive behemoth, hulking four-legged reptilian beast with thick rocky gray hide, small horns, heavy stocky build, immovable defensive stance`
- **Animations:** Idle / Move(重々しい歩行) / NormalAttack(頭突き/踏みつけ) / Skill=DamageAndHeal(踏みつけ+吸収の光) / Death(倒れ込む)
- 補足: 四足獣。自動リグ非対応の可能性あり。

---

## 高コストユニット(コスト4〜5)

### YoungDragon (★4, Monster/Hero/Mage)
- **Prompt:** `...a young dragon, bipedal reptilian creature with small wings, deep red and gold scales, sharp horns, glowing throat before a breath attack, fierce yet youthful proportions`
- **Animations:** Idle / Move / NormalAttack(噛みつき/爪攻撃) / Skill=AreaDamage(ブレス攻撃) / Death(崩れ落ちる)
- 補足: 二足歩行なので自動リグは通る可能性が高いが、翼と尾の追加ボーンは手動調整が必要になりやすい。

### Warlord (★4, Human/Hero/Warrior)
- **Prompt:** `...a human warlord in ornate red and black battle armor with a flowing tattered cape, wielding a large two-handed sword, commanding heroic stance, battle-hardened face`
- **Animations:** Idle / Move / NormalAttack(大剣の横薙ぎ) / Skill=DamageAndShield(強い一撃+自分に盾の光) / Death(崩れ落ちる)

### NightBlade (★4, Human/Hero/Assassin)
- **Prompt:** `...a human hero assassin in sleek black and silver form-fitting armor, dual short swords, glowing blue accents, agile dynamic mid-strike pose`
- **Animations:** Idle / Move(俊敏な動き) / NormalAttack(双剣での斬撃) / Skill=DamageAndHeal(高速乱舞+吸血の光) / Death(崩れ落ちる)

### FlameDrake (★4, Monster/Hero/Mage)
- **Prompt:** `...a fearsome flame drake dragon, bipedal with large wings, deep orange and black scales, glowing ember cracks across its body, horns curling backward, breathing fire pose`
- **Animations:** Idle / Move / NormalAttack(爪攻撃/短い火炎) / Skill=AreaDamage(範囲火炎ブレス) / Death(崩れ落ちる)

### ChimeraLord (★5, Monster/Human/Hero/Warrior)
- **Prompt:** `...a towering chimera lord, hybrid creature with a muscular humanoid torso, a lion's mane, dragon-scaled arms, and goat horns, wielding a massive war-hammer, ornate dark armor plating, imposing legendary boss-like design`
- **Animations:** Idle / Move / NormalAttack(大槌の一撃) / Skill=AreaDamage(地面叩きつけ+咆哮の範囲攻撃) / Death(崩れ落ちる)
- 補足: 最上位ユニットなので、他ユニットよりディテール/エフェクトを豪華にしても良い。

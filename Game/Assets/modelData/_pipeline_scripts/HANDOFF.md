# 引継ぎメモ: ユニット3Dモデル準備タスク

作成日: 2026-08-25。Notionタスク「ユニットの3Dモデル準備(制作・調達)」(期限8/27)の作業引継ぎ。

## タスクの背景

`Game/UnitDef.h`にモデル参照フィールドが無く、`Game/Assets/modelData/`にSlime等のオリジナルユニットモデルが存在しなかった(unityChanのサンプル資産のみ)。全18ユニット分の3DモデルをMeshyAIで生成し、リギング・アニメーション付与・エンジン用ファイル(.tkm/.tks/.tka)への変換までを行うタスク。

## 今回たどり着いたパイプライン(GUI操作なしで自動化済み)

1. **MeshyAIでテキスト→3D生成**(ユーザー側で実施済み)。プロンプトは`Game/Assets/modelData/UnitModelPrompts.md`参照。
2. **Meshy自体のAuto-Rig機能は失敗**(デフォルメ体型・非T字ポーズのため)。
3. 代わりに**Blenderをコマンドライン(`--background --python`)で操作し、自動リギング+5アニメーション(Idle/Move/NormalAttack/Skill/Death)を生成**。
   - メッシュの実頂点分布から肩・肘・手・股関節・膝・足首の位置をヒューリスティックで推定してボーンを配置(`_pipeline_scripts/rig_and_animate_unit.py`)
   - Blenderの自動ウェイト(Heat diffusion)でスキニング
   - ハマった罠: 足首ボーンが短すぎるとウェイト計算で上のボーンに頂点を取られて足が千切れる → 足首ボーンを地面まで伸ばすことで解決済み(スクリプト内に反映済み)
   - ハマった罠: Blenderは保存時に参照されていないActionをガベージコレクションする。`use_fake_user = True`を必ず立てること(`reanimate_and_export.py`で修正済み)
4. **1ユニット5アクション分を個別のFBXとして書き出す**(1本のFBXに全アクションを詰めるとフレーム境界があいまいになるため、必ずアクション毎に別ファイルにする。`export_actions_separately.py`/`reanimate_and_export.py`参照)
5. **3ds Max 2026を`3dsmaxbatch.exe`でヘッドレス実行**し、FBXをインポート→`DirectX_9_Shader`マテリアル(`tools/3dsMaxShader/k2EngineShader.fx`)を割り当て→スタジオ標準の`tkmExporter.ms`/`tksExporter.ms`を使って`.tkm`/`.tks`/`.tka`を出力(`_pipeline_scripts/export_unit_action.ms`)
   - `tkmExporter.ms`の公開関数`SaveTkm`はダイアログを強制的に開く作りなのでバッチ実行不可。`tkmExporter_batch.ms`はその1点だけ改変したコピー(それ以外はオリジナルのロジックをそのまま流用)
   - `3dsmaxbatch.exe`の標準出力はShift-JISでエンコードされるので、`iconv -f SHIFT-JIS -t UTF-8`で読む必要がある
   - `-mxsString "key:value"`で渡した値は`maxOps.mxsCmdLineArgs[#key]`(Name型のキー)で受け取る。文字列キーでは失敗する
6. **アルベド/法線/メタリックラフネスのPNGをDDSに変換**(`png_to_dds.py`、Blenderでpixelを読み取り自前でDX10形式DDSヘッダを書き出す)。エンジンの`TkmFile.cpp`はtkmに書かれたファイル名の拡張子を強制的に`.dds`に置き換えて同じディレクトリから探すので、`{Unit}_albedo.dds`のように**`.tkm`と同じフォルダ・同じベース名**で置く必要がある。

## 進捗状況

- **リグ・アニメーション付与が完了(12体)**: Goblin, Swordsman, Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord
  - リグ・アニメーション付きBlenderファイルは`_pipeline_scripts/rigged_blends/{Unit}_rigged.blend`に保存済み(スクラッチパッドは消える可能性があるためプロジェクト内に退避済み)
  - **実際にゲームの`Assets/modelData/`に配置済みなのはGoblinのみ**(`Goblin.tkm`, `Goblin.tks`, `Goblin_{Idle,Move,NormalAttack,Skill,Death}.tka`, `Goblin_{albedo,normal,metallicRoughness}.dds`)。他11体は`rigged_blends/`のBlenderファイルから`reanimate_and_export.py`→3ds Maxエクスポートの手順を再実行すれば同様に出せる。
  - **ただし★下記「新たなブロッカー」参照。この12体はテクスチャ自体が壊れているため、リグ・アニメーションが正しくても見た目としては使い物にならない。**
- **未着手(6体、非人型)**: Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake — 四足・非人型のため今回のBlender自動リグ手法がそのまま使えない可能性が高い。当初の方針(`UnitModelPrompts.md`参照)では、締切を考慮して今回は骨なし静的メッシュ+コード側の疑似アニメーションで妥協する案を提示していた。
- **素材一式**: `Game/Assets/modelData/_meshy_raw/{UnitName}/`に18体全ての生FBX+テクスチャを整理済み(zipは同ディレクトリに残置)

## 表示確認ブロッカー → 2026-08-25 解決済み

`Game::Render(RenderContext& rc)`が`m_hexGridRenderer.Draw(rc, ...)`しか呼んでおらず、`m_testModelRender.Draw(rc)`の呼び出しが漏れていたのが原因だった。

`ModelRender`は`Update()`(アニメーション進行・ワールド行列更新)と`Draw(rc)`(ビューフラスタムカリング判定の上で`g_renderingEngine->AddRenderObject(this)`によりそのフレームの描画キューに登録)が分離された設計になっており、**`Draw(rc)`を毎フレーム呼ばない限りモデルは一切描画キューに載らない**(`ModelRender.cpp`450行目`ModelRender::Draw`参照)。`Game.cpp::Update()`には`m_testModelRender.Update()`を追加済みだったが、`Render()`側への追加が漏れていたため、Goblinもunitychanも「読み込みはできているが描画キューに一度も登録されない」状態だった。`Game.cpp::Render()`に`m_testModelRender.Draw(rc);`を追加してビルド確認済み。F5実行でGoblinが画面に表示されることを確認できた。

前セッションで疑っていた「`TkmFile::Load`内で`filePath`がNULLになる」現象は、狙い通り**MSVCデバッガの表示アーティファクト(関数プロローグ完了前にブレークして見た目上NULLに見えるだけ)**であり、Draw(rc)呼び出し漏れとは無関係な的外れな観測だったことが確定した(実際にfopenは成功していた)。この件は解決済みなので以後気にしなくてよい。

スケールについても切り分け済み: `SetTRS`のscale値`50.0`はhexSize(=50)を基準にした値だったが、実際のモデル単位に対しては大きすぎ、カメラがモデルの内部にめり込んで画面全体が「粉々」に見える状態になっていた(実際は単なるスケール過大)。`10.0`程度で1ヘックスマスに収まるサイズになることを確認した。最終的な適正値は非人型ユニットも含めて後で統一的に決める必要がある。

## 新たなブロッカー(★今回最大の発見。2026-08-25)

Draw(rc)追加後、Goblinは画面に表示されたが**表面が「粉々に砕けたガラス」のような、鋭い断片が乱反射する見た目になり、ゴブリンのシルエット以外は原形を留めていない**状態だった(スクリーンショット参照)。

### 切り分け経緯

1. スケールが原因かと疑い`1.0`まで下げたところ、小さいながらも「粉々」の塊のまま → スケール要因ではなく形状/シェーディングの問題と判明。
2. アニメーション(Move)が原因かと疑い、`PlayAnimation`呼び出しをコメントアウトしてIdle(デフォルトの0番)で確認 → **Idleでも全く同じ形状**。アニメーション固有の問題ではないと判明。
3. **Blenderで`rigged_blends/Goblin_rigged.blend`を直接ヘッドレスレンダリングして確認**(`--background --python`でEeveeレンダー) → **バインドポーズ・Idle・Move全て正常な、綺麗な緑色ゴブリンが表示された**。→ リグ・ウェイト・メッシュ自体には一切問題がないことが確定。
4. `reanimate_and_export.py`と同じ手順でFBXを再エクスポートし、新規Blenderシーンにインポートしなおして確認 → **同じ「粉々」形状が再現**(ただしテクスチャパス未解決のためマゼンタ表示)。アニメーションを一切含まない静止バインドポーズのみのFBXでも同様に再現。→ 一見FBXエクスポート/インポートが怪しく見えたが、これはテスト方法の不備だった(後述)。
5. ボーンのrest行列(`bone.matrix_local`)、頂点座標、頂点法線、頂点グループ(ボーンウェイト)を元ファイルとFBXラウンドトリップ後とで全て比較 → **全て完全に一致**(浮動小数点の誤差レベルの差異のみ)。スキニング/ジオメトリは一切壊れていないことが確定。
6. Armatureモディファイアを完全に外してポーズ変形なしの生メッシュだけをレンダリングしても**同じ「粉々」形状が再現** → **スキニング/リグは完全に無関係**と最終確定。ジオメトリそのもの(頂点位置)は正しいが、シェーディング(法線マップ)が壊れているという方向に絞り込んだ。

### 根本原因: MeshyAIが出力した元テクスチャそのものが壊れている

`Game/Assets/modelData/_meshy_raw/Goblin/Goblin_albedo.png`と`Goblin_normal.png`を直接開いて確認したところ、**どちらも「UVアトラスが完全にシャッフルされた」状態の画像だった**。ゴブリンの目や歯、体のパーツと思われる断片が、本来あるべき連続した1枚の展開図ではなく、無関係な位置にバラバラに散らばって配置されている(fbxやtkm変換の話ではなく、PNGファイルの中身そのものが最初からこうなっている)。

これがまさに画面で見えていた「粉々のガラス片」の正体: アルベドは元々カモフラージュ柄で色のランダム性が目立ちにくいため一見「camo柄の変なゴブリン」程度に見えるが、法線マップは空間的な連続性が完全に失われているため、陰影計算(`RenderToGBufferFor3DModel.fx`の`GetNormalFromNormalMap`、ごく標準的なタンジェント空間デコード)の結果が画素ごとにバラバラな方向を向き、あの鋭い乱反射・シャッタード状の見た目になっていた。

**さらに`_meshy_raw/Swordsman/Swordsman_albedo.png`も確認したところ、全く同じ「UVアトラスシャッフル」症状が確認できた。** → これはGoblin固有の生成ミスではなく、**このセッションでMeshyAIから出力した18体分のテクスチャ全てに共通する、根本的な生成/エクスポート不良の可能性が高い**。つまり「完了」と分類されていた12体も、リグ・アニメーションは問題なくてもテクスチャ側がこのままでは全滅の可能性がある。

**結論: Blender側のリグ・スキニング・FBX変換・3dsMaxのtkmExporter・エンジン側のシェーダーは全て無罪。原因はMeshyAIが生成/書き出ししたテクスチャ画像そのもの(UVアトラスの配置が壊れている)。** ユーザーが当初指摘していた「tkExporterのテクスチャ反転バグでは?」という仮説は方向性としては正しかった(テクスチャが疑わしいという直感は正解)が、実際は反転ではなく、UV配置そのものの破損だった。

## 全18体テクスチャ全数確認 → 2026-08-25 完了(18/18全滅を確定)

`_meshy_raw/{Unit}/{Unit}_albedo.png`を18体全てについて目視確認した(Goblin, Swordsman, Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord, Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake)。**全18体が同一の「UVアトラスシャッフル」症状**であることを確定した。人型・非人型、リグ済み/未着手を問わず全滅であり、この後の対応(再ダウンロード確認・再生成)は18体全部を対象に検討する必要がある。もはや「数体サンプル確認」のフェーズは不要。

## Goblin再ダウンロード検証 → 2026-08-25 完了(ダウンロード破損ではないことを確定)

ユーザーがMeshyAI管理画面からGoblinを再ダウンロードし、`_meshy_raw/ゴブリン/Meshy_AI_stylized_toon_shaded__0825014407_texture*.png`として配置。albedo(`..._texture.png`)・normal(`..._texture_normal.png`)とも目視確認したところ、**既存の`Goblin_albedo.png`と全く同じ「UVアトラスシャッフル」症状が再現した**(normalマップも同様に空間的連続性のないパッチ状)。

→ **ダウンロード時の破損ではなく、MeshyAI側の生成/エクスポート自体が壊れていることが確定**。次はMeshyのテクスチャベイク設定(リトポロジー有無、UVアトラスのオプションなど)を変更しての再生成を試すか、それでも直らなければ別手段(手動UV展開・別ツールでのベイク等)を検討する段階。

## ★★★ 根本原因の訂正 → 2026-08-25 解決済み(最重要更新)

**上記の「MeshyAIが出力したテクスチャそのものが壊れている」という結論は誤りだった。** テクスチャ・UVは最初から一切壊れておらず、真の原因は**`png_to_dds.py`のY軸(V軸)反転処理のバグ**だった。

### 発見の経緯

ユーザーから「テクスチャのベイクをBlenderでやってほしい」と依頼され、Blenderで`_meshy_raw/ゴブリン/`の再ダウンロードFBXをUV展開図ごとEeveeでレンダリングしたところ、**UVがどれだけ細切れでも、3Dモデルにマッピングした状態では完全に正常な緑色のゴブリンが表示された**(`goblin_render_check.png`相当)。これで「テクスチャ画像そのものが壊れている」という前セッションの結論に矛盾が生じ、再調査した。

切り分け手順:
1. `Goblin_rigged.blend`(リグ済み、頂点座標が再ダウンロードFBXと完全一致することを確認済み)に対し、Smart UV Projectで新しいUV(まとまった島)を作成し、そこへ元テクスチャをBlenderのEmitベイクで再投影 → Blender上のレンダーでは正常。
2. それを3ds Max経由で`.tkm`/`.dds`化してゲーム内表示 → **やはり「粉々」のまま**(黒い塗りつぶし+三角形単位の急な明暗)。UV再ベイクだけでは直らなかった。
3. 法線マップを`(128,128,255)`のフラット(ニュートラル)画像に差し替えても症状は変化せず → **法線マップ/タンジェント計算は無罪と確定**。
4. albedoの`png_to_dds.py`変換で行っている「Blenderのrow0(下)→DDSのrow0(上)」というY軸反転処理を**外して**(反転なしで)DDS化 → **ゲーム内で正常な緑色のゴブリンが表示された**。法線マップとmetallicRoughnessも同様に反転なしで変換し、フル品質で最終確認済み。

### 結論

`png_to_dds.py`のY軸反転(`src_y = h - 1 - y`)が誤りだった。3ds Maxの`tkmExporter_batch.ms`は`gettvert`で取得したUV.vをそのまま(反転せずに)書き出しており、3ds MaxのV軸はBlender/OpenGL同様V=0が下。DDS側で追加のY反転をかけると、書き込まれたUV座標と実際の画像データの行が食い違い、サンプリング位置が別のUV島や余白(黒)にズレる。UVアイランドが細かければ細かいほど、このズレの影響で「粉々のガラス片」のような見た目になっていた。**GoblinもSwordsmanも他16体も、テクスチャ生成自体は最初から正常だった可能性が高い。**

ユーザーが当初指摘していた「tkExporterのテクスチャ反転バグでは?」という仮説がそのまま正解だった。

`png_to_dds.py`本体は修正済み(Y軸反転処理を削除し、原因をコメントで明記)。

### Goblinの対応状況

- `rigged_blends/Goblin_rigged.blend`: Smart UV Projectで新UV(`island_margin=0.05`)を作成し、元テクスチャ4種(albedo/normal/metallic/roughness, `margin=32`でベイク)を再投影する形にBlenderファイル自体を更新済み。バックアップは`rigged_blends/Goblin_rigged.blend.bak`。
  - **UV再ベイク自体は不要だった可能性が高い**(反転バグを直すだけで、元の細切れUVのテクスチャでも直った可能性がある)が、結果的にUVアイランドが以前よりまとまったので、そのまま採用して問題ない。
- 5アクション(Idle/Move/NormalAttack/Skill/Death)全て`.tkm`/`.tks`/`.tka`を作り直し、`Game/Assets/modelData/`に配置済み。ビルド・実行してゲーム内で全アクション相当のIdle表示を確認済み(正常な緑色ゴブリンが表示される)。
  - 配置前の旧ファイル一式は`Game/Assets/modelData/_goblin_backup_before_rebake/`にバックアップ済み。
- `export_unit_action.ms`内の`scriptsDir`(旧セッションのスクラッチパスがハードコードされていた)を`Game/Assets/modelData/_pipeline_scripts/`に修正済み。次セッション以降もこのまま使える。

## Swordsmanで検証 → 2026-08-25 完了(UV再ベイク不要と確定)

Swordsmanで「UV再ベイクをせず、既存の`_meshy_raw/Swordsman/Swordsman_{albedo,normal,metallicRoughness}.png`をそのまま修正済み`png_to_dds.py`でDDS化するだけ」の最小構成を試した。`rigged_blends/Swordsman_rigged.blend`には現状Idleアクションしかない(他4アクションは未作成)ため、Idleのみで検証。

手順: `export_actions_separately.py`でIdle FBXを書き出し→テクスチャ3種を`Goblin_*.png`という名前でコピー(3ds MaxのtkmExporterはalbedoPath等に渡したファイル名のベース名をtkmに焼き込むため、動作確認用に一時的にGoblin名義で通した)→`export_unit_action.ms`で3ds Max変換→`png_to_dds.py`(修正済み)でDDS化→`Game/Assets/modelData/`のGoblin一式に一時的に上書き→ビルド済みGame.exeで表示確認。

**結果: 一発で正常なSwordsman(オレンジ髪・鎧・剣・緑マント)が表示された。** UV再ベイクは一切不要で、既存のUV・既存のテクスチャのまま、DDS変換のY軸反転を外すだけで直ることが確定した。確認後、Goblin一式は正しい最終版(`goblin_tkm_final`由来、md5一致確認済み)に復元済み。

→ **残り16体(Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord, Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake)も、Goblinで行ったBlenderでのUV再ベイク作業は不要。** 各ユニットについて「3ds MaxでUV/テクスチャそのまま(albedoPath等を修正済み`png_to_dds.py`でDDS化した`_meshy_raw/{Unit}/`のPNGに向ける)→エクスポート」するだけで直る見込みが高い。

## 次のセッションでまずやること(優先度順) → 2026-08-25 追記: 量産パイプライン確立・Priest/Archer完了、Knight処理中に中断

### 今回セッションで分かったこと・やったこと

- **リグ済み10体(Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord)は全て`{Unit}_Idle`アクションしか無いことを確認済み**(Swordsmanと同じ状態。`bpy.data.actions`で確認済み)。ボーン名(`Hip_L`/`Hip_R`/`Knee_L`/`Knee_R`/`Shoulder_L`/`Shoulder_R`/`Elbow_L`/`Elbow_R`/`Chest`/`Hips`等)は全ユニット共通なので、**`reanimate_and_export.py <blend> <outDir> <UnitName>`をそのまま実行すれば5アクション全て(Idle/Move/NormalAttack/Skill/Death、フレーム範囲はそれぞれ0-24/0-24/0-14/0-18/0-30固定)を再生成しつつFBXも書き出せる**ことを確認済み(Priestで実施・成功)。
- **3dsmaxbatch呼び出しの重要な落とし穴を発見・解決済み**:
  1. `3dsmaxbatch.exe`は`<script_file>`を**最初の引数**に置く必要がある(`-mxsString`等のオプションより後ろに置くと即座に失敗し、tkm/tkaが一切生成されない。exit code 126)。
  2. `-mxsString "key:value"`の`value`にWindows形式のバックスラッシュパス(`C:\foo\bar`)を渡すと、MAXScript側で`g_useNormalMap`等のプロパティが見つからないというエラーで失敗する(`DirectX_9_Shader`のeffectFile読み込みが実質失敗する)。**必ずフォワードスラッシュ(`C:/foo/bar`)で渡すこと。**
  - この2点を修正した結果、Priestで一発成功(ビルド→実行→スクリーンショットで綺麗な白ローブ+杖のPriestを確認済み)。
- **量産バッチスクリプトを作成・実行**: `_pipeline_scripts/`ではなくスクラッチパッド(`.../scratchpad/batch_convert.ps1`、セッション終了で消える一時ファイル)に、上記知見を反映した「reanimate_and_export.py → 3dsmaxbatch×5アクション → png_to_dds.py×3テクスチャ → Game/Assets/modelData/へコピー」を9体分(Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord)ループするスクリプトを書いて実行した。**このスクリプト自体は次セッションには残っていない(スクラッチパッドは消える)ので、上記の知見(スクリプト起動順・パス区切り)を元に次セッションで再度組み立てること。**
- **Warlordのみ`_meshy_raw/Warlord/`に`Warlord_metallicRoughness.png`が無く`Warlord_metallic.png`しか無い**ことを確認済み。Warlordの`metallicPath`には`Warlord_metallic.png`を指定すること。他9体は`{Unit}_metallicRoughness.png`で揃っている。
- **ユーザーからPC電源オフのため中断依頼があり、実行中だったバッチ処理(Knightの3dsmax変換中)を安全に停止した。** `Game/Assets/modelData/`への書き込みは各ユニットの全ステップ成功後にまとめてコピーする設計だったため、**中断時点でKnight以降のファイルが`Game/Assets/modelData/`に中途半端な状態で残ることは無い**(確認済み)。

### 現在の完了状況(2026-08-26セッションで更新、18体中)

- **完了・配置済み(12体、人型は全て完了)**: Goblin, Priest, Archer(前セッションまで) + **Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord(2026-08-26セッションで量産パイプライン一括実行し追加)**
  - Swordsmanは前セッションでは検証用にGoblin名義でしか変換しておらず、`Game/Assets/modelData/`に`Swordsman.*`として本配置されていなかった欠落があったため、今回まとめて本配置した。
  - 9体とも`{Unit}.tkm`, `{Unit}.tks`, `{Unit}_{Idle,Move,NormalAttack,Skill,Death}.tka`, `{Unit}_{albedo,normal,metallicRoughness}.dds`の計10ファイルが`Game/Assets/modelData/`に存在し、非ゼロバイトであることを確認済み(全ファイルサイズチェック済み)。
  - **Warlordのみ画面表示確認済み**(`Game.cpp`の検証用コードを一時的にWarlordに差し替えてビルド・実行→エラーダイアログ「テクスチャのロードに失敗しました。Assets/modelData/Warlord_metallic.dds」が発生→原因判明・修正済み(下記)→再実行して正常表示を確認。確認後は`Game.cpp`をGoblinに戻し再ビルド済み)。
  - **Warlord固有のバグを発見・修正済み**: Warlordだけ`_meshy_raw/Warlord/`に`Warlord_metallicRoughness.png`が無く`Warlord_metallic.png`を使ったため、3ds Maxの`tkmExporter`がmetallicテクスチャのパスとして渡した`Warlord_metallic.png`のベース名(`Warlord_metallic`)をtkmファイルに焼き込んだ。一方DDS変換側は他ユニットと揃えて機械的に`Warlord_metallicRoughness.dds`という名前で出力していたため、tkmが参照するファイル名(`Warlord_metallic.dds`)と実際に存在するファイル名(`Warlord_metallicRoughness.dds`)が食い違い、ロードエラーになった。**`Warlord_metallicRoughness.dds`を`Warlord_metallic.dds`という名前でコピーして追加することで解決済み**(中身は同じテクスチャ、ファイル名だけの問題)。両方のファイルが`Game/Assets/modelData/`に存在する。→ 次に`UnitDef`にモデルパスを持たせる際、Warlordだけmetallicテクスチャのファイル名規則が`_metallic.dds`である点に注意(他11体は`_metallicRoughness.dds`)。
  - Warlordの表示自体は緑肌・鎧・目とも正常な陰影で、「粉々」バグの再発は無いことを確認した。ただし検証用コードのスケール(`10.0`)がWarlordのメッシュサイズに対して大きすぎ、カメラがモデルの頭部にめり込んで顔全体は見えなかった(スケール調整は後日の統合作業でユニットごとに行う想定であり、今回は見送った)。
  - **残り8体(Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, NightBlade, ChimeraLord)は画面表示未確認**(ファイル一式が揃っていること、ログにエラーが出ていないことは確認済みだが、実際にゲーム内で表示して目で見た確認はしていない)。
- **未着手(残り6体、非人型)**: Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake — 四足・非人型のため今回のBlender自動リグ手法がそのまま使えない可能性が高い。

### 次セッションでやること

1. **(任意・推奨)今回一括変換した残り8体(Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, NightBlade, ChimeraLord)を実際にビルド・実行して画面表示確認する。** 手順は下記「表示確認の手順」参照。`Game.cpp`の`m_testModelRender`が読み込むファイル名を対象ユニット名に一時的に差し替えれば確認できる(確認後はGoblinに戻すこと)。Warlordで見つかったような「tkmが参照するテクスチャ名と実ファイル名の不一致」がまだ他の個体で潜んでいる可能性はゼロではない(ただしWarlordはmetallicRoughness.pngが無いという特殊ケースだったため、他11体では起きない可能性が高い)。
2. **最優先: 残り6体(非人型: Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake)のリグ手法を決める。** 四足用リグ、または当初案(骨なし静的メッシュ+コード側疑似アニメーション)のどちらで行くかを検討し、決まれば`rig_and_animate_unit.py`相当のスクリプトを作るか、既存スクリプトを流用する。テクスチャ変換(`png_to_dds.py`)自体は人型と同じ手順で問題ないはず。
3. 全18体で表示確認が取れたら、`UnitDef.h`/`UnitDatabase.cpp`にモデル・アニメーションのファイルパス参照フィールドを追加し、`Game.cpp`の検証用コード(`m_testModelRender`関連、`[検証用]`コメント箇所)を本実装に置き換える。

## 現在のGame.cpp/Game.hの状態

`Game.h`に検証用メンバーを追加済み(`ModelRender m_testModelRender; AnimationClip m_testAnimClips[5];`)。`Game.cpp::Start()`冒頭・`Update()`冒頭・`Render()`内に`[検証用]`コメント付きで呼び出しを追加済み(スケールは`10.0`、`PlayAnimation`は切り分けのためコメントアウト中)。ビルドは通ることを確認済み(v145ツールセット = `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\...`を使用。通常のVS2022(v143)ではビルドできないので注意)。テクスチャ問題が解決するまでは、この検証用コードはそのまま残しておいてよい(削除は本実装置き換え時でよい)。

## 次セッション用ツールパス・実務メモ(2026-08-25時点で確認済み)

このマシンでの各ツールの絶対パス(`find`し直さなくてよい):

- Blender 4.5: `C:\Program Files\Blender Foundation\Blender 4.5\blender.exe`(`--background --python <script>.py -- <args>`で自動化)
- 3ds Max 2026 バッチ: `C:\Program Files\Autodesk\3ds Max 2026\3dsmaxbatch.exe`(`-mxsString "key:value"`で`export_unit_action.ms`に引数を渡す。1回の呼び出しにつき1ユニット1アクション。標準出力はUTF-16LE寄りの混在エンコードで文字化けしやすいので、成否は出力ファイルの有無で判定するのが確実)
- MSBuild(v145, VS 2026 Community): `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`。**Git Bashで`/p:Configuration=Debug`のように`/`始まりの引数を渡すとパス展開されて壊れるので、ビルドはPowerShellツールから実行すること**(`& "...\MSBuild.exe" "Game.sln" /p:Configuration=Debug /p:Platform=x64 /m`)。

### ユニット1体分の変換パイプライン(UV再ベイク不要、Swordsmanで確定済みの最短ルート)

1. `blender.exe --background --python export_actions_separately.py -- <blend> <outDir> <UnitName>` でアクション別FBXを書き出す(`rigged_blends/{Unit}_rigged.blend`のアクション名は`{Unit}_{ActionName}`。実際に何アクション揃っているかは`bpy.data.actions`を見るまで不明 — SwordsmanはIdleのみだった)。
2. アクションごとに`3dsmaxbatch.exe`で`export_unit_action.ms`を実行。`albedoPath`/`normalPath`/`metallicPath`は`_meshy_raw/{Unit}/{Unit}_{albedo,normal,metallicRoughness}.png`をそのまま渡してよい(tkmにはこのパスのベース名がテクスチャ参照として焼き込まれるので、最終配置先のファイル名と一致させること)。`tkmOutPath`は全アクションで同じ`{Unit}.tkm`、`tkaOutPath`だけアクションごとに変える。
3. `blender.exe --background --python png_to_dds.py -- <in.png> <out.dds>`(修正済み、反転なし)でalbedo/normal/metallicRoughnessをDDS化。
4. `Game/Assets/modelData/`に`{Unit}.tkm`, `{Unit}.tks`, `{Unit}_{Action}.tka`×N, `{Unit}_albedo.dds`, `{Unit}_normal.dds`, `{Unit}_metallicRoughness.dds`をコピー。

### 表示確認の手順(GUIレスで実施)

1. PowerShellツールでビルド(上記)。
2. `Start-Process -FilePath "Game\x64\Debug\Game.exe" -WorkingDirectory "Game\x64\Debug"の親である"Game"フォルダ -PassThru`でプロセス起動、`Start-Sleep`で数秒待つ。
3. **起動直後のウィンドウは最小化されていることが多い** — `user32.dll`の`IsIconic`/`ShowWindow(hwnd, 9)`(SW_RESTORE)で復元してから`GetWindowRect`+`Graphics.CopyFromScreen`でスクリーンショットを撮ること(最小化のままキャプチャすると数十px四方の空画像になる)。
4. `EnumChildWindows`+`GetWindowText`で子ウィンドウ(エラーダイアログの「OK」ボタンやメッセージ文字列)を読めば、`テクスチャのロードに失敗しました。Assets/modelData/xxx.dds`のようなエラー内容を画面を見ずに確認できる。ウィンドウタイトルが「エラー」になっていたら要注意。`FindWindow(null, "エラー")`は0を返すことがある(所有プロセスが違う「エラー」ウィンドウが他にもあるため)ので、`EnumWindows`+`GetWindowThreadProcessId`で対象PIDのトップレベルウィンドウに絞ってから探すこと。
5. **(2026-08-26発見の罠)** ただの`ShowWindow`+`SetForegroundWindow`では前面化が失敗することがあり、その状態で`CopyFromScreen`すると同じ画面座標に重なっている**別のウィンドウ(全く無関係な常駐アプリ等)を誤ってキャプチャしてしまう**(サイズ・座標は取得できるのでエラーにはならず、気付きにくい)。確実に前面化するには、`GetForegroundWindow`で現在のフォアグラウンドのスレッドIDを取得→`AttachThreadInput`で自スレッドと関連付け→`SetWindowPos`でHWND_TOPMOST→`BringWindowToTop`→`SetForegroundWindow`→スクリーンショット→`AttachThreadInput`解除→`SetWindowPos`でHWND_NOTOPMOST(貼り付いたままにしない)、という手順を踏むこと。スクリーンショット取得後は`GetForegroundWindow()`で実際に狙ったhwndになっているか、あるいは撮れた画像自体を目視して対象アプリの内容(FPS表示など)が写っているか必ず確認する。
6. 確認後は`Stop-Process -Id <pid> -Force`で終了する(次のビルド/差し替えの前に必ず終了させること。実行中はdds/tkmファイルがロックされないが、ファイル差し替え後の再読み込みは行われないため再起動が必要)。

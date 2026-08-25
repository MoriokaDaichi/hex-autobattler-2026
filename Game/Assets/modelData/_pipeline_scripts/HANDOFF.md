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

## 次のセッションでまずやること(優先度順)

1. **最優先: MeshyAI側のテクスチャ出力を疑う。** 具体的には
   - 他の生成済みユニット(Priest, Archer等)の`_meshy_raw/{Unit}/{Unit}_albedo.png`も同様に壊れていないか数体サンプル確認する。
   - MeshyAIの管理画面/ダウンロード履歴から、Goblin・Swordsman等を**再ダウンロード**して同じ壊れ方が再現するか確認する(ダウンロード時の破損か、Meshy側の生成自体がこうなっているのかの切り分け)。
   - Meshyのテクスチャベイク設定(リトポロジー有無、UVアトラスのオプションなど)を変更して再生成することで直るかどうか試す。
   - 直らない場合、18体全部の作り直しが必要になる可能性があるため、早めにこの結論をNotionタスクの期限(8/27)に照らして報告する。
2. テクスチャが正しく生成できることを確認できたら、Goblinで`png_to_dds.py`から`.tkm`配置までの手順を再実行し、ゲーム内で正常に表示されることを最終確認する。
3. 表示確認が取れたら、残り11体(rigged_blends配下)のtkm/tks/tka出力→`Game/Assets/modelData/`配置。
4. 非人型6体(Slime/Direwolf/Griffin/Behemoth/YoungDragon/FlameDrake)の対応方針を決めて着手。
5. `UnitDef.h`/`UnitDatabase.cpp`にモデル・アニメーションのファイルパス参照フィールドを追加し、`Game.cpp`の検証用コード(`m_testModelRender`関連、`[検証用]`コメント箇所)を本実装に置き換える。

## 現在のGame.cpp/Game.hの状態

`Game.h`に検証用メンバーを追加済み(`ModelRender m_testModelRender; AnimationClip m_testAnimClips[5];`)。`Game.cpp::Start()`冒頭・`Update()`冒頭・`Render()`内に`[検証用]`コメント付きで呼び出しを追加済み(スケールは`10.0`、`PlayAnimation`は切り分けのためコメントアウト中)。ビルドは通ることを確認済み(v145ツールセット = `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\...`を使用。通常のVS2022(v143)ではビルドできないので注意)。テクスチャ問題が解決するまでは、この検証用コードはそのまま残しておいてよい(削除は本実装置き換え時でよい)。

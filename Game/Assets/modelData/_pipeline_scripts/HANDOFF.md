# 引継ぎメモ: ユニット3Dモデル準備タスク

作成日: 2026-08-25、最終更新: 2026-08-26(セッション2)。Notionタスク「ユニットの3Dモデル準備(制作・調達)」(期限8/27)。

このファイルは次セッション引継ぎ用で、現状・確立済み手順・次にやることのみを載せる。過去の試行錯誤や誤診断の経緯は[WORKLOG.md](WORKLOG.md)に分離済み。

## タスク概要

全18ユニットの3DモデルをMeshyAIで生成し、リギング・アニメーション付与・エンジン用ファイル(.tkm/.tks/.tka)への変換を行う。`Game/UnitDef.h`にはまだモデル参照フィールドが無いため、表示確認は`Game.cpp`内の検証用コード(`m_testModelRender`)で行っている。

## 確立済みパイプライン(1ユニットあたりの最短手順)

人型・四足共通の流れ:

1. MeshyAIでテキスト→3D生成(ユーザー側で実施済み、プロンプトは`UnitModelPrompts.md`)。
2. Blenderをヘッドレス実行して自動リギング+5アニメーション(Idle/Move/NormalAttack/Skill/Death、フレーム範囲はそれぞれ0-24/0-24/0-14/0-18/0-30固定)を生成。
   - **人型12体**: `rig_and_animate_unit.py`。ボーン名共通(`Hip_L`/`Hip_R`/`Knee_L`/`Knee_R`/`Shoulder_L`/`Shoulder_R`/`Elbow_L`/`Elbow_R`/`Chest`/`Hips`等)なので初回リグ後は`reanimate_and_export.py <blend> <outDir> <UnitName>`で5アクション一括生成・FBX書き出しができる。
   - **四足5体(Direwolf/Griffin/Behemoth/YoungDragon/FlameDrake)**: `rig_and_animate_quadruped.py <fbx> <outBlend> <outFbx> <outDir> <UnitName> <head_sign> <has_wings(0/1)>`。ボーン名共通(`Hips/Spine/Chest/Neck/Head`, `TailRoot/Mid/Tip`, `FrontShoulder/Elbow/Ankle_L/R`, `BackHip/Knee/Ankle_L/R`, 翼ありのみ`WingRoot/Mid/Tip_L/R`)。5体とも`head_sign=1.0`(頭がY+側)で統一できた。詳細は下記「四足リグの詳細」参照。
3. `export_actions_separately.py`でアクションごとに個別FBXを書き出す(1本にまとめるとフレーム境界が曖昧になるため。人型・四足どちらもボーン名非依存で共通利用可)。
4. `3dsmaxbatch.exe`で`export_unit_action.ms`を実行しFBX→`.tkm`/`.tks`/`.tka`に変換。
   - `albedoPath`/`normalPath`/`metallicPath`は`_meshy_raw/{Unit}/{Unit}_{albedo,normal,metallicRoughness}.png`のファイルそのものでよい(**UV再ベイクは不要**)が、**必ず絶対パスで渡すこと**(相対パスだと3ds Max側でファイルが解決できずtkmにテクスチャが一切埋め込まれず、ゲーム内で均一な赤色になる。下記「解決済みの罠」参照)。
   - `shaderFxPath`は`tools/3dsMaxShader/k2EngineShader.fx`(絶対パス)を指定する。
   - tkmにはこのパスのベース名がテクスチャ参照として焼き込まれるので、最終配置先のファイル名と一致させること。
5. `png_to_dds.py`(修正済み、Y軸反転なし)でalbedo/normal/metallicRoughnessをDDS化。
6. `Game/Assets/modelData/`に`{Unit}.tkm`, `{Unit}.tks`, `{Unit}_{Action}.tka`×5, `{Unit}_{albedo,normal,metallicRoughness}.dds`をコピー。

### 四足リグの詳細(`rig_and_animate_quadruped.py`)

Meshy出力5体は座標系が統一されており、Y軸=前後(進行)方向、Z軸=上下、X軸=左右。Y軸に沿ったバンド分割で胴体の断面積プロファイルを見て、胴体中心→(頭側)くびれ→再拡大を首・頭、→(尾側)急激な先細りを尻尾根本として検出する(人型のneck_band検出と同じ発想)。脚は胴体下部の頂点をX・Yで4クラスタ(前後左右)に分けて検出。翼(Griffin/YoungDragon/FlameDrakeのみ`has_wings=1`)は胴体上部かつ体幹幅を大きく超える頂点群として検出。Death等で`Hips`ボーンに人型と同じ軸でX回転をかけると胴体が捻れる(四足の`Hips`はY軸方向に伸びるボーンで人型のZ軸方向と軸の意味が違うため)ので要注意。

## 解決済みの罠(結論のみ。詳しい切り分け経緯は[WORKLOG.md](WORKLOG.md)参照)

- **DDS変換のY軸反転バグ(最重要)**: `png_to_dds.py`が行っていたY軸反転が誤りだった。3ds Maxの`gettvert`はV=0が下(Blender/OpenGLと同じ)なので反転不要。反転させるとUVサンプリング位置がズレ、「粉々のガラス片」のような見た目になっていた(法線マップ・テクスチャ・リグ・スキニング自体は全て無罪)。修正済み。
- **`Draw(rc)`呼び出し漏れ**: `Game::Render()`に`m_testModelRender.Draw(rc)`が無いとモデルが描画キューに一切載らない。追加済み。
- **足首ボーンが短いとウェイト計算で上のボーンに頂点を取られる**→足首ボーンを地面まで伸ばして解決済み(`rig_and_animate_unit.py`に反映済み)。
- **Blenderが未参照Actionをガベージコレクションする**→`use_fake_user = True`で解決済み(`reanimate_and_export.py`に反映済み)。
- **`3dsmaxbatch.exe`はスクリプトファイルを最初の引数に置く必要がある**(`-mxsString`等より後ろに置くと即失敗、exit code 126)。
- **`-mxsString "key:value"`のパスはフォワードスラッシュ必須**(バックスラッシュだと`DirectX_9_Shader`のeffectFile読み込みが失敗する)。
- **Warlordのみ`_metallicRoughness.png`が無く`_metallic.png`しか無い**→tkmが参照するファイル名(`Warlord_metallic.dds`)と実ファイル名が食い違うため、`Warlord_metallicRoughness.dds`を`Warlord_metallic.dds`という名前でも複製して配置済み(他11体は`_metallicRoughness`で統一)。
- **3dsmaxbatchへのテクスチャパスは相対パスだとtkmに埋め込まれない(最重要)**→`albedoPath`/`normalPath`/`metallicPath`が相対パスだと3ds Max側で`openBitMap`が失敗しundefinedのまま扱われ、tkmのマテリアル情報(ファイル名)が空文字列で書き込まれる。ゲーム内では均一な赤色になる(エラーダイアログは出ない)。必ず絶対パス(`C:/my/...`)で渡すこと。人型12体のtkmは(理由不明だが)問題なかったことをGoblinで確認済み。
- **`shaderFxPath`に3ds Max付属`DefaultPhong.fx`は使えない**→`g_useNormalMap`等のプロパティが無くエラー。`tools/3dsMaxShader/k2EngineShader.fx`を使うこと。
- **四足の`Hips`ボーンに人型と同じ軸でDeath用のX回転をかけると胴体が捻れる**→四足の`Hips`はY軸(前後)方向に伸びるボーンで人型のZ軸方向と軸の意味が違うため。`rig_and_animate_quadruped.py`では脚を折り畳んでZ位置を沈める方式に変更済み。

## 現在の進捗状況(2026-08-26セッション2時点、18体中)

- **完了・配置済み(17体)**:
  - 人型12体: Goblin, Priest, Archer, Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord
  - 四足5体: Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake
  - 全員`.tkm`/`.tks`/`.tka`×5/`.dds`×3が`Game/Assets/modelData/`に存在し非ゼロバイトであることを確認済み。
  - **画面表示を目視確認済みなのはGoblin, Warlord(人型)、Direwolf, Griffin(四足)のみ**。残り13体はファイル一式が揃っていることのみ確認済みで、実際の表示確認はまだ。
- **要修正(1体)**: Slime — `_meshy_raw/Slime/Slime.fbx`と付属テクスチャが実際には無関係な人型キャラクター(赤マント+甲冑)のデータになっている。MeshyAI管理画面から正しいSlimeモデルを再生成/再ダウンロードする必要がある(ユーザー側作業)。再取得できれば四足リグではなく別途(元々単純形状なので骨なし静的メッシュ+スケールアニメ等でも可)対応する。
- リグ済みBlenderファイルは`_pipeline_scripts/rigged_blends/{Unit}_rigged.blend`(人型)に保存済み。四足5体のリグ済みblendは今回セッションのスクラッチパッドに生成しただけで恒久保存はしていない(再生成する場合は`rig_and_animate_quadruped.py`を再実行すれば数分で作り直せる)。素材一式(生FBX+テクスチャ)は`Game/Assets/modelData/_meshy_raw/{UnitName}/`に18体全て整理済み(Slimeのみ内容が誤り)。

## 次セッションでやること(優先度順)

1. **最優先**: Slimeの再生成/再ダウンロード(ユーザー側でMeshyAI管理画面から対応)。取得できたら四足5体と同じ変換パイプライン、または単純形状ならもっと簡易な対応で`.tkm`/`.tka`等を作る。
2. (任意・推奨)完了済みだが表示未確認の13体(人型10体: Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, NightBlade, ChimeraLord, Priest, Archer / 四足3体: Behemoth, YoungDragon, FlameDrake)を実際にビルド・実行して画面表示確認する。手順は下記「表示確認の手順」参照。`Game.cpp`の`m_testModelRender`が読み込むファイル名を対象ユニット名に一時的に差し替えれば確認できる(確認後はGoblinに戻すこと)。
3. 全18体の表示確認が取れたら、`UnitDef.h`/`UnitDatabase.cpp`にモデル・アニメーションのファイルパス参照フィールドを追加し、`Game.cpp`の検証用コード(`m_testModelRender`関連、`[検証用]`コメント箇所)を本実装に置き換える。

## Game.cpp/Game.hの現状

`Game.h`に検証用メンバー(`ModelRender m_testModelRender; AnimationClip m_testAnimClips[5];`)を追加済み。`Game.cpp::Start()`冒頭・`Update()`冒頭・`Render()`内に`[検証用]`コメント付きで呼び出しを追加済み(スケールは`10.0`、`PlayAnimation`は切り分けのためコメントアウト中)。本実装置き換え時までそのまま残しておいてよい。ビルドにはv145ツールセット(VS2026)が必須(通常のVS2022=v143ではビルド不可)。

## ツールパス・実務メモ

- Blender 4.5: `C:\Program Files\Blender Foundation\Blender 4.5\blender.exe`(`--background --python <script>.py -- <args>`で自動化)
- 3ds Max 2026 バッチ: `C:\Program Files\Autodesk\3ds Max 2026\3dsmaxbatch.exe`(`-mxsString "key:value"`で`export_unit_action.ms`に引数を渡す。1回の呼び出しにつき1ユニット1アクション。標準出力は文字化けしやすいので、成否は出力ファイルの有無で判定するのが確実)
- MSBuild(v145, VS 2026 Community): `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`。**Git Bashで`/p:Configuration=Debug`のように`/`始まりの引数を渡すとパス展開されて壊れるので、ビルドはPowerShellツールから実行すること**(`& "...\MSBuild.exe" "Game.sln" /p:Configuration=Debug /p:Platform=x64 /m`)。
- `export_unit_action.ms`内の`scriptsDir`は`Game/Assets/modelData/_pipeline_scripts/`に修正済み(旧セッションのスクラッチパスがハードコードされていた問題は解決済み)。

## 表示確認の手順(GUIレスで実施)

1. PowerShellツールでビルド(上記)。
2. `Start-Process -FilePath "Game\x64\Debug\Game.exe" -WorkingDirectory "Game" -PassThru`でプロセス起動、`Start-Sleep`で数秒待つ。
3. **起動直後のウィンドウは最小化されていることが多い** — `user32.dll`の`IsIconic`/`ShowWindow(hwnd, 9)`(SW_RESTORE)で復元してから`GetWindowRect`+`Graphics.CopyFromScreen`でスクリーンショットを撮ること。
4. `EnumChildWindows`+`GetWindowText`で子ウィンドウ(エラーダイアログの文言)を読めば、`テクスチャのロードに失敗しました。Assets/modelData/xxx.dds`のようなエラー内容を画面を見ずに確認できる。`FindWindow(null, "エラー")`は無関係な別プロセスの「エラー」ウィンドウに阻まれ0を返すことがあるので、`EnumWindows`+`GetWindowThreadProcessId`で対象PIDに絞ってから探すこと。
5. **確実に前面化するには**、`GetForegroundWindow`で現在のフォアグラウンドのスレッドIDを取得→`AttachThreadInput`で自スレッドと関連付け→`SetWindowPos`でHWND_TOPMOST→`BringWindowToTop`→`SetForegroundWindow`→スクリーンショット→`AttachThreadInput`解除→`SetWindowPos`でHWND_NOTOPMOST、という手順を踏むこと(単純な`ShowWindow`+`SetForegroundWindow`だと前面化に失敗し、同じ画面座標の別の常駐アプリを誤ってキャプチャすることがある)。撮れた画像は対象アプリの内容が写っているか必ず目視確認する。
6. 確認後は`Stop-Process -Id <pid> -Force`で終了する(ファイル差し替え後の再読み込みには再起動が必要なため)。

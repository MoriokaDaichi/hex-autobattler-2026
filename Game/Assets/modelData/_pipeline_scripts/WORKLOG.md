# 作業ログ: ユニット3Dモデル準備タスク

過去セッションでの試行錯誤・切り分け経緯の記録。次セッションの引継ぎに必要な「現状・次にやること」は[HANDOFF.md](HANDOFF.md)を参照。こちらは経緯を後から追えるようにするためのログで、通常は読む必要がない。

## 2026-08-25: 表示確認ブロッカーの解決

`Game::Render(RenderContext& rc)`が`m_hexGridRenderer.Draw(rc, ...)`しか呼んでおらず、`m_testModelRender.Draw(rc)`の呼び出しが漏れていたのが原因。`ModelRender`は`Update()`(アニメーション進行)と`Draw(rc)`(描画キューへの登録)が分離された設計で、`Draw(rc)`を毎フレーム呼ばない限りモデルは描画キューに一切載らない(`ModelRender.cpp`450行目`ModelRender::Draw`)。`Game.cpp::Render()`に`m_testModelRender.Draw(rc);`を追加して解決。

前セッションで疑っていた「`TkmFile::Load`内で`filePath`がNULLになる」現象は、MSVCデバッガの表示アーティファクト(関数プロローグ完了前にブレークして見た目上NULLに見えるだけ)であり無関係と判明(実際にfopenは成功していた)。

スケールについても切り分け済み: `SetTRS`のscale値`50.0`はhexSize(=50)基準の値だったが実モデル単位に対して過大で、カメラがモデル内部にめり込み画面全体が「粉々」に見えていた。`10.0`程度で1ヘックスマスに収まることを確認(最終値は非人型ユニットも含めて後で統一的に決める必要あり)。

## 2026-08-25: 「粉々」ブロッカーの切り分け(誤診断の経緯を含む)

Draw(rc)追加後、Goblinは表示されたが表面が「粉々に砕けたガラス」のような見た目になった。切り分け手順:

1. スケールを`1.0`まで下げても「粉々」の塊のまま → スケール要因ではないと判明。
2. `PlayAnimation`をコメントアウトしIdle固定でも同じ形状 → アニメーション固有の問題ではない。
3. Blenderで`rigged_blends/Goblin_rigged.blend`を直接ヘッドレスレンダリング(Eevee)→ バインドポーズ・Idle・Move全て正常な緑色ゴブリンが表示された → リグ・ウェイト・メッシュ自体には問題なしと確定。
4. `reanimate_and_export.py`と同じ手順でFBX再エクスポート→新規シーンにインポートし直すと同じ「粉々」形状が再現(テクスチャパス未解決でマゼンタ表示)。静止バインドポーズのみでも再現 → 一見FBXエクスポート/インポートが怪しく見えたが、後述の通りテスト方法の不備だった。
5. ボーンのrest行列・頂点座標・頂点法線・頂点グループを元ファイルとFBXラウンドトリップ後で比較 → 完全一致。スキニング/ジオメトリは無罪と確定。
6. Armatureモディファイアを完全に外して生メッシュのみレンダリングしても同じ「粉々」形状 → スキニング/リグは完全に無関係と最終確定。シェーディング(法線マップ)が壊れている方向に絞り込み。

### 誤診断: 「MeshyAI出力テクスチャそのものが壊れている」説(後に訂正)

`_meshy_raw/Goblin/Goblin_albedo.png`と`Goblin_normal.png`を直接開くと「UVアトラスが完全にシャッフルされた」状態の画像に見えた。`Swordsman_albedo.png`も同症状を確認し、18体全て(Goblin, Swordsman, Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord, Slime, Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake)で同一症状を目視確認(2026-08-25完了)。

ユーザーがMeshyAI管理画面からGoblinを再ダウンロードして検証したが、同じ症状が再現し「ダウンロード破損ではない」ことを確認(2026-08-25完了)。この時点では「MeshyAI側の生成/エクスポート自体が壊れている」という結論に至っていたが、**これは誤りだった**(下記参照)。

## 2026-08-25: ★根本原因の訂正(最重要)

「MeshyAIが出力したテクスチャそのものが壊れている」という上記の結論は誤り。テクスチャ・UVは最初から一切壊れておらず、真の原因は`png_to_dds.py`のY軸(V軸)反転処理のバグだった。

### 発見の経緯

ユーザー依頼で`_meshy_raw/ゴブリン/`の再ダウンロードFBXをBlenderでUV展開図ごとEeveeでレンダリングしたところ、UVがどれだけ細切れでも3Dモデルにマッピングした状態では完全に正常な緑色のゴブリンが表示された。これで前セッションの結論と矛盾が生じ再調査。

切り分け手順:
1. `Goblin_rigged.blend`にSmart UV Projectで新UVを作成し元テクスチャをBlenderのEmitベイクで再投影 → Blender上のレンダーでは正常。
2. それを3ds Max経由で`.tkm`/`.dds`化しゲーム内表示 → やはり「粉々」のまま。UV再ベイクだけでは直らなかった。
3. 法線マップをフラット画像に差し替えても症状は変化せず → 法線マップ/タンジェント計算は無罪と確定。
4. albedoの`png_to_dds.py`変換で行っていた「Blenderのrow0(下)→DDSのrow0(上)」というY軸反転処理を**外して**DDS化 → ゲーム内で正常な緑色のゴブリンが表示された。法線マップ・metallicRoughnessも反転なしで変換しフル品質で最終確認。

### 結論

`png_to_dds.py`のY軸反転(`src_y = h - 1 - y`)が誤りだった。3ds Maxの`tkmExporter_batch.ms`は`gettvert`で取得したUV.vをそのまま(反転せずに)書き出しており、3ds MaxのV軸はBlender/OpenGL同様V=0が下。DDS側で追加のY反転をかけると書き込まれたUV座標と実際の画像データの行が食い違い、サンプリング位置が別のUV島や余白(黒)にズレていた。UVアイランドが細かいほどこのズレの影響で「粉々のガラス片」のような見た目になる。GoblinもSwordsmanも他16体もテクスチャ生成自体は最初から正常だった可能性が高い。ユーザーが当初指摘していた「tkExporterのテクスチャ反転バグでは?」という仮説がそのまま正解だった。

`png_to_dds.py`本体は修正済み(Y軸反転処理を削除)。

### Goblinの対応状況

`rigged_blends/Goblin_rigged.blend`はSmart UV Projectで新UV(`island_margin=0.05`)を作成し元テクスチャ4種(`margin=32`でベイク)を再投影する形に更新済み(バックアップ: `rigged_blends/Goblin_rigged.blend.bak`)。UV再ベイク自体は不要だった可能性が高い(反転バグを直すだけで元の細切れUVでも直った可能性がある)が、結果的にUVアイランドがまとまったのでそのまま採用。5アクション分`.tkm`/`.tks`/`.tka`を作り直し配置済み(旧ファイルは`Game/Assets/modelData/_goblin_backup_before_rebake/`にバックアップ)。

## 2026-08-25: Swordsmanで検証(UV再ベイク不要と確定)

「UV再ベイクをせず、既存の`_meshy_raw/Swordsman/`のPNGをそのまま修正済み`png_to_dds.py`でDDS化するだけ」の最小構成を試した(Idleのみ、テクスチャは検証のため一時的にGoblin名義でエクスポート)。**結果: 一発で正常なSwordsman(オレンジ髪・鎧・剣・緑マント)が表示された。** UV再ベイクは一切不要で、DDS変換のY軸反転を外すだけで直ることが確定。確認後Goblin一式は正しい最終版に復元済み(md5一致確認済み)。

→ 残り16体もBlenderでのUV再ベイク作業は不要。「3ds MaxでUV/テクスチャそのまま→エクスポート」するだけで直る見込み。

## 2026-08-26: 量産パイプライン確立

- リグ済み10体(Priest, Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord)は全て`{Unit}_Idle`アクションしか無いことを確認(Swordsmanと同じ状態)。ボーン名が全ユニット共通のため`reanimate_and_export.py <blend> <outDir> <UnitName>`で5アクション一括生成・FBX書き出しが可能(Priestで実施・成功)。
- `3dsmaxbatch`呼び出しの落とし穴を発見: (1)スクリプトファイルは最初の引数に置く必要がある(後ろだと即失敗、exit code 126)。(2)`-mxsString`のパスはフォワードスラッシュ必須(バックスラッシュだと`DirectX_9_Shader`のプロパティエラー)。この2点を修正しPriestで一発成功(白ローブ+杖のPriestを画面確認)。
- スクラッチパッドに9体分(Archer, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord)の量産バッチ(`reanimate_and_export.py`→3dsmaxbatch×5アクション→`png_to_dds.py`×3テクスチャ→配置)を作成・実行。スクラッチパッドはセッション終了で消えるため、次回同様のことをする場合は本ログの知見を元に再構築が必要。
- Warlordのみ`_meshy_raw/Warlord/`に`Warlord_metallicRoughness.png`が無く`Warlord_metallic.png`しか無いことを確認。
- ユーザーのPC電源オフに伴い、Knightの3dsmax変換中だったバッチ処理を安全停止。`Game/Assets/modelData/`への書き込みは全ステップ成功後にまとめてコピーする設計のため、中断時点でKnight以降のファイルが中途半端な状態で残ることはないことを確認済み。

### 完了状況の詳細(2026-08-26セッション終了時点)

- 12体(Goblin, Priest, Archer, Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, Warlord, NightBlade, ChimeraLord)とも`{Unit}.tkm`, `{Unit}.tks`, `{Unit}_{Idle,Move,NormalAttack,Skill,Death}.tka`, `{Unit}_{albedo,normal,metallicRoughness}.dds`の計10ファイルが`Game/Assets/modelData/`に存在し非ゼロバイトであることを確認済み。
- Warlordのみ画面表示確認済み(`Game.cpp`の検証用コードを一時的にWarlordに差し替えてビルド・実行→エラーダイアログ「テクスチャのロードに失敗しました。Assets/modelData/Warlord_metallic.dds」発生→原因判明・修正→再実行して正常表示を確認)。
- **Warlord固有バグの詳細**: `_meshy_raw/Warlord/`に`Warlord_metallicRoughness.png`が無く`Warlord_metallic.png`を使ったため、3ds Maxの`tkmExporter`がmetallicテクスチャのパスとして渡した`Warlord_metallic.png`のベース名(`Warlord_metallic`)をtkmファイルに焼き込んだ。一方DDS変換側は他ユニットと揃えて機械的に`Warlord_metallicRoughness.dds`という名前で出力していたため、tkmが参照するファイル名と実際のファイル名が食い違いロードエラーになった。`Warlord_metallicRoughness.dds`を`Warlord_metallic.dds`という名前でも複製して追加することで解決(中身は同じテクスチャ、ファイル名だけの問題)。
- Warlordの表示自体は緑肌・鎧・目とも正常な陰影で「粉々」バグの再発は無いことを確認。ただし検証用コードのスケール(`10.0`)がWarlordのメッシュサイズに対して大きすぎ、カメラがモデルの頭部にめり込み顔全体は見えなかった(スケール調整は後日の統合作業でユニットごとに行う想定のため今回は見送り)。
- Swordsmanは前セッションでは検証用にGoblin名義でしか変換しておらず`Swordsman.*`として本配置されていなかった欠落があったため、このセッションでまとめて本配置した。

## 2026-08-26(セッション2): 非人型5体の四足自動リグ確立、3dsmaxbatchテクスチャ未埋め込みバグの発見

### Slimeのデータ破損を発見

非人型6体をBlenderでプレビューレンダーしたところ、`Slime.fbx`(および付属テクスチャ)が実際には赤マント+甲冑を着た人型キャラクターのデータであることが判明。他5体(Direwolf, Griffin, Behemoth, YoungDragon, FlameDrake)は見た目通り正しいモデルだった。ユーザー判断で「Slimeは後回し、まず残り5体を優先」となったため、今回はSlimeの対応は行っていない(次回、MeshyAI管理画面からの再生成/再ダウンロードが必要)。

### 四足自動リグ手法の確立

残り5体(Direwolf/Griffin/Behemoth/YoungDragon/FlameDrake、いずれも四足+尻尾、Griffin/YoungDragon/FlameDrakeは翼あり)は人型と全く異なる体型のため、既存の`rig_and_animate_unit.py`(腰/首カットのZ軸ベース検出)はそのまま使えない。新規に`rig_and_animate_quadruped.py`を作成:

- 全5体ともMeshy出力の座標系はY軸が前後(進行)方向、Z軸が上下、X軸が左右で統一されていることをプレビューレンダーで確認。
- 胴体プロファイルはY軸に沿ってNバンド分割し、断面積(X幅×Z高さ)のピーク位置を胴体中心、そこから頭側への「くびれ→再拡大」を首→頭、尾側への「急激な先細り」を尻尾根本として検出(人型のneck_band検出と同じ発想をY軸に適用)。
- 脚は各Y軸方向の分割点よりZが低い頂点群をX(左右)・Y(前後)で4クラスタ(前左右/後左右)に分け、人型のleg_landmarksと同様にAnkle/Knee/Hip(Shoulder)の3点を検出。
- 翼(Griffin/YoungDragon/FlameDrakeのみ、`has_wings`フラグで有効化)は胴体上部かつ体幹幅を大きく超えてX方向に飛び出た頂点群を左右に分けて検出。
- 頭がY+/Y-どちらを向いているかは自動判定せず`head_sign`引数で明示指定(5体とも実際にはY+側に頭があり、`head_sign=1.0`で統一できた)。
- ボーン名: `Hips/Spine/Chest/Neck/Head`, `TailRoot/TailMid/TailTip`, `FrontShoulder_L/R・FrontElbow_L/R・FrontAnkle_L/R`, `BackHip_L/R・BackKnee_L/R・BackAnkle_L/R`, (翼あり時)`WingRoot/Mid/Tip_L/R`。
- 5体全てBlender上でIdle/Move(対角トロット歩容)/NormalAttack/Skill/Deathの5アクションを生成し、レンダー確認でメッシュ崩壊が無いことを確認。

### 罠: DeathポーズでHipsボーンにX軸回転をかけると胴体が捻れる

人型の`Death`アクションをそのまま踏襲し`Hips`ボーンにX軸回転を与えたところ、Direwolfの体が「エビ反り」のように不自然に丸まった。原因は人型の`Hips`ボーン(Z軸=上下方向に伸びる)と四足の`Hips`ボーン(Y軸=前後方向に伸びる)でボーンのローカル軸の向きが異なり、同じ`rotation_euler`の意味が変わるため。対策として`Hips`の大きな回転はやめ、前脚肘・後脚膝を折り畳んで`Hips`のZ位置を沈めるアプローチに変更し解決(`rig_and_animate_quadruped.py`のDeathアクション参照)。

### 罠: 3dsmaxbatchに相対パスのテクスチャパスを渡すとtkmにテクスチャが一切埋め込まれない(最重要)

Direwolfを3ds Max変換→DDS化→配置→ゲーム内表示確認したところ、モデル全体が陰影の無い均一な赤色で表示された(エラーダイアログは出ない)。tkmファイルのバイナリを直接パースして調査した結果(ヘッダー: byte version, byte flatShading, short meshPartsCount, 各meshPartでlong numMat→long numVertex→byte indexSize+3byte padding→各materialでalbedo/normal/spec/reflection/refractionをそれぞれ`long 文字列長 + 文字列本体 + ヌル終端1byte`で格納)、albedo/normal/specの文字列長が全て0(空)になっていることが判明。

原因は`export_unit_action.ms`に`albedoPath`等を`"Game/Assets/modelData/_meshy_raw/Direwolf/Direwolf_albedo.png"`のような相対パスで渡していたため、3dsmaxbatchプロセス内の`openBitMap`がファイルを解決できずundefinedのまま`mat.g_albedo`等に代入され、結果としてtkmにファイル名が書き込まれなかったこと。`albedoPath`/`normalPath`/`metallicPath`を`C:/my/01_project/02_GameProject/GameTemplate/...`のような絶対パスに変更したところ即座に解決(tkmに`Direwolf_albedo.png`等が正しく埋め込まれ、ゲーム内でも正常なテクスチャが表示された)。

念のため既存の人型`Goblin.tkm`も同じ手法でバイナリ検証したところ、こちらは`Goblin_albedo.png`等が正しく埋め込まれていた。つまり過去セッションでは(3dsmaxbatchのカレントディレクトリ状態がたまたま一致していたなどの理由で)相対パスでも問題が起きなかったが、今回のセッションでは環境差により顕在化したとみられる。今後は常に絶対パスを使うのが安全。

### 罠: shaderFxPathにDefaultPhong.fxを指定するとエラーになる

3ds Max付属の`hardwareshaders/DefaultPhong.fx`を`shaderFxPath`に指定したところ、「未知のプロパティ "g_useNormalMap"」エラーで変換が失敗した。k2Engine用のカスタムDirectX_9_Shaderである`tools/3dsMaxShader/k2EngineShader.fx`(`g_albedo`/`g_normal`/`g_metallicAndSmoothMap`/`g_useNormalMap`等のプロパティを持つ)を指定することで解決。

### 本番変換・配置・表示確認

Direwolf/Behemoth/Griffin/YoungDragon/FlameDrakeの5体を`export_actions_separately.py`→3dsmaxbatch(絶対パス)×5アクション→`png_to_dds.py`×3テクスチャの手順で本番変換し、`Game/Assets/modelData/`に配置(各10ファイル: tkm/tks/tka×5/dds×3)。DirewolfとGriffin(翼あり)は実際にビルド・実行して画面表示確認済み(共に正常、テクスチャ・スキニングとも破綻なし)。Behemoth/YoungDragon/FlameDrakeはファイル一式の配置のみ確認(表示確認は次回任意)。

3dsmaxbatchが1回タイムアウト(5分)した以外は全て一発成功。タイムアウトしたBehemoth Skill/Deathアクションは個別に再実行して解決(3dsMax自体はハングしておらずプロセスは正常終了していた)。

## 2026-08-26(セッション3): 表示未確認13体の確認、エンジン側バッファオーバーフローバグの発見・修正

### Swordsman/ShadowStalkerで再現するクラッシュの調査

表示未確認だった13体を順番に確認する作業中、SwordsmanとShadowStalkerで`Debug Assertion Failed! Expression: ("Buffer too small", 0)`(`minkernel/crts/ucrt/src/appcrt/stdio/output.cpp` line 282)というクラッシュが再現することを発見。Knight/Cultistは正常だった。

切り分け手順:
1. tkm/tks/tka/ddsのバイナリ構造を全てPowerShellで直接パースして検証(ヘッダー、マテリアルのテクスチャファイル名、頂点座標・法線・UVのNaN/Inf有無、ボーンインデックス範囲、インデックスバッファの範囲、DDSヘッダーのwidth/height)→**全て正常、ファイルの整合性に一切問題なし**という結論に至った。
2. tka⇔tks⇔tkmを他ユニット(Knight)のものと入れ替えるクロステストを実施→**tkm自体(具体的にはメッシュ/マテリアル初期化コード)が原因**と判明(tkaもtksも無関係)。
3. `k2EngineLow/graphics/MeshParts.cpp`の`InitFromTkmFile`→`CreateMeshFromTkmMesh`内、マテリアルキャッシュキー生成用の`sprintf_s(materiayKey, MAX_PATH, "%s, %s, %s, %s, %d×12, %s, %s, %s", fxFilePath, vsEntryPointFunc, vsSkinEntryPointFunc, psEntryPointFunc, ...colorBufferFormat[8]..., alphaBlendMode, isDepthWrite, isDepthTest, cullMode, albedoMapFileName, normalMapFileName, specularMapFileName)`に一時的な診断ログ(`fopen`でファイルに書き出す方式、`OutputDebugString`はGUIレス環境で確認できないため不採用)を仕込んで実測。

### 根本原因: MAX_PATH(260バイト)バッファオーバーフロー(ユニット名依存)

実測の結果、`isShadowReciever=true`の場合`psEntryPointFunc`が`"PSMainShadowReciever"`(20文字)になり、これと`vsEntryPointFunc`(32文字)・`vsSkinEntryPointFunc`(36文字)・`fxFilePath`(RenderGBufferパスで53文字)・3つのテクスチャファイル名(ユニット名を含む)を全部連結すると、**ユニット名が9文字以上のとき合計文字列長がMAX_PATH(260バイト)を超えてバッファオーバーフローする**ことが判明(区切り文字・数値部分を含めた概算式: `235 + 3×ユニット名文字数` ≤ 259 が安全条件、つまりユニット名8文字までが安全)。

該当した/しそうだったユニット: Swordsman(9文字)❌、ShadowStalker(13文字)❌ともに実クラッシュを確認。OrcBerserker(12)・NightBlade(10)・ChimeraLord(11)・YoungDragon(11)・FlameDrake(10)も計算上危険域だったが、後述の修正後に全て正常動作を確認したため実際にクラッシュしていたかは未検証(修正が先に入ったため)。Knight(6)/Cultist(7)/Goblin(6)/Priest(6)/Archer(6)/Paladin(7)/Warlord(7)/Direwolf(8)/Griffin(7)/Behemoth(8)は計算上ぎりぎり安全域。

### 修正

`k2EngineLow/graphics/MeshParts.cpp`の該当箇所を`char materiayKey[MAX_PATH]`→`char materiayKey[1024]`、`sprintf_s(materiayKey, MAX_PATH, ...)`→`sprintf_s(materiayKey, sizeof(materiayKey), ...)`に変更。修正後、Swordsman/ShadowStalkerとも正常表示を確認。ユニット固有のデータ不備ではなく純粋なエンジン側バグだったため、`Game/`以外だが例外的に修正した。

### 事故: 日本語コメントのエンコーディング破損と復旧

上記修正の際、Editツールで`MeshParts.cpp`(元はShift-JIS/LF)を編集したところ、ファイル全体がUTF-8/CRLFとして誤って読み書きされ、Shift-JISの日本語コメントが不正なUTF-8として解釈されU+FFFD(置換文字)に文字化けした状態で保存されてしまう事故が発生(他セッションの`git diff`チェックで発覆)。

対応: `git show HEAD:<path> > <path>`で元のShift-JIS内容を復元した上で、PowerShellで**Latin-1(1バイト=1文字のロスレスなコードページ)としてファイルを読み書き**することでバイト単位の安全な文字列置換を行い、意図した2行の修正だけを再適用した(`[System.Text.Encoding]::GetEncoding(28591)`を使用)。`git diff`が最終的に意図した2行分の差分のみになっていることを確認済み。

**教訓**: 日本語(Shift-JIS)コメントを含むk2EngineLow/k2Engine配下のファイルをEditツールで編集すると、エンコーディングを壊す危険がある。エンジン共有コードに手を入れる際は、編集後に`file <path>`や`git diff`でエンコーディング/差分範囲を必ず確認すること。バイト単位の置換が必要な場合はLatin-1ラウンドトリップが安全。

### 表示確認完了

Swordsman, Knight, Cultist, ShadowStalker, OrcBerserker, Paladin, NightBlade, ChimeraLord, Priest, Archer(人型10体)、Behemoth, YoungDragon, FlameDrake(四足3体)の計13体、全て画面表示を目視確認し正常(テクスチャ・スキニングとも破綻なし)。これで18体中17体(Slime以外)の表示確認が完了。

## 2026-08-26(セッション3続き): Slime再生成データの変換・単一ボーンリグの確立(18体全完了)

ユーザーがMeshyAIでSlimeを再生成。`_meshy_raw/Slime/Meshy_AI_stylized_toon_shaded__0826060225_texture_fbx/`に新しいfbx+テクスチャ(albedo/normal/metallic/roughness、Warlordと同様metallicRoughness結合版は無し)が追加された。旧(誤った人型データの)`Slime.fbx`等はフォルダ直下にそのまま残っているため、新データはサブフォルダ名で区別して扱った。

Blenderでプレビューレンダーしたところ、今度こそ正しいSlime(涙滴型の胴体、頭頂部に角状の突起、左右に小さな腕、下部に短い足×2、大きな目)であることを確認。四足リグ・人型リグどちらのボーン構造にも適合しない極めてシンプルな形状だったため、`rig_and_animate_simple.py`を新規作成: 全頂点を単一の`Root`ボーンにウェイト1.0でバインドし、5アクションを`Root`ボーンのスケール・位置変化(スクワッシュ&ストレッチ)だけで表現する方式(Idle=呼吸、Move=バウンス跳躍、NormalAttack=前方への伸縮、Skill=大きく膨張、Death=潰れて縮む)。

変換時の注意点(再掲): テクスチャファイル名が`Meshy_AI_stylized_toon_shaded__0826060225_texture*.png`という長い名前だったため、他ユニットと同じ命名規則(`Slime_albedo.png`等)にリネームしてから3ds Max変換に渡した(そのままでも動作した可能性はあるが、命名規則を揃える目的と、tkmに焼き込まれる参照名を短く保つため)。Warlordと同様`metallicRoughness`結合版が無く`metallic.png`のみのため、`specMap`として`metallic.png`をそのまま渡し、DDS変換後のファイル名(`Slime_metallic.dds`)もそれに合わせた。

3ds Max変換・DDS化・配置・ゲーム内表示確認まで一気通貫で実施し、正常な緑色のSlime表示を確認(テクスチャ・シェーディングとも破綻なし)。これで**18体全ての表示確認が完了**。

-- headless tkm/tks/tka exporter driver for one (unit, action) pair
-- expects globals set via -mxsString before this file is filein'd:
--   fbxPath, tkmOutPath, tkaOutPath, albedoPath, normalPath, metallicPath, shaderFxPath
-- (all passed through maxOps.mxsCmdLineArgs)

fbxPath = maxOps.mxsCmdLineArgs[#fbxPath]
tkmOutPath = maxOps.mxsCmdLineArgs[#tkmOutPath]
tkaOutPath = maxOps.mxsCmdLineArgs[#tkaOutPath]
albedoPath = maxOps.mxsCmdLineArgs[#albedoPath]
normalPath = maxOps.mxsCmdLineArgs[#normalPath]
metallicPath = maxOps.mxsCmdLineArgs[#metallicPath]
shaderFxPath = maxOps.mxsCmdLineArgs[#shaderFxPath]
frameStart = (maxOps.mxsCmdLineArgs[#frameStart]) as integer
frameEnd = (maxOps.mxsCmdLineArgs[#frameEnd]) as integer

scriptsDir = "C:/Users/1264033/AppData/Local/Temp/claude/c--my-01-project-02-GameProject-GameTemplate/3e1ab223-e114-4d2e-abbe-057b44050cd5/scratchpad/"
fileIn (scriptsDir + "tkExporterCommon.ms")
fileIn (scriptsDir + "tksExporter.ms")
fileIn (scriptsDir + "tkmExporter_batch.ms")

resetMaxFile #noPrompt

FBXImporterSetParam "Animation" true
FBXImporterSetParam "Cameras" false
FBXImporterSetParam "Lights" false
FBXImporterSetParam "Skin" true
FBXImporterSetParam "Mode" #create
importFile fbxPath #noPrompt using:FBXIMP

meshObj = undefined
for o in objects do (
	if superclassof o == geometryclass then meshObj = o
)
if meshObj == undefined then (
	format "ERROR: no mesh object found in %\n" fbxPath
) else (
	-- set up the DirectX shader material expected by the tkm exporter
	mat = DirectX_9_Shader()
	mat.effectFile = shaderFxPath
	mat.g_useNormalMap = true
	if albedoPath != unsupplied and albedoPath != "" then mat.g_albedo = openBitMap albedoPath
	if normalPath != unsupplied and normalPath != "" then mat.g_normal = openBitMap normalPath
	if metallicPath != unsupplied and metallicPath != "" then mat.g_metallicAndSmoothMap = openBitMap metallicPath
	meshObj.material = mat

	exporter = TkmExporterForScript()
	exporter.SaveTkm #(meshObj) tkmOutPath false
	format "TKM/TKS EXPORT ATTEMPTED: %\n" tkmOutPath

	-- tka export: reuse g_objectParams populated by SaveTkm's TrySaveSkeleton call
	if g_objectParams.count == 0 then (
		format "ERROR: g_objectParams empty, skeleton not captured\n"
	) else (
		deltaTime = 1.0 / 30.0
		file = fopen tkaOutPath "wb"
		numKey = (frameEnd - frameStart + 1) * g_objectParams.count
		WriteLong file numKey
		WriteLong file 0 -- no note track events
		fclose file

		file = fopen tkaOutPath "ab"
		for i = frameStart to frameEnd do (
			sliderTime = i
			boneId = 0
			keyTime = (i - frameStart) * deltaTime
			for o in g_objectParams do (
				n = o.n
				WriteLong file boneId
				WriteFloat file keyTime
				transformMatrix = Matrix3 1
				if n.parent == undefined then (
					transformMatrix = n.transform
				) else (
					transformMatrix = n.transform * (inverse n.parent.transform)
				)
				for j = 1 to 4 do (
					WriteFloat file transformMatrix[j].x
					WriteFloat file transformMatrix[j].y
					WriteFloat file transformMatrix[j].z
				)
				boneId += 1
			)
		)
		fclose file
		format "TKA EXPORT DONE: % (frames %-%)\n" tkaOutPath frameStart frameEnd
	)
)
format "ALL DONE\n"

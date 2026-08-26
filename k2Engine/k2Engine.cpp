#include "k2EnginePreCompile.h"
#include "k2Engine.h"
#include "dbg/dbgVectorRenderer.h"

namespace nsK2Engine {
	K2Engine* K2Engine::m_instance = nullptr;
	SceneLight* g_sceneLight = nullptr;
	RenderingEngine* g_renderingEngine = nullptr;
	CollisionObjectManager* g_collisionObjectManager = nullptr;
	K2Engine* g_k2Engine = nullptr;
	void K2Engine::Init(const InitData& initData)
	{
		g_k2Engine = this;
		g_engine = &m_k2EngineLow;
		g_collisionObjectManager = &m_collisionObjectManager;
		g_renderingEngine = &m_renderingEngine;

		raytracing::InitData raytracintInitData;
		raytracintInitData.m_expandShaderResource = &m_renderingEngine.GetRaytracingLightData();
		raytracintInitData.m_expandShaderResourceSize = sizeof(m_renderingEngine.GetRaytracingLightData());
#ifdef COPY_RAYTRACING_FRAMEBUFFER
		// �t���[���o�b�t�@�ɃR�s�[����ꍇ�́A���C�g���̏o�͐�̃J���[�o�b�t�@�̃t�H�[�}�b�g��
		// �t���[���o�b�t�@�Ɠ����ɂ���B
		raytracintInitData.m_outputColorBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif // #ifdef COPY_RAYTRACING_FRAMEBUFFER
		m_k2EngineLow.Init(
			initData.hwnd, 
			initData.frameBufferWidth, 
			initData.frameBufferHeight,
			raytracintInitData
		);
		m_renderingEngine.Init(initData.isSoftShadow);
#ifdef K2_DEBUG
		// �f�o�b�O�x�N�g�������_���[���쐬����B
		m_vectorRenderer = NewGO<nsDbg::VectorRenderer>(0);
#endif // #ifdef K2_DEBUG
	}
	K2Engine::~K2Engine()
	{
#ifdef K2_DEBUG
		DeleteGO(m_vectorRenderer);
#endif
		g_renderingEngine = nullptr;
		g_collisionObjectManager = nullptr;
		g_engine = nullptr;
	}

	void K2Engine::Execute()
	{
		auto& renderContext = g_graphicsEngine->GetRenderContext();

		g_engine->BeginFrame();

		g_engine->ExecuteUpdate();
		// �����_�����O�G���W���̍X�V�B
		m_renderingEngine.Update();
		
		g_engine->ExecuteRender();
		//�����_�����O�G���W�������s�B		
		m_renderingEngine.Execute(renderContext);
		
		//�����蔻��`��B
		g_engine->DebubDrawWorld();

		//////////////////////////////////////
		//�G��`���R�[�h�������̂͂����܂ŁI�I�I
		//////////////////////////////////////
		g_engine->EndFrame();
	}
#ifdef K2_DEBUG
	void K2Engine::DrawVector(const Vector3& vector, const Vector3& origin, const char* name)
	{
		nsDbg::VectorRenderer::SRenderData renderData;
		renderData.origin = origin;
		renderData.vector = vector;
		renderData.name = name;
		m_vectorRenderer->AddVector(renderData);
	}
	void K2Engine::SetDrawVectorEnable()
	{
		m_vectorRenderer->EnableRender();
	}
	void K2Engine::SetDrawVectorDisable()
	{
		m_vectorRenderer->DisableRender();
	}
#endif // #ifdef K2_DEBUG
}
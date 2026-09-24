#include "k2EngineLowPreCompile.h"
#include "FontEngine.h"

using namespace std;
using namespace DirectX;

namespace nsK2EngineLow {
	FontEngine::~FontEngine()
	{
		if (m_srvDescriptorHeap != nullptr) {
			m_srvDescriptorHeap->Release();
		}
	}
	void FontEngine::Init()
	{
		auto d3dDevice = g_graphicsEngine->GetD3DDevice();

		// ディスクリプタヒープを作成。
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		auto hr = d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap));

		ResourceUploadBatch re(d3dDevice);
		re.Begin();
		// SpriteBatchのパイプラインステートを作成する。
		RenderTargetState renderTargetState;
		renderTargetState.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		renderTargetState.numRenderTargets = 1;
		renderTargetState.dsvFormat = DXGI_FORMAT_D32_FLOAT;
		renderTargetState.sampleMask = UINT_MAX;
		renderTargetState.sampleDesc.Count = 1;

		// SpriteBatchのデフォルトブレンド設定(s_DefaultBlendDesc)はプリマルチプライドアルファ
		// (SrcBlend=ONE)前提だが、本エンジンのFont::Draw()呼び出し元はストレートアルファ
		// (RGBはフル値、colorのwだけを透明度として使う)で色を渡している。SrcBlend=ONEのままだと
		// アルファを下げてもソースRGBが常にフル強度で加算され、フェード演出が効かない
		// (font-rendering-bugfixesタスクで判明)。ストレートアルファ用のブレンド記述に差し替える。
		D3D12_BLEND_DESC straightAlphaBlendDesc = {};
		straightAlphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
		straightAlphaBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		straightAlphaBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		straightAlphaBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		straightAlphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		straightAlphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		straightAlphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		straightAlphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		SpriteBatchPipelineStateDescription sprBatchDesc(renderTargetState, &straightAlphaBlendDesc);

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<FLOAT>(1920);
		viewport.Height = static_cast<FLOAT>(1080);
		// Spriteバッチを作成。
		m_spriteBatch = make_unique<SpriteBatch>(
			d3dDevice,
			re,
			sprBatchDesc,
			&viewport);
		// SpriteFontを作成。
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_spriteFont = make_unique<SpriteFont>(
			d3dDevice,
			re,
			L"Assets/font/myfile.spritefont",
			cpuHandle,
			gpuHandle);
		// 未収録文字(全角記号等)を描画しようとするとSpriteFont::Impl::FindGlyph()が例外を送出して
		// クラッシュする(font-rendering-bugfixesタスクで判明)。ASCIIの'?'はmyfile.spritefontに
		// 確実に収録されている基本文字のため、代替グリフとして設定しクラッシュを防ぐ。
		m_spriteFont->SetDefaultCharacter(L'?');

		re.End(g_graphicsEngine->GetCommandQueue());
	}

	void FontEngine::BeginDraw(RenderContext& rc)
	{
		auto commandList = g_graphicsEngine->GetCommandList();
		auto d3dDevice = g_graphicsEngine->GetD3DDevice();
		m_spriteBatch->Begin(
			commandList,
			SpriteSortMode_Deferred,
			g_matIdentity
		);
		commandList->SetDescriptorHeaps(1, &m_srvDescriptorHeap);
	}
	void FontEngine::EndDraw(RenderContext& rc)
	{
		m_spriteBatch->End();
	}

	void FontEngine::Draw(
		const wchar_t* text,
		const Vector2& position,
		const Vector4& color,
		float rotation,
		float scale,
		Vector2 pivot
	)
	{
		m_spriteFont->DrawString(
			m_spriteBatch.get(),
			text,
			position.vec,
			color,
			rotation,
			DirectX::XMFLOAT2(pivot.x, pivot.y),
			scale
		);
	}
}
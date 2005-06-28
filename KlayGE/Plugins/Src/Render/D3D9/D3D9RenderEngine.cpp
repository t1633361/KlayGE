// D3D9RenderEngine.cpp
// KlayGE D3D9渲染引擎类 实现文件
// Ver 2.7.0
// 版权所有(C) 龚敏敏, 2003-2005
// Homepage: http://klayge.sourceforge.net
//
// 2.7.0
// 改进了Render (2005.6.16)
// 去掉了TextureCoordSet和DisableTextureStage (2005.6.26)
// TextureAddressingMode, extureFiltering和TextureAnisotropy移到Texture中 (2005.6.27)
//
// 2.4.0
// 增加了PolygonMode (2005.3.20)
//
// 2.0.4
// 去掉了WorldMatrices (2004.4.3)
//
// 2.0.3
// 优化了Render (2004.2.22)
//
// 2.0.1
// 重构了Render (2003.10.10)
//
// 2.0.0
// 初次建立 (2003.8.15)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Util.hpp>

#include <KlayGE/Light.hpp>
#include <KlayGE/Material.hpp>
#include <KlayGE/Viewport.hpp>
#include <KlayGE/VertexBuffer.hpp>
#include <KlayGE/RenderTarget.hpp>
#include <KlayGE/RenderEffect.hpp>

#include <KlayGE/D3D9/D3D9RenderSettings.hpp>
#include <KlayGE/D3D9/D3D9RenderWindow.hpp>
#include <KlayGE/D3D9/D3D9Texture.hpp>
#include <KlayGE/D3D9/D3D9VertexStream.hpp>
#include <KlayGE/D3D9/D3D9IndexStream.hpp>
#include <KlayGE/D3D9/D3D9Mapping.hpp>

#include <cassert>
#include <algorithm>
#include <cstring>

#include <KlayGE/D3D9/D3D9RenderEngine.hpp>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d9.lib")

namespace KlayGE
{
	// 构造函数
	/////////////////////////////////////////////////////////////////////////////////
	D3D9RenderEngine::D3D9RenderEngine()
						: cullingMode_(RenderEngine::CM_None),
							clearFlags_(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER)
	{
		// Create our Direct3D object
		d3d_ = MakeCOMPtr(Direct3DCreate9(D3D_SDK_VERSION));
		Verify(d3d_.get() != NULL);

		adapterList_.Enumerate(d3d_);
	}

	// 析构函数
	/////////////////////////////////////////////////////////////////////////////////
	D3D9RenderEngine::~D3D9RenderEngine()
	{
		renderEffect_.reset();
		renderTargetList_.clear();

		currentVertexDecl_.reset();

		d3dDevice_.reset();
		d3d_.reset();
	}

	// 返回渲染系统的名字
	/////////////////////////////////////////////////////////////////////////////////
	std::wstring const & D3D9RenderEngine::Name() const
	{
		static const std::wstring name(L"Direct3D9 Render System");
		return name;
	}

	// 获取D3D接口
	/////////////////////////////////////////////////////////////////////////////////
	boost::shared_ptr<IDirect3D9> const & D3D9RenderEngine::D3D() const
	{
		return d3d_;
	}

	// 获取D3D Device接口
	/////////////////////////////////////////////////////////////////////////////////
	boost::shared_ptr<IDirect3DDevice9> const & D3D9RenderEngine::D3DDevice() const
	{
		return d3dDevice_;
	}

	// 获取D3D适配器列表
	/////////////////////////////////////////////////////////////////////////////////
	D3D9AdapterList const & D3D9RenderEngine::D3DAdapters() const
	{
		return adapterList_;
	}

	// 获取当前适配器
	/////////////////////////////////////////////////////////////////////////////////
	D3D9Adapter const & D3D9RenderEngine::ActiveAdapter() const
	{
		return adapterList_.Adapter(adapterList_.CurrentAdapterIndex());
	}

	// 开始渲染
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StartRendering()
	{
		bool gotMsg;
		MSG  msg;

		::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

		RenderTarget& renderTarget(**RenderEngine::ActiveRenderTarget());
		while (WM_QUIT != msg.message)
		{
			// 如果窗口是激活的，用 PeekMessage()以便我们可以用空闲时间渲染场景
			// 不然, 用 GetMessage() 减少 CPU 占用率
			if (renderTarget.Active())
			{
				gotMsg = (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0);
			}
			else
			{
				gotMsg = (::GetMessage(&msg, NULL, 0, 0) != 0);
			}

			if (gotMsg)
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			else
			{
				// 在空余时间渲染帧 (没有等待的消息)
				if (renderTarget.Active())
				{
					renderTarget.Update();
				}
			}
		}
	}

	// 设置环境光
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::AmbientLight(Color const & col)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(col.r(), col.g(), col.b(), 1.0f)));
	}

	// 设置清除颜色
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::ClearColor(Color const & clr)
	{
		clearClr_ = D3DCOLOR_COLORVALUE(clr.r(), clr.g(), clr.b(), 1.0f);
	}

	// 设置光影类型
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::ShadingType(ShadeOptions so)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_SHADEMODE, D3D9Mapping::Mapping(so)));
	}

	// 打开/关闭光源
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::EnableLighting(bool enabled)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_LIGHTING, enabled));
	}

	// 建立渲染窗口
	/////////////////////////////////////////////////////////////////////////////////
	RenderWindowPtr D3D9RenderEngine::CreateRenderWindow(std::string const & name,
		RenderSettings const & settings)
	{
		assert(dynamic_cast<D3D9RenderSettings const *>(&settings) != NULL);

		D3D9RenderWindowPtr win(new D3D9RenderWindow(d3d_, this->ActiveAdapter(), name,
			static_cast<D3D9RenderSettings const &>(settings)));

		d3dDevice_ = win->D3DDevice();

		this->ActiveRenderTarget(this->AddRenderTarget(win));

		this->DepthBufferDepthTest(settings.depthBuffer);
		this->DepthBufferDepthWrite(settings.depthBuffer);

		// get caps
		d3d_->GetDeviceCaps(this->ActiveAdapter().AdapterNo(), D3DDEVTYPE_HAL, &caps_);

		return win;
	}

	// 设置剪裁模式
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::CullingMode(CullMode mode)
	{
		cullingMode_ = mode;

		if ((*RenderEngine::ActiveRenderTarget())->RequiresTextureFlipping())
		{
			if (CM_Clockwise == mode)
			{
				mode = CM_AntiClockwise;
			}
			else
			{
				if (CM_AntiClockwise == mode)
				{
					mode = CM_Clockwise;
				}
			}
		}

		TIF(d3dDevice_->SetRenderState(D3DRS_CULLMODE, D3D9Mapping::Mapping(mode)));
	}

	// 设置多变性填充模式
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::PolygonMode(FillMode mode)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_FILLMODE, D3D9Mapping::Mapping(mode)));
	}

	// 设置光源
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::SetLight(uint32_t index, Light const & lt)
	{
		D3DLIGHT9 d3dLight;
		std::memset(&d3dLight, 0, sizeof(d3dLight));

		d3dLight.Type = D3D9Mapping::Mapping(lt.lightType);

		if (Light::LT_Spot == lt.lightType)
		{
			d3dLight.Falloff	= lt.spotFalloff;
			d3dLight.Theta		= lt.spotInner;
			d3dLight.Phi		= lt.spotOuter;
		}

		d3dLight.Diffuse	= D3D9Mapping::MappingToFloat4Color(lt.diffuse);
		d3dLight.Specular	= D3D9Mapping::MappingToFloat4Color(lt.specular);
		d3dLight.Ambient	= D3D9Mapping::MappingToFloat4Color(lt.ambient);

		if (lt.lightType != Light::LT_Directional)
		{
			d3dLight.Position = D3D9Mapping::Mapping(lt.position);
		}
		if (lt.lightType != Light::LT_Point)
		{
			d3dLight.Direction = D3D9Mapping::Mapping(lt.direction);
		}

		d3dLight.Range = lt.range;
		d3dLight.Attenuation0 = lt.attenuationConst;
		d3dLight.Attenuation1 = lt.attenuationLinear;
		d3dLight.Attenuation2 = lt.attenuationQuad;

		TIF(d3dDevice_->SetLight(index, &d3dLight));
	}

	// 打开/关闭某个光源
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::LightEnable(uint32_t index, bool enabled)
	{
		TIF(d3dDevice_->LightEnable(index, enabled));
	}

	// 实现设置世界矩阵
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DoWorldMatrix()
	{
		D3DMATRIX d3dmat(D3D9Mapping::Mapping(worldMat_));
		TIF(d3dDevice_->SetTransform(D3DTS_WORLD, &d3dmat));
	}

	// 实现设置观察矩阵
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DoViewMatrix()
	{
		D3DMATRIX d3dMat(D3D9Mapping::Mapping(viewMat_));
		TIF(d3dDevice_->SetTransform(D3DTS_VIEW, &d3dMat));
	}

	// 实现设置投射矩阵
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DoProjectionMatrix()
	{
		D3DMATRIX d3dMat(D3D9Mapping::Mapping(projMat_));

		if ((*activeRenderTarget_)->RequiresTextureFlipping())
		{
			d3dMat._22 = -d3dMat._22;
		}

		TIF(d3dDevice_->SetTransform(D3DTS_PROJECTION, &d3dMat));
	}

	// 设置材质
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::SetMaterial(Material const & m)
	{
		D3DMATERIAL9 material;

		material.Diffuse	= D3D9Mapping::MappingToFloat4Color(m.diffuse);
		material.Ambient	= D3D9Mapping::MappingToFloat4Color(m.ambient);
		material.Specular	= D3D9Mapping::MappingToFloat4Color(m.specular);
		material.Emissive	= D3D9Mapping::MappingToFloat4Color(m.emissive);
		material.Power		= m.shininess;

		TIF(d3dDevice_->SetMaterial(&material));
	}

	// 设置当前渲染目标，该渲染目标必须已经在列表中
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::ActiveRenderTarget(RenderTargetListIterator iter)
	{
		RenderEngine::ActiveRenderTarget(iter);

		IDirect3DSurface9* backBuffer;
		(*activeRenderTarget_)->CustomAttribute("DDBACKBUFFER", &backBuffer);
		TIF(d3dDevice_->SetRenderTarget(0, backBuffer));

		IDirect3DSurface9* zBuffer;
		(*activeRenderTarget_)->CustomAttribute("D3DZBUFFER", &zBuffer);
		TIF(d3dDevice_->SetDepthStencilSurface(zBuffer));

		this->CullingMode(cullingMode_);

		Viewport const & vp((*iter)->GetViewport());
		D3DVIEWPORT9 d3dvp = { vp.left, vp.top, vp.width, vp.height, 0, 1 };
		TIF(d3dDevice_->SetViewport(&d3dvp));
	}

	// 开始一帧
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::BeginFrame()
	{
		TIF(d3dDevice_->Clear(0, NULL, clearFlags_, clearClr_, 1, 0));

		TIF(d3dDevice_->BeginScene());
	}

	// 渲染
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::Render(VertexBuffer const & vb)
	{
		assert(vb.VertexStreamEnd() - vb.VertexStreamBegin() != 0);

		D3DPRIMITIVETYPE primType;
		uint32_t primCount;
		D3D9Mapping::Mapping(primType, primCount, vb);

		numPrimitivesJustRendered_ += primCount;
		numVerticesJustRendered_ += vb.UseIndices() ? vb.NumIndices() : vb.NumVertices();

		VertexDeclType shaderDecl;
		shaderDecl.reserve(currentDecl_.size());

		D3DVERTEXELEMENT9 element;

		for (VertexBuffer::VertexStreamConstIterator iter = vb.VertexStreamBegin();
			iter != vb.VertexStreamEnd(); ++ iter)
		{
			VertexStream& stream(*(*iter));
			assert(dynamic_cast<D3D9VertexStream*>(&stream) != NULL);

			D3D9Mapping::Mapping(element, shaderDecl.size(), stream);
			shaderDecl.push_back(element);

			D3D9VertexStream& d3d9vs(static_cast<D3D9VertexStream&>(stream));
			TIF(d3dDevice_->SetStreamSource(element.Stream,
				d3d9vs.D3D9Buffer().get(), 0,
				static_cast<UINT>(stream.sizeElement() * stream.ElementsPerVertex())));
		}

		{
			element.Stream		= 0xFF;
			element.Type		= D3DDECLTYPE_UNUSED;
			element.Usage		= 0;
			element.UsageIndex	= 0;
			shaderDecl.push_back(element);
		}

		// Clear any previous steam sources
		for (size_t i = shaderDecl.size() - 1; i < currentDecl_.size(); ++ i)
		{
			d3dDevice_->SetStreamSource(static_cast<uint32_t>(i), NULL, 0, 0);
		}


		if ((currentDecl_.size() != shaderDecl.size())
			|| std::memcmp(&currentDecl_[0], &shaderDecl[0],
								sizeof(shaderDecl[0]) * shaderDecl.size()) != 0)
		{
			currentDecl_ = shaderDecl;

			IDirect3DVertexDeclaration9* theVertexDecl;
			TIF(d3dDevice_->CreateVertexDeclaration(&currentDecl_[0], &theVertexDecl));
			currentVertexDecl_ = MakeCOMPtr(theVertexDecl);
		}

		TIF(d3dDevice_->SetVertexDeclaration(currentVertexDecl_.get()));


		if (vb.UseIndices())
		{
			D3D9IndexStream& d3dis(static_cast<D3D9IndexStream&>(*vb.GetIndexStream()));
			d3dDevice_->SetIndices(d3dis.D3D9Buffer().get());

			for (uint32_t i = 0; i < renderPasses_; ++ i)
			{
				renderEffect_->BeginPass(i);

				TIF(d3dDevice_->DrawIndexedPrimitive(primType, 0, 0,
					static_cast<UINT>(vb.NumVertices()), 0, primCount));

				renderEffect_->EndPass();
			}
		}
		else
		{
			for (uint32_t i = 0; i < renderPasses_; ++ i)
			{
				renderEffect_->BeginPass(i);

				TIF(d3dDevice_->DrawPrimitive(primType, 0, primCount));

				renderEffect_->EndPass();
			}
		}
	}

	// 结束一帧
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::EndFrame()
	{
		TIF(d3dDevice_->EndScene());
	}

	// 打开/关闭深度测试
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DepthBufferDepthTest(bool depthTest)
	{
		if (depthTest)
		{
			TIF(d3dDevice_->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE));
		}
		else
		{
			TIF(d3dDevice_->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE));
		}
	}

	// 打开/关闭深度缓存
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DepthBufferDepthWrite(bool depthWrite)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_ZWRITEENABLE, depthWrite));
	}

	// 设置深度比较函数
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DepthBufferFunction(CompareFunction depthFunction)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_ZFUNC, D3D9Mapping::Mapping(depthFunction)));
	}

	// 设置深度偏移
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::DepthBias(uint16_t bias)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_DEPTHBIAS, bias));
	}

	// 设置雾化效果
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::Fog(FogMode mode, Color const & color,
		float expDensity, float linearStart, float linearEnd)
	{
		if (Fog_None == mode)
		{
			// just disable
			d3dDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
			d3dDevice_->SetRenderState(D3DRS_FOGENABLE, false);
		}
		else
		{
			// Allow fog
			d3dDevice_->SetRenderState(D3DRS_FOGENABLE, true);

			// Set pixel fog mode
			d3dDevice_->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
			d3dDevice_->SetRenderState(D3DRS_FOGTABLEMODE, D3D9Mapping::Mapping(mode));

			d3dDevice_->SetRenderState(D3DRS_FOGCOLOR, D3D9Mapping::MappingToUInt32Color(color));
			d3dDevice_->SetRenderState(D3DRS_FOGSTART, *reinterpret_cast<uint32_t*>(&linearStart));
			d3dDevice_->SetRenderState(D3DRS_FOGEND, *reinterpret_cast<uint32_t*>(&linearEnd));
			d3dDevice_->SetRenderState(D3DRS_FOGDENSITY, *reinterpret_cast<uint32_t*>(&expDensity));
		}
	}

	// 设置纹理
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::SetTexture(uint32_t stage, TexturePtr const & texture)
	{
		if (!texture)
		{
			TIF(d3dDevice_->SetTexture(stage, NULL));
		}
		else
		{
			assert(dynamic_cast<D3D9Texture*>(texture.get()) != NULL);

			D3D9Texture const & d3d9Tex = static_cast<D3D9Texture const &>(*texture);
			TIF(d3dDevice_->SetTexture(stage, d3d9Tex.D3DBaseTexture().get()));

			// Set addressing mode
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_ADDRESSU,
				D3D9Mapping::Mapping(texture->TextureAddressingMode(Texture::TAT_Addr_U))));
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_ADDRESSV,
				D3D9Mapping::Mapping(texture->TextureAddressingMode(Texture::TAT_Addr_V))));
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_ADDRESSW,
				D3D9Mapping::Mapping(texture->TextureAddressingMode(Texture::TAT_Addr_W))));

			// Set filter
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_MINFILTER,
				D3D9Mapping::MappingToMinFilter(caps_, texture->TextureFiltering(Texture::TFT_Min))));
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_MAGFILTER,
				D3D9Mapping::MappingToMagFilter(caps_, texture->TextureFiltering(Texture::TFT_Mag))));
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_MIPFILTER,
				D3D9Mapping::MappingToMipFilter(caps_, texture->TextureFiltering(Texture::TFT_Mip))));

			// Set anisotropy
			uint32_t max_anisotropy_ = std::min(texture->TextureAnisotropy(), caps_.MaxAnisotropy);
			TIF(d3dDevice_->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, max_anisotropy_));
		}
	}

	// 获取最大纹理阶段数
	/////////////////////////////////////////////////////////////////////////////////
	uint32_t D3D9RenderEngine::MaxTextureStages()
	{
		return caps_.MaxSimultaneousTextures;
	}

	// 计算纹理坐标
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::TextureCoordCalculation(uint32_t stage, TexCoordCalcMethod m)
	{
		HRESULT hr = S_OK;
		D3DXMATRIX matTrans;
		switch (m)
		{
		case TCC_None:
			hr = d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
			D3DXMatrixIdentity(&matTrans);
			hr = d3dDevice_->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &matTrans);
			break;

		case TCC_EnvironmentMap:
			// Sets the flags required for an environment map effect
			hr = d3dDevice_->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
			hr = d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL);
			hr = d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

			D3DXMatrixIdentity(&matTrans);
			matTrans._11 = 0.5f;
			matTrans._41 = 0.5f;
			matTrans._22 = -0.5f;
			matTrans._42 = 0.5f;
			hr = d3dDevice_->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &matTrans);
			break;

		case TCC_EnvironmentMapPlanar:
			// Sets the flags required for an environment map effect
			hr = d3dDevice_->SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
			hr = d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
			hr = d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

			D3DXMatrixIdentity(&matTrans);
			matTrans._11 = 0.5f;
			matTrans._41 = 0.5f;
			matTrans._22 = -0.5f;
			matTrans._42 = 0.5f;
			hr = d3dDevice_->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &matTrans);
			break;
		}

		TIF(hr);
	}

	// 设置纹理矩阵
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::TextureMatrix(uint32_t stage, Matrix4 const & mat)
	{
		if (Matrix4::Identity() == mat)
		{
			TIF(d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
		}
		else
		{
			// Set 2D input
			// TODO: deal with 3D coordinates when cubic environment mapping supported
			TIF(d3dDevice_->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2));

			D3DMATRIX d3dMat(D3D9Mapping::Mapping(mat));
			TIF(d3dDevice_->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &d3dMat));
		}
	}

	// 打开模板缓冲区
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilCheckEnabled(bool enabled)
	{
		if (enabled)
		{
			clearFlags_ |= D3DCLEAR_STENCIL;
		}
		else
		{
			clearFlags_ &= ~D3DCLEAR_STENCIL;
		}

		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILENABLE, enabled));
	}

	// 硬件是否支持模板缓冲区
	/////////////////////////////////////////////////////////////////////////////////
	bool D3D9RenderEngine::HasHardwareStencil()
	{
		IDirect3DSurface9* surf;
		D3DSURFACE_DESC surfDesc;
		d3dDevice_->GetDepthStencilSurface(&surf);
		boost::shared_ptr<IDirect3DSurface9> surf_ptr = MakeCOMPtr(surf);
		surf_ptr->GetDesc(&surfDesc);

		if (D3DFMT_D24S8 == surfDesc.Format)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	// 设置模板位数
	/////////////////////////////////////////////////////////////////////////////////
	uint16_t D3D9RenderEngine::StencilBufferBitDepth()
	{
		return 8;
	}

	// 设置模板比较函数
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferFunction(CompareFunction func)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILFUNC, D3D9Mapping::Mapping(func)));
	}

	// 设置模板缓冲区参考值
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferReferenceValue(uint32_t refValue)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILREF, refValue));
	}

	// 设置模板缓冲区掩码
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferMask(uint32_t mask)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILMASK, mask));
	}

	// 设置模板缓冲区测试失败后的操作
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferFailOperation(StencilOperation op)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILFAIL, D3D9Mapping::Mapping(op)));
	}

	// 设置模板缓冲区深度测试失败后的操作
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferDepthFailOperation(StencilOperation op)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILZFAIL, D3D9Mapping::Mapping(op)));
	}

	// 设置模板缓冲区通过后的操作
	/////////////////////////////////////////////////////////////////////////////////
	void D3D9RenderEngine::StencilBufferPassOperation(StencilOperation op)
	{
		TIF(d3dDevice_->SetRenderState(D3DRS_STENCILPASS, D3D9Mapping::Mapping(op)));
	}
}

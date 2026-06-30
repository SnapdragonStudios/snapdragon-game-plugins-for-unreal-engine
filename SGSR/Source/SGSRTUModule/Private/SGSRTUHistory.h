//============================================================================================================
//
//
//                  Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "CoreMinimal.h"
#include "SceneRendering.h"
#include "HAL/Platform.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8
#include "Templates/RefCounting.h"
#endif
class FSGSRTU;

//////GSR state, deletion handled by RHI
struct FSGSRState : public FRHIResource
{
	FSGSRState()
		: FRHIResource(RRT_None)
		, LastUsedFrame(~0u)
	{
	}
	~FSGSRState()
	{
	}

	uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}

	uint32 Release() const
	{
		return FRHIResource::Release();
	}

	uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	uint64 LastUsedFrame;
	uint32 ViewID;
};
typedef TRefCountPtr<FSGSRState> SGSRstateRef;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
using ICustomTemporalAAHistory = UE::Renderer::Private::ITemporalUpscaler::IHistory;
#endif

/////ICustomTemporalAAHistory for GSR
class FGSRTUHistory final : public ICustomTemporalAAHistory, 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 8
	public FRefCountBase
#else
	public TRefCountingMixin<FGSRTUHistory>
#endif
{
public:
	FGSRTUHistory(SGSRstateRef state, FSGSRTU* upscaler);
	virtual ~FGSRTUHistory();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 2
	virtual const TCHAR* GetDebugName() const override;
	virtual uint64 GetGPUSizeBytes() const override;
#endif

	//////originally in .cpp
	void Setstate(SGSRstateRef state);

	inline SGSRstateRef const& Getstate() const
	{
		return GSR;
	}
#if ENGINE_MINOR_VERSION < 8
#if	ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 5
	uint32 AddRef() const final
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
	FReturnedRefCountValue AddRef() const final
#endif
	{
		return FRefCountBase::AddRef();
	}
	uint32 Release() const final
	{
		return FRefCountBase::Release();
	}

	uint32 GetRefCount() const final
	{
		return FRefCountBase::GetRefCount();
	}
#else
	virtual void AddRef() const override
	{
		TRefCountingMixin<FGSRTUHistory>::AddRef();
	}

	virtual FReturnedRefCountValue Release() const override
	{
		return TRefCountingMixin<FGSRTUHistory>::Release();
	}

	virtual FReturnedRefCountValue GetRefCount() const override
	{
		return TRefCountingMixin<FGSRTUHistory>::GetRefCount();
	}
#endif //ENGINE_MINOR_VERSION < 8



private:
	SGSRstateRef GSR;
	FSGSRTU* upscaler;
};
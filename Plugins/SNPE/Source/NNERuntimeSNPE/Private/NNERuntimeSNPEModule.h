//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#pragma once

#include "Modules/ModuleInterface.h"

#include "NNERuntimeSNPE.h"
#include "UObject/WeakObjectPtrTemplates.h"


class FNNERuntimeSNPEModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TWeakObjectPtr<UNNERuntimeSNPE> NNERuntimeSNPE;
	void* LibraryHandle = nullptr;
};
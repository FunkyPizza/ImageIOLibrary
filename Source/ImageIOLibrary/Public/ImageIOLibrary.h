// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// Copyright Lambda Works, Samuel Metters 2020. All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FImageIOLibraryModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

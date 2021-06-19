// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright Lambda Works, Samuel Metters 2020. All rights reserved.


#pragma once

#include "CoreMinimal.h"


class ImageDialogManager
{

public:
	ImageDialogManager();
	virtual ~ImageDialogManager();

	virtual bool OpenFileDial(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, bool MultipleFiles, TArray<FString>& OutFilenames);

	virtual bool SaveFileDial(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, bool MultipleFiles, TArray<FString>& OutFilenames);
};

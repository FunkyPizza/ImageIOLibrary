// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright Lambda Works, Samuel Metters 2020. All rights reserved.

// This class is responsible for Dialogs on the Windows platform.

#pragma once

#include "CoreMinimal.h"
#include "ImageDialogManager.h"



class ImageDialogManagerWin : public ImageDialogManager
{
public:
	virtual bool OpenFileDial(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, bool MultipleFiles, TArray<FString>& OutFilenames) override;
	virtual bool SaveFileDial(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, bool MultipleFiles, TArray<FString>& OutFilenames)override;
	
private:
	bool FileDialShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, bool MultipleFiles, TArray<FString>& OutFilenames, int32& OutFilterIndex);

};


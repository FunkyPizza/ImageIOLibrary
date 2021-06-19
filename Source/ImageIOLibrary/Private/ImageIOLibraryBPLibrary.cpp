// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright Lambda Works, Samuel Metters 2020. All rights reserved.

#include "ImageIOLibraryBPLibrary.h"
#include "ImageIOLibrary.h"

#include "Runtime/Core/Public/Async/Async.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"

#include "Serialization/BufferArchive.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

UImageIOLibraryBPLibrary::UImageIOLibraryBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

/***** Creating Texture 2D *****/

bool UImageIOLibraryBPLibrary::CreateTexture2DFromImageFile(UTexture2D*& Texture2D, FImageSize &Size, FString PathToImage)
{
	UTexture2D* ReturnTexture2D = nullptr;

	// Check if the file exists first
	if (!FPaths::FileExists(PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Image not found: %s"), *PathToImage);
		return false;
	}

	// Load the compressed byte data from the file
	TArray<uint8> FileData;
	if(!FFileHelper::LoadFileToArray(FileData, *PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load image: %s"), *PathToImage);
		return false;
	}

	//Create an ImageWrapperModule to read image file
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Detect the image type using the ImageWrapper module
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
	if(ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to recognise image format: %s"), *PathToImage);
		return false;
	}

	//Create Texture2D from FileData
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if(ImageWrapper.IsValid() && ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		TArray<uint8> UncompressedRGBA;
		if(ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
		{
			// Create the Texture2D and makes sure it is valid
			ReturnTexture2D = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_R8G8B8A8);
			if (!ReturnTexture2D)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create Texture2D from file: %s"), *PathToImage);
				return false;
			}

			// Saves the texture to memory ready to be used at runtime

			void* TextureData = ReturnTexture2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedRGBA.GetData(), UncompressedRGBA.Num());
			ReturnTexture2D->PlatformData->Mips[0].BulkData.Unlock();
			ReturnTexture2D->UpdateResource();

			Size = FImageSize(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
		}
	}

	Texture2D = ReturnTexture2D;
	return true;
}

bool UImageIOLibraryBPLibrary::CreateTexture2DFromBitmap(UTexture2D*& Texture2D, TArray<FColor> Bitmap, FImageSize Size)
{
	if (Bitmap.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No color data to create the Texture2D with."));
		return false;
	}
	if (Bitmap.Num() != Size.X * Size.Y)
	{

		UE_LOG(LogTemp, Error, TEXT("The size of the input Bitmap doesn't match the input size. (Check CreateTexture2DFromBitmap arguments)."));
		return false;
	}

	UTexture2D* ReturnTexture2D = nullptr;


	//Create an ImageWrapperModule create the Texture2D
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	//Convert ColorData to bytes
	TArray<uint8> FileData;
	FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, FileData);

	// Create an image format to ensure our ColorData is valid
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to recognise image format."));
		return false;
	}

	//Create Image wrapper to create the Texture2D
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create the image wrapper."));
		return false;
	}

	if (ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		TArray<uint8> UncompressedRGBA;
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
		{
			// Create the Texture2D and makes sure it is valid
			ReturnTexture2D = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_R8G8B8A8);
			if (!ReturnTexture2D)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to create Texture2D from ColorData"));
				return false;
			}

			// Saves the texture to memory ready to be used at runtime
			void* TextureData = ReturnTexture2D->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedRGBA.GetData(), UncompressedRGBA.Num());
			ReturnTexture2D->PlatformData->Mips[0].BulkData.Unlock();
			ReturnTexture2D->UpdateResource();
		}

		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to read the compressed image data."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to compress the image data."));
		return false;
	}

	Texture2D = ReturnTexture2D;
	return true;
}

bool UImageIOLibraryBPLibrary::CreateTexture2DFromScreenshot(UTexture2D*& Texture2D, UObject* WorldContextObject)
{
	UTexture2D* ReturnTexture2D;

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);

	if (World)
	{
		UGameViewportClient * ViewportClient = World->GetGameViewport();

		FViewport* InViewport = ViewportClient->Viewport;
		FImageSize Size = FImageSize(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y);

		TArray<FColor> Bitmap;
		bool bScreenshotSuccessful = GetViewportScreenShot(InViewport, Bitmap); //Editor only needs custom game viewport class with overriden Draw() and ReadPixels inside to work in packaged and standalone
		if (bScreenshotSuccessful)
		{
			for (auto& Color : Bitmap)
			{
				Color.A = 255;
			}

			CreateTexture2DFromBitmap(ReturnTexture2D, Bitmap, Size);

			Texture2D = ReturnTexture2D;
			return true;
		}
	}
	return false;
}


/***** Save to disk *****/

bool UImageIOLibraryBPLibrary::SaveBitmapAsPNG(FString FilePath, TArray<FColor> Bitmap, FImageSize Size)
{
	if (Bitmap.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No color data to create the Texture2D with."));
		return false;
	}

	//Convert ColorData to bytes
	TArray<uint8> FileData;
	FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, FileData);
	FFileHelper::SaveArrayToFile(FileData, *FilePath);

	return true;
}

bool UImageIOLibraryBPLibrary::SaveTexture2DAsPNG(UTexture2D* Texture2D, FString FilePath)
{
	TArray<FColor> Bitmap;
	FImageSize dummySize;
	if (GetTextureBitmap(Bitmap, dummySize, Texture2D))
	{
		if (SaveBitmapAsPNG(FilePath, Bitmap, FImageSize( Texture2D->GetSizeX(), Texture2D->GetSizeY())))
		{
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("Could not save Texture2D to disk."));
	return false;
}


/***** Texture 2D *****/

EPixelFormat UImageIOLibraryBPLibrary::GetTexturePixelFormat(bool& Success, UTexture2D* Texture2D)
{
	EPixelFormat PixelFormat = EPixelFormat::PF_A1;

	if (!Texture2D->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("Texture doesn't seem to be valid, can't return pixel format."));
		Success = false;
		return PixelFormat;
	}

	PixelFormat = Texture2D->GetPixelFormat(0);
	Success = true;
	return PixelFormat;
};

bool UImageIOLibraryBPLibrary::GetTextureSize(FImageSize &Size, int &PixelCount, UTexture2D* Texture2D)
{
	if (!Texture2D->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("Texture doesn't seem to be valid, can't return texture size."));
		return false;
	}

	Size = FImageSize(Texture2D->GetSizeX(), Texture2D->GetSizeY());
	PixelCount = (int)Texture2D->GetSizeX() * (int)Texture2D->GetSizeY();
	return true;
}

bool UImageIOLibraryBPLibrary::GetTextureBitmap(TArray<FColor> &Bitmap, FImageSize &Size, UTexture2D* Texture2D)
{
	if (!Texture2D->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("Texture doesn't seem to be valid, can't return coloor data."));
		return false;
	}

	// Backup current texture settings
	TextureCompressionSettings OldCompressionSettings = Texture2D->CompressionSettings;
	//TextureMipGenSettings OldMipGenSettings = Texture2D->MipGenSettings;  //Editor only, doesn't compile in shipping
	bool OldSRGB = Texture2D->SRGB;

	// Set texture settings to make sure LockReadOnly returns its value properly and more efficiently.
	Texture2D->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	//Texture2D->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;  //Editor only, doesn't compile in shipping
	Texture2D->SRGB = false;
	Texture2D->UpdateResource();

	// Get a reference to the texture's color data array
	// This is a pointer to an array of uint8 that is converted to FColor on the fly thanks to the cast.
	FColor* FormatedImageData = (FColor*)Texture2D->PlatformData->Mips[0].BulkData.LockReadOnly();

	//Read the color data
	TArray<FColor> ReturnColorData;
	for (int Y = 0; Y < Texture2D->GetSizeY(); Y++)
	{
		for (int X = 0; X < Texture2D->GetSizeX(); X++)
		{
			//For some reason red and green gets swapped here so we have to swap them again
			FColor tempPixel = FormatedImageData[Y * Texture2D->GetSizeX() + X];
			tempPixel = FColor(tempPixel.B, tempPixel.G, tempPixel.R, tempPixel.A);

			ReturnColorData.Add(tempPixel);
		}
	}

	// Reset the texture settings to what it was
	Texture2D->PlatformData->Mips[0].BulkData.Unlock();
	Texture2D->CompressionSettings = OldCompressionSettings;
	//Texture2D->MipGenSettings = OldMipGenSettings; //Editor only, doesn't compile in shipping
	Texture2D->SRGB = OldSRGB;
	Texture2D->UpdateResource();

	//Returns the color data
	Bitmap = ReturnColorData;
	Size = FImageSize(Texture2D->GetSizeX(), Texture2D->GetSizeY());
	return true;
}

bool UImageIOLibraryBPLibrary::GetTexturePixelColor(FColor &PixelColor, UTexture2D* Texture2D, int XIndex, int YIndex)
{
	if (!Texture2D->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("Texture doesn't seem to be valid, can't return coloor data."));
		return false;
	}

	FVector2D TextureSize = FVector2D(Texture2D->GetSizeX(), Texture2D->GetSizeY());

	FTexture2DMipMap& Mip = Texture2D->PlatformData->Mips[0]; //A reference 
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* raw = NULL;
	raw = (uint8*)Data;

	FColor returnPixelColor = FColor(0, 0, 0, 255);
	returnPixelColor.B = raw[4 * (XIndex * (int)TextureSize.X + YIndex) + 0];
	returnPixelColor.G = raw[4 * (XIndex * (int)TextureSize.X + YIndex) + 1];
	returnPixelColor.R = raw[4 * (XIndex * (int)TextureSize.X + YIndex) + 2];
	returnPixelColor.A = raw[4 * (XIndex * (int)TextureSize.X + YIndex) + 3];

	PixelColor = returnPixelColor;
	return true;
}


/***** Image Operations *****/

EImageIOFormat UImageIOLibraryBPLibrary::GetImageFormat(bool &Success, FString PathToImage)
{
	EImageFormat ImageFormat = EImageFormat::Invalid;
	EImageIOFormat ReturnImageFormat = EImageIOFormat::Invalid;

	// Check if the file exists first
	if (!FPaths::FileExists(PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Image not found: %s"), *PathToImage);

		Success = false;
		return ReturnImageFormat;
	}

	// Load the compressed byte data from the file
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load image: %s"), *PathToImage);

		Success = false;
		return ReturnImageFormat;
	}

	//Create an ImageWrapperModule to read image file
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Detect the image type using the ImageWrapper module
	ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to recognise image format: %s"), *PathToImage);

		Success = false;
		return ReturnImageFormat;
	}

	ReturnImageFormat = EImageFormatToEImageIOFormat(ImageFormat);

	Success = true;
	return ReturnImageFormat;
}

bool UImageIOLibraryBPLibrary::GetImageSize(FImageSize &Size, FString PathToImage)
{
	FImageSize OutImageSize;

	// Check if the file exists first
	if (!FPaths::FileExists(PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Image not found: %s"), *PathToImage);
		return false;
	}


	// Load the compressed byte data from the file
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *PathToImage))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load image: %s"), *PathToImage);
		return false;
	}

	//Create an ImageWrapperModule to read image file
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Detect the image type using the ImageWrapper module
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to recognise image format: %s"), *PathToImage);
		return false;
	}
	//Create Texture2D from FileData
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (ImageWrapper)
	{
		OutImageSize = FImageSize(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
		Size = OutImageSize;
		return true;
	}

	return false;
}

TArray<uint8> UImageIOLibraryBPLibrary::GetBitmapBytes(TArray<FColor> Bitmap, FImageSize Size)
{
	if (Bitmap.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No color data to create the Texture2D with."));
		return TArray<uint8>();
	}

	//Convert ColorData to bytes
	TArray<uint8> FileData;
	FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, FileData);
	return FileData;
}


/***** Bitmap Operations *****/

TArray<FColor> UImageIOLibraryBPLibrary::ResizeBitmap(TArray<FColor> Bitmap, FImageSize Size, FImageSize NewSize)
{
	TArray<FColor> OutBitmap;

	if (Bitmap.Num() > 0 && Size.X != 0 && Size.Y != 0 && NewSize.X != 0 && NewSize.Y != 0)
	{
		FImageUtils::ImageResize(Size.X, Size.Y, Bitmap, NewSize.X, NewSize.Y, OutBitmap, false);
	}
	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::SetBitmapHueSaturationLuminance(TArray<FColor> Bitmap, float Hue, float Saturation, float Luminance)
{
	float tempHueValue = FMath::Clamp(Hue, 0.0f, 360.0f);
	float tempSaturationValue = Saturation; 
	float tempLuminanceValue = Luminance;
	TArray<FColor> OutBitmap;

	for (FColor pixel : Bitmap)
	{
		FLinearColor HSV = FLinearColor::FromSRGBColor(pixel).LinearRGBToHSV();

		// Sets the hue and makes sure values <0 or >360 get looped. (If a pixel's hue = -1, it will become 359, if it is = 361 it will become 1)
			float newHue = HSV.R + tempHueValue;
			if (newHue >= 360.0f)
			{
				HSV.R = FMath::Clamp(newHue - 360.0f, 0.0f, 360.0f);
			}
			else if (newHue <= 0.0f)
			{
				HSV.R = FMath::Clamp(newHue + 360.0f, 0.0f, 360.0f);
			}
			else
			{
				HSV.R = newHue;
			}

		// Sets the saturation 
			HSV.G = FMath::Clamp(HSV.G * tempSaturationValue, 0.0f, 1.0f);

		// Sets the luminance
			HSV.B = FMath::Clamp(HSV.B * tempLuminanceValue, 0.0f, 1.0f);

		OutBitmap.Add(HSV.HSVToLinearRGB().ToFColor(true));
	}

	return OutBitmap;

}

TArray<FColor> UImageIOLibraryBPLibrary::SetBitmapContrast(TArray<FColor> Bitmap, float Contrast)
{
	float tempContrast = FMath::GetMappedRangeValueClamped(FVector2D(0, 2), FVector2D(-255, 255), Contrast);
	float factor = (259.0 * (tempContrast + 255.0)) / (255.0 * (259.0 - tempContrast));
	TArray<FColor> OutBitmap;

	for (FColor pixel : Bitmap)
	{
		OutBitmap.Add(FColor(
			FMath::Clamp(factor * (pixel.R - 128) + 128, 0.0f, 255.0f),
			FMath::Clamp(factor * (pixel.G - 128) + 128, 0.0f, 255.0f),
			FMath::Clamp(factor * (pixel.B - 128) + 128, 0.0f, 255.0f)
			));
	}

	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::SetBitmapBrightness(TArray<FColor> Bitmap, float Brightness)
{
	float tempBrightness = FMath::GetMappedRangeValueClamped(FVector2D(0, 2), FVector2D(-255, 255), Brightness);
	TArray<FColor> OutBitmap;

	for (FColor pixel : Bitmap)
	{
		OutBitmap.Add(FColor(
			FMath::Clamp(pixel.R + tempBrightness, 0.0f, 255.0f),
			FMath::Clamp(pixel.G + tempBrightness, 0.0f, 255.0f),
			FMath::Clamp(pixel.B + tempBrightness, 0.0f, 255.0f)
			));
	}

	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::Add_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB)
{
	TArray<FColor> OutBitmap;

	if (BitmapA.Num() > 0 && BitmapB.Num() > 0)
	{
		int minIndex = (BitmapA.Num() < BitmapB.Num()) ? BitmapA.Num() : BitmapB.Num();

		for (int i = 0; i < minIndex; i++)
		{
			if (BitmapA[i].A == BitmapB[i].A == 0)
			{
				FColor pixel = FColor(0, 0, 0, 0);
				OutBitmap.Add(pixel);
			}

			else 
			{
				FColor pixel = BitmapA[i];
				pixel += BitmapB[i];
				OutBitmap.Add(pixel);
			}
		}

		return OutBitmap;
	}

	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::Multiply_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB)
{
	TArray<FColor> OutBitmap;

	if (BitmapA.Num() > 0 && BitmapB.Num() > 0)
	{
		int minIndex = (BitmapA.Num() < BitmapB.Num()) ? BitmapA.Num() : BitmapB.Num();

		for (int i = 0; i < minIndex; i++)
		{
			FLinearColor pixel; 
			FLinearColor A = BitmapA[i];
			FLinearColor B = BitmapB[i];
			pixel.R = FMath::Clamp((A.R * B.R), 0.0f, 1.0f);
			pixel.G = FMath::Clamp((A.G * B.G), 0.0f, 1.0f);
			pixel.B = FMath::Clamp((A.B * B.B), 0.0f, 1.0f);
			pixel.A = FMath::Clamp((A.A * B.A), 0.0f, 1.0f);
			OutBitmap.Add(pixel.ToFColor(false));
		}

		return OutBitmap;
	}

	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::Divide_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB)
{
	TArray<FColor> OutBitmap;

	if (BitmapA.Num() > 0 && BitmapB.Num() > 0)
	{
		int minIndex = (BitmapA.Num() < BitmapB.Num()) ? BitmapA.Num() : BitmapB.Num();

		for (int i = 0; i < minIndex; i++)
		{
			FLinearColor pixel;
			FLinearColor A = BitmapA[i];
			FLinearColor B = BitmapB[i];
			pixel.R = FMath::Clamp((A.R / B.R), 0.0f, 1.0f);
			pixel.G = FMath::Clamp((A.G / B.G), 0.0f, 1.0f);
			pixel.B = FMath::Clamp((A.B / B.B), 0.0f, 1.0f);
			pixel.A = FMath::Clamp((A.A / B.A), 0.0f, 1.0f);
			OutBitmap.Add(pixel.ToFColor(false));
		}

		return OutBitmap;
	}

	return OutBitmap;
}


TArray<FColor> UImageIOLibraryBPLibrary::Add_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint)
{
	TArray<FColor> OutBitmap;
	TArray<FColor> TintBitmap;

	if (BitmapA.Num() > 0)
	{
		TintBitmap.SetNumUninitialized(BitmapA.Num());
		for(int i = 0; i < TintBitmap.Num(); i++)
		{
			TintBitmap[i] = Tint.ToFColor(false);
		}

		OutBitmap = Add_Bitmap(BitmapA, TintBitmap);
	}

	return OutBitmap;

}

TArray<FColor> UImageIOLibraryBPLibrary::Multiply_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint)
{
	TArray<FColor> OutBitmap;
	TArray<FColor> TintBitmap;

	if (BitmapA.Num() > 0)
	{
		TintBitmap.SetNumUninitialized(BitmapA.Num());
		for (int i = 0; i < TintBitmap.Num(); i++)
		{
			TintBitmap[i] = Tint.ToFColor(false);
		}

		OutBitmap = Multiply_Bitmap(BitmapA, TintBitmap);
	}

	return OutBitmap;
}

TArray<FColor> UImageIOLibraryBPLibrary::Divide_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint)
{
	TArray<FColor> OutBitmap;
	TArray<FColor> TintBitmap;

	if (BitmapA.Num() > 0)
	{
		TintBitmap.SetNumUninitialized(BitmapA.Num());
		for (int i = 0; i < TintBitmap.Num(); i++)
		{
			TintBitmap[i] = Tint.ToFColor(false);
		}

		OutBitmap = Divide_Bitmap(BitmapA, TintBitmap);
	}

	return OutBitmap;
}


/***** Bitmap Filters *****/

TArray<FColor> UImageIOLibraryBPLibrary::ApplyBitmapFilter(TArray<FColor> Bitmap, FImageSize Size, FBitmapFilter Filter)
{
	TArray<FColor> OutBitmap;

	// Parse the filter struct into individual variables
	int filterWidth = Filter.Size.X;
	int filterHeight = Filter.Size.Y;
	TArray<float> filter = Filter.Filter;
	float factor = Filter.Factor;
	float bias = Filter.Bias;

	int offset = FMath::FloorToInt(filter.Num() / 2);

	// For each pixel in the bitmap
	for (int Y = 0; Y < Size.Y; Y++) {
		for (int X = 0; X < Size.X; X++) {

			// Working pixel's index in the bitmap
			int pixelIndex = FMath::Clamp(Y * Size.X + X, 0, Bitmap.Num() - 1);

			//New working pixel's default values
			uint8 tempRed = 0;
			uint8 tempGreen = 0;
			uint8 tempBlue = 0;
			uint8 tempAlpha = Filter.ColourChannel == EFilterColourChannel::RGBA ? Bitmap[pixelIndex].A : (uint8)255;

			// Calculate the new colour of the working pixel
			for (int filterY = 0; filterY < filterHeight; filterY++) {
				for (int filterX = 0; filterX < filterWidth; filterX++) {

					// Get the working pixel's neighbours' index in the bitmap
					int nPixelX = (X + filterX - FMath::FloorToInt(filterWidth / 2));
					int nPixelY = (Y + filterY - FMath::FloorToInt(filterHeight / 2));
					int nPixelIndex = FMath::Clamp(nPixelY * Size.X + nPixelX, 0, Bitmap.Num() - 1);

					//Get the corresponding filter index to apply
					int nFilterIndex = FMath::Clamp(filterY * filterWidth + filterX, 0, filter.Num() - 1);

					tempRed += Bitmap[nPixelIndex].R * (filter[nFilterIndex] * Filter.Factor) + Filter.Bias;
					tempGreen += Bitmap[nPixelIndex].G * (filter[nFilterIndex] * Filter.Factor) + Filter.Bias;
					tempBlue += Bitmap[nPixelIndex].B * (filter[nFilterIndex] * Filter.Factor) + Filter.Bias;
				}
			}

 			//OutBitmap.Add(SetPixelColourChannel(FColor(tempRed, tempGreen, tempBlue, tempAlpha), Filter.ColourChannel));
			OutBitmap.EmplaceAt(pixelIndex, SetPixelColourChannel(FColor(tempRed, tempGreen, tempBlue, tempAlpha), Filter.ColourChannel));

		}
	}
	return OutBitmap;
}

FBitmapFilter UImageIOLibraryBPLibrary::GetBitmapFilter(EBitmapFilterType BitmapFilter, bool OverrideColourChannel, EFilterColourChannel ColourChannelOverride)
{
	// See https://en.wikipedia.org/wiki/Kernel_(image_processing) or https://setosa.io/ev/image-kernels/

	switch (BitmapFilter)
	{
	case Identity:
		return FBitmapFilter(FImageSize(3, 3), { 0,0,0,0,1,0,0,0,0 }, 1.0f, 0.0f, OverrideColourChannel? ColourChannelOverride : EFilterColourChannel::RGBA);

	case BoxBlur:
		return FBitmapFilter(FImageSize(3, 3), { 1,1,1,1,1,1,1,1,1 }, 0.11111111f, 0.0f, OverrideColourChannel ? ColourChannelOverride : EFilterColourChannel::RGBA);

	case Gaussian1:
		return FBitmapFilter(FImageSize(3, 3), { 1,2,1,2,4,2,1,2,1 }, 0.0625f, 0.0f, OverrideColourChannel ? ColourChannelOverride : EFilterColourChannel::RGBA);

	case Gaussian2:
		return FBitmapFilter(FImageSize(5, 5), { 1,4,6,4,1,4,16,24,16,4,6,24,36,24,6,4,16,24,16,4,1,4,6,4,1 }, 0.00390625f, 0.0f, OverrideColourChannel ? ColourChannelOverride : EFilterColourChannel::RGBA);

	case Sharpen:
		return FBitmapFilter(FImageSize(3, 3), { 0,-1, 0, -1 , 5, -1, 0,-1, 0 }, 1.0f, 0.0f, OverrideColourChannel ? ColourChannelOverride : EFilterColourChannel::RGBA);

	case EdgeDetection:
		return FBitmapFilter(FImageSize(3, 3), { -1,-1,-1,-1,8,-1,-1,-1,-1 }, 1.0f, 0.0f, OverrideColourChannel ? ColourChannelOverride : EFilterColourChannel::Greyscale);
	}

	return FBitmapFilter();
}


FColor UImageIOLibraryBPLibrary::SetPixelColourChannel(FColor Pixel, EFilterColourChannel ColourChannel)
{
	FColor OutPixel;

	switch (ColourChannel)
	{
	case EFilterColourChannel::RGB:
		OutPixel.R = Pixel.R;
		OutPixel.G = Pixel.G;
		OutPixel.B = Pixel.B;
		OutPixel.A = (uint8)255;
		break;
		return OutPixel;

	case EFilterColourChannel::RGBA:
		OutPixel.R = Pixel.R;
		OutPixel.G = Pixel.G;
		OutPixel.B = Pixel.B;
		OutPixel.A = Pixel.A;
		break;
		return OutPixel;

	case EFilterColourChannel::R:
		OutPixel.R = Pixel.R;
		OutPixel.G = (uint8)0;
		OutPixel.B = (uint8)0;
		OutPixel.A = (uint8)255;
		break;
		return OutPixel;

	case EFilterColourChannel::G:
		OutPixel.R = (uint8)0;
		OutPixel.G = Pixel.G;
		OutPixel.B = (uint8)0;
		OutPixel.A = (uint8)255;
		break;
		return OutPixel;

	case EFilterColourChannel::B:
		OutPixel.R = (uint8)0;
		OutPixel.G = (uint8)0;
		OutPixel.B = Pixel.B;
		OutPixel.A = (uint8)255;
		break;
		return OutPixel;

	case EFilterColourChannel::A:
		OutPixel.R = Pixel.A;
		OutPixel.G = Pixel.A;
		OutPixel.B = Pixel.A;
		OutPixel.A = (uint8)0;
		break;
		return OutPixel;

	case EFilterColourChannel::Greyscale:

		uint8 greyscaledPixel = (Pixel.R * 0.2989) + (Pixel.G * 0.5870) + (Pixel.B * 0.1140);

		OutPixel.R = greyscaledPixel;
		OutPixel.G = greyscaledPixel;
		OutPixel.B = greyscaledPixel;
		OutPixel.A = (uint8)255;
		break;
		return OutPixel;
	}
	return OutPixel;
}

/***** Private *****/

EImageIOFormat UImageIOLibraryBPLibrary::EImageFormatToEImageIOFormat(EImageFormat ImageFormat)
{
	switch (ImageFormat)
	{
	default:
		return EImageIOFormat::Invalid;

	case EImageFormat::Invalid:
		return EImageIOFormat::Invalid;

	case EImageFormat::PNG:
		return EImageIOFormat::PNG;

	case EImageFormat::JPEG:
		return EImageIOFormat::JPEG;

	case EImageFormat::GrayscaleJPEG:
		return EImageIOFormat::GrayscaleJPEG;

	case EImageFormat::BMP:
		return EImageIOFormat::BMP;

	case EImageFormat::ICO:
		return EImageIOFormat::ICO;

	case EImageFormat::EXR:
		return EImageIOFormat::EXR;

	case EImageFormat::ICNS:
		return EImageIOFormat::ICNS;
	}
}

EImageFormat UImageIOLibraryBPLibrary::EImageIOFormatToEImageFormat(EImageIOFormat ImageFormat)
{
	switch (ImageFormat)
	{
	default:
		return EImageFormat::Invalid;

	case EImageIOFormat::Invalid:
		return EImageFormat::Invalid;

	case EImageIOFormat::PNG:
		return EImageFormat::PNG;

	case EImageIOFormat::JPEG:
		return EImageFormat::JPEG;

	case EImageIOFormat::GrayscaleJPEG:
		return EImageFormat::GrayscaleJPEG;

	case EImageIOFormat::BMP:
		return EImageFormat::BMP;

	case EImageIOFormat::ICO:
		return EImageFormat::ICO;

	case EImageIOFormat::EXR:
		return EImageFormat::EXR;

	case EImageIOFormat::ICNS:
		return EImageFormat::ICNS;
	}
}



/***** WORK IN PROGRESS *****/

bool UImageIOLibraryBPLibrary::SaveTexture2D(UTexture2D* Texture2D, EImageIOFormat ImageFormat, FString FilePath)
{
	//Create an ImageWrapperModule create the Texture2D
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	//Convert ColorData to bytes
	TArray<FColor> Bitmap;
	FImageSize dummySize;
	GetTextureBitmap(Bitmap, dummySize, Texture2D);
	TArray<uint8> FileData;
	FImageUtils::CompressImageArray(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Bitmap, FileData);

	// Create an image format to ensure our ColorData is valid
	EImageFormat tempImageFormat = EImageIOFormatToEImageFormat(ImageFormat);
	if (tempImageFormat == EImageFormat::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to recognise image format."));
		return false;
	}

	//Create Image wrapper to create the Texture2D
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(tempImageFormat);
	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create the image wrapper."));
		return false;
	}

	if (ImageWrapper->SetRaw(FileData.GetData(), FileData.Num(), Texture2D->GetSizeX(), Texture2D->GetSizeY(), ERGBFormat::RGBA, 8))
	{
		FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(0), *FilePath);
		return true;
	}

	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed compress the image data."));
	}
	return false;
}

void UImageIOLibraryBPLibrary::BlurBitmapAsync(const FOnBitmapBlurred& OnBitmapBlurComplete, TArray<FColor> Bitmap, FImageSize Size, float BlurStrength, int BlurRadius)
{
	Async(EAsyncExecution::TaskGraphMainThread, [&]()
	{
		TArray<FColor> Result = ApplyBitmapFilter(Bitmap, Size, FBitmapFilter());
		UTexture2D* ResultTexture;
		CreateTexture2DFromBitmap(ResultTexture, Result, Size);
		OnBitmapBlurComplete.ExecuteIfBound(ResultTexture);
	});
}
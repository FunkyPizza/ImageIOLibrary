// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright Lambda Works, Samuel Metters 2020. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "Math/Vector2D.h"
#include "ImageDialogManager.h"
#include "Win/ImageDialogManagerWin.h"
#include "Mac/ImageDialogManagerMac.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Engine.h"
#include "IImageWrapperModule.h"
#include "ImageIOLibraryBPLibrary.generated.h"

/* Image format to import/Export */
UENUM(BlueprintType)
enum class EImageIOFormat : uint8
{
	/** Invalid or unrecognized format. */
	Invalid = 0 UMETA(DisplayName = "Invalid"),

	/** Portable Network Graphics. */
	PNG = 1 UMETA(DisplayName = "PNG"),

	/** Joint Photographic Experts Group. */
	JPEG UMETA(DisplayName = "JPEG"),

	/** Single channel JPEG. */
	GrayscaleJPEG UMETA(DisplayName = "GrayscaleJPEG"),

	/** Windows Bitmap. */
	BMP UMETA(DisplayName = "BMP"),

	/** Windows Icon resource. */
	ICO UMETA(DisplayName = "ICO"),

	/** OpenEXR (HDR) image file format. */
	EXR UMETA(DisplayName = "EXR"),

	/** Mac icon. */
	ICNS UMETA(DisplayName = "ICNS")

};

/* Image format to import/Export */
UENUM(BlueprintType)
enum class EFilterColourChannel : uint8
{
	/** Apply the filter on RGB channels (preserving alpha values) */
	RGB			UMETA(DisplayName = "RGB"),

	/** Apply the filter on all 4 channels */
	RGBA		UMETA(DisplayName = "RGBA"),

	/** Apply the filter only on the Red channel (only returns this channel) */
	R			UMETA(DisplayName = "R"),

	/** Apply the filter only on the Green channel (only returns this channel) */
	G			UMETA(DisplayName = "G"),

	/** Apply the filter only on the Blue channel (only returns this channel) */
	B			UMETA(DisplayName = "B"),

	/** Apply the filter only on the Alpha channel (only returns this channel) */
	A			UMETA(DisplayName = "A (Alpha)"),

	/** Convert the image to greyscale and apply the filter on it. */
	Greyscale	UMETA(DisplayName = "Greyscale"),

};

/* My compiler had issues with FVector2D, so recreated this to make it work. */
USTRUCT(BlueprintType)
struct FImageSize
{
	GENERATED_BODY()

		UPROPERTY(BlueprintReadWrite, Category = "Image Size Property")
		int X;

	UPROPERTY(BlueprintReadWrite, Category = "Image Size Property")
		int Y;

	FImageSize()
	{
		X = 0;
		Y = 0;
	}

	FImageSize(int inSizeX, int inSizeY)
	{
		X = inSizeX;
		Y = inSizeY;
	}

	FImageSize(FVector2D InVector2D)
	{
		X = InVector2D.X;
		Y = InVector2D.Y;
	}
};

/* Bitmap filter, Kernel, Convolution Matrix there are so many names for these. See https://en.wikipedia.org/wiki/Kernel_(image_processing) .*/
USTRUCT(BlueprintType)
struct FBitmapFilter 
{
	GENERATED_BODY()

	/* Size of the filter. */
	UPROPERTY(BlueprintReadWrite, Category = "FilterProperty")
	FImageSize Size;

	/* The filter's matrix values. */
	UPROPERTY(BlueprintReadWrite, Category = "FilterProperty")
	TArray<float> Filter;

	/* Matrix value multiplier, useful only if you want to work with integer matrix values, else leave default (=1). */
	UPROPERTY(BlueprintReadWrite, Category = "FilterProperty")
	float Factor;
	
	/* Matrix value addition, gets added to the filter values when applied. */
	UPROPERTY(BlueprintReadWrite, Category = "FilterProperty")
	float Bias;

	/* Useful if you want to only apply the filter to a specifc channel. */
	UPROPERTY(BlueprintReadWrite, Category = "FilterProperty")
	EFilterColourChannel ColourChannel;

	FBitmapFilter()
	{
		Size = FImageSize(3,3);
		Filter = {
	   0,0,0,
	   0,1,0,
	   0,0,0	
	};
		Factor = 1.0f;
		Bias = 0.0f;
		ColourChannel = EFilterColourChannel::RGB;
	}

	FBitmapFilter(FImageSize InSize, TArray<float> InFilter)
	{
		Size = InSize;
		Filter = InFilter;
		Factor = 1.0f;
		Bias = 0.0f;
		ColourChannel = EFilterColourChannel::RGB;
	}

	FBitmapFilter(FImageSize InSize, TArray<float> InFilter, float InFactor, float InBias, EFilterColourChannel InColourChannel)
	{
		Size = InSize;
		Filter = InFilter;
		Factor = InFactor;
		Bias = InBias; 
		ColourChannel = InColourChannel;
	}
};

/* List the filters that were hard coded here. */
UENUM(BlueprintType)
enum EBitmapFilterType
{
	Identity			UMETA(DisplayName = "Identity (No filter)"),
	Sharpen				UMETA(DisplayName = "Sharpen (3x3)"),
	BoxBlur				UMETA(DisplayName = "Box Blur"),
	Gaussian1			UMETA(DisplayName = "Gaussian (3x3)"),
	Gaussian2			UMETA(DisplayName = "Gaussian (5x5)"),
	EdgeDetection		UMETA(DisplayName = "Edge Detection"),
};


//DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnBitmapBlurred, TArray<FColor>, OutBitmap, FImageSize, OutSize);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnBitmapBlurred, UTexture2D*, Texture2D);

UCLASS()
class UImageIOLibraryBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/***** Creating Texture 2D *****/

	/* Loads the image at the specified path and returns a Texture2D. Supports PNG, JPEG, EXR, BMP, ICO and ICNS.
	@param PathToImage	Path to the image file to load.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "CreateTexture2DFromImageFile", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool CreateTexture2DFromImageFile(UTexture2D*& Texture2D, FImageSize &Size, FString PathToImage);

	/* Creates a texture 2D from the specified Bitmap and image size.
	@param Bitmap	The bitmap to create the Texture2D from.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "CreateTexture2DFromBitmap", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool CreateTexture2DFromBitmap(UTexture2D*& Texture2D, TArray<FColor> Bitmap, FImageSize Size);

	/* Takes a screenshot and returns it as a Texture 2D. This doesn't capture the UI.
	@param WorldContextObject	This has to be any valid object that is instanced in the world (if called from an actor or widget, you can use the Self node).
	*/
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", DisplayName = "CreateTexture2DFromScreenshot(PIEEditorOnly)", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool CreateTexture2DFromScreenshot(UTexture2D*& Texture2D, UObject* WorldContextObject);


	/***** Texture 2D *****/

	/* Gets the pixel format of the specified Texture 2D.
	@param Texture2D	The texture to get the pixel format from.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTexturePixelFormat", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static EPixelFormat GetTexturePixelFormat(bool& Success, UTexture2D* Texture2D);

	/* Gets the resolution of the image used for the specified Texture2D.
	@param Texture2D	The texture to get the size from.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTextureSize", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool GetTextureSize(FImageSize &Size, int &PixelCount, UTexture2D* Texture2D);

	/* It will return a FColor for every single pixel of the specified texture. 
	This process is not async! This means it can freeze the game while processing.
	@param Texture2D	The texture to get the bitmap from.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTextureBitmap", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool GetTextureBitmap(TArray<FColor> &Bitmap, FImageSize &Size, UTexture2D* Texture2D);

	/* Returns the FColor of a specific pixel in the input Texture 2D.
	@param Texture2D	The texture to get the pixel from.
	@param XIndex		X position of the pixel to read.
	@param YIndex		Y position of the pixel to read.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetTexturePixelColor", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool GetTexturePixelColor(FColor &PixelColor, UTexture2D* Texture2D, int XIndex, int YIndex);


	/***** Save to disk *****/

	/* Saves the specified Texture2D as a PNG file. Make sure to include ".png" in the filepath.
	@param Texture2D	The texture to save.
	@param FilePath		The path to save the image to.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveTexture2DAsPNG", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
		static bool SaveTexture2DAsPNG(UTexture2D* Texture2D, FString FilePath);

	/* Saves the specified Bitmap as a PNG file. Make sure to include ".png" in the filepath.
	@param FilePath		The path to save the image to.
	@param Bitmap		The bitmap to save.
	@param Size			The bitmap's resolution.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveBitmapAsPNG", Keywords = "ImageIOLibrary"), Category = "ImageIOLibrary")
		static bool SaveBitmapAsPNG(FString FilePath, TArray<FColor> Bitmap, FImageSize Size);


	/***** Image Operations *****/

	/* Returns the file format of the image file at the specified path. Returns Invalid if the file format isn't supported.
	@param PathToImage	The path to the image file.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetImageFormat", Keywords = "ImageIOLibrary"), Category = "ImageIOLibrary")
		static EImageIOFormat GetImageFormat(bool &Success, FString PathToImage);

	/* Returns the size of the image file at the specified path. Returns false if the file format isn't supported.
	@param PathToImage	The path to the image file.
	@param Size			The resolution of the bitmap to edit.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetImageSize", Keywords = "ImageIOLibrary"), Category = "ImageIOLibrary")
		static bool GetImageSize(FImageSize &Size, FString PathToImage);


	/***** Bitmap Operations *****/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetBitmapBytes", Keywords = "ImageIOLibrary bitmap resize"), Category = "ImageIOLibrary")
	static TArray<uint8> GetBitmapBytes(TArray<FColor> Bitmap, FImageSize Size);

	/* This resizes the resolution of a Bitmap image. Use this to make sure two bitmaps have the same resolution before performing operations on them. 
	@param Bitmap		The bitmap to edit.
	@param Size			The resolution of the bitmap to edit.
	@param NewSize		The bitmap's new resolution.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ResizeBitmap", Keywords = "ImageIOLibrary bitmap resize"), Category = "ImageIOLibrary")
		static TArray<FColor> ResizeBitmap(TArray<FColor> Bitmap, FImageSize Size, FImageSize NewSize);

	/** Sets the bitmap's Hue, Saturation and Lumniance values (HSV values). Value range from 0 to 2 except hue which is 0-360. This is a destructive action! Changes cannot be undone using the returned bitmap.
	@param Bitmap		The bitmap to edit.
	@param Size			The resolution of the bitmap to edit.
	@param Hue			The bitmap's new Hue value (0 to 360). Default is 0.
	@param Saturation	The bitmap's new Saturation multiplier (works best between 0 to 2). Default is 1.
	@param Luminance	The bitmap's new Luminance or Value multiplier (works best between 0 to 2). Default is 1.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "SetBitmapHueSaturationLuminance", Keywords = "ImageIOLibrary bitmap hue saturation luminance value"), Category = "ImageIOLibrary")
		static TArray<FColor> SetBitmapHueSaturationLuminance(TArray<FColor> Bitmap, float Hue = 0.0f, float Saturation = 1.0f, float Luminance = 1.0f);

	/** Sets the bitmap's contrast. Value range from 0 to 2. This is a destructive action! Changes cannot be undone using the returned bitmap.
	@param Bitmap		The bitmap to edit.
	@param Size			The resolution of the bitmap to edit.
	@param Contrast		The bitmap's new contrast value (0 to 2). Default is 1.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "SetBitmapContrast", Keywords = "ImageIOLibrary bitmap contrast value"), Category = "ImageIOLibrary")
		static TArray<FColor> SetBitmapContrast(TArray<FColor> Bitmap, float Contrast = 1.0f);

	/** Sets the bitmap's brightness.  Value range from 0 to 2. This is a destructive action! Changes cannot be undone using the returned bitmap.
@param Bitmap		The bitmap to edit.
@param Size			The resolution of the bitmap to edit.
@param Brightness		The bitmap's new brightness value (0 to 2). Default is 1.
*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "SetBitmapBrightness", Keywords = "ImageIOLibrary bitmap Brightness value"), Category = "ImageIOLibrary")
		static TArray<FColor> SetBitmapBrightness(TArray<FColor> Bitmap, float Brightness = 1.0f);

		/* Addition Bitmap + Bitmap. This simulates the Additive blend mode. Make sure both images have the same resolution. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap + Bitmap", CompactNodeTitle = "+", Keywords = "ImageIOLibrary + add plus", CommutativeAssociativeBinaryOperator = "true"), Category = "ImageIOLibrary")
		static TArray<FColor> Add_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB);

	/* Multiplication Bitmap * Bitmap. This simulates the Multiply blend mode. Make sure both images have the same resolution. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap * Bitmap", CompactNodeTitle = "*", Keywords = "ImageIOLibrary * multiply", CommutativeAssociativeBinaryOperator = "true"), Category = "ImageIOLibrary")
		static TArray<FColor> Multiply_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB);

	/* Division Bitmap / Bitmap. This simulates the Divide blend mode. Make sure both images have the same resolution. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap / Bitmap", CompactNodeTitle = "/", Keywords = "ImageIOLibrary / divide", CommutativeAssociativeBinaryOperator = "true"), Category = "ImageIOLibrary")
		static TArray<FColor> Divide_Bitmap(TArray<FColor> BitmapA, TArray<FColor> BitmapB);

	/* Addition Bitmap + Color. This simulates the Additive blend mode. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap + Color", CompactNodeTitle = "+", Keywords = "ImageIOLibrary + add plus tint"), Category = "ImageIOLibrary")
		static TArray<FColor> Add_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint);

	/* Multiplication Bitmap * Color. This simulates the Multiply blend mode. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap * Color", CompactNodeTitle = "*", Keywords = "ImageIOLibrary * multiply tint"), Category = "ImageIOLibrary")
		static TArray<FColor> Multiply_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint);

	/* Division Bitmap / Color. This simulates the Divide blend mode. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitmap / Color", CompactNodeTitle = "/", Keywords = "ImageIOLibrary / divide tint"), Category = "ImageIOLibrary")
		static TArray<FColor> Divide_ColorBitmap(TArray<FColor> BitmapA, FLinearColor Tint);


	/***** Bitmap Filters *****/

/* This applies the input filter on the input bitmap by convolution. Use GetBitmapFilter() or look up Image filtering kernels to create your own filters.
@param Bitmap		The bitmap to edit.
@param Size			The resolution of the bitmap to edit.
@param Filter		This is the filter you wish to apply to the bitmap. (See "Kernel (Image Processing)" on Wikipedia).
*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ApplyBitmapFilter", Keywords = "ImageIOLibrary bitmap filter blur sharpen"), Category = "ImageIOLibrary")
		static TArray<FColor> ApplyBitmapFilter(TArray<FColor> Bitmap, FImageSize Size, FBitmapFilter Filter);

	/* This returns filters based on the BitmapFilter enum. Some filters won't work if applied to all channels (RGBA) though you can override it if you wish so.
	@param BitmapFilter				Select which hardcode filter to return.
	@param OverrideColourChannel	Tick this if you want to override the default colour channel(s) the filter is applied on.
	@param ColourChannelOverride	If OverrideColourChannel is true, this is the colour channel(s) to override the filter's default with.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetBitmapFilter", Keywords = "ImageIOLibrary bitmap filter blur sharpen"), Category = "ImageIOLibrary")
		static FBitmapFilter GetBitmapFilter(EBitmapFilterType BitmapFilter, bool OverrideColourChannel, EFilterColourChannel ColourChannelOverride);

	/* This returns filters based on the BitmapFilter enum.
@param BitmapFilter				Select which hardcode filter to return.
@param OverrideColourChannel	Tick this if you want to override the default colour channel(s) the filter is applied on.
@param ColourChannelOverride	If OverrideColourChannel is true, this is the colour channel(s) to override the filter's default with.
*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "SetPixelColourChannel", Keywords = "ImageIOLibrary bitmap colour color channel pixel"), Category = "ImageIOLibrary")
		static FColor SetPixelColourChannel(FColor Pixel, EFilterColourChannel ColourChannel);


	/***** Open/Save file dialogs *****/

	/*This will open a Folder Select dialog. The FilePath return value contain the path for the file selected, its name and its extension.
@param DialogTitle		Title of the dialog window.
@param DefaultPath		Path to open by default (default is blank).
@param FileTypes		The file type filter (you can add as many as you need). The format is: [Type Name] (*.[Type Extension]*)|*.[Type Extension]*|
*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OpenFileDialog", Keywords = "ImageIOLibrary open file save"), Category = "ImageIOLibrary")
		static FORCEINLINE bool OpenFileDialog(FString &FilePath, FString DialogTitle = "Select a file", FString DefaultPath = "", FString FileTypes = "All Files (*.*)|*.*|")
	{
		if (GEngine && GEngine->GameViewport)
		{
			const void *ParentWindowHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();

			if (ParentWindowHandle)
			{
				TArray<FString> pathsToFiles;

				// Use IDesktopPlatform (editor and dev only)
#if PLATFORM_LINUX
				IDesktopPlatform *DesktopPlatform = FDesktopPlatformModule::Get();
				if (!DesktopPlatform || !ParentWindowHandle)
				{
					// Couldn't initialise the platform's references
					return false;
				}

				else
				{
					EFileDialogFlags::Type Flags;

					// Set the flag based on param
					if (false)
					{
						Flags = EFileDialogFlags::Type::Multiple;
					}
					else
					{
						Flags = EFileDialogFlags::Type::None;
					}

					// Opens the dialog window
					if (DesktopPlatform->OpenFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, FString(""), FileTypes, Flags, pathsToFiles))
					{
						// Checks that there is at least 1 path to return
						if (pathsToFiles.Num() > 0)
						{
							// Checks that the first path isn't empty (this could be improved)
							if (pathsToFiles[0] != "")
							{
								// Success
								FilePath = pathsToFiles[0];
								return true;
							}
						}
					}
				}

				// Failure
				return false;
#endif

				// Use DialogManager.h for both windows and mac
				ImageDialogManager *DialogMan;
#if PLATFORM_WINDOWS

				DialogMan = new ImageDialogManagerWin();

#endif

#if PLATFORM_MAC

				DialogMan = new ImageDialogManagerMac();
#endif

				if (DialogMan)
				{
					if (DialogMan->OpenFileDial(ParentWindowHandle, DialogTitle, DefaultPath, TEXT(""), FileTypes, false, pathsToFiles))
					{
						if (pathsToFiles[0] != TEXT(""))
						{
							FilePath = pathsToFiles[0];

							delete DialogMan;
							return true;
						}
					}

					delete DialogMan;
				}
			}
		}
		return false;
	}

	/*This will open a File Save dialog. The return value contains the path to the file selected, its name and extension.
	@param DialogTitle		Title of the dialog window.
	@param DefaultPath		Path to open by default (default is blank).
	@param DefaultFileName	Name to give the file by default.
	@param FileTypes		The file type filter (you can add as many as you need). The format is: [Type Name] (*.[Type Extension]*)|*.[Type Extension]*|
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveFileDialog", Keywords = "ImageIOLibrary open file save"), Category = "ImageIOLibrary")
		static FORCEINLINE bool SaveFileDialog(FString &SaveToPath, FString DialogTitle = "Select a file", FString DefaultPath = "", FString DefaultFileName = "", FString FileTypes = "All Files (*.*)|*.*|")
	{
		if (GEngine && GEngine->GameViewport)
		{
			const void *ParentWindowHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();

			if (ParentWindowHandle)
			{

				TArray<FString> pathsToFiles;
				FString ReturnedPath;

				// Use IDesktopPlatform (editor and dev only)
#if PLATFORM_LINUX
				IDesktopPlatform *DesktopPlatform = FDesktopPlatformModule::Get();
				if (!DesktopPlatform || !ParentWindowHandle)
				{
					// Couldn't initialise the platform's references
					return false;
				}
				else
				{

					// Opens the dialog window
					if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFileName, FileTypes, EFileDialogFlags::Type::None, pathsToFiles))
					{
						ReturnedPath = pathsToFiles[0];

						// Check that the path isn't empty
						if (ReturnedPath != FString(""))
						{
							// Success
							SaveToPath = ReturnedPath;
							return true;
						}
					}
				}

				//Failure
				return false;
#endif

				// Use DialogManager.h for both windows and mac
				ImageDialogManager *DialogMan;
#if PLATFORM_WINDOWS

				DialogMan = new ImageDialogManagerWin();

#endif

#if PLATFORM_MAC

				DialogMan = new ImageDialogManagerMac();
#endif

				if (DialogMan)
				{
					if (DialogMan->SaveFileDial(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFileName, FileTypes, false, pathsToFiles))
					{
						if (pathsToFiles[0] != TEXT(""))
						{
							ReturnedPath = pathsToFiles[0];
							delete DialogMan;

							SaveToPath = ReturnedPath;
							return true;
						}
					}

					delete DialogMan;
				}
			}
		}
		return false;
	}




	/***** WORK IN PROGRESS *****/

	/* Saves the specified Texture2D to a specific image format. Make sure to include the file extension in the filepath. */
	//UFUNCTION(BlueprintCallable, meta = (DisplayName = "SaveTexture2D", Keywords = "ImageIOLibrary"), Category = "Texture2D I/O")
	static bool SaveTexture2D(UTexture2D* Texture2D, EImageIOFormat ImageFormat, FString FilePath);


	/* This applies a standard average blur to the specified bitmap. BlurStrength is a value between 0 and 1. */
	//UFUNCTION(BlueprintCallable, meta = (DisplayName = "BlurBitmapAsync", Keywords = "ImageIOLibrary bitmap blur", AutoCreateRefTerm = "OnBitmapBlurred"), Category = "ImageIOLibrary")
		static void BlurBitmapAsync(const FOnBitmapBlurred& OnBitmapBlurComplete, TArray<FColor> Bitmap, FImageSize Size, float BlurStrength, int BlurRadius);

private:

	// These convert from the engine's EImageFormat (not supported in BPs) to EImageIOFormat and vice versa.
	static EImageIOFormat EImageFormatToEImageIOFormat(EImageFormat ImageFormat);
	static EImageFormat EImageIOFormatToEImageFormat(EImageIOFormat ImageFormat);

};
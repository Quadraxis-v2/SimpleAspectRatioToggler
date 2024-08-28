#include <cstdlib>
#include <ios>
#include <cstring>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <gctypes.h>
#include <gcutil.h>
#include <ogc/isfs.h>
#include <malloc.h>
#include <ogc/conf.h>

static void* SpXfb = 0;
static GXRModeObj* SpGXRmode = 0;


u16 LoadOffsets(s32 iFileDescriptor, u16** paurItemOffsets);
u16 FindItem(s32 iFileDescriptor, u16* paurItemOffsets, u16 urItemCount, const std::string& CsItemName);


//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
{
//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	ISFS_Initialize();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	SpGXRmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	SpXfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(SpGXRmode));

	// Initialise the console, required for printf
	console_init(SpXfb, 20, 20, SpGXRmode->fbWidth, SpGXRmode->xfbHeight, 
		SpGXRmode->fbWidth * VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(SpGXRmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(SpXfb);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if (SpGXRmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	std::printf("\x1b[2;0H");

	s32 iFileDescriptor{-1};
	s32 iError{ISFS_OK};
	u16 urOffset ATTRIBUTE_ALIGN(32) {};
	char acItemName[6] ATTRIBUTE_ALIGN(32) {};
	u8 uyAspectRatio ATTRIBUTE_ALIGN(32) {};

	try
	{
		if ((iFileDescriptor = ISFS_Open("/shared2/sys/SYSCONF", ISFS_OPEN_RW)) < 0)
			throw std::ios_base::failure(std::string{"Error opening SYSCONF, ret = "} + 
				std::to_string(iFileDescriptor));

		// Seek lookup table item offset
		if ((iError = ISFS_Seek(iFileDescriptor, -8, 2)) < 0)
			throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
				std::to_string(iError));

		if ((iError = ISFS_Read(iFileDescriptor, &urOffset, 2)) < 0)
			throw std::ios_base::failure(std::string{"Error reading lookup table, ret = "} + 
				std::to_string(iError));

		if (urOffset != 0)
		{
			// Seek real item offset
			if ((iError = ISFS_Seek(iFileDescriptor, urOffset, 0)) < 0)
				throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
					std::to_string(iError));

			if ((iError = ISFS_Read(iFileDescriptor, &urOffset, 2)) < 0)
				throw std::ios_base::failure(std::string{"Error reading item offset, ret = "} + 
					std::to_string(iError));

			// Seek name of item
			if ((iError = ISFS_Seek(iFileDescriptor, urOffset + 1, 0)) < 0)
				throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
					std::to_string(iError));

			if ((iError = ISFS_Read(iFileDescriptor, &acItemName, 6)) < 0)
				throw std::ios_base::failure(std::string{"Error reading item name, ret = "} + 
					std::to_string(iError));

			if (!strncmp(reinterpret_cast<char*>(&acItemName), "IPL.AR", 6)) urOffset += 7;
			else 
			{
				u16* paurItemOffsets{nullptr};
				u16 urItemCount{LoadOffsets(iFileDescriptor, &paurItemOffsets)};
				urOffset = FindItem(iFileDescriptor, paurItemOffsets, urItemCount, "IPL.AR");
				delete[] paurItemOffsets;
			}
		}
		else
		{
			u16* paurItemOffsets{nullptr};
			u16 urItemCount{LoadOffsets(iFileDescriptor, &paurItemOffsets)};
			urOffset = FindItem(iFileDescriptor, paurItemOffsets, urItemCount, "IPL.AR");
			delete[] paurItemOffsets;
		}

		if ((iError = ISFS_Read(iFileDescriptor, &uyAspectRatio, 1)) < 0)
			throw std::ios_base::failure(std::string{"Error reading item name, ret = "} + 
				std::to_string(iError));

		uyAspectRatio = (uyAspectRatio == CONF_ASPECT_4_3 ? CONF_ASPECT_16_9 : CONF_ASPECT_4_3);

		// Toggle value
		if ((iError = ISFS_Seek(iFileDescriptor, -1, 1)) < 0)
			throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
				std::to_string(iError));

		if ((iError = ISFS_Write(iFileDescriptor, &uyAspectRatio, 1)) < 0)
			throw std::ios_base::failure(std::string{"Error writing to SYSCONF, ret = "} + 
				std::to_string(iError));
	}
	catch (const std::ios_base::failure& CiosBaseFailure)
	{
		std::printf("%s\nPress A to try again.\nPress HOME to exit", CiosBaseFailure.what());
	}

	if (iFileDescriptor >= 0) ISFS_Close(iFileDescriptor);

	while(iError < 0) // Give option to retry
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 uiPressed = WPAD_ButtonsDown(0);

		if (uiPressed & WPAD_BUTTON_A)
		{
			VIDEO_ClearFrameBuffer(SpGXRmode, SpXfb, COLOR_BLACK);
			std::printf("\x1b[2;0H");

			try
			{
				if ((iFileDescriptor = ISFS_Open("/shared2/sys/SYSCONF", ISFS_OPEN_WRITE)) < 0)
					throw std::ios_base::failure(std::string{"Error opening SYSCONF, ret = "} + 
						std::to_string(iFileDescriptor));

				if ((iError = ISFS_Seek(iFileDescriptor, urOffset, 0)) < 0)
					throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
						std::to_string(iError));

				if ((iError = ISFS_Write(iFileDescriptor, &uyAspectRatio, 1)) < 0)
					throw std::ios_base::failure(std::string{"Error writing to SYSCONF, ret = "} + 
						std::to_string(iError));
			}
			catch (const std::ios_base::failure& CiosBaseFailure)
			{
				std::printf("%s\nPress A to try again.\nPress HOME to exit", CiosBaseFailure.what());
			}
			
			if (iFileDescriptor >= 0) ISFS_Close(iFileDescriptor);
		}

		// We return to the launcher application via exit
		if (uiPressed & WPAD_BUTTON_HOME) 
		{
			ISFS_Deinitialize();
			std::exit(0);
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	ISFS_Deinitialize();
	return 0;
}


u16 LoadOffsets(s32 iFileDescriptor, u16** paurItemOffsets)
{
	// Search for item
	u16 urItemCount ATTRIBUTE_ALIGN(32) {};
	s32 iError{};

	// Seek number of items
	if ((iError = ISFS_Seek(iFileDescriptor, 4, 0)) < 0)
		throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
			std::to_string(iError));

	if ((iError = ISFS_Read(iFileDescriptor, &urItemCount, 2)) < 0)
		throw std::ios_base::failure(std::string{"Error reading number of items, ret = "} + 
			std::to_string(iError));

	*paurItemOffsets = static_cast<u16*>(memalign(32, urItemCount << 1));

	// Read all item offsets
	if ((iError = ISFS_Read(iFileDescriptor, *paurItemOffsets, urItemCount << 1)) < 0)
		throw std::ios_base::failure(std::string{"Error reading number of items, ret = "} + 
			std::to_string(iError));

	return urItemCount;
}


u16 FindItem(s32 iFileDescriptor, u16* paurItemOffsets, u16 urItemCount, const std::string& CsItemName)
{
	s32 iError{};
	char acItemName[CsItemName.length()] ATTRIBUTE_ALIGN(32) {};

	// Check all offsets
	for (s32 i = 0; i < urItemCount; ++i)
	{
		if ((iError = ISFS_Seek(iFileDescriptor, paurItemOffsets[i] + 1, 0)) < 0)
			throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
				std::to_string(iError));

		if ((iError = ISFS_Read(iFileDescriptor, &acItemName, CsItemName.length())) < 0)
			throw std::ios_base::failure(std::string{"Error reading item name, ret = "} + 
				std::to_string(iError));

		if (!strncmp(reinterpret_cast<char*>(&acItemName), CsItemName.c_str(), CsItemName.length())) 
			return paurItemOffsets[i] + 7;
	}

	throw std::ios_base::failure("Item not found");
}

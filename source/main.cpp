#include <cstdlib>
#include <ios>
#include <cstring>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <gctypes.h>
#include <gcutil.h>
#include <ogc/isfs.h>

static void* SpXfb = 0;
static GXRModeObj* SpGXRmode = 0;


u16 FindItem(s32 iFileDescriptor);


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
	u16 urOffset ATTRIBUTE_ALIGN(0x20) {};
	char acItemName[6] ATTRIBUTE_ALIGN(0x20) {};
	u8 uyNextAspectRatio ATTRIBUTE_ALIGN(0x20) {};

	try
	{
		if ((iFileDescriptor = ISFS_Open("/shared2/sys/SYSCONF", ISFS_OPEN_READ)) < 0)
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
			else urOffset = FindItem(iFileDescriptor);
		}
		else urOffset = FindItem(iFileDescriptor);

		if ((iError = ISFS_Read(iFileDescriptor, &uyNextAspectRatio, 1)) < 0)
			throw std::ios_base::failure(std::string{"Error reading item name, ret = "} + 
				std::to_string(iError));
	}
	catch (const std::ios_base::failure& CiosBaseFailure)
	{
		std::printf("%s\n", CiosBaseFailure.what());
	}

	if (iFileDescriptor >= 0) 
	{
		ISFS_Close(iFileDescriptor);
		iFileDescriptor = -1;
	}

	uyNextAspectRatio = (uyNextAspectRatio == CONF_ASPECT_4_3 ? CONF_ASPECT_16_9 : CONF_ASPECT_4_3);

	std::printf("Press A to toggle aspect ratio. Current value: %s\n", 
		(uyNextAspectRatio == CONF_ASPECT_4_3 ? "16:9" : "4:3"));

	std::printf("Press HOME to exit.");

	while(1) 
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

				if ((iError = ISFS_Write(iFileDescriptor, &uyNextAspectRatio, 1)) < 0)
					throw std::ios_base::failure(std::string{"Error writing to SYSCONF, ret = "} + 
						std::to_string(iError));
			}
			catch (const std::ios_base::failure& CiosBaseFailure)
			{
				std::printf("%s. Perhaps try again?\n", CiosBaseFailure.what());
			}
			
			if (iFileDescriptor >= 0) 
			{
				ISFS_Close(iFileDescriptor);
				iFileDescriptor = -1;
			}

			if (iError >= 0) uyNextAspectRatio = (uyNextAspectRatio == CONF_ASPECT_4_3 ? 
				CONF_ASPECT_16_9 : CONF_ASPECT_4_3);
			
			std::printf("Press A to toggle aspect ratio. Current value: %s\n", 
				(uyNextAspectRatio == CONF_ASPECT_4_3 ? "16:9" : "4:3"));
			
			std::printf("Press HOME to exit.");
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

	return 0;
}


u16 FindItem(s32 iFileDescriptor)
{
	// Search for item
	u16 urItemCount ATTRIBUTE_ALIGN(0x20) {};
	s32 iError{};

	// Seek number of items
	if ((iError = ISFS_Seek(iFileDescriptor, 4, 0)) < 0)
		throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
			std::to_string(iError));

	if ((iError = ISFS_Read(iFileDescriptor, &urItemCount, 2)) < 0)
		throw std::ios_base::failure(std::string{"Error reading number of items, ret = "} + 
			std::to_string(iError));

	u16 aurItemOffsets[urItemCount] ATTRIBUTE_ALIGN(0x20) {};

	// Read all item offsets
	if ((iError = ISFS_Read(iFileDescriptor, &aurItemOffsets, urItemCount << 1)) < 0)
		throw std::ios_base::failure(std::string{"Error reading number of items, ret = "} + 
			std::to_string(iError));

	// Check all offsets
	char acItemName[6] ATTRIBUTE_ALIGN(0x20) {};

	for (s32 i = 0; i < urItemCount; ++i)
	{
		if ((iError = ISFS_Seek(iFileDescriptor, aurItemOffsets[i] + 1, 0)) < 0)
			throw std::ios_base::failure(std::string{"Error seeking SYSCONF, ret = "} + 
				std::to_string(iError));

		if ((iError = ISFS_Read(iFileDescriptor, &acItemName, 6)) < 0)
			throw std::ios_base::failure(std::string{"Error reading item name, ret = "} + 
				std::to_string(iError));

		if (!strncmp(reinterpret_cast<char*>(&acItemName), "IPL.AR", 6)) return aurItemOffsets[i] + 7;
	}

	throw std::ios_base::failure("Item not found");
}

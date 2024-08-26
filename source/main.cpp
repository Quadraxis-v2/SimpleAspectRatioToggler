#include <cstdlib>
#include <cstdint>
#include <ios>

#include <gccore.h>
#include <wiiuse/wpad.h>

static void* SpXfb = 0;
static GXRModeObj* SpGXRmode = 0;

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

	int32_t iFileDescriptor = -1;
	uint8_t* puyValue = static_cast<uint8_t*>(std::aligned_alloc(32, 1));

	try
	{
		if ((iFileDescriptor = ISFS_Open("/shared2/sys/SYSCONF", ISFS_OPEN_READ)) < 0)
			throw std::ios_base::failure("Error opening SYSCONF");
	
		if (ISFS_Seek(iFileDescriptor, 0x4DF, 0) < 0)
			throw std::ios_base::failure("Error seeking SYSCONF");

		if (ISFS_Read(iFileDescriptor, puyValue, 1) < 0)
			throw std::ios_base::failure("Error reading SYSCONF");
	}
	catch(const std::ios_base::failure& CiosBaseFailure)
	{
		std::printf("%s\n", CiosBaseFailure.what());
	}
	
	if (iFileDescriptor >= 0) ISFS_Close(iFileDescriptor);
	iFileDescriptor = -1;
	std::printf("Press A to toggle aspect ratio. Current value: %s\n", *puyValue ? "16:9" : "4:3");

	std::printf("Press HOME to exit.");

	while(1) 
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		uint32_t uiPressed = WPAD_ButtonsDown(0);

		if (uiPressed & WPAD_BUTTON_A)
		{
			*puyValue = *puyValue ? 0 : 1;

			VIDEO_ClearFrameBuffer(SpGXRmode, SpXfb, COLOR_BLACK);
			std::printf("\x1b[2;0H");

			try
			{
				if ((iFileDescriptor = ISFS_Open("/shared2/sys/SYSCONF", ISFS_OPEN_WRITE)) < 0)
					throw std::ios_base::failure("Error opening SYSCONF");

				if (ISFS_Seek(iFileDescriptor, 0x4DF, 0) < 0)
					throw std::ios_base::failure("Error seeking SYSCONF");

				if (ISFS_Write(iFileDescriptor, puyValue, 1) < 0)
					throw std::ios_base::failure("Error writing to SYSCONF");
			}
			catch(const std::ios_base::failure& CiosBaseFailure)
			{
				std::printf("%s\n Perhaps try again?", CiosBaseFailure.what());
			}
			
			if (iFileDescriptor >= 0) ISFS_Close(iFileDescriptor);
			iFileDescriptor = -1;

			std::printf("Press A to toggle aspect ratio. Current value: %s\n", 
				*puyValue ? "16:9" : "4:3");
			
			std::printf("Press HOME to exit.");
		}

		// We return to the launcher application via exit
		if (uiPressed & WPAD_BUTTON_HOME) 
		{
			ISFS_Deinitialize();
			exit(0);
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}

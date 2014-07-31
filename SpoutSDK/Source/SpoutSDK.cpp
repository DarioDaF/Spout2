﻿// ================================================================
//
//		SpoutSDK
//
//		The Main Spout class - used by Sender and Receiver classes
//
//		Revisions :
//		14-07-14	- SelectSenderPanel - return true was missing.
//		16-07-14	- deleted fbo & texture in SpoutCleanup - test for OpenGL context
//					- used CopyMemory in FlipVertical instead of memcpy
//					- cleanup
//		18-07-14	- removed SpoutSDK local fbo and texture - used in the interop class now
//		22-07-14	- added option for DX9 or DX11
//		25-07-14	- Malcolm Bechard mods to header to enable compilation as a dll
//					- ReceiveTexture - release receiver if the sender no longer exists
//					- ReceiveImage same change - to be tested
//		27-07-14	- CreateReceiver - bUseActive flag instead of null name
//
// ================================================================
/*
		Copyright (c) 2014>, Lynn Jarvis. All rights reserved.

		Redistribution and use in source and binary forms, with or without modification, 
		are permitted provided that the following conditions are met:

		1. Redistributions of source code must retain the above copyright notice, 
		   this list of conditions and the following disclaimer.

		2. Redistributions in binary form must reproduce the above copyright notice, 
		   this list of conditions and the following disclaimer in the documentation 
		   and/or other materials provided with the distribution.

		THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	AND ANY 
		EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
		OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE	ARE DISCLAIMED. 
		IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
		INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
		PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
		INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
		LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
		OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "SpoutSDK.h"

Spout::Spout()
{
	g_Width				= 0;
	g_Height			= 0;
	g_ShareHandle		= 0;
	g_Format			= 0;
	g_TexID				= 0;
	g_hWnd				= NULL;		// handle to render window
	g_SharedMemoryName[0] = 0;		// No name to start 
	bDxInitOK			= false;
	bMemoryShareInitOK	= false;
	bMemory				= false;	// user memoryshare override
	bInitialized		= false;
	bChangeRequested	= true;		// set for initial
	bUseActive			= false;	// Use the active sender for CreateReceiver
	bSpoutPanelOpened	= false;	// Selection panel "spoutpanel.exe" opened
}


//---------------------------------------------------------
Spout::~Spout()
{
	// This is the end, so cleanup and close directx or memoryshare
	SpoutCleanUp(true);

}

// Public functions
bool Spout::CreateSender(char* sendername, unsigned int width, unsigned int height, DWORD dwFormat)
{
	// bool bRet = false;
	bool bMemoryMode = true;

	// Make sure it has initialized
	// A render window must be visible for initSharing to work
	if(!OpenSpout()) {
		return false;
	}
	if(bDxInitOK) {
		bMemoryMode = false;
		strcpy_s(g_SharedMemoryName, 256, sendername);
	}
	
	// Initialize as a sender in either memory or texture mode
	return(InitSender(g_hWnd, sendername, width, height, dwFormat, bMemoryMode));


} // end CreateSender


// ------------------------------------------
//	Update the texture info of a sender
//	Used when a sender's texture changes size
// ------------------------------------------
bool Spout::UpdateSender(char *sendername, unsigned int width, unsigned int height)
{
	HANDLE hSharehandle;
	DWORD dwFormat;
	unsigned int w, h;

	// Make sure it has initialized
	if(!bInitialized || !bDxInitOK) return false;
	if(strcmp(g_SharedMemoryName, sendername) != 0) return false;

	// Retrieve the shared texture format first - the sender must exist
	if(senders.GetSenderInfo(sendername, w, h, hSharehandle, dwFormat)) {
		// No need to free the interop or directx
		// Initialize the GL/DX interop and create a new shared texture (false = sender)
		// This will also re-create the sender and update the sender info
		interop.CreateInterop(g_hWnd, sendername, width, height, dwFormat, false);
		// Get the new sender width, height and share handle into local copy
		senders.GetSenderInfo(g_SharedMemoryName, g_Width, g_Height, g_ShareHandle, g_Format);
		// printf("Spout::UpdateSender (%s) %dx%d\n", g_SharedMemoryName, g_Width, g_Height);
		return true;
	}

	return false;
		
} // end UpdateSender



void Spout::ReleaseSender(DWORD dwMsec) 
{
	if(bMemoryShareInitOK) {
		return;
	}

	if(g_SharedMemoryName[0] > 0) {
		// printf("Spout::ReleaseSender (%s)\n", g_SharedMemoryName);
		senders.ReleaseSenderName(g_SharedMemoryName); // if not registered it does not matter
	}
	SpoutCleanUp();
	bInitialized = false; // LJ DEBUG - needs tracing
	
	Sleep(dwMsec); // LJ DEBUG - not needed - debugging aid only

}

// 27.07-14 - change logic to allow an optional user flag to use the active sender
bool Spout::CreateReceiver(char* sendername, unsigned int &width, unsigned int &height, bool bActive)
{

	char UserName[256];
	UserName[0] = 0; // OK to do this internally

	// printf("Spout::CreateReceiver - bActive = %d, bUseActive = %d\n", bActive, bUseActive);

	// Use the active sender if the user wants it or the sender name is not set
	if(bActive || sendername[0] == 0) {
		bUseActive = true;
		// printf("    No passed sender name - bActive = %d, bUseActive = %d\n", bActive, bUseActive);
	}
	else {
		// Try to find the sender with the name sent or over-ride with user flag
		strcpy_s(UserName, 256, sendername);
		bUseActive = bActive; // set global flag to use the active sender or not
		// printf("    Sender name (%s), bActive = %d bUseActive = %d\n", UserName, bActive, bUseActive);
	}

	if(OpenReceiver(UserName, width, height)) {
		strcpy_s(sendername, 256, UserName); // pass back the sendername used
		return true;
	}
	
	return false;
}


void Spout::ReleaseReceiver() 
{
	// can be done without a check here if(bDxInitOK || bMemoryShareInitOK)
	SpoutCleanUp();
}


// If the local texure has changed dimensions this will return false
bool Spout::SendTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned char * pDib;
	unsigned char * pBits;
	unsigned char * rgbBuffer;
	BITMAPINFOHEADER * pbmih; // pointer to it

	if(bDxInitOK) {
		// width, g_Width should all be the same
		// But it is the responsibility of the application to reset any texture that is being sent out.
		if(width != g_Width || height != g_Height) {
			// printf("Spout::SendTexture - size changed\n    from %dx%d\n    to %dx%d\n", g_Width, g_Height, width, height);
			return(UpdateSender(g_SharedMemoryName, width, height));
		}
		return(interop.WriteTexture(TextureID, TextureTarget, width, height, bInvert));
	}
	else if(bMemoryShareInitOK) {
		// Memoryshare mode has to get the texture pixels into a bitmap and and write them to shared memory
		int imagesize = width*height*3+sizeof(BITMAPINFOHEADER); // RGB bitmap
		rgbBuffer = (unsigned char *)malloc(imagesize*sizeof(unsigned char));
		if(rgbBuffer) {
			// Create a bitmap header
			pDib = rgbBuffer;
			pbmih = (BITMAPINFOHEADER FAR *)(pDib); // local memory pointer
			pBits = (unsigned char *)(pDib + sizeof(BITMAPINFOHEADER)); // pointer to DIB image bits
			// Fill the header
			pbmih->biSize          = sizeof(BITMAPINFOHEADER);
			pbmih->biWidth         = (LONG)width;
			pbmih->biHeight        = (LONG)height;
			pbmih->biPlanes        = 1;
			pbmih->biBitCount      = 24;
			pbmih->biCompression   = 0;
			pbmih->biSizeImage     = 0;
			pbmih->biXPelsPerMeter = 0;
			pbmih->biYPelsPerMeter = 0;
			pbmih->biClrUsed       = 0;
			pbmih->biClrImportant  = 0;

			// Get the pixels of the passed texture into the bitmap
			glBindTexture(TextureTarget, TextureID);
			glGetTexImage(TextureTarget, 0,  GL_RGB,  GL_UNSIGNED_BYTE, pBits);
			glBindTexture(TextureTarget, 0);

			// Default invert flag is true so do the flip to get it the
			// right way up unless the user has specifically indicated not to
			// LJ DEBUG - needs tracing semder/receiver - possible double invert - default false?
			if(bInvert) FlipVertical(pBits, width, height);

			// Write the header plus the image data to shared memory
			MemoryShare.WriteToMemory(pDib, imagesize);
				
			free((void *)rgbBuffer);

			return true;
			
		}
	}
	return false;
} // end SendTexture


// If the local texure has changed dimensions this will return false
bool Spout::SendImage(unsigned char* pixels, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned char * pDib;
	unsigned char * pBits;
	unsigned char * rgbBuffer;
	BITMAPINFOHEADER * pbmih; // pointer to it

	if(bDxInitOK) {
		int imagesize = width*height*3; // RGB bitmap
		rgbBuffer = (unsigned char *)malloc(imagesize*sizeof(unsigned char));
		if(!rgbBuffer) return false;

		// width, g_Width should all be the same
		if(width != g_Width || height != g_Height) {
			UpdateSender(g_SharedMemoryName, g_Width, g_Height);
		}
		if(bInvert) {
			CopyMemory(rgbBuffer, pixels, width*height*3); //format is RGB
			FlipVertical(rgbBuffer, width, height);
			interop.WriteTexturePixels(rgbBuffer, width, height);
		}
		else {
			interop.WriteTexturePixels(pixels, width, height);
		}
		free((void *)rgbBuffer);

		return true; // no checks for now
	}
	else if(bMemoryShareInitOK) {
		// Memoryshare mode has to get the texture pixels into a bitmap and and write them to shared memory
		int imagesize = width*height*3+sizeof(BITMAPINFOHEADER); // RGB bitmap
		rgbBuffer = (unsigned char *)malloc(imagesize*sizeof(unsigned char));
		if(rgbBuffer) {
			// Create a bitmap header
			pDib = rgbBuffer;
			pbmih = (BITMAPINFOHEADER FAR *)(pDib); // local memory pointer
			pBits = (unsigned char *)(pDib + sizeof(BITMAPINFOHEADER)); // pointer to DIB image bits
			// Fill the header
			pbmih->biSize          = sizeof(BITMAPINFOHEADER);
			pbmih->biWidth         = (LONG)width;
			pbmih->biHeight        = (LONG)height;
			pbmih->biPlanes        = 1;
			pbmih->biBitCount      = 24;
			pbmih->biCompression   = 0;
			pbmih->biSizeImage     = 0;
			pbmih->biXPelsPerMeter = 0;
			pbmih->biYPelsPerMeter = 0;
			pbmih->biClrUsed       = 0;
			pbmih->biClrImportant  = 0;

			// Get the pixels of the passed image into the bitmap
			CopyMemory(pBits, pixels, width*height*3); //format is RGB

			// Default invert flag is true so do the flip to get it the
			// right way up unless the user has specifically indicated not to
			if(bInvert) FlipVertical(pBits, width, height);

			// Write the header plus the image data to shared memory
			MemoryShare.WriteToMemory(pDib, imagesize);
				
			free((void *)rgbBuffer);

			return true;
			
		}
	}

	return false;
} // end SendImage


//
// ReceiveTexture
//
bool Spout::ReceiveTexture(char* name, unsigned int &width, unsigned int &height, GLuint TextureID, GLuint TextureTarget)
{
	// bool bRet = false;
	char newname[256];
	unsigned int newWidth, newHeight;
	DWORD dwFormat;
	HANDLE hShareHandle;
	unsigned char *src;
	BITMAPINFOHEADER * pbmih; // pointer to it
	unsigned int imagesize;
	unsigned char * rgbBuffer;

	// Has it initialized yet ?
	if(!bInitialized) {
		// The name passed is the name to try to connect to 
		// unless the bUseActive flag is set or the name is not initialized
		// in which case it will try to find the active sender
		// Width and height are passed back as well
		if(OpenReceiver(name, newWidth, newHeight)) {
			// OpenReceiver will also set the global name, width, height and format
			// Pass back the new name, width and height to the caller
			strcpy_s(name, 256, newname);
			// Check the width and height passed against the sneder detected
			if(width != g_Width || height != g_Height) {
				// Return the new sender name and dimensions
				// The change has to be detected by the application
				width  = newWidth;
				height = newHeight;
				return true; 
			}
		}
		else {
			// Initialization failure - the sender is not there 
			// Quit to let the app try again
			return false;
		}
	} // endif not initialized

	if(bDxInitOK) {

		// Check to see if SpoutPanel has been opened 
		// the globals are reset if it has been
		// And the sender name will be different to that passed
		CheckSpoutPanel();

		// Test to see whether the current sender is still there
		if(senders.CheckSender(g_SharedMemoryName, newWidth, newHeight, hShareHandle, dwFormat)) {
			// Current sender still exists
			// Has the width, height, texture format changed
			// LJ DEBUG no global sharehandle
			if(newWidth > 0 && newHeight > 0) {
				if(newWidth  != g_Width 
				|| newHeight != g_Height
				|| dwFormat  != g_Format
				|| strcmp(name, g_SharedMemoryName) != 0 ) {
					// Re-initialize the receiver
					if(OpenReceiver(g_SharedMemoryName, newWidth, newHeight)) {				
						// OpenReceiver will set the global name, width, height and texture format
						// Set the global texture ID here
						// g_TexID = TextureID;
						// Pass back the new current name and size
						strcpy_s(name, 256, g_SharedMemoryName);
						width  = g_Width;
						height = g_Height;
						// Return the new sender name and dimensions
						// The change has to be detected by the application
						return true;
					} // OpenReceiver OK
					else {
						// need what here
						return false;
					}
				} // width, height, format or name have changed
			} // width and height are zero
			else {
				// need what here
				return false;
			}
		} // endif CheckSender found a sender
		else {
			g_SharedMemoryName[0] = 0; // sender no longer exists
			ReleaseReceiver(); // Start again
			return false;
		} // CheckSender did not find the sender - probably closed

		// Sender exists and everything matched

		// globals are now all current, so pass back the current name and size
		// so that there is no change found by the host
		strcpy_s(name, 256, g_SharedMemoryName);
		width  = g_Width;
		height = g_Height;

		// If a valid texture was passed, read the shared texture into it
		if(TextureID > 0 && TextureTarget > 0) {

			if(interop.ReadTexture(TextureID, TextureTarget, g_Width, g_Height)) {
				// printf("Spout::ReceiveTexture - return 3\n");
				return true;
			}
			else {
				// printf("Spout::ReceiveTexture - error 3\n");
				return false;
			}
		}

		return true;
	} // was initialized in texture mode
	else {
		// Memoryshare mode - problem for reading the size beforehand is that
		// the framerate is halved. Reading the whole image assumes that the sender
		// does not reduce in size, but it has worked successfully so far.
		// Only solution is to always allocate a buffer of the desktop size.
		imagesize = g_Width*g_Height*3;
		rgbBuffer = (unsigned char *)malloc(GetSystemMetrics(SM_CXSCREEN)*GetSystemMetrics(SM_CYSCREEN) + sizeof(BITMAPINFOHEADER));
		if(rgbBuffer) {
			if(MemoryShare.ReadFromMemory(rgbBuffer, (sizeof(BITMAPINFOHEADER) + imagesize))) {
				pbmih = (BITMAPINFOHEADER *)rgbBuffer;

				// return for zero width and height
				if(pbmih->biWidth == 0 || pbmih->biHeight == 0) {
					free((void *)rgbBuffer);
					return false;
				}

				// check the size received to see if it matches the size passed in
				if((unsigned int)pbmih->biWidth != width || (unsigned int)pbmih->biHeight != height) {
					// return changed width and height
					width  = (unsigned int)pbmih->biWidth;
					height = (unsigned int)pbmih->biHeight;
				} // endif size changed
				else {
					// otherwise transfer the image data to the texture pixels - Note RGB only
					src = rgbBuffer + sizeof(BITMAPINFOHEADER);
					glBindTexture(TextureTarget, TextureID);
					glTexSubImage2D(TextureTarget, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, src);
					glBindTexture(TextureTarget, 0);
				} // endif size matches
			} // endif MemoryShare.ReadFromMemory
			free((void *)rgbBuffer);
			return true;
		} // end buffer alloc OK

		return true;

	}

	// printf("Spout::ReceiveTexture - error 4\n");

	return false;

} // end ReceiveTexture (name, TextureID, TextureTarget)


// Note was RGB only. Format passed should go through now and work.
bool Spout::ReceiveImage(char* name, unsigned int &width, unsigned int &height, unsigned char* pixels, int glFormat)
{
	// bool bRet = false;
	char newname[256];
	unsigned int newWidth, newHeight;
	DWORD dwFormat;
	HANDLE hShareHandle;
	unsigned char *src;
	BITMAPINFOHEADER * pbmih; // pointer to it
	unsigned int imagesize;
	unsigned char * rgbBuffer;

	// Has it initialized yet ?
	if(!bInitialized) {
		// The name passed is the name to try to connect to unless the name is null
		// in which case it will try to find the active sender
		// Width and height are passed back as well
		if(OpenReceiver(name, newWidth, newHeight)) {
			// OpenReceiver will also set the global name, width, height and format
			// Pass back the new name, width and height to the caller
			strcpy_s(name, 256, newname);
			// Check the width and height passed against the sneder detected
			if(width != g_Width || height != g_Height) {
				// Return the new sender name and dimensions
				// The change has to be detected by the application
				width  = newWidth;
				height = newHeight;
				return true; 
			}
		}
		else {
			// initialization failure
			return false;
		}
	} // endif not initialized

	if(!bMemoryShareInitOK && bDxInitOK) {

		// Check to see if SpoutPanel has been opened 
		// the globals are reset if it has been
		CheckSpoutPanel();
		// To be tested - receivetexture works OK
		// Test to see whether the current sender is still there
		if(senders.CheckSender(g_SharedMemoryName, newWidth, newHeight, hShareHandle, dwFormat)) {
			// Current sender still exists
			// Has the width, height, texture format changed
			// LJ DEBUG no global sharehandle
			if(newWidth > 0 && newHeight > 0) {
				if(newWidth  != g_Width 
				|| newHeight != g_Height
				|| dwFormat  != g_Format
				|| strcmp(name, g_SharedMemoryName) != 0 ) {
					// Re-initialize the receiver
					if(OpenReceiver(g_SharedMemoryName, newWidth, newHeight)) {				
						// OpenReceiver will set the global name, width, height and texture format
						// Set the global texture ID here
						// g_TexID = TextureID;
						// Pass back the new current name and size
						strcpy_s(name, 256, g_SharedMemoryName);
						width  = g_Width;
						height = g_Height;
						// Return the new sender name and dimensions
						// The change has to be detected by the application
						return true;
					} // OpenReceiver OK
					else {
						// need what here
						return false;
					}
				} // width, height, format or name have changed
			} // width and height are zero
			else {
				// need what here
				return false;
			}
		} // endif CheckSender found a sender
		else {
			g_SharedMemoryName[0] = 0; // sender no longer exists
			ReleaseReceiver(); // Start again
			return false;
		} // CheckSender did not find the sender - probably closed

		// Sender exists and everything matched

		// globals are now all current, so pass back the current name and size
		// so that there is no change found by the host
		strcpy_s(name, 256, g_SharedMemoryName);
		width  = g_Width;
		height = g_Height;

		// If a valid pixel pointer was passed, read the shared texture into it
		if(pixels) {
			// Default format is GL_RGB
			if(interop.ReadTexturePixels(pixels, g_Width, g_Height, glFormat)) {
				return true;
			}
			else {
				return false;
			}
		}
		return true;
	} // was initialized in texture mode
	else {
		// Memoryshare mode - problem for reading the size beforehand is that
		// the framerate is halved. Reading the whole image assumes that the sender
		// does not reduce in size, but it has worked successfully so far.
		// Only solution is to always allocate a buffer of the desktop size.
		imagesize = g_Width*g_Height*3;
		rgbBuffer = (unsigned char *)malloc(GetSystemMetrics(SM_CXSCREEN)*GetSystemMetrics(SM_CYSCREEN) + sizeof(BITMAPINFOHEADER));
		if(rgbBuffer) {
			if(MemoryShare.ReadFromMemory(rgbBuffer, (sizeof(BITMAPINFOHEADER) + imagesize))) {
				pbmih = (BITMAPINFOHEADER *)rgbBuffer;
				// return for zero width and height
				if(pbmih->biWidth == 0 || pbmih->biHeight == 0) {
					free((void *)rgbBuffer);
					return false;
				}
				// check the size received to see if it matches the size passed in
				if((unsigned int)pbmih->biWidth != width || (unsigned int)pbmih->biHeight != height) {
					// return changed width and height
					width  = (unsigned int)pbmih->biWidth;
					height = (unsigned int)pbmih->biHeight;
				} // endif size changed
				else {
					// otherwise transfer the image data to the texture pixels - Note RGB only
					src = rgbBuffer + sizeof(BITMAPINFOHEADER);
					CopyMemory(pixels, src, width*height*3); // assumes format is RGB
				} // endif size matches
			} // endif MemoryShare.ReadFromMemory
			free((void *)rgbBuffer);
			return true;
		} // end buffer alloc OK
	}
	return false;
}

//---------------------------------------------------------
bool Spout::BindSharedTexture()
{
	return interop.BindSharedTexture();
}


//---------------------------------------------------------
bool Spout::UnBindSharedTexture()
{
	return interop.UnBindSharedTexture();
}


//---------------------------------------------------------
bool Spout::DrawSharedTexture(float max_x, float max_y)
{
	return interop.DrawSharedTexture(max_x, max_y);
}


// Compatibility is tested by loading OpenGL extensions
// and initializing DirectX and calling wglDXOpenDeviceNV
bool Spout::GetMemoryShareMode()
{
	// If already initialized, return what it initialized as
	if(bDxInitOK || bMemoryShareInitOK) {
		if(bMemoryShareInitOK) return true;
		else return false;
	}
	else {
		// otherwise do the compatibiliy test
		if(interop.GLDXcompatible()) {
			return false;
		}
		else {
			return true;
		}
	}
}


// Set memoryshare mode true or false
bool Spout::SetMemoryShareMode(bool bMemoryMode)
{

	bMemory = bMemoryMode; // force memoryshare mode

	// If already initialized, re-initialze
	if(bDxInitOK || bMemoryShareInitOK) {
		SpoutCleanUp();
		return OpenSpout();
	}

	return true;
}

//
// selectSenderPanel - used by a receiver
// Optional message argument
bool Spout::SelectSenderPanel(char *message)
{
	HANDLE hMutex1;
	HMODULE module;
	char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH];

	if(!bDxInitOK) {
		// MessageBoxA(NULL, "Spout running in memoryshare mode\nThere can only be one sender\nno sender selection available", "Spout", MB_OK);
		return false;
	}

	// For a texture share receiver pop up SpoutPanel to allow the user to select a sender
	// The selected sender is then the "Active" sender and this receiver switches to it.
	// SpoutPanel.exe has to be in the same folder as this executable
	// This rather complicated process avoids having to use a dialog within a dll
	// which causes problems with FreeFrameGL plugins and Max eternals

	// First check whether the panel is already running
	// Try to open the application mutex.
	hMutex1 = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");

	if (!hMutex1) {
		// No mutex, so not running, so can open it
		// Find the path of the host program where SpoutPanel should have been copied
		module = GetModuleHandle(NULL);
		GetModuleFileNameA(module, path, MAX_PATH);

		_splitpath_s(path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, NULL, 0);
		_makepath_s(path, MAX_PATH, drive, dir, "SpoutPanel", ".exe");

		//
		// Use  ShellExecuteEx so we can test its return value later
		//
		ZeroMemory(&ShExecInfo, sizeof(ShExecInfo));
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.hwnd = NULL;
		ShExecInfo.lpVerb = NULL;
		ShExecInfo.lpFile = path;
		// ShExecInfo.lpParameters = "";
		ShExecInfo.lpParameters = message;
		ShExecInfo.lpDirectory = NULL;
		ShExecInfo.nShow = SW_SHOW;
		ShExecInfo.hInstApp = NULL;	
		ShellExecuteExA(&ShExecInfo);
		Sleep(125); // alow time for SpoutPanel to open 0.125s
		// Returns straight away here but multiple instances of SpoutPanel
		// are prevented in it's WinMain procedure by the mutex.
		// An infinite wait here causes problems.
		// The flag "bSpoutPanelOpened" is set here to indicate that the user
		// has opened the panel to select a sender. This flag is local to 
		// this process so will not affect any other receiver instance
		// Then when the selection panel closes, sender name is tested
		bSpoutPanelOpened = true;
		return true;
	} // The mutex exists, so another instance is already running	
	CloseHandle(hMutex1);		
	return false;
} // end selectSenderPanel


int Spout::GetSenderCount() {
	std::set<string> SenderNameSet;
	if(senders.GetSenderNames(&SenderNameSet)) {
		return(SenderNameSet.size());
	}
	return 0;
}

//
// Get a sender name given an index and knowing the sender count
// index						- in
// sendername					- out
// sendernameMaxSize			- in
bool Spout::GetSenderName(int index, char* sendername, int sendernameMaxSize)
{
	std::set<string> SenderNameSet;
	std::set<string>::iterator iter;
	string namestring;
	char name[256];
	int i;

	if(senders.GetSenderNames(&SenderNameSet)) {
		if(SenderNameSet.size() < (unsigned int)index) {
			return false;
		}
		i = 0;
		for(iter = SenderNameSet.begin(); iter != SenderNameSet.end(); iter++) {
			namestring = *iter; // the name string
			strcpy_s(name, 256, namestring.c_str()); // the 256 byte name char array
			if(i == index) {
				strcpy_s(sendername, sendernameMaxSize, name); // the passed name char array
				break;
			}
			i++;
		}
		return true;
	}
	return false;
}


// Alll of these redundant - can be directly in the Receiver class . Change/Test
//---------------------------------------------------------
bool Spout::GetActiveSender(char* Sendername)
{
	return senders.GetActiveSender(Sendername);
}


//---------------------------------------------------------
bool Spout::SetActiveSender(char* Sendername)
{
	return senders.SetActiveSender(Sendername);
}


bool Spout::GetSenderInfo(char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	return senders.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}



bool Spout::GetVerticalSync()
{
	return interop.GetVerticalSync();
}


bool Spout::SetVerticalSync(bool bSync)
{
	return interop.SetVerticalSync(bSync);
}



// ========================================================== //
// ==================== LOCAL FUNCTIONS ===================== //
// ========================================================== //

// Find if the sender exists
// If the name begins with a null character, or the bUseActive flag has been set
// return the active sender name if that exists
bool Spout::OpenReceiver (char* theName, unsigned int& theWidth, unsigned int& theHeight)
{

	char Sendername[256]; // user entered Sender name
	DWORD dwFormat;
	unsigned int width;
	unsigned int height;
	bool bMemoryMode = true;

	// printf("Spout::OpenReceiver\n");

	// A valid name is sent and the user does not want to use the active sender
	if(theName[0] && !bUseActive) {
		// printf("    (%s) %dx%d - bUseActive = %d\n", theName, theWidth, theHeight, bUseActive);
		strcpy_s(Sendername, 256, theName);
	}
	else {
		// printf("Spout::OpenReceiver (use active sender) %dx%d -  - bUseActive = %d\n", theWidth, theHeight, bUseActive);
		Sendername[0] = 0;
	}

	// Set initial size to that passed in
	width  = theWidth;
	height = theHeight;

	// Make sure it has been initialized
	if(!OpenSpout()) {
		// printf("    Spout::OpenReceiver error 1\n");
		return false;
	}
	// No senders for texture mode - just return false
	if(!bMemoryShareInitOK && bDxInitOK && GetSenderCount() == 0) {
		// printf("    Spout::OpenReceiver error 2\n");
		return false;
	}

	// Render window must be visible for initSharing to work
	g_hWnd = WindowFromDC(wglGetCurrentDC()); 

	if(!bMemoryShareInitOK && bDxInitOK) bMemoryMode = false;

	// Check compatibility
	if(!bMemoryShareInitOK && bDxInitOK) { 
		bMemoryMode = false;
		// Find if the sender exists
		// Or if a null name given return the active sender if that exists
		if(!senders.FindSender(Sendername, width, height, g_ShareHandle, dwFormat)) {

			// Given name not found ? - has SpoutPanel been opened ?
			// the globals are reset if it has been
			if(CheckSpoutPanel()) {
				// set vars for below
				strcpy_s(Sendername, 256, g_SharedMemoryName);
				width    = g_Width;
				height   = g_Height;
				dwFormat = g_Format;
			}
			else {
				// printf("    Spout::OpenReceiver error 3\n");
			    return false;
			}
		}
		else if(bMemoryShareInitOK) {
			// TODO : Find a memoryshare sender if running
		}

		// Set the globals
		strcpy_s(g_SharedMemoryName, 256, Sendername);
		g_Width  = width;
		g_Height = height;
		g_Format = dwFormat;


	}

	// printf("Spout::OpenReceiver found (%s) %dx%d)\n", Sendername, width, height);

	// Initialize a receiver in either memoryshare or texture mode
	if(InitReceiver(g_hWnd, Sendername, width, height, bMemoryMode)) {
		// Pass back the sender name and size now that the global
		// width and height have been set
		strcpy_s(theName, 256, Sendername); // LJ DEBUG global?
		theWidth  = g_Width;
		theHeight = g_Height;
		return true;
	}

	// printf("    Spout::OpenReceiver error 4\n");

	return false;

} // end OpenReceiver


bool Spout::InitSender (HWND hwnd, char* theSendername, unsigned int theWidth, unsigned int theHeight, DWORD dwFormat, bool bMemoryMode) 
{

	// Texture share mode quit if there is no image size to initialize with
	// Memoryshare can detect a Sender while the receiver is running
	if(!bMemoryMode && (theWidth == 0 || theHeight == 0)) {
		return false;
	}

	// only try dx if :
	//	- the user memory mode flag is not set
	//	- Hardware is compatible
	if(bGLDXcompatible && !bMemoryMode) {
		
		// Initialize the GL/DX interop and create a new shared texture (false = sender)
		if(!interop.CreateInterop(hwnd, theSendername, theWidth, theHeight, dwFormat, false)) {  // False for a sender
			// printf("Spout::InitSender - CreateInterop failed\n");
			return false;
		}

		// Set global name
		strcpy_s(g_SharedMemoryName, 256, theSendername);
				
		// Get the sender width, height and share handle into local copy
		senders.GetSenderInfo(g_SharedMemoryName, g_Width, g_Height, g_ShareHandle, g_Format);

		bDxInitOK			= true;
		bMemoryShareInitOK	= false;
		bInitialized = true;

		return true;
	} 
	// ================== end sender initialize ==============================

	// if it did not initialize, try to set up for memory share transfer
	if(!bInitialized) {
		
		// Set globals - they will be reset by a receiver but are needed for a sender
		g_Width  = theWidth;
		g_Height = theHeight;
		bMemoryShareInitOK = InitMemoryShare(false); // sender
		if(bMemoryShareInitOK) {
			bDxInitOK	 = false;
			bInitialized = true;
			return true;
		}
	}

	return false;

} // end InitSender


bool Spout::InitReceiver (HWND hwnd, char* theSendername, unsigned int theWidth, unsigned int theHeight, bool bMemoryMode) 
{

	char sendername[256];
	unsigned int width = 0;
	unsigned int height = 0;
	DWORD format;
	HANDLE sharehandle;
	
	if(theSendername[0]) {
		strcpy_s(sendername, 256, theSendername); // working name local to this function
	}
	else {
		sendername[0] = 0;
	}


	// Texture share mode quit if there is no image size to initialize with
	// Memoryshare can detect a Sender while the receiver is running
	if(!bMemoryMode && (theWidth == 0 || theHeight == 0)) {
		return false;
	}

	// bChangeRequested is set when the Sender name, image size or share handle changes
	// or the user selects another Sender - everything has to be reset if already initialized
	if(bChangeRequested) {
		// ReleaseMemoryShare(); // done in spoutcleanup
		SpoutCleanUp();
		bDxInitOK			= false;
		bMemoryShareInitOK	= false;
		bInitialized		= false;
		bChangeRequested	= false; // only do it once
	}
	
	//
	// only try dx if :
	//	- the user memory mode flag is not set
	//	- Hardware is compatible
	if(bGLDXcompatible && !bMemoryMode) {
		//
		// ============== Set up for a RECEIVER ============
		//
		// Find a sender and return the name, width, height, sharehandle and format
		if(!senders.FindSender(sendername, width, height, sharehandle, format)) {
			return false;
		}
		
		// Initialize the receiver interop (this will create globals local to the interop class)
		if(!interop.CreateInterop(hwnd, sendername, width, height, format, true)) { // true meaning receiver
			// printf("Spout::InitReceiver - CreateInterop failed\n");
			return false;
		}

		// Set globals here
		g_Width  = width;
		g_Height = height;
		g_ShareHandle = sharehandle;
		g_Format = format;
		strcpy_s(g_SharedMemoryName, 256, sendername);

		bDxInitOK = true;
		bMemoryShareInitOK = false;
		bInitialized = true;

		return true;
	} // ================== end receiver initialize ==============================

	// if it did not initialize, try to set up for memory share transfer
	if(!bInitialized) {
		// Set globals - they will be reset by a receiver but are needed for a sender
		g_Width  = theWidth;
		g_Height = theHeight;
		bMemoryShareInitOK = InitMemoryShare(true); // receiver
		if(bMemoryShareInitOK) {
			bDxInitOK	 = false;
			bInitialized = true;
			return true;
		}
	}

	return false;

} // end InitReceiver


bool Spout::InitMemoryShare(bool bReceiver) 
{
	// initialize shared memory
	if(!bMemoryShareInitOK) bMemoryShareInitOK = MemoryShare.Initialize();

	if(bMemoryShareInitOK) {
		if(!bReceiver) {
			// Set the sender mutex so that if a receiver attempts to read
			// and a sender is not present, there is no wait delay
			MemoryShare.CreateSenderMutex();
			return true;
		}
		else {
			// A receiver - is a memoryshare sender running ?
			if(MemoryShare.GetImageSizeFromSharedMemory(g_Width, g_Height)) {
				// global width and height have now been set
				return true;
			}
			else {
				MemoryShare.DeInitialize();
				return false;
			}
		}
	}
	else {
		MemoryShare.DeInitialize();
		bMemoryShareInitOK = false;
		// drop though and return fail
	} // end memory share initialize

	return false;

} // end InitMemoryShare


bool Spout::ReleaseMemoryShare()
{
	if(bMemoryShareInitOK) MemoryShare.DeInitialize();
	bMemoryShareInitOK = false;

	return true;
}

//
// SpoutCleanup
//
void Spout::SpoutCleanUp(bool bExit)
{
	interop.CleanupInterop(bExit); // true means it is the exit so don't call wglDXUnregisterObjectNV
	
	bDxInitOK = false;
	g_ShareHandle = NULL;
	g_Width	= 0;
	g_Height= 0;
	g_Format = 0;

	// important - we no longer want the global shared memory name and need to reset it
	g_SharedMemoryName[0] = 0; 

	// Set default for CreateReceiver
	bUseActive = false;

	// Important - everything is reset (see ReceiveTexture)
	bInitialized = false;

	ReleaseMemoryShare(); // de-init MemoryShare if it has been initiaized

}

// ========= USER SELECTION PANEL TEST =====
//
//	This is necessary because the exit code needs to be tested
//
bool Spout::CheckSpoutPanel()
{
		SharedTextureInfo TextureInfo;
		HANDLE hMutex;
		DWORD dwExitCode;
		char newname[256];
		bool bRet = false;

		// If SpoutPanel has been activated, test if the user has clicked OK
		if(bDxInitOK && bSpoutPanelOpened) { // User has activated spout panel
			hMutex = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");
			if (!hMutex) { // It has now closed
				bSpoutPanelOpened = false;
				// call GetExitCodeProcess() with the hProcess member of SHELLEXECUTEINFO
				// to get the exit code from SpoutPanel
				if(ShExecInfo.hProcess) {
					GetExitCodeProcess(ShExecInfo.hProcess, &dwExitCode);
					// Only act if exit code = 0 (OK)
					if(dwExitCode == 0) {
						// get the active sender
						if(senders.GetActiveSender(newname)) { // returns the active sender name
							if(interop.getSharedInfo(newname, &TextureInfo)) {
								strcpy_s(g_SharedMemoryName, 256, newname);
								g_Width  = (unsigned int)TextureInfo.width;
								g_Height = (unsigned int)TextureInfo.height;
								g_Format = TextureInfo.format;
								// printf("CheckSpoutPanel(%s), %dx%d\n", g_SharedMemoryName, g_Width, g_Height);
								bRet = true; // will pass on next call to receivetexture
							}
						}
						else {
							// printf("CheckSpoutPanel no active sender\n");
						}
					}
				}
			} // end mutex check
			CloseHandle(hMutex);
		}
		return bRet;
} // ========= END USER SELECTION PANEL =====



// Adapted from FreeImage function
// Flip the image vertically along the horizontal axis.
bool Spout::FlipVertical(unsigned char *src, unsigned int width, unsigned int height) 
{
		BYTE *From, *Mid;

		// swap the buffer
		int pitch = width*3; // RGB

		// copy between aligned memories
		Mid = (BYTE*)malloc(pitch * sizeof(BYTE));
		if (!Mid) return false;

		From = src;
	
		unsigned int line_s = 0;
		unsigned int line_t = (height-1)*pitch;

		for(unsigned int y = 0; y<height/2; y++) {
			CopyMemory(Mid, From + line_s, pitch);
			CopyMemory(From + line_s, From + line_t, pitch);
			CopyMemory(From + line_t, Mid, pitch);
			/*
			memcpy(Mid, From + line_s, pitch);
			memcpy(From + line_s, From + line_t, pitch);
			memcpy(From + line_t, Mid, pitch);
			*/
			line_s += pitch;
			line_t -= pitch;
		}

		free((void *)Mid);

		return true;
}


bool Spout::OpenSpout()
{
	HDC hdc;
	// Safety return if already initialized
	if(bDxInitOK || bMemoryShareInitOK) {
		bGLDXcompatible = bDxInitOK;
		return true;
	}

	// Has the global memoryshare over-ride flag been set?
	if(!bMemory) {
		hdc = wglGetCurrentDC(); // OpenGl device context is needed
		if(!hdc) {
			// MessageBoxA(NULL, "    Cannot get GL device context", "OpenSpout", MB_OK);
			return false;
		}
		g_hWnd = WindowFromDC(hdc); // can be null though
		if(interop.LoadGLextensions()) { // did the extensions load OK ?

			// printf("OpenSpout() - GetDX9() = %d\n", GetDX9());

			// Initialize DirectX and prepare GLDX interop
			if(interop.OpenDirectX(g_hWnd, GetDX9())) { // did the NVIDIA open interop extension work ?
				bDxInitOK = true; // DirectX initialization OK
				bMemoryShareInitOK = false;
				bGLDXcompatible = true; // Set global compatibility flag as well
				RedrawWindow(g_hWnd, NULL, NULL, RDW_ERASENOW | RDW_ALLCHILDREN); // DirectX init needs a window redraw
				return true; 
			}
		}
	}

	// Drop through and try to initialize shared memory
	bDxInitOK = false;
	bGLDXcompatible = false;
	bMemoryShareInitOK = false;
	if(MemoryShare.Initialize()) {
		bMemoryShareInitOK = true;
		return true;
	}

	return false;
}

void Spout::SetDX9(bool bDX9)
{
	// printf("Spout::SetDX9(%d)\n", bDX9);
	interop.UseDX9(bDX9);
}


bool Spout::GetDX9()
{
	// printf("Spout::GetDX9() = %d\n",interop.isDX9());

	return interop.isDX9();
}
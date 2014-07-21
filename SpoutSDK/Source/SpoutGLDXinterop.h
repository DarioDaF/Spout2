/*

			spoutGLDXinterop.h

		LJ - leadedge@adam.com.au

		Functions to manage texture sharing using the NVIDIA GL/DX opengl extensions

		https://www.opengl.org/registry/specs/NV/DX_interop.txt


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
#pragma once
#ifndef __spoutGLDXinterop__ // standard way as well
#define __spoutGLDXinterop__

#include "spoutGLextensions.h"
#include "spoutDirectX.h"
#include "spoutSenderNames.h"

#include <windowsx.h>
#include <d3d9.h>	// DX9
#include <d3d11.h>	// DX11
#include <gl/gl.h>

class spoutGLDXinterop {

	public:

		spoutGLDXinterop();
		~spoutGLDXinterop();

		// Initialization functions
		bool LoadGLextensions(); // Load required opengl extensions
		bool OpenDirectX(HWND hWnd);
		bool CreateInterop(HWND hWnd, char* sendername, unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive = true);
		bool CheckInterop(HWND hWnd); // Check for successful open of the interop
		void CleanupInterop(bool bExit = false); // Cleanup with flag to avoid unknown crash bug

		void setSharedMemoryName(char* sharedMemoryName, bool bReceive = true); 
		bool getSharedInfo(char* sharedMemoryName, SharedTextureInfo* info);
		bool setSharedInfo(char* sharedMemoryName, SharedTextureInfo* info);
		bool ReadTexturePixels(unsigned char *pixels, unsigned int width, unsigned int height, int glFormat = GL_RGB);
		bool ReadTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height);
		bool WriteTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false);
		bool WriteTexturePixels(unsigned char *pixels, unsigned int width, unsigned int height);
		#ifdef USE_PBO_EXTENSIONS
		bool LoadTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, unsigned char *data);
		#endif

		bool BindSharedTexture();
		bool UnBindSharedTexture();
		bool DrawSharedTexture(float max_x = 1.0, float max_y = 1.0);

		DXGI_FORMAT	DX11format; // the DX11 texture format to be used
		void SetDX11format(DXGI_FORMAT textureformat); // set format by user

		void CleanupDirectX();

		// Utilities
		bool GLDXcompatible();
		bool GetVerticalSync();
		bool SetVerticalSync(bool bSync = true);

		spoutSenderNames senders;	// Sender management
		spoutDirectX spoutdx;	// DirectX class

		// 21.07.14 - made public for debugging
		GLuint	m_glTexture;		// the linked OpenGL texture
		HANDLE	m_hInteropDevice;	// handle to the DX/GL interop device
		HANDLE	m_hInteropObject;	// handle to the DX/GL interop object (the shared texture)
		// Locks for gl/dx interop functions
		HRESULT LockInteropObject(HANDLE hDevice, HANDLE *hObject);
		HRESULT UnlockInteropObject(HANDLE hDevice, HANDLE *hObject);


	protected:

		bool m_bInitialized;	// this instance initialized flag
		bool bFBOavailable;		// fbo extensions available
		bool bBLITavailable;	// fbo blit extensions available
		bool bPBOavailable;		// pbo extensions available
		bool bSWAPavailable;	// swap extensions available

		HWND					m_hWnd;				// parent window
		HANDLE					m_hSharedMemory;	// handle to the texture info shared memory
		SharedTextureInfo		m_TextureInfo;		// local texture info structure

		// DX11
		ID3D11Device*			g_pd3dDevice;
		ID3D11DeviceContext*	g_pImmediateContext;
		D3D_DRIVER_TYPE			g_driverType;
		D3D_FEATURE_LEVEL		g_featureLevel;
		ID3D11Texture2D*		g_pSharedTexture; // The shared DX11 texture

		bool OpenDirectX11(HWND hWnd, ID3D11Device* &hDXdevice, HANDLE &hInteropDevice); // Initialize and prepare DirectX11
		bool CreateDX11device(HWND hWnd); // Create a DX11 device
		HANDLE LinkGLDXtextures(HANDLE hInteropDevice, ID3D11Texture2D* pSharedTexture, HANDLE dxShareHandle, GLuint glTextureID); // , HANDLE &hInteropObject);

		// GLuint	m_glTexture;		// the OpneGL texture linked to it
		// GLuint	m_fbo;				// a local frame buffer object used for texture transfers
		// HANDLE	m_hInteropDevice;	// handle to the DX/GL interop device
		// HANDLE	m_hInteropObject;	// handle to the DX/GL interop object (the shared texture)
		HANDLE	m_dxShareHandle;	// shared DX texture handle

		bool getSharedTextureInfo(char* sharedMemoryName);
		bool setSharedTextureInfo(char* sharedMemoryName);

		// Locks for gl/dx interop functions
		// HRESULT LockInteropObject(HANDLE hDevice, HANDLE *hObject);
		// HRESULT UnlockInteropObject(HANDLE hDevice, HANDLE *hObject);
		
		// Texture access event lock handles
		HANDLE	m_hReadEvent;  // this instance handle to read event
		HANDLE	m_hWriteEvent; // this instance handle to write event

};

#endif

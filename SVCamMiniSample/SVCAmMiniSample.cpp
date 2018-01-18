#include "stdafx.h"
#include "conio.h"
#include <chrono>
#include "SVCamSystem.h"

int SaveImage(size_t _Width, size_t _Height, unsigned char *ImageData)
{
	int Width = (int)_Width;
	int Height = (int)_Height;

	// Check image alignment
	if (Width % 4 == 0)
	{
		BITMAPINFO *bitmapinfo;

		// Generate and fill a bitmap info structure
		bitmapinfo = (BITMAPINFO *)new char[sizeof(BITMAPINFOHEADER)];

		bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapinfo->bmiHeader.biWidth = Width;
		bitmapinfo->bmiHeader.biHeight = -Height;
		bitmapinfo->bmiHeader.biBitCount = 24;
		bitmapinfo->bmiHeader.biPlanes = 1;
		bitmapinfo->bmiHeader.biClrUsed = 0;
		bitmapinfo->bmiHeader.biClrImportant = 0;
		bitmapinfo->bmiHeader.biCompression = BI_RGB;
		bitmapinfo->bmiHeader.biSizeImage = Width * Height * 3; //Width*Height*3 | 0
		bitmapinfo->bmiHeader.biXPelsPerMeter = 0;
		bitmapinfo->bmiHeader.biYPelsPerMeter = 0;

		// save to file_path
		__int64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		string fn = "C:\\images\\" + std::to_string(now) + ".bmp";
		FILE* pFile = fopen(fn.c_str(), "wb");
		//errno_t errorCode = fopen_s(&pFile, fn.c_str(), "w");

		if (pFile == NULL)
		{
			return 0;
		}

		BITMAPFILEHEADER bmfh;                         // Other BMP header
		int nBitsOffset = sizeof(BITMAPFILEHEADER) + bitmapinfo->bmiHeader.biSize;
		LONG lImageSize = bitmapinfo->bmiHeader.biSizeImage;
		LONG lFileSize = nBitsOffset + lImageSize;
		bmfh.bfType = 'B' + ('M' << 8);
		bmfh.bfOffBits = nBitsOffset;
		bmfh.bfSize = lFileSize;
		bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

		// Write the bitmap file header               // Saving the first header to file
		UINT nWrittenFileHeaderSize = fwrite(&bmfh, 1, sizeof(BITMAPFILEHEADER), pFile);

		// And then the bitmap info header            // Saving the second header to file
		UINT nWrittenInfoHeaderSize = fwrite(&bitmapinfo->bmiHeader, 1, sizeof(BITMAPINFOHEADER), pFile);

		// Finally, write the image data itself
		//-- the data represents our drawing          // Saving the file content in lpBits to file
		UINT nWrittenDIBDataSize = fwrite(ImageData, 1, lImageSize, pFile);
		fclose(pFile); // closing the file.

		delete[]bitmapinfo;
		return 1;
	}
}

unsigned long __stdcall AcquisitionThread(void *context)
{
	SVCamAcquisition * svacq = (SVCamAcquisition*)context;
	if (NULL == svacq)
		return -1;

	printf("%s AcquisitionThread Running \n", __FUNCTION__);

	while (!svacq->acqTerminated)
	{
		if (svacq->imageBufferInfo.size() != 0)
		{
			// Obtain the image data pointer and characteristics
			SV_BUFFER_INFO  *NewImageInfo = svacq->imageBufferInfo.front();
			printf("new image:  %d \n", NewImageInfo->pImagePtr);
			// assuming RGB 
			int pDestLength = 3 * NewImageInfo->iSizeX * NewImageInfo->iSizeY;
			unsigned char * ImageData_RGB = new unsigned char [pDestLength];
			SVUtilBufferBayerToRGB(*NewImageInfo, ImageData_RGB, pDestLength);
			//SaveImage(NewImageInfo->iSizeX, NewImageInfo->iSizeY, ImageData_RGB);
			svacq->imageBufferInfo.pop_front();
			delete NewImageInfo;
		}
		else
		{
			WaitForSingleObject(svacq->m_newImage, 1000);
			ResetEvent(svacq->m_newImage);
		}
	}
	printf("%s AcquisitionThread Exiting \n", __FUNCTION__);

	return 0;
}

void PrintDevInfo(const SV_DEVICE_INFO &info)
{
	printf("******************Device Info*****************\n");
	printf("uid:%s\n", info.uid);
	printf("Vendor:%s\n", info.vendor);
	printf("Model:%s\n", info.model);
	printf("DisplayName:%s\n", info.displayName);
	printf("TLType:%s\n", info.tlType);
	printf("Access Status:0x%x\n", info.accessStatus);
	printf("User Defined Name:%s\n", info.userDefinedName);
	printf("Serial Number:%s\n", info.serialNumber);
	printf("Version:%s\n", info.version);

}

void PrintFeatureInfo(vector<SVCamFeaturInf*> sv_camfeatureinf)
{
	printf("--------------------------------------------------------------------\n");

	printf("******************Camera Feature Info*****************\n");
	printf(" ");
	for (std::vector<SVCamFeaturInf*>::iterator i = sv_camfeatureinf.begin() + 1; i != sv_camfeatureinf.end(); i++)
	{
		printf("|");
		for (int j = 0; j < (*i)->SVFeaturInf.level; j++)
			printf("--");

		printf((*i)->SVFeaturInf.displayName);
		printf(": "); printf((*i)->strValue);   printf("\n ");
	}

	printf("--------------------------------------------------------------------\n");
}

// demonstration  for GigE vision device, the same procedure can be used for usb and camera link devices.
int _tmain(int argc, _TCHAR* argv[])
{
	//1)************************Initialisation****************************************************

	printf("Initialisation...\n");

	SVCamSystem *sv_cam = NULL;
	char SVGenicamRoot[1024] = { 0 };
	char SVGenicamCache[1024] = { 0 };
	char SVSCti[1024] = { 0 };
	char SVCLProtocol[1024] = { 0 };

	DWORD dwRet = GetEnvironmentVariableA("SVS_GENICAM_ROOT", SVGenicamRoot, sizeof(SVGenicamRoot));
	if (0 == dwRet)
	{
		printf("GetEnvironmentVariableA SVS_GENICAM_ROOT failed! :%d\n", GetLastError());
		goto Exit;
	}

#ifdef _WIN64
	dwRet = GetEnvironmentVariableA("SVS_SDK_GENTL", SVSCti, sizeof(SVSCti));
	if (0 == dwRet)
	{
		printf("GetEnvironmentVariableA SVS_SDK_GENTL failed! :%d\n", GetLastError());
		goto Exit;
	}
#else
	dwRet = GetEnvironmentVariableA("SVS_SDK_GENTL32", SVSCti, sizeof(SVSCti));
	if (0 == dwRet)
	{
		printf("GetEnvironmentVariableA SVS_SDK_GENTL32 failed! :%d\n", GetLastError());
		goto Exit;
	}
#endif

	dwRet = GetEnvironmentVariableA("SVS_GENICAM_CLPROTOCOL", SVCLProtocol, sizeof(SVCLProtocol));
	if (0 == dwRet)
	{
		printf("GetEnvironmentVariableA SVS_GENICAM_CLPROTOCOL failed! :%d\n", GetLastError());
		goto Exit;
	}

	dwRet = GetEnvironmentVariableA("SVS_GENICAM_CACHE", SVGenicamCache, sizeof(SVCLProtocol));
	if (0 == dwRet)
	{
		printf("GetEnvironmentVariableA SVS_GENICAM_CACHE failed! :%d\n", GetLastError());
		goto Exit;
	}

	// load dlls and intialize device modules
	SV_RETURN  ret = SVLibInit(SVSCti, SVGenicamRoot, SVGenicamCache, SVCLProtocol);
	if (ret != SV_ERROR_SUCCESS)
	{
		printf(":%s SVLibInit Failed!:%d\n", __FUNCTION__, ret);
		goto Exit;
	}

	//For GigE the camera IP has to configured (use the SVIPConfig.exe configuration Tool).
	SV_TL_TYPE  tl_type = TL_U3V; /* TL_U3V / TL_CL / TL_GEV */
	sv_cam = new SVCamSystem(tl_type);

	//2)************************Device discovery and enumeration**********************************

	printf("Device discovery and enumeration...\n");
	//Open the System module
	uint32_t tlCount = 0;
	ret = SVLibSystemGetCount(&tlCount);
	bool State = false;

	// initialize device and get transport layer info
	for (uint32_t i = 0; i < tlCount; i++)
	{
		SV_TL_INFO tlInfo = { 0 };
		ret = SVLibSystemGetInfo(i, &tlInfo);
		if (SV_ERROR_SUCCESS != ret)
		{
			continue;
		}

		if ((TL_GEV == tl_type && 0 == _stricmp("GEV", tlInfo.tlType)) ||
			(TL_U3V == tl_type && 0 == _stricmp("U3V", tlInfo.tlType)) ||
			(TL_CL == tl_type && 0 == _stricmp("CL", tlInfo.tlType)))
			State = sv_cam->SVCamSystemInit(i);
	}

	unsigned int enumtimeout = 1000;
	if (State)
		sv_cam->EnumDevices(enumtimeout); // I case of successfully enumeration a device info will be stored in a devInfoList.

	if (sv_cam->devInfoList.size() == 0)
	{
		printf(" no device found !! \n");
		goto Exit;
	}

	//3)************************open camera device******************************************

	printf("open camera device...\n");
	//Open the first camera in the device list.
	SV_DEVICE_INFO*  devinf = sv_cam->devInfoList.front();

	//If device is successfully opened a new camera will be stored in a sv_cam_list.
	sv_cam->openDevice(*devinf);

	Camera *cam = NULL;

	if (sv_cam->sv_cam_list.size() != 0)
		cam = sv_cam->sv_cam_list.front();

	if (cam == NULL)
	{
		printf("opening camera device failed !! \n");
		goto Exit;

	}

	//************************Display Camera info***********************************************
	PrintDevInfo(*devinf);

	//************************Display Camera feature info**************************************
	cam->sv_cam_feature->getDeviceFeatureList(SV_Beginner);
	if (cam->sv_cam_feature->featureInfolist.size() != 0)
		PrintFeatureInfo(cam->sv_cam_feature->featureInfolist);

	//************************Use feature to configure camera***********************************

		// set Trigger Mode to Software Triger 
	SV_FEATURE_HANDLE hFeature = NULL;
	SVFeatureGetByName(cam->sv_cam_acq->hRemoteDev, "TriggerMode", &hFeature);
	SVFeatureSetValueInt64Enum(cam->sv_cam_acq->hRemoteDev, hFeature, 1);

	// set Exposure time 10 ms.
	hFeature = NULL;
	SVFeatureGetByName(cam->sv_cam_acq->hRemoteDev, "ExposureTime", &hFeature);
	SVFeatureSetValueFloat(cam->sv_cam_acq->hRemoteDev, hFeature, 10000); // (60 ms) 

//4)************************open streaming channel and start acquisition*******************

	printf("open streaming channel and start acquisition...\n");
	unsigned int bufcount = 4;
	cam->sv_cam_acq->AcquisitionStart(bufcount);

	// start acquisition Thread
	//HANDLE acquisitionThread = CreateThread(NULL, 0, AcquisitionThread, (void *)sv_cam->sv_cam_list.front()->sv_cam_acq, 0, NULL);

	// grabe images (software Trigger).
	hFeature = NULL;
	UINT32 timeOut = 1000;
	SVFeatureGetByName(cam->sv_cam_acq->hRemoteDev, "TriggerSoftware", &hFeature);

	// grabe 10 images
	for (int i = 0; i < 1; i++)
	{
		printf("----------------------------------\n");
		SVFeatureCommandExecute(cam->sv_cam_acq->hRemoteDev, hFeature, timeOut);
		printf("Trigger %-10d \n", i + 1);
		Sleep(500);
	}



	if (cam->sv_cam_acq->imageBufferInfo.size() != 0)
	{
		// Obtain the image data pointer and characteristics
		SV_BUFFER_INFO  *NewImageInfo = cam->sv_cam_acq->imageBufferInfo.front();
		printf("new image:  %d \n", NewImageInfo->pImagePtr);
		// assuming RGB 
		int pDestLength = 3 * NewImageInfo->iSizeX * NewImageInfo->iSizeY;
		unsigned char * ImageData_RGB = new unsigned char[pDestLength];
		SVUtilBufferBayerToRGB(*NewImageInfo, ImageData_RGB, pDestLength);
		SaveImage(NewImageInfo->iSizeX, NewImageInfo->iSizeY, ImageData_RGB);
		cam->sv_cam_acq->imageBufferInfo.pop_front();
		delete NewImageInfo;
	}

	/*WaitForSingleObject(cam->sv_cam_acq->m_newImage, 1000);
	ResetEvent(cam->sv_cam_acq->m_newImage);

	CloseHandle(acquisitionThread);*/

	cam->sv_cam_acq->AcquisitionStop();
	//4)************************close camera and free all resources*********************

	printf("free all resources...\n");

Exit:
	printf("--------------------------------------------------------------------\n");
	if (sv_cam)
		delete sv_cam;

	printf("Close the library and free all the allocated resources.\n");

	printf("--------------------------------------------------------------------\n");
	printf("Press any key to quit...\n");
	_getch();
	SVLibClose();

	return 0;
}
#include "stdafx.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <winusb.h>
#include <strsafe.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <comdef.h>
#include "audio_player.h"
#include "log.h"

#define CONFIG_FILENAME "DeviceInterface_GUID.config"
#define ADB_EXECUTABLE "adb.exe"
#define KILL_SERVER_COMMAND "kill-server"

#define ADB_GUID_KEY "ADB_DeviceInterface_GUID="
#define AUDIO_GUID_KEY "Audio_DeviceInterface_GUID="


AudioPlayer audio_player;

typedef struct INTERFACE_SELECTOR {
	BYTE class_;
	BYTE sub_class;
	BYTE protocol;
	
} INTERFACE_SELECTOR;

const INTERFACE_SELECTOR adb_interface = {
	0xff, //class_
	0x42, //sub_class
	0x1, //protocol
	
};

const INTERFACE_SELECTOR audio_stream_interface = {
	0x01, //class_
	0x02, //sub_class
	0x0, //protocol
	
};

typedef struct _DEVICE_DATA {

	BOOL                    HandlesOpen;
	WINUSB_INTERFACE_HANDLE WinusbHandle;
	HANDLE                  DeviceHandle;
	TCHAR                   DevicePath[MAX_PATH];

	USHORT ProductID;
	USHORT VendorID;

	WINUSB_PIPE_INFORMATION_EX  PipeIn;
	WINUSB_PIPE_INFORMATION_EX  PipeOut;

} DEVICE_DATA, *PDEVICE_DATA;




HRESULT
RetrieveDevice(
_Out_     PDEVICE_DATA DeviceData,
_In_ const GUID &guid
)
{
	BOOL                             bResult = FALSE;
	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         interfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
	ULONG                            length;
	ULONG                            requiredLength = 0;
	HRESULT                          hr = S_OK;
	
	DWORD lastError = 0;

	DeviceData->HandlesOpen = false;
	DeviceData->DeviceHandle = INVALID_HANDLE_VALUE;
	DeviceData->WinusbHandle = INVALID_HANDLE_VALUE;

	//
	// Enumerate all devices exposing the interface
	//
	deviceInfo = SetupDiGetClassDevs(&guid,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (deviceInfo == INVALID_HANDLE_VALUE) {

		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	
	
	interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	

	
	
	for (int memberIndex = 0;; ++memberIndex)
	{
		bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
			NULL,
			&guid,
			memberIndex,
			&interfaceData);

		if (FALSE == bResult) {

			//
			// We would see this error if no devices were found
			//
			lastError = GetLastError();
			
			if (ERROR_NO_MORE_ITEMS == lastError)
			{
				hr = HRESULT_FROM_WIN32(lastError);
				break;
			}



		}



		//
		// Get the size of the path string
		// We expect to get a failure with insufficient buffer
		//
		bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&interfaceData,
			NULL,
			0,
			&requiredLength,
			NULL);

		if (FALSE == bResult) {
			lastError = GetLastError();			
			if (ERROR_INSUFFICIENT_BUFFER != lastError)
			{
				hr = HRESULT_FROM_WIN32(lastError);
				SetupDiDestroyDeviceInfoList(deviceInfo);
				return hr;
			}
				
		}


		//
		// Allocate temporary space for SetupDi structure
		//
		detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
			LocalAlloc(LMEM_FIXED, requiredLength);

		if (NULL == detailData)
		{
			hr = E_OUTOFMEMORY;
			break;
		}

		detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		length = requiredLength;

		//
		// Get the interface's path string
		//
		bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&interfaceData,
			detailData,
			length,
			&requiredLength,
			NULL);


		if (bResult)
		{
			DeviceData->WinusbHandle = INVALID_HANDLE_VALUE;
			DeviceData->DeviceHandle = INVALID_HANDLE_VALUE;

			do
			{
				DeviceData->DeviceHandle = CreateFile(detailData->DevicePath,
					GENERIC_WRITE | GENERIC_READ,
					FILE_SHARE_WRITE | FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
					NULL);

				if (INVALID_HANDLE_VALUE == DeviceData->DeviceHandle) {

					//hr = HRESULT_FROM_WIN32(GetLastError());
					break;
				}

				bResult = WinUsb_Initialize(DeviceData->DeviceHandle,
					&DeviceData->WinusbHandle);

				if (FALSE == bResult) {

					//hr = HRESULT_FROM_WIN32(GetLastError());					
					break;
				}
			} while (false);


			if (DeviceData->WinusbHandle != INVALID_HANDLE_VALUE)
			{

				ULONG lengthTransferred = 0;
				USB_DEVICE_DESCRIPTOR usbDescriptor;
				bResult = WinUsb_GetDescriptor(
					DeviceData->WinusbHandle,
					USB_DEVICE_DESCRIPTOR_TYPE,
					0,
					0,
					(PUCHAR)&usbDescriptor,
					sizeof(USB_DEVICE_DESCRIPTOR),
					&lengthTransferred
					);
				


				if (bResult)
				{																				

					//picking up the first device found
					
					DeviceData->ProductID = usbDescriptor.idProduct;
					DeviceData->VendorID = usbDescriptor.idVendor;
					DeviceData->HandlesOpen = true;
					hr = S_OK;

					StringCbCopy(DeviceData->DevicePath,
						sizeof(DeviceData->DevicePath),
						detailData->DevicePath);


						
						
					break;
					

				}
			}




		}




		if (detailData != NULL)
		{
			LocalFree(detailData);
			detailData = NULL;
		}

		if (DeviceData->HandlesOpen)
			break;

		//close the unwanted handles
		if (DeviceData->WinusbHandle != INVALID_HANDLE_VALUE)
		{
			WinUsb_Free(DeviceData->WinusbHandle);
			DeviceData->WinusbHandle = INVALID_HANDLE_VALUE;
		}

		if (DeviceData->DeviceHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(DeviceData->DeviceHandle);
			DeviceData->DeviceHandle = INVALID_HANDLE_VALUE;
		}

	}
			
	

	SetupDiDestroyDeviceInfoList(deviceInfo);

	return hr;
}


HRESULT Control(WINUSB_INTERFACE_HANDLE hDeviceHandle, 
	const WINUSB_SETUP_PACKET &packet, BYTE *data)
{
	if (hDeviceHandle == INVALID_HANDLE_VALUE)
	{
		return HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
	}

	BOOL bResult = TRUE;			
	ULONG cbSent = 0;

	bResult = WinUsb_ControlTransfer(hDeviceHandle, packet, data, packet.Length, &cbSent, 0);
	if (!bResult)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	else
		return S_OK;
	

	//printf("Control Data: %d \nActual data transferred: %d.\n", length, cbSent);



}

HRESULT SwitchToInterface(DEVICE_DATA &deviceData, const INTERFACE_SELECTOR &interfaceSelector)
{	
	for (UCHAR index = 0;; ++index)
	{
		WINUSB_INTERFACE_HANDLE associatedInterface = INVALID_HANDLE_VALUE;
		if (!WinUsb_GetAssociatedInterface(
			deviceData.WinusbHandle,
			index,
			&associatedInterface
			))
		{
			
			HRESULT hr = HRESULT_FROM_WIN32(GetLastError());		
			return hr;
			

			
		}

		if (associatedInterface != INVALID_HANDLE_VALUE)
		{
			
			USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
			
			BOOL bResult = WinUsb_QueryInterfaceSettings(associatedInterface, 0, &InterfaceDescriptor);

			/*ULONG lengthTransferred = 0;
			USB_ENDPOINT_DESCRIPTOR usbDescriptor;
			BOOL bResult2 = WinUsb_GetDescriptor(
				associatedInterface,
				USB_ENDPOINT_DESCRIPTOR_TYPE,
				0,
				0,
				(PUCHAR)&usbDescriptor,
				sizeof(USB_DEVICE_DESCRIPTOR),
				&lengthTransferred
				);*/


			if (bResult)
			{
				
				if (
					InterfaceDescriptor.bInterfaceClass == interfaceSelector.class_ &&
					InterfaceDescriptor.bInterfaceSubClass == interfaceSelector.sub_class &&
					InterfaceDescriptor.bInterfaceProtocol == interfaceSelector.protocol)
				{										
					
					if (deviceData.WinusbHandle != INVALID_HANDLE_VALUE)
					{
						WinUsb_Free(deviceData.WinusbHandle);
					}
					deviceData.WinusbHandle = associatedInterface;

					break;
				}

			}

			WinUsb_Free(associatedInterface);
		}

	}

	return S_OK;
}





HRESULT PrepareStreamInterface(DEVICE_DATA &deviceData)
{

	
	HRESULT hr = S_OK;
#if 0
#define SET_ADDRESS 0x05
#define SET_CONFIGURATION 0x09

	WINUSB_SETUP_PACKET packet;
	ULONG lengthTransferred = 0;

	USB_CONFIGURATION_DESCRIPTOR configDescriptor;
	if (!WinUsb_GetDescriptor(
		deviceData.WinusbHandle,
		USB_CONFIGURATION_DESCRIPTOR_TYPE,
		0,
		0,
		(PUCHAR)&configDescriptor,
		sizeof(USB_CONFIGURATION_DESCRIPTOR),
		&lengthTransferred
		))
	{

		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	packet.RequestType = 0x0;
	packet.Request = SET_ADDRESS;
	packet.Value = 0x0001;
	packet.Index = 0;
	packet.Length = 0;


	hr = Control(deviceData.WinusbHandle, packet, NULL);
	if (FAILED(hr))
	{
		return hr;
	}

	packet.RequestType = 0x0;
	packet.Request = SET_CONFIGURATION;
	packet.Value = configDescriptor.bConfigurationValue;
	packet.Index = 0;
	packet.Length = 0;


	hr = Control(deviceData.WinusbHandle, packet, NULL);
	if (FAILED(hr))
	{
		return hr;
	}
#endif
	
	bool pipeFound = false;
	int alternateSettingNumber = -1;
	while(!pipeFound)
	{		
		++alternateSettingNumber;
		USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
		ZeroMemory(&InterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));

		WINUSB_PIPE_INFORMATION_EX Pipe;
		ZeroMemory(&Pipe, sizeof(WINUSB_PIPE_INFORMATION_EX));


		if (!WinUsb_QueryInterfaceSettings(deviceData.WinusbHandle, alternateSettingNumber, &InterfaceDescriptor))
		{
			DWORD lastError = GetLastError();
			if (lastError == ERROR_NO_MORE_ITEMS)
			{
				hr = HRESULT_FROM_WIN32(lastError);
				break;
			}
		}


		for (int index = 0; index < InterfaceDescriptor.bNumEndpoints; index++)
		{
			if (WinUsb_QueryPipeEx(deviceData.WinusbHandle, alternateSettingNumber, index, &Pipe))
			{
				if (Pipe.PipeType == UsbdPipeTypeIsochronous)
				{

					if (USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId))
					{										
						deviceData.PipeIn = Pipe;

						/*UCHAR buffer[1024];
						ULONG bufferLength = sizeof(buffer);
						if (WinUsb_GetPipePolicy(deviceData.WinusbHandle, Pipe.PipeId, MAXIMUM_TRANSFER_SIZE, &bufferLength, buffer))
						{
						LOGI("buffer length: %u, %u", bufferLength, *(unsigned int*)buffer);
						}*/

						pipeFound = true;
					}
					else if (USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId))
					{						
						deviceData.PipeOut = Pipe;
					}


				}
			}
		}
	}

	
	
	if (pipeFound)
	{
		if (!WinUsb_SetCurrentAlternateSetting(deviceData.WinusbHandle, alternateSettingNumber))
		{			
			hr = HRESULT_FROM_WIN32(GetLastError());			
		}
	}

	return hr;
}

BOOL EnableAOA(const DEVICE_DATA &deviceData)
{
// <https://source.android.com/devices/accessories/aoa2>
#define AOA_REQUEST_TYPE (0x02 << 5)
#define AOA_GET_PROTOCOL     51
#define AOA_START_ACCESSORY  53
#define AOA_SET_AUDIO_MODE   58

#define AUDIO_MODE_NO_AUDIO               0
#define AUDIO_MODE_S16LSB_STEREO_44100HZ  1

#define AOA_PRODUCT_ID_LOWERBOUND 0x2D02
#define AOA_PRODUCT_ID_UPPERBOUND 0x2D05

	if (deviceData.ProductID >= AOA_PRODUCT_ID_LOWERBOUND && deviceData.ProductID <= AOA_PRODUCT_ID_UPPERBOUND)
	{
		LOGI("AOA version 2 has already been enabled");
		return true;
	}
		

	WINUSB_SETUP_PACKET packet;
	ZeroMemory(&packet, sizeof(WINUSB_SETUP_PACKET));
	BYTE data[2];

	
	packet.RequestType = USB_ENDPOINT_DIRECTION_MASK | AOA_REQUEST_TYPE;
	packet.Request = AOA_GET_PROTOCOL;
	packet.Value = 0;
	packet.Index = 0;
	packet.Length = sizeof(data);

	if (FAILED(Control(deviceData.WinusbHandle, packet, data)))
	{		
		LOGE("Fail to retrieve AOA protocol");
		return false;
	}
	USHORT version = *((USHORT*)data);	
	LOGI("AOA version: %u", version);

	if (version < 2)
	{		
		LOGE("Higher AOA version (2) is required!");
		return false;
	}
	
	packet.RequestType = AOA_REQUEST_TYPE;
	packet.Request = AOA_SET_AUDIO_MODE;
	packet.Value = AUDIO_MODE_S16LSB_STEREO_44100HZ;
	packet.Index = 0; //unused
	packet.Length = 0;

	if (FAILED(Control(deviceData.WinusbHandle, packet, NULL)))
	{		
		LOGE("Fail to switch to AOA audio mode");
		return false;
	}

	packet.RequestType = AOA_REQUEST_TYPE;
	packet.Request = AOA_START_ACCESSORY;
	packet.Value = 0; //unused
	packet.Index = 0; //unused
	packet.Length = 0;

	if (FAILED(Control(deviceData.WinusbHandle, packet, NULL)))
	{		
		LOGE("Fail to start the accessory interface");
		return false;
	}

	//waiting a short moment for audio interface getting ready
	Sleep(3000);
	return true;
}

BOOL WINAPI SignalHandler(DWORD signal) {

	if (signal == CTRL_C_EVENT)
	{
		LOGI("Stopping AudioPlayer...");
		audio_player.Stop();
	}

	return TRUE;
}

bool ParseGUID(const std::string &line, const char *const key, GUID &guid)
{
	size_t numOfCharConverted = 0;
	std::wstring guidStr;
	std::string value;
	unsigned int prefixIndex = 0, surfixIndex = line.length() - 1;
	while(true)
	{
		if (line[prefixIndex] != ' ')
		{
			if (line[prefixIndex] == '#')
				return false;
			else
				break;
		}
		++prefixIndex;
	}
	while (true)
	{
		if (line[surfixIndex] != ' ')
			break;					
		--surfixIndex;
	}

	value = line.substr(prefixIndex, ++surfixIndex);

	size_t location = value.find(key);
	if (location == std::string::npos)
		return false;

	value = value.substr(location + strlen(key));
	guidStr.resize(value.length()+1);
	if (mbstowcs_s(&numOfCharConverted, &guidStr[0], value.length() + 1, value.c_str(), value.length()))
		return false;
	if (FAILED(CLSIDFromString((LPCOLESTR)guidStr.c_str(), &guid)))
		return false;

	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	unsigned int offset = 0;
	std::ifstream ifs;
	GUID adbGUID, audioGUID;
	DEVICE_DATA deviceData;
	HRESULT hr = S_OK;
	
	char path[256];
	path[0] = '"';
	do
	{
		//The argument 1, if provided, is taken as a designated path to locate adb.exe and config file
		if (argc == 2)
		{
			if (strcpy_s(path+1, sizeof(path)-1, argv[1]))
			{
				LOGE("The path to this executable seems too long. Try to store it at a different location.");
				break;
			}
		}
		else
		{
			GetModuleFileName(
				NULL,
				path + 1,
				sizeof(path) - 1
				);
		}

		char *pathEnding = strrchr(path, '\\') + 1;
		unsigned int pathLengthToDirectory = pathEnding - path;		

		// the extra '2' chars are for the ending '"' and a space between full path to ADB_EXECUTABLE and KILL_SERVER_COMMAND		
		if (strcpy_s(pathEnding, sizeof(path) - pathLengthToDirectory - 2, ADB_EXECUTABLE))
		{
			LOGE("The path to this executable seems too long. Try to store it at a different location.");
			break;
		}
		offset = strlen(ADB_EXECUTABLE);
		pathEnding[offset] = '"';
		pathEnding[offset + 1] = ' ';
		offset += 2;
		if (strcpy_s(pathEnding + offset, sizeof(path) - pathLengthToDirectory - offset, KILL_SERVER_COMMAND))
		{
			LOGE("The path to this executable seems too long. Try to store it at a different location.");
			break;
		}
		//try to kill the adb server, since it occupies the adb interface which makes following attempts to open the same interface fail
		system(path);
		

		if (strcpy_s(pathEnding, sizeof(path) - pathLengthToDirectory, CONFIG_FILENAME))
		{
			LOGE("The path to this executable seems too long. Try to store it at a different location.");
			break;
		}				
		//need to skip the leading '"'
		ifs.open(path+1);
		if (!ifs.good())
		{
			LOGE("Fail to read device GUIDs from \"%s\"", path + 1);
			break;
		}


		std::string line, value;


		while (std::getline(ifs, line))
		{
			if (line.length() > 0)
			{
				if (ParseGUID(line, ADB_GUID_KEY, adbGUID))
					continue;
				if (ParseGUID(line, AUDIO_GUID_KEY, audioGUID))
					continue;
			}
		}
		ifs.close();




		hr = RetrieveDevice(&deviceData, adbGUID);

		if (FAILED(hr) || !deviceData.HandlesOpen)
		{
			LOGE("Fail to get the device handle: %s", _com_error(hr).ErrorMessage());
			break;
		}

		LOGI("Device: VID = 0x%p, PID = 0x%p", deviceData.VendorID, deviceData.ProductID);

		if (!EnableAOA(deviceData))
			break;

		//closing the adb handle
		WinUsb_Free(deviceData.WinusbHandle);
		CloseHandle(deviceData.DeviceHandle);

		

		hr = RetrieveDevice(&deviceData, audioGUID);

		if (FAILED(hr) || !deviceData.HandlesOpen)
		{
			LOGE("Fail to get the device handle: %s", _com_error(hr).ErrorMessage());
			break;
		}
		hr = SwitchToInterface(deviceData, audio_stream_interface);
		if (FAILED(hr))
		{
			LOGE("Fail to switch to audio stream interface: %s", _com_error(hr).ErrorMessage());
			break;
		}

		hr = PrepareStreamInterface(deviceData);
		if (FAILED(hr))
		{
			LOGE("Fail to prepare audio stream interface: %s", _com_error(hr).ErrorMessage());
			break;
		}

		if (!SetConsoleCtrlHandler(SignalHandler, TRUE)) {
			LOGE("Fail to register CTRL-C ahandler");
			break;
		}


		audio_player.Start(deviceData.WinusbHandle, deviceData.PipeIn);
		audio_player.Wait();


		WinUsb_Free(deviceData.WinusbHandle);
		CloseHandle(deviceData.DeviceHandle);
	} while (false);

	
	std::cout << "Press enter to exit...";
	std::cin.get();
	
	return 0;
}
#include "Windows.h"
#include "SetupAPI.h"
#include "hidsdi.h"

#include <errhandlingapi.h>
#include <iostream>
#include <memory>
#include <string.h>
#include <string>
#include <winbase.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Hid.lib")

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }
    
    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);
    
    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);
            
    return message;
}

void print_error(const std::string& msg)
{
  std::cout << msg << std::endl;
}

void fatal_error(int error, const std::string& msg) {
  std::cout << msg << ": " << error << std::endl;
  exit(error);
}

struct DeviceInfo
{
  typedef std::string Str;

  Str szDeviceName;
  // 设备类
  Str szDeviceClass;
  // 设备显示名
  Str szDeviceDesc;
  // 设备驱动
  Str szDriverName;
  // 设备实例
  DWORD dwDevIns;
  // 设备标志类
  GUID guid;
};

bool find_touch_screen(DeviceInfo& out)
{
  bool found = false;

  // 获取HID设备类的GUID
  GUID guid;
  HidD_GetHidGuid(&guid);

  // 获取HID设备类的句柄集
  auto DeviceInfoSet = SetupDiGetClassDevs(
      &guid, nullptr, nullptr, DIGCF_DEVICEINTERFACE);

  if (DeviceInfoSet == INVALID_HANDLE_VALUE) {
    fatal_error(GetLastError(), "无法获取hid设备信息");
  }

  SP_DEVINFO_DATA DeviceInfoData;
  ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
  DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  DWORD DeviceIndex = 0;

  while (true) {
    if (!SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData)) {
      auto error = GetLastError();
      // 259 表示没有更多的数据
      if(error == 259){
        break;
      }
      fatal_error(error, "获取设备信息失败");
    }

    TCHAR szClassBuf[MAX_PATH] = {0};
    TCHAR szDescBuf[MAX_PATH] = {0};
    TCHAR szDriver[MAX_PATH] = {0};

#define GET_DEVICE_REGISTRY_PROPERTY(prop, buffer)                             \
    if (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, prop,  \
                                          NULL, (PBYTE)buffer, MAX_PATH - 1,     \
                                          NULL))                                 \
      { std::cout << "获取"#prop"设备信息失败" << std::endl; }

    // 获取类名
    GET_DEVICE_REGISTRY_PROPERTY(SPDRP_CLASS, szClassBuf)
    GET_DEVICE_REGISTRY_PROPERTY(SPDRP_DEVICEDESC, szDescBuf)
    GET_DEVICE_REGISTRY_PROPERTY(SPDRP_DRIVER, szDriver)

#define PRINT_MSG(x) std::cout << x << std::endl;

    if(0 == strcmp(szClassBuf, "HIDClass")
      // && 0 == strcmp(szDescBuf, "符合 HID 标准的触摸屏")
      && 0 == strcmp(szDriver, "{745a17a0-74d3-11d0-b6fe-00a0c90f57da}\\0012") )
      {
        out.dwDevIns = DeviceInfoData.DevInst;
        out.guid = DeviceInfoData.ClassGuid;
        found = true;
      }

    ++DeviceIndex;
  }

  SetupDiDestroyDeviceInfoList(DeviceInfoSet);

  return found;
}

bool SetDeviceStatus(const DeviceInfo& Device, bool Enable)
{
  bool result = true;

  do {
    SP_DEVINFO_DATA DeviceInfoData;
    ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    HDEVINFO hDevInfo;
    // 得到设备 HDEVINFO
    hDevInfo = SetupDiGetClassDevs(&Device.guid,0,0,DIGCF_PRESENT);
    if(hDevInfo == INVALID_HANDLE_VALUE){
      print_error("can not get specify device info");
      result = false;
      break;
    }

    // 判断是否有这个设备
    result = false;
    int index = 0;
    while(SetupDiEnumDeviceInfo(hDevInfo, index++,&DeviceInfoData)){
      if(DeviceInfoData.DevInst == Device.dwDevIns)
      {
        result = true;
        break;
      }
    }
    if(!result){
      print_error("设置对象无法打开");
    }
    else{
      // 初始化属性
      SP_PROPCHANGE_PARAMS propChange;
      propChange.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
      propChange.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
      propChange.Scope = DICS_FLAG_GLOBAL;
      propChange.StateChange = Enable ? DICS_ENABLE :DICS_DISABLE;
      propChange.HwProfile = 0;

      if(SetupDiSetClassInstallParams(hDevInfo,&DeviceInfoData,(SP_CLASSINSTALL_HEADER*)&propChange, sizeof(propChange)))
      {
        if(!SetupDiChangeState(hDevInfo, &DeviceInfoData))
        {
          print_error("无法更改指定设备的状态");
          print_error(GetLastErrorAsString());
          result = false;
        }
      }
      else {
          print_error("无法设置设备安装参数");
          print_error(GetLastErrorAsString());
          result = false;
      }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
  } while(false);

  return result;
}

int main(int, char **) {
  DeviceInfo touch_screen;
  if(find_touch_screen(touch_screen)){
    if(!SetDeviceStatus(touch_screen, true)){
      std::cout << "更改指定设备状态失败" << std::endl;
    }
  }
  else{
    std::cout << "无法找到指定设备" << std::endl;
  }
}

/**
 * HMD extension for JediAcademy
 *
 *  Copyright 2014 by Jochen Leopold <jochen.leopold@model-view.com>
 */

#ifndef HmdDeviceOculusSdk_H
#define HmdDeviceOculusSdk_H

#define OVR_OS_LINUX

#include "IHmdDevice.h"
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
//#include <openhmd.h>

class HmdDeviceOculusSdk : public IHmdDevice
{
public:

    HmdDeviceOculusSdk();
    virtual ~HmdDeviceOculusSdk();

    virtual bool Init(bool allowDummyDevice = false);
    virtual void Shutdown();

    virtual std::string GetInfo();

    virtual bool HasDisplay();
    virtual std::string GetDisplayDeviceName();
    virtual bool GetDisplayPos(int& rX, int& rY);

    virtual bool GetDeviceResolution(int& rWidth, int& rHeight);
    virtual bool GetOrientationRad(float& rPitch, float& rYaw, float& rRoll);

    ovrHmd GetHmd() { return mpHmd; }

protected:
    void ConvertQuatToEuler(const float* quat, float& rYaw, float& rPitch, float& rRoll);
    void DetectDisplay();

private:
    // disable copy constructor
    HmdDeviceOculusSdk(const HmdDeviceOculusSdk&);
    HmdDeviceOculusSdk& operator=(const HmdDeviceOculusSdk&);

    bool mIsInitialized;
    //ohmd_context* mpCtx;
    ovrHmd mpHmd;

    std::string mInfo;
    int mDisplayWidth;
    int mDisplayHeight;

    int mDisplayId;
    int mDisplayX;
    int mDisplayY;
    std::string mDisplayDeviceName;
};

#endif

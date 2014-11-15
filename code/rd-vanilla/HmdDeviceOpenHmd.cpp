#include "HmdDeviceOpenHmd.h"

#include "../../game/q_shared.h"
#include "../../client/client.h"
#include "../../qcommon/fixedmap.h"

#ifdef USE_SDL2
#ifdef LINUX
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#endif

#include <string.h>
#include <iostream>
#include <cstdio>
#include <algorithm>

using namespace std;

HmdDeviceOpenHmd::HmdDeviceOpenHmd()
    :mIsInitialized(false)
    ,mpCtx(NULL)
    ,mpHmd(NULL)
    ,mDisplayWidth(0)
    ,mDisplayHeight(0)
    ,mDisplayId(-1)
    ,mDisplayX(0)
    ,mDisplayY(0)
{

}

HmdDeviceOpenHmd::~HmdDeviceOpenHmd()
{

}

bool HmdDeviceOpenHmd::Init(bool allowDummyDevice)
{
    mpCtx = ohmd_ctx_create();
    int num_devices = ohmd_ctx_probe(mpCtx);
    if (num_devices < 0)
    {
        Com_Printf("failed to probe devices: %s\n", ohmd_ctx_get_error(mpCtx));
        return false;
    }

    int deviceCount = ohmd_ctx_probe(mpCtx);

    int foundDevice = -1;
    for (int i=0; i<deviceCount; i++)
    {
        const char* product = ohmd_list_gets(mpCtx, i, OHMD_PRODUCT);
        if (allowDummyDevice || strcmp(product, "Dummy Device") != 0)
        {
            if (foundDevice < 0)
            {
                foundDevice = i;
            }
            Com_Printf("Found devices: %s.\n", product);
        }
    }

    if (foundDevice < 0)
    {
        Com_Printf("No hmd device found.\n");
        return false;
    }


    mpHmd = ohmd_list_open_device(mpCtx, foundDevice);

    if (!mpHmd)
    {
        Com_Printf("failed to open device: %s\n", ohmd_ctx_get_error(mpCtx));
        return false;
    }

    mInfo = "HmdDeviceOpenHmd";

    const char* productName = ohmd_list_gets(mpCtx, 0, OHMD_PRODUCT);
    if (strlen(productName) > 0)
    {
        mInfo += " ";
        mInfo += productName;
    }

    const char* vendorName = ohmd_list_gets(mpCtx, 0, OHMD_VENDOR);
    if (strlen(vendorName) > 0)
    {
        mInfo += " ";
        mInfo += vendorName;
    }

    ohmd_device_geti(mpHmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &mDisplayWidth);
    ohmd_device_geti(mpHmd, OHMD_SCREEN_VERTICAL_RESOLUTION, &mDisplayHeight);

    DetectDisplay();

    mIsInitialized = true;

    return true;
}

void HmdDeviceOpenHmd::Shutdown()
{
    if (mpCtx == NULL)
    {
        return;
    }

    mInfo = "";

    mpHmd = NULL;

    ohmd_ctx_destroy(mpCtx);
    mpCtx = NULL;
}

string HmdDeviceOpenHmd::GetInfo()
{
    return mInfo;
}

bool HmdDeviceOpenHmd::HasDisplay()
{
    if (!mIsInitialized)
    {
        return false;
    }

    return true;
}

string HmdDeviceOpenHmd::GetDisplayDeviceName()
{
    return mDisplayDeviceName;
}

bool HmdDeviceOpenHmd::GetDisplayPos(int& rX, int& rY)
{
    if (!mIsInitialized)
    {
        return false;
    }

    rX = mDisplayX;
    rY = mDisplayY;
    return true;
}

bool HmdDeviceOpenHmd::GetDeviceResolution(int& rWidth, int& rHeight)
{
    if (!mIsInitialized || mDisplayWidth <= 0)
    {
        return false;
    }


    rWidth = mDisplayWidth;
    rHeight = mDisplayHeight;

    return true;
}

bool HmdDeviceOpenHmd::GetOrientationRad(float& rPitch, float& rYaw, float& rRoll)
{
    if (!mIsInitialized)
    {
        return false;
    }

    ohmd_ctx_update(mpCtx);

    float quat[4];
    ohmd_device_getf(mpHmd, OHMD_ROTATION_QUAT, &quat[0]);
    ConvertQuatToEuler(&quat[0], rYaw, rPitch, rRoll);

    //Com_Printf("pitch=%.2f yaw=%.2f roll=%.2f\n", RAD2DEG(rPitch), RAD2DEG(rYaw), RAD2DEG(rRoll));

    return true;
}

void HmdDeviceOpenHmd::ConvertQuatToEuler(const float* quat, float& rYaw, float& rPitch, float& rRoll)
{
    //https://svn.code.sf.net/p/irrlicht/code/trunk/include/quaternion.h
    // modified to get yaw before pitch

    float W = quat[3];
    float X = quat[1];
    float Y = quat[0];
    float Z = quat[2];

    float sqw = W*W;
    float sqx = X*X;
    float sqy = Y*Y;
    float sqz = Z*Z;

    float test = 2.0f * (Y*W - X*Z);

    if (test > (1.0f - 0.000001f))
    {
        // heading = rotation about z-axis
        rRoll = (-2.0f*atan2(X, W));
        // bank = rotation about x-axis
        rYaw = 0;
        // attitude = rotation about y-axis
        rPitch = M_PI/2.0f;
    }
    else if (test < (-1.0f + 0.000001f))
    {
        // heading = rotation about z-axis
        rRoll = (2.0f*atan2(X, W));
        // bank = rotation about x-axis
        rYaw = 0;
        // attitude = rotation about y-axis
        rPitch = M_PI/-2.0f;
    }
    else
    {
        // heading = rotation about z-axis
        rRoll = atan2(2.0f * (X*Y +Z*W),(sqx - sqy - sqz + sqw));
        // bank = rotation about x-axis
        rYaw = atan2(2.0f * (Y*Z +X*W),(-sqx - sqy + sqz + sqw));
        // attitude = rotation about y-axis
        test = max(test, -1.0f);
        test = min(test, 1.0f);
        rPitch = asin(test);
    }
}

void HmdDeviceOpenHmd::DetectDisplay()
{
#ifdef USE_SDL2

    // we don't get any information about the display position from OpenHmd
    // so let's try to find the display witch SDL2

    // if no display is found the rift has to be the main monitor

    int displayCount = SDL_GetNumVideoDisplays();
    for (int i=0; i<displayCount; i++)
    {
        const char* displayName = SDL_GetDisplayName(i);
        if (strcmp(displayName, "Rift DK 7\"") == 0)
        {
            mDisplayId = i;
            mDisplayDeviceName = displayName;
            break;
        }

        SDL_Rect r;
        int ret = SDL_GetDisplayBounds(i, &r);
        if (ret == 0 && r.w == 1920 && r.h == 1080)
        {
            // this is a fallback, if the display name is not correct
            mDisplayId = i;
            mDisplayDeviceName = displayName;
        }
        else if (ret != 0)
        {
            const char* error = SDL_GetError();
            Com_Printf("SDL_GetDisplayBounds failed: %s\n", error);

        }

        //Com_Printf("display name: %s\n", displayName);
        //flush(std::cout);
    }

    if (mDisplayId >= 0)
    {
        SDL_Rect r;
        int ret = SDL_GetDisplayBounds(mDisplayId, &r);
        if (ret == 0)
        {
            mDisplayX = r.x;
            mDisplayY = r.y;

            //Com_Printf("display x=%d y=%d\n", r.x, r.y);
            //flush(std::cout);
        }
    }

#endif
}

#include "FactoryHmdDevice.h"
#include "HmdDeviceMouse.h"
#ifdef USE_OPENHMD
#include "HmdDeviceOpenHmd.h"
#include "HmdRendererOculusOpenHmd.h"
#endif

//#ifdef USE_OVR
#include "HmdDeviceOculusSdk.h"
#include "HmdRendererOculusSdk.h"
//#endif

#include "HmdRendererOculus.h"

#include <vector>
#include <memory.h>

using namespace std;

IHmdDevice* FactoryHmdDevice::CreateHmdDevice(bool allowDummyDevice)
{
    vector<IHmdDevice*> devices;

#ifdef USE_OPENHMD
    devices.push_back(new HmdDeviceOpenHmd());
#endif

//#ifdef USE_OVR
    devices.push_back(new HmdDeviceOculusSdk());
//#endif

#ifdef OVR_SIM_MOUSE
    devices.push_back(new HmdDeviceMouse());
#endif

    IHmdDevice* pSelectedDevice = NULL;

    for (unsigned int i=0; i<devices.size(); i++)
    {
        if (pSelectedDevice == NULL)
        {
            bool worked = devices[i]->Init(allowDummyDevice);
            if (worked)
            {
                pSelectedDevice = devices[i];
            }
            else
            {
                devices[i]->Shutdown();
            }
        }

        if (pSelectedDevice != devices[i])
        {
            delete devices[i];
            devices[i] = NULL;
        }
    }

    devices.clear();

    return pSelectedDevice;
}

IHmdRenderer* FactoryHmdDevice::CreateRendererForDevice(IHmdDevice* pDevice)
{
    if (pDevice == NULL)
    {
        return NULL;
    }

    int width = 0;
    int height = 0;

    bool needsRenderer = pDevice->GetDeviceResolution(width, height);
    if (!needsRenderer)
    {
        return NULL;
    }


#ifdef USE_OPENHMD
    HmdDeviceOpenHmd* pOpenHmd = dynamic_cast<HmdDeviceOpenHmd*>(pDevice);
    if (pOpenHmd != NULL)
    {
        //HmdRendererOculus* pRenderer = new HmdRendererOculus();
        HmdRendererOculusOpenHmd* pRenderer = new HmdRendererOculusOpenHmd(pOpenHmd);
        return pRenderer;
    }
#endif

//#ifdef USE_OVR
    HmdDeviceOculusSdk* pOculusSdk = dynamic_cast<HmdDeviceOculusSdk*>(pDevice);
    if (pOculusSdk != NULL)
    {
        //HmdRendererOculus* pRenderer = new HmdRendererOculus();
        HmdRendererOculusSdk* pRenderer = new HmdRendererOculusSdk(pOculusSdk);
        return pRenderer;
    }
//#endif

    HmdDeviceMouse* pHmdMouse = dynamic_cast<HmdDeviceMouse*>(pDevice);
    if (pHmdMouse != NULL)
    {
        HmdRendererOculus* pRenderer = new HmdRendererOculus();
        return pRenderer;
    }

    return NULL;
}

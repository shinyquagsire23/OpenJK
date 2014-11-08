/**
 * HMD extension for JediAcademy
 *
 *  Copyright 2014 by Jochen Leopold <jochen.leopold@model-view.com>
 */

#ifndef HmdRendererOculusSdk_H
#define HmdRendererOculusSdk_H

#include "IHmdRenderer.h"
#include "RenderTool.h"
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <OVR_CAPI_GL.h>

//#include <openhmd.h>

class HmdDeviceOculusSdk;

class HmdRendererOculusSdk : public IHmdRenderer
{
public:
    HmdRendererOculusSdk(HmdDeviceOculusSdk* pHmdDeviceOculusSdk);
    virtual ~HmdRendererOculusSdk();

    virtual bool Init(int windowWidth, int windowHeight, PlatformInfo platformInfo);
    virtual void Shutdown();

    virtual std::string GetInfo();

    virtual bool HandlesSwap();

    virtual bool GetRenderResolution(int& rWidth, int& rHeight);

    virtual void BeginRenderingForEye(int leftEye);
    virtual void EndFrame();

    virtual bool GetCustomProjectionMatrix(float* rProjectionMatrix, float zNear, float zFar, float fov);
    virtual bool GetCustomViewMatrix(float* rViewMatrix, float xPos, float yPos, float zPos, float bodyYaw);

    virtual bool Get2DViewport(int& rX, int& rY, int& rW, int& rH);

private:
    const char* GetVertexShader();
    const char* GetPixelShader();

    static const int FBO_COUNT = 2;
    RenderTool::FrameBufferInfo mFboInfos[FBO_COUNT];
    RenderTool::FrameBufferInfo mainBuffer;
    RenderTool::FrameBufferInfo testBuffer;

    bool mIsInitialized;

    GLhandleARB  mOculusProgram;

    int mWindowWidth;
    int mWindowHeight;
    int mRenderWidth;
    int mRenderHeight;
    ovrGLTexture eyeTextures[2];
    ovrPosef eyePoses[2];
    ovrVector3f eyeOffsets[2];
    ovrEyeRenderDesc eye_rdesc[2];
    int frameIndex;
    union ovrGLConfig glcfg;
    unsigned int hmd_caps;
    unsigned int distort_caps;
    const bool RENDER_WITH_DISTORT = 1;

    int mCurrentFbo;

    HmdDeviceOculusSdk* mpDevice;
    ovrHmd mpHmd;
    ovrTrackingState mpTs;

    float mInterpupillaryDistance;
};

#endif

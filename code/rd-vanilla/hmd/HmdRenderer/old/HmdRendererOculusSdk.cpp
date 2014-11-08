#include "HmdRendererOculusSdk.h"
#include "../HmdDevice/HmdDeviceOculusSdk.h"
#include "../../renderer/tr_local.h"
#include "PlatformInfo.h"
#include "../ClientHmd.h"

#include <math.h>


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>


HmdRendererOculusSdk::HmdRendererOculusSdk(HmdDeviceOculusSdk* pHmdDeviceOculusSdk)
    :mIsInitialized(true)
    ,mOculusProgram(0)
    ,mWindowWidth(0)
    ,mWindowHeight(0)
    ,mRenderWidth(0)
    ,mRenderHeight(0)
    ,mCurrentFbo(-1)
    ,mpDevice(pHmdDeviceOculusSdk)
    ,mpHmd(NULL)
    ,mInterpupillaryDistance(0)
{

}

HmdRendererOculusSdk::~HmdRendererOculusSdk()
{

}

bool HmdRendererOculusSdk::Init(int windowWidth, int windowHeight, PlatformInfo platformInfo)
{
    if (mpDevice == NULL || mpDevice->GetHmd() == NULL)
    {
        return false;
    }

    mpHmd = mpDevice->GetHmd();
    frameIndex = 0;

    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;

    // use higher render resolution for a better result
    mRenderWidth = mWindowWidth/2;
    mRenderHeight = mWindowHeight;

    bool worked_ = RenderTool::CreateFrameBuffer(mainBuffer, mRenderWidth*2, mRenderHeight);
    if (!worked_)
    {
        return false;
    }

    for (int i=0; i<FBO_COUNT; i++)
    {
        bool worked = RenderTool::CreateFrameBuffer(mFboInfos[i], mRenderWidth, mRenderHeight);
        if (!worked)
        {
            return false;
        }
    }

    ovrRecti eyeRenderViewport[2];
    eyeRenderViewport[0].Pos.x = 0;
    eyeRenderViewport[0].Pos.y = 0;
    eyeRenderViewport[0].Size.w = mpHmd->Resolution.w;
    eyeRenderViewport[0].Size.h = mpHmd->Resolution.h;

    eyeRenderViewport[1].Pos.x = mpHmd->Resolution.w/2;
    eyeRenderViewport[1].Pos.y = 0;
    eyeRenderViewport[1].Size.w = mpHmd->Resolution.w;
    eyeRenderViewport[1].Size.h = mpHmd->Resolution.h;

    // Start the sensor which provides the Rift’s pose and motion.
    ovrHmd_ConfigureTracking(mpHmd, ovrTrackingCap_Orientation |
    ovrTrackingCap_MagYawCorrection |
    ovrTrackingCap_Position, 0);

    // Query the HMD for the current tracking state.
    mpTs = ovrHmd_GetTrackingState(mpHmd, ovr_GetTimeInSeconds());

    eyeTextures[0].OGL.Header.API = ovrRenderAPI_OpenGL;
    eyeTextures[0].OGL.Header.TextureSize.w = mpHmd->Resolution.w;
    eyeTextures[0].OGL.Header.TextureSize.h = mpHmd->Resolution.h;
    eyeTextures[0].OGL.Header.RenderViewport = eyeRenderViewport[0];
    eyeTextures[0].OGL.TexId = mFboInfos[0].ColorBuffer;
 
    eyeTextures[1].OGL.Header.API = ovrRenderAPI_OpenGL;
    eyeTextures[1].OGL.Header.TextureSize.w = mpHmd->Resolution.w;
    eyeTextures[1].OGL.Header.TextureSize.h = mpHmd->Resolution.h;
    eyeTextures[1].OGL.Header.RenderViewport = eyeRenderViewport[1];
    eyeTextures[1].OGL.TexId = mFboInfos[1].ColorBuffer;

    //mOculusProgram = RenderTool::CreateShaderProgram(GetVertexShader(), GetPixelShader());

    mInterpupillaryDistance = OVR_DEFAULT_IPD;

    // Configure OpenGL.
    memset(&glcfg, 0, sizeof glcfg);
    glcfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    glcfg.OGL.Header.RTSize = mpHmd->Resolution;
    glcfg.OGL.Header.Multisample = 1;
    glcfg.OGL.Disp = glXGetCurrentDisplay();
    glcfg.OGL.Win = glXGetCurrentDrawable();

    hmd_caps = ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction;
    ovrHmd_SetEnabledCaps(mpHmd, hmd_caps);

    distort_caps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette | ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive;
    if(!ovrHmd_ConfigureRendering(mpHmd, &glcfg.Config, distort_caps, mpHmd->DefaultEyeFov, eye_rdesc)) {
        fprintf(stderr, "failed to configure distortion renderer\n");
    }

    /* disable the retarded "health and safety warning" */
    ovrhmd_EnableHSWDisplaySDKRender(mpHmd, 0);

    mCurrentFbo = -1;
    mIsInitialized = true;

    return true;
}


void HmdRendererOculusSdk::Shutdown()
{
    mpHmd = NULL;

    if (!mIsInitialized)
    {
        return;
    }

    qglBindFramebuffer(GL_FRAMEBUFFER, 0);

    mIsInitialized = false;
}

std::string HmdRendererOculusSdk::GetInfo()
{
    return "HmdRendererOculusSdk";
}

bool HmdRendererOculusSdk::HandlesSwap()
{
    return false;
}

bool HmdRendererOculusSdk::GetRenderResolution(int& rWidth, int& rHeight)
{
    if (!mIsInitialized)
    {
        return false;
    }

    rWidth = mRenderWidth;
    rHeight = mRenderHeight;

    return true;
}


void HmdRendererOculusSdk::BeginRenderingForEye(bool leftEye)
{
    if (!mIsInitialized)
    {
        return;
    }

    memset(eyeOffsets, 0, sizeof(ovrVector3f) * 2);
    ++frameIndex;

    mpTs = ovrHmd_GetTrackingState(mpHmd, ovr_GetTimeInSeconds());
    ovrHmd_GetEyePoses(mpHmd, frameIndex, eyeOffsets, eyePoses, nullptr);

    ovrFrameTiming hmdFrameTiming = ovrHmd_BeginFrame(mpHmd, ovr_GetTimeInSeconds());

    int fboId = 0;
    if (!leftEye && FBO_COUNT > 1)
    {
        fboId = 1;
    }

    /*if (mCurrentFbo == fboId)
    {
        return;
    }*/

    mCurrentFbo = fboId;

    qglBindFramebuffer(GL_FRAMEBUFFER, mFboInfos[mCurrentFbo].Fbo);
    RenderTool::ClearFBO(mFboInfos[mCurrentFbo]);
}

void HmdRendererOculusSdk::EndFrame()
{
    if (!mIsInitialized)
    {
        return;
    }

    //RenderTool::DrawFbos(&mFboInfos[0], FBO_COUNT, mWindowWidth, mWindowHeight, mOculusProgram);

    ovrHmd_EndFrame(mpHmd, eyePoses, (ovrTexture*)eyeTextures);
    qglUseProgramObjectARB(0);

   //Fixes a lot of drawing issues :/
   qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   qglBindBuffer(GL_ARRAY_BUFFER, 0);
}


bool HmdRendererOculusSdk::GetCustomProjectionMatrix(float* rProjectionMatrix, float zNear, float zFar, float fov)
{
    if (!mIsInitialized || mpHmd == NULL)
    {
        return false;
    }


    float fov_rad = DEG2RAD(fov);
    float aspect = 0;
    aspect = mWindowWidth / (2.0f* mWindowHeight);

    if (fov_rad == 0 || aspect == 0)
    {
        return false;
    }


    float hScreenSize = 0.125760f; //Eww, magic numbers. Can't find an equivalent in OVR SDK. :<
    float lensSeparation = OVR_DEFAULT_IPD;
    //ohmd_device_getf(mpHmd, OHMD_SCREEN_HORIZONTAL_SIZE, &hScreenSize);


    float screenCenter = hScreenSize / 4.0f;
    float lensShift = screenCenter - lensSeparation / 2.0f;
    float projOffset = 2.0f * lensShift / hScreenSize;
    projOffset *= mCurrentFbo == 0 ? 1.f : -1.f;

    glm::mat4 perspMat = glm::perspective(fov_rad, aspect, zNear, zFar);
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(projOffset, 0, 0));
    perspMat = translate * perspMat;

    memcpy(rProjectionMatrix, &perspMat[0][0], sizeof(float)*16);

    return true;
}

bool HmdRendererOculusSdk::GetCustomViewMatrix(float* rViewMatrix, float xPos, float yPos, float zPos, float bodyYaw)
{
    if (!mIsInitialized)
    {
        return false;
    }

    // get current hmd rotation
    float quat[4];
    ovrPosef pose = mpTs.HeadPose.ThePose;
    quat[0] = pose.Orientation.x;
    quat[1] = pose.Orientation.y;
    quat[2] = pose.Orientation.z;
    quat[3] = pose.Orientation.w;
    //ohmd_device_getf(mpHmd, OHMD_ROTATION_QUAT, &quat[0]);
    glm::quat hmdRotation = glm::inverse(glm::quat(quat[3], quat[0], quat[1], quat[2]));

    // change hmd orientation to game coordinate system
    glm::quat convertHmdToGame = glm::rotate(glm::quat(1.0f,0.0f,0.0f,0.0f), (float)DEG2RAD(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    convertHmdToGame = glm::rotate(convertHmdToGame, (float)DEG2RAD(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 hmdRotationMat = glm::mat4_cast(hmdRotation) * glm::mat4_cast(convertHmdToGame);


    // convert body transform to matrix
    glm::mat4 bodyPosition = glm::translate(glm::mat4(1.0f), glm::vec3(-xPos, -yPos, -zPos));
    glm::quat bodyYawRotation = glm::rotate(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), (float)(DEG2RAD(-bodyYaw)), glm::vec3(0.0f, 0.0f, 1.0f));

    // create view matrix
    glm::mat4 viewMatrix = hmdRotationMat * glm::mat4_cast(bodyYawRotation) * bodyPosition;

    //meter to game unit (game unit = feet*2)
    float meterToGame = 3.28084f*2.0f;
    // apply ipd
    float halfIPD = mInterpupillaryDistance * 0.5f * meterToGame * (mCurrentFbo == 0 ? 1.0f : -1.0f);

    glm::mat4 translateIpd = glm::translate(glm::mat4(1.0f), glm::vec3(halfIPD/2, 0, 0));
    viewMatrix = translateIpd * viewMatrix;

    memcpy(rViewMatrix, &viewMatrix[0][0], sizeof(float) * 16);


    return true;
}

bool HmdRendererOculusSdk::Get2DViewport(int& rX, int& rY, int& rW, int& rH)
{
    // shrink the gui for the HMD display
    float scale = 0.3f;
    float aspect = 1.0f;

    rW = mRenderWidth * scale;
    rH = mRenderWidth* scale * aspect;

    rX = (mRenderWidth - rW)/2.0f;
    int xOff = mRenderWidth/12.5f;
    xOff *= mCurrentFbo == 0 ? 1 : -1;
    rX += xOff;

    rY = (mRenderHeight - rH)/2;

    return true;
}



const char* HmdRendererOculusSdk::GetVertexShader()
{
    return
         "#version 150\n"
         "uniform mat4 MVPMatrix;\n"
         "in vec3 position;\n"
         "void main()\n"
         "{\n"
         "    gl_Position = MVPMatrix * vec4(position, 1.0);\n"
         "}";
}

const char* HmdRendererOculusSdk::GetPixelShader()
{
    // only works with DK1

    return

         "#version 150\n"
         "out vec4 outputColor;\n"
         "void main()\n"
         "{\n"
         "    outputColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
         "}";


}

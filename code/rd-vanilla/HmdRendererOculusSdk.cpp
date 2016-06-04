#include "HmdRendererOculusSdk.h"
#include "HmdDeviceOculusSdk.h"
#include "tr_local.h" 
#include "qgl.h" 
#include "PlatformInfo.h"
#include "ClientHmd.h"

#include <math.h>


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <OVR_CAPI_GL.h>
#include <CAPI/CAPI_HSWDisplay.h>

#include "tr_local.h"


HmdRendererOculusSdk::HmdRendererOculusSdk(HmdDeviceOculusSdk* pHmdDeviceOculusSdk)
    :mIsInitialized(false)
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
void *vr_ipc_buf;

bool HmdRendererOculusSdk::Init(int windowWidth, int windowHeight, PlatformInfo platformInfo)
{
    bodyMove = 9001.0f;
    struct stat sb;
    int fdSrc;		
	fdSrc = open("/tmp/ripvrcontroller", O_RDONLY);
    Com_Printf("Opening /tmp/ripvrcontroller\n");
    if (fdSrc != -1)
    {
        Com_Printf("fdSrc != -1\n");
        if (fstat(fdSrc, &sb) != -1)
        {
            Com_Printf("fstat gud\n");
            if (sb.st_size != 0)
            {
                Com_Printf("size good\n");
                vr_ipc_buf = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fdSrc, 0);
                Com_Printf("vr_ipc_buf mapped to %x\n", vr_ipc_buf);
            }
        }
    }

    Com_Printf("HMD setting up...\n");
    if (mpDevice == NULL || mpDevice->GetHmd() == NULL)
    {
        Com_Printf("HMD failed to set up\n");
        return false;
    }

    mpHmd = mpDevice->GetHmd();
    frameIndex = 0;

    mWindowWidth = mpHmd->Resolution.w;//windowWidth;
    mWindowHeight = mpHmd->Resolution.h;//windowHeight;

    // use higher render resolution for a better result
    mRenderWidth = (mWindowWidth/2);//*1.71f;
    mRenderHeight = mWindowHeight;//*1.71f;

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
    eyeRenderViewport[0].Size.w = mpHmd->Resolution.w/2;
    eyeRenderViewport[0].Size.h = mpHmd->Resolution.h;

    eyeRenderViewport[1].Pos.x = 0;
    eyeRenderViewport[1].Pos.y = 0;
    eyeRenderViewport[1].Size.w = mpHmd->Resolution.w/2;
    eyeRenderViewport[1].Size.h = mpHmd->Resolution.h;

    // Start the sensor which provides the Rift’s pose and motion.
    ovrHmd_ConfigureTracking(mpHmd, ovrTrackingCap_Orientation |
    ovrTrackingCap_MagYawCorrection |
    ovrTrackingCap_Position, 0);

    // Query the HMD for the current tracking state.
    mpTs = ovrHmd_GetTrackingState(mpHmd, ovr_GetTimeInSeconds());

    eyeTextures[0].OGL.Header.API = ovrRenderAPI_OpenGL;
    eyeTextures[0].OGL.Header.TextureSize.w = mpHmd->Resolution.w/2;
    eyeTextures[0].OGL.Header.TextureSize.h = mpHmd->Resolution.h;
    eyeTextures[0].OGL.Header.RenderViewport = eyeRenderViewport[0];
    eyeTextures[0].OGL.TexId = mFboInfos[0].ColorBuffer;
 
    eyeTextures[1].OGL.Header.API = ovrRenderAPI_OpenGL;
    eyeTextures[1].OGL.Header.TextureSize.w = mpHmd->Resolution.w/2;
    eyeTextures[1].OGL.Header.TextureSize.h = mpHmd->Resolution.h;
    eyeTextures[1].OGL.Header.RenderViewport = eyeRenderViewport[1];
    eyeTextures[1].OGL.TexId = mFboInfos[1].ColorBuffer;

    mOculusProgram = RenderTool::CreateShaderProgram(GetVertexShader(), GetPixelShader());

    mInterpupillaryDistance = OVR_DEFAULT_IPD;

    // Configure OpenGL.
    memset(&glcfg, 0, sizeof glcfg);
    glcfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    glcfg.OGL.Header.BackBufferSize = mpHmd->Resolution;
    glcfg.OGL.Header.Multisample = 1;
    //glcfg.OGL.Disp = glXGetCurrentDisplay();
    //glcfg.OGL.Win = glXGetCurrentDrawable();

    hmd_caps = ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction;
    ovrHmd_SetEnabledCaps(mpHmd, hmd_caps);

    distort_caps = ovrDistortionCap_Overdrive;//| ovrDistortionCap_TimeWarp;//  | ovrDistortionCap_Vignette;
    if(RENDER_WITH_DISTORT)
    {
        if(!ovrHmd_ConfigureRendering(mpHmd, &glcfg.Config, distort_caps, mpHmd->DefaultEyeFov, eye_rdesc)) 
        {
            Com_Printf("failed to configure distortion renderer\n");
        }
    }

    /* disable the retarded "health and safety warning" */
    //ovrhmd_EnableHSWDisplaySDKRender(mpHmd, false);

    mCurrentFbo = -1;
    mIsInitialized = true;
    Com_Printf("HMD Successfully Set up\n");
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
    return true;
}

bool HmdRendererOculusSdk::GetRenderResolution(int& rWidth, int& rHeight)
{
    /*if (!mIsInitialized)
    {
        return false;
    }*/

    rWidth = mRenderWidth;
    rHeight = mRenderHeight;

    return true;
}

void HmdRendererOculusSdk::BeginRenderingForEye(int leftEye)
{
    bool main = 0;
    if (!mIsInitialized)
    {
        return;
    }

    memset(eyeOffsets, 0, sizeof(ovrVector3f) * 2);
    ++frameIndex;

    mpTs = ovrHmd_GetTrackingState(mpHmd, ovr_GetTimeInSeconds());
    ovrHmd_GetEyePoses(mpHmd, frameIndex, eyeOffsets, eyePoses, 0);
ovrHmd_DismissHSWDisplay(mpHmd);

    ovrFrameTiming hmdFrameTiming = ovrHmd_BeginFrame(mpHmd, ovr_GetTimeInSeconds());

    int fboId = 0;
    if (leftEye == 0 && FBO_COUNT > 1)
    {
        fboId = 1;
    }

    /*if (mCurrentFbo == fboId)
    {
        return;
    }*/

    mCurrentFbo = fboId;
    if(leftEye < 2)
        qglBindFramebuffer(GL_FRAMEBUFFER, mFboInfos[mCurrentFbo].Fbo);
    else
        qglBindFramebuffer(GL_FRAMEBUFFER,0);
    RenderTool::ClearFBO(mFboInfos[mCurrentFbo]);
}

void HmdRendererOculusSdk::EndFrame()
{
    if (!mIsInitialized)
    {
        return;
    }

     //RenderTool::DrawFbosToFboAndDrawDatFbo(mainBuffer, &mFboInfos[0], FBO_COUNT, mWindowWidth, mWindowHeight, mOculusProgram);
	RenderTool::DrawFbos(&mFboInfos[0], FBO_COUNT, mWindowWidth, mWindowHeight, mOculusProgram);

    if(RENDER_WITH_DISTORT)
    {
        //qglDisable(GL_SCISSOR_TEST);
        ovrHmd_EndFrame(mpHmd, eyePoses, (ovrTexture*)eyeTextures);
        //qglEnable(GL_SCISSOR_TEST);

        //qglUseProgramObjectARB(0);

       //Fixes a lot of drawing issues :/
       qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
       qglBindBuffer(GL_ARRAY_BUFFER, 0);
    }

	ClientHmd::Get()->UpdateGame(bodyYaw);
}


bool HmdRendererOculusSdk::GetCustomProjectionMatrix(float* rProjectionMatrix, float zNear, float zFar, float fov)
{
    if (!mIsInitialized || mpHmd == NULL)
    {
        return false;
    }


    float fov_rad = DEG2RAD(fov);
    float aspect = 0;
    aspect = mRenderWidth*2 / (2.1f* mRenderHeight);

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

bool HmdRendererOculusSdk::GetCustomViewMatrix(float* rViewMatrix, float xPos, float yPos, float zPos, float bodyYaw_)
{
    if (!mIsInitialized)
    {
        return false;
    }
    
    int stick_x_raw = *(int*)(vr_ipc_buf+(sizeof(int)*11));
	int stick_y_raw = *(int*)(vr_ipc_buf+(sizeof(int)*10));
	int stick_x2_raw = *(int*)(vr_ipc_buf+(sizeof(int)*28));
	int stick_y2_raw = *(int*)(vr_ipc_buf+(sizeof(int)*27));
	    
	float stick_x = -(float(stick_x_raw) - 525) / 525;
    float stick_y = -(float(stick_y_raw) - 525) / 525;
    float stick_x2 = -(float(stick_x2_raw) - 525) / 525;
    float stick_y2 = -(float(stick_y2_raw) - 525) / 525;

    //meter to game unit (game unit = feet*2)
    float meterToGame = 3.28084f*2.0f*5.0f;

	float bodyDiff = lastBodyYaw - bodyYaw_;

	// Make an imaginary box where the crosshair can roam free from the head.
	// Similar to HL2/TF2's aiming setup. TODO: Make box width & height adjustable w/ cvars
	// To work with 360/0 degree looping, the difference in yaw is checked to be
	// over 300 (aka an unusual turn amount). This amount *can* be hit normally but
	// it's unlikely to be hit in normal play. TODO: Maybe fix that to be better.
	const int BOX_WIDTH = 100; //In degrees. Higher values tend to glitch more stereoscopically.
	
	if(!bodyDiffOnce)
	{
		bodyMove = bodyYaw_;
		bodyDiff = 0.0f;
		//bodyYaw_ -= BOX_WIDTH / 2;
		bodyDiffOnce = true;
    }

	/*if((bodyYaw_ < (bodyMove - BOX_WIDTH) || bodyYaw_ > (bodyMove + BOX_WIDTH)))
	{
		//bodyTotalDiff += bodyDiff;
		if(bodyYaw_ < (bodyMove - BOX_WIDTH) && (bodyDiff < 300 && bodyDiff > -300))
		{
			bodyTotalDiff = BOX_WIDTH;
			bodyMove = bodyYaw_ + bodyTotalDiff;
		}
		else if(bodyYaw_ > (bodyMove + BOX_WIDTH) && (bodyDiff < 300 && bodyDiff > -300))
		{
			bodyTotalDiff = -BOX_WIDTH;
			bodyMove = bodyYaw_ + bodyTotalDiff;
		}
		else if(bodyDiff > 300)
		{
			bodyTotalDiff = -BOX_WIDTH;
			bodyMove = bodyYaw_ + bodyTotalDiff;
		}
		else if(bodyDiff < -300)
		{
			bodyTotalDiff = BOX_WIDTH;
			bodyMove = bodyYaw_ + bodyTotalDiff;
		}

		bodyYaw = bodyYaw_ + bodyTotalDiff;
	}
	else
	{
		bodyTotalDiff = 0;
		bodyYaw = bodyMove;
	}*/

	//More simple version. Smoother and doesn't bug out at 360/0 degrees, but has a ton of bugs in regards to external cams.
	if(bodyDiff < 300 && bodyDiff > -300)
		bodyTotalDiff += bodyDiff;

	//For some reason when you die or go into a cam your yaw skyrockets and leaves it weird afterwards (offset or whatnot). A bit hacky but works to reset yaw after deaths.
	if(fabsf(bodyYaw) > 500)
	{
		bodyTotalDiff = 0;
		bodyModDiff = 0;
		bodyDiff = 0;
		bodyMove = bodyYaw_;
		bodyYaw = bodyMove;
		bodyDiffOnce = false;
	}

	if(fabsf(bodyTotalDiff) >= BOX_WIDTH)
	{
		bodyYaw = bodyYaw_ + bodyModDiff;
		bodyMove = bodyYaw_ + bodyModDiff;
		bodyTotalDiff = (bodyTotalDiff < 0 ? -BOX_WIDTH : BOX_WIDTH);
	}
	else if(fabsf(stick_x) > 0.6f)
	{
	    bodyYaw = bodyYaw_ + bodyModDiff;
		bodyMove = bodyYaw_ + bodyModDiff;
	}
	else
	{
		bodyYaw = bodyMove;
		bodyModDiff += bodyDiff;
	}


	/*if(bodyDiff > 20 || bodyDiff < -20)
	{
		Com_Printf("[HMD] Current body diff: %f\n", bodyDiff);
		Com_Printf("[HMD] Current eye: %i\n", mCurrentFbo);
		Com_Printf("[HMD] Current yaw: %f\n", bodyYaw_);
		Com_Printf("[HMD] Current applied yaw: %f\n", bodyYaw);
		//Com_Printf("[HMD] Current body move: %f\n", bodyMove);
	}*/


    // get current hmd rotation
    float quat[4];
    //bodyYaw = bodyYaw_;
    ovrPosef pose = mpTs.HeadPose.ThePose;
    quat[0] = pose.Orientation.x;
    quat[1] = pose.Orientation.y;
    quat[2] = pose.Orientation.z;
    quat[3] = pose.Orientation.w;
    glm::quat hmdRotation = glm::inverse(glm::quat(quat[3], quat[0], quat[1], quat[2]));

    // change hmd orientation to game coordinate system
    glm::quat convertHmdToGame = glm::rotate(glm::quat(1.0f,0.0f,0.0f,0.0f), (float)DEG2RAD(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    convertHmdToGame = glm::rotate(convertHmdToGame, (float)DEG2RAD(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 hmdRotationMat = glm::mat4_cast(hmdRotation) * glm::mat4_cast(convertHmdToGame);

	//Com_Printf("X: %f\n", pose.Orientation.x);
	//Com_Printf("Y: %f\n", pose.Orientation.y);
	//Com_Printf("Z: %f\n\n", pose.Orientation.z);

	const float positional_amplification = 1.0f;

	float quat_pos[4];
    quat_pos[0] = (-pose.Position.z * positional_amplification * meterToGame * cos((float)DEG2RAD(bodyYaw))) - (-pose.Position.x * positional_amplification * meterToGame * sin((float)DEG2RAD(bodyYaw)));
    quat_pos[1] = pose.Position.y * positional_amplification * meterToGame;
    quat_pos[2] = (-pose.Position.z * positional_amplification * meterToGame * sin((float)DEG2RAD(bodyYaw))) + (-pose.Position.x * positional_amplification * meterToGame * cos((float)DEG2RAD(bodyYaw)));
    quat_pos[3] = 0;
	glm::mat4 hmdPosition = glm::translate(glm::mat4(1.0f), glm::vec3(-quat_pos[0], -quat_pos[2], -quat_pos[1]));

    // convert body transform to matrix
    glm::mat4 bodyPosition = glm::translate(glm::mat4(1.0f), glm::vec3(-xPos, -yPos, -zPos));
    glm::quat bodyYawRotation = glm::rotate(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), (float)(DEG2RAD(-bodyYaw)), glm::vec3(0.0f, 0.0f, 1.0f));

    // create view matrix
    glm::mat4 viewMatrix = hmdRotationMat * glm::mat4_cast(bodyYawRotation) * bodyPosition * hmdPosition;

    // apply ipd
    float halfIPD = mInterpupillaryDistance * 0.5f * meterToGame * (mCurrentFbo == 0 ? 1.0f : -1.0f);

    glm::mat4 translateIpd = glm::translate(glm::mat4(1.0f), glm::vec3(halfIPD/2, 0, 0));
    viewMatrix = translateIpd * viewMatrix;

    memcpy(rViewMatrix, &viewMatrix[0][0], sizeof(float) * 16);

lastBodyYaw = bodyYaw_;

    return true;
}

bool HmdRendererOculusSdk::Get2DViewport(int& rX, int& rY, int& rW, int& rH)
{
    // shrink the gui for the HMD display
    bool in_camera_out = ri.Cvar_Get( "cg_in_camera", "0", 0 )->integer;
    float scale = in_camera_out ? 1.0f : 0.3f;
    float aspect = 1.0f;

    rW = mRenderWidth * scale;
    rH = mRenderWidth* scale * aspect;

    rX = (mRenderWidth - rW)/2.0f;
    int xOff = mRenderWidth/3.5f;

    //meter to game unit (game unit = feet*2)
    float meterToGame = 3.28084f*2.0f*5.0f;
    // apply ipd
    float halfIPD = mInterpupillaryDistance * 0.5f * meterToGame * (mCurrentFbo == 0 ? 1.0f : -1.0f);

    xOff *= mCurrentFbo == 0 ? 1 : -1;
    rX += halfIPD;

    rY = (mRenderHeight - rH)/2;

    return true;
}



const char* HmdRendererOculusSdk::GetVertexShader()
{
    return
        "void main() {\n"
        "   gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "   gl_Position = ftransform();\n"
        "}";
}

const char* HmdRendererOculusSdk::GetPixelShader()
{
    // only works with DK1

    return

        "#version 120\n"
        "\n"
        "            vec2 HmdWarp(vec2 in01, vec2 LensCenter)\n"
        "            {\n"
        "                return 1;\n"
        "            }\n"
        "            \n"
        "            void main()\n"
        "            {\n"
        "                // The following two variables need to be set per eye\n"
        "                vec2 LensCenter = gl_FragCoord.x < (1920/2) ? LeftLensCenter : RightLensCenter;\n"
        "                vec2 ScreenCenter = gl_FragCoord.x < (1920/2) ? LeftScreenCenter : RightScreenCenter;\n"
        "            \n"
        "                vec2 oTexCoord = gl_FragCoord.xy / vec2(1920, 800);\n"
        "            \n"
        "                vec2 tc = HmdWarp(oTexCoord, LensCenter);\n"
        "                if (any(bvec2(clamp(tc,ScreenCenter-vec2(0.25,0.5), ScreenCenter+vec2(0.25,0.5)) - tc)))\n"
        "                {\n"
        "                    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "                    return;\n"
        "                }\n"
        "            \n"
        "                tc.x = gl_FragCoord.x < (1920/2) ? (2.0 * tc.x) : (2.0 * (tc.x - 0.5));\n"
        "                gl_FragColor = texture2D(warpTexture, tc);\n"
        "            }";


}

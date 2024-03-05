#include "sound/SI2.hpp"
#include "console/CVar.hpp"

bool OutboundChatVolumeHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool InboundChatVolumeHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool VoiceChatModeHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool VoiceActivationSensitivityHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool EnableMicrophoneHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool EnableVoiceChatHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool SelfMuteHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool PushToTalkButtonHandler(CVar*, const char*, const char*, void*) {
    return true;
}

bool EnableReverb_CVarCallback(CVar*, const char*, const char*, void*) {
    return true;
}

bool VoiceChatInputDriverIndex_CVarCallback(CVar*, const char*, const char*, void*) {
    return true;
}

bool VoiceChatOutputDriverIndex_CVarCallback(CVar*, const char*, const char*, void*) {
    return true;
}

bool OutputDriverIndex_CVarCallback(CVar*, const char*, const char*, void*) {
    return true;
}


void SI2::RegisterCVars() {
    CVar::Register("StartTalkingDelay", "", 0, "0.0", 0, 5, 0, 0, 0);
    CVar::Register("StartTalkingTime", "", 0, "1.0", 0, 5, 0, 0, 0);
    CVar::Register("StopTalkingDelay", "", 0, "0.0", 0, 5, 0, 0, 0);
    CVar::Register("StopTalkingTime", "", 0, "2.0", 0, 5, 0, 0, 0);
    CVar::Register("OutboundChatVolume", "The software amplification factor (0.0 - 2.0)", 0, "1.0", &OutboundChatVolumeHandler, 5, 0, 0, 0);
    CVar::Register("InboundChatVolume", "The volume of all other chat you hear (0.0 - 1.0)", 0, "1.0", &InboundChatVolumeHandler, 5, 0, 0, 0);
    CVar::Register("VoiceChatMode", "Push to talk(0) or voice activation(1)", 0, "0", &VoiceChatModeHandler, 5, 0, 0, 0);
    CVar::Register("VoiceActivationSensitivity", "Sensitivity of the microphone (0.0 - 1.0)", 0, "0.4", &VoiceActivationSensitivityHandler, 5, 0, 0, 0);
    CVar::Register("EnableMicrophone", "Enables the microphone so you can speak.", 0, "1", &EnableMicrophoneHandler, 5, 0, 0, 0);
    CVar::Register("EnableVoiceChat", "Enables the voice chat feature.", 0, "0", &EnableVoiceChatHandler, 5, 0, 0, 0);
    CVar::Register("VoiceChatSelfMute", "Turn off your ability to talk.", 0, "0", &SelfMuteHandler, 5, 0, 0, 0);
    CVar::Register("PushToTalkButton", "String representation of the Push-To-Talk button.", 0, "`", &PushToTalkButtonHandler, 5, 0, 0, 0);
    CVar::Register("Sound_OutputQuality", "sound quality, default 1 (medium)", 0, "1", 0, 7, 0, 0, 0);
    CVar::Register("Sound_NumChannels", "number of sound channels", 0, "32", 0, 7, 0, 0, 0);
    CVar::Register("Sound_EnableReverb", "", 0, "0", &EnableReverb_CVarCallback, 7, 0, 0, 0);
    CVar::Register("Sound_EnableSoftwareHRTF", "", 0, "0", 0, 7, 0, 0, 0);
    CVar::Register("Sound_VoiceChatInputDriverIndex", "", 0, "0", &VoiceChatInputDriverIndex_CVarCallback, 7, 0, 0, 0);
    CVar::Register("Sound_VoiceChatInputDriverName", "", 0, "Primary Sound Capture Driver", 0, 7, 0, 0, 0);
    CVar::Register("Sound_VoiceChatOutputDriverIndex", "", 0, "0", &VoiceChatOutputDriverIndex_CVarCallback, 7, 0, 0, 0);
    CVar::Register("Sound_VoiceChatOutputDriverName", "", 0, "Primary Sound Driver", 0, 7, 0, 0, 0);
    CVar::Register("Sound_OutputDriverIndex", "", 0, "0", &OutputDriverIndex_CVarCallback, 7, 0, 0, 0);
    CVar::Register("Sound_OutputDriverName", "", 0, "Primary Sound Driver", 0, 7, 0, 0, 0);
    CVar::Register("Sound_DSPBufferSize", "sound buffer size, default 0", 0, "0", 0, 7, 0, 0, 0);
    CVar::Register("Sound_EnableHardware", "Enables Hardware", 0, "0", 0, 7, 0, 0, 0);
    CVar::Register("Sound_EnableMode2", "test", 0, "0", 0, 7, 0, 0, 0);
    CVar::Register("Sound_EnableMixMode2", "test", 0, "0", 0, 7, 0, 0, 0);
}

//===-- StringExtractorGDBRemote.h ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef utility_StringExtractorGDBRemote_h_
#define utility_StringExtractorGDBRemote_h_

// C Includes
// C++ Includes
#include <string>
// Other libraries and framework includes
// Project includes
#include "Utility/StringExtractor.h"

class StringExtractorGDBRemote : public StringExtractor
{
public:

    StringExtractorGDBRemote() :
        StringExtractor ()
    {
    }

    StringExtractorGDBRemote(const char *cstr) :
        StringExtractor (cstr)
    {
    }
    StringExtractorGDBRemote(const StringExtractorGDBRemote& rhs) :
        StringExtractor (rhs)
    {
    }

    virtual ~StringExtractorGDBRemote()
    {
    }

    enum ServerPacketType
    {
        eServerPacketType_nack = 0,
        eServerPacketType_ack,
        eServerPacketType_invalid,
        eServerPacketType_unimplemented,
        eServerPacketType_interrupt, // CTRL+c packet or "\x03"
        eServerPacketType_A, // Program arguments packet
        eServerPacketType_qfProcessInfo,
        eServerPacketType_qsProcessInfo,
        eServerPacketType_qC,
        eServerPacketType_qGroupName,
        eServerPacketType_qHostInfo,
        eServerPacketType_qLaunchGDBServer,
        eServerPacketType_qKillSpawnedProcess,
        eServerPacketType_qLaunchSuccess,
        eServerPacketType_qProcessInfoPID,
        eServerPacketType_qSpeedTest,
        eServerPacketType_qUserName,
        eServerPacketType_qGetWorkingDir,
        eServerPacketType_QEnvironment,
        eServerPacketType_QLaunchArch,
        eServerPacketType_QSetDisableASLR,
        eServerPacketType_QSetSTDIN,
        eServerPacketType_QSetSTDOUT,
        eServerPacketType_QSetSTDERR,
        eServerPacketType_QSetWorkingDir,
        eServerPacketType_QStartNoAckMode,
        eServerPacketType_qPlatform_shell,
        eServerPacketType_qPlatform_mkdir,
        eServerPacketType_qPlatform_chmod,
        eServerPacketType_vFile_open,
        eServerPacketType_vFile_close,
        eServerPacketType_vFile_pread,
        eServerPacketType_vFile_pwrite,
        eServerPacketType_vFile_size,
        eServerPacketType_vFile_mode,
        eServerPacketType_vFile_exists,
        eServerPacketType_vFile_md5,
        eServerPacketType_vFile_stat,
        eServerPacketType_vFile_symlink,
        eServerPacketType_vFile_unlink,
      // debug server packages
        eServerPacketType_QEnvironmentHexEncoded,
        eServerPacketType_QListThreadsInStopReply,
        eServerPacketType_QRestoreRegisterState,
        eServerPacketType_QSaveRegisterState,
        eServerPacketType_QSetLogging,
        eServerPacketType_QSetMaxPacketSize,
        eServerPacketType_QSetMaxPayloadSize,
        eServerPacketType_QSetEnableAsyncProfiling,
        eServerPacketType_QSyncThreadState,
        eServerPacketType_QThreadSuffixSupported,

        eServerPacketType_qsThreadInfo,
        eServerPacketType_qfThreadInfo,
        eServerPacketType_qGetPid,
        eServerPacketType_qGetProfileData,
        eServerPacketType_qGDBServerVersion,
        eServerPacketType_qMemoryRegionInfo,
        eServerPacketType_qMemoryRegionInfoSupported,
        eServerPacketType_qProcessInfo,
        eServerPacketType_qRcmd,
        eServerPacketType_qRegisterInfo,
        eServerPacketType_qShlibInfoAddr,
        eServerPacketType_qStepPacketSupported,
        eServerPacketType_qSyncThreadStateSupported,
        eServerPacketType_qThreadExtraInfo,
        eServerPacketType_qThreadStopInfo,
        eServerPacketType_qVAttachOrWaitSupported,
        eServerPacketType_qWatchpointSupportInfo,
        eServerPacketType_qWatchpointSupportInfoSupported,

        eServerPacketType_vAttach,
        eServerPacketType_vAttachWait,
        eServerPacketType_vAttachOrWait,
        eServerPacketType_vAttachName,
        eServerPacketType_vCont,
        eServerPacketType_vCont_actions, // vCont?

        eServerPacketType_stop_reason, // '?'

        eServerPacketType_c,
        eServerPacketType_C,
        eServerPacketType_D,
        eServerPacketType_g,
        eServerPacketType_G,
        eServerPacketType_H,
        eServerPacketType_k,
        eServerPacketType_m,
        eServerPacketType_M,
        eServerPacketType_p,
        eServerPacketType_P,
        eServerPacketType_s,
        eServerPacketType_S,
        eServerPacketType_T,
        eServerPacketType_Z,
        eServerPacketType_z,

        eServerPacketType__M,
        eServerPacketType__m,
    };
    
    ServerPacketType
    GetServerPacketType () const;

    enum ResponseType
    {
        eUnsupported = 0,
        eAck,
        eNack,
        eError,
        eOK,
        eResponse
    };

    ResponseType
    GetResponseType () const;

    bool
    IsOKResponse() const;

    bool
    IsUnsupportedResponse() const;

    bool
    IsNormalResponse () const;

    bool
    IsErrorResponse() const;

    // Returns zero if the packet isn't a EXX packet where XX are two hex
    // digits. Otherwise the error encoded in XX is returned.
    uint8_t
    GetError();
    
    size_t
    GetEscapedBinaryData (std::string &str);

};

#endif  // utility_StringExtractorGDBRemote_h_

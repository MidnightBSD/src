//===-- StringExtractorGDBRemote.cpp ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
#include <string.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "Utility/StringExtractorGDBRemote.h"



StringExtractorGDBRemote::ResponseType
StringExtractorGDBRemote::GetResponseType () const
{
    if (m_packet.empty())
        return eUnsupported;

    switch (m_packet[0])
    {
    case 'E':
        if (m_packet.size() == 3 &&
            isxdigit(m_packet[1]) &&
            isxdigit(m_packet[2]))
            return eError;
        break;

    case 'O':
        if (m_packet.size() == 2 && m_packet[1] == 'K')
            return eOK;
        break;

    case '+':
        if (m_packet.size() == 1)
            return eAck;
        break;

    case '-':
        if (m_packet.size() == 1)
            return eNack;
        break;
    }
    return eResponse;
}

StringExtractorGDBRemote::ServerPacketType
StringExtractorGDBRemote::GetServerPacketType () const
{
#define PACKET_MATCHES(s) ((packet_size == (sizeof(s)-1)) && (strcmp((packet_cstr),(s)) == 0))
#define PACKET_STARTS_WITH(s) ((packet_size >= (sizeof(s)-1)) && ::strncmp(packet_cstr, s, (sizeof(s)-1))==0)
    
    // Empty is not a supported packet...
    if (m_packet.empty())
        return eServerPacketType_invalid;

    const size_t packet_size = m_packet.size();
    const char *packet_cstr = m_packet.c_str();
    switch (m_packet[0])
    {
    case '\x03':
        if (packet_size == 1) return eServerPacketType_interrupt;
        break;

    case '-':
        if (packet_size == 1) return eServerPacketType_nack;
        break;

    case '+':
        if (packet_size == 1) return eServerPacketType_ack;
        break;

    case 'A':
        return eServerPacketType_A;
            
    case 'Q':

        switch (packet_cstr[1])
        {
        case 'E':
            if (PACKET_STARTS_WITH ("QEnvironment:"))           return eServerPacketType_QEnvironment;
            if (PACKET_STARTS_WITH ("QEnvironmentHexEncoded:")) return eServerPacketType_QEnvironmentHexEncoded;
            break;

        case 'S':
            if (PACKET_MATCHES ("QStartNoAckMode"))               return eServerPacketType_QStartNoAckMode;
            if (PACKET_STARTS_WITH ("QSaveRegisterState"))        return eServerPacketType_QSaveRegisterState;
            if (PACKET_STARTS_WITH ("QSetDisableASLR:"))          return eServerPacketType_QSetDisableASLR;
            if (PACKET_STARTS_WITH ("QSetSTDIN:"))                return eServerPacketType_QSetSTDIN;
            if (PACKET_STARTS_WITH ("QSetSTDOUT:"))               return eServerPacketType_QSetSTDOUT;
            if (PACKET_STARTS_WITH ("QSetSTDERR:"))               return eServerPacketType_QSetSTDERR;
            if (PACKET_STARTS_WITH ("QSetWorkingDir:"))           return eServerPacketType_QSetWorkingDir;
            if (PACKET_STARTS_WITH ("QSetLogging:"))              return eServerPacketType_QSetLogging;
            if (PACKET_STARTS_WITH ("QSetMaxPacketSize:"))        return eServerPacketType_QSetMaxPacketSize;
            if (PACKET_STARTS_WITH ("QSetMaxPayloadSize:"))       return eServerPacketType_QSetMaxPayloadSize;
            if (PACKET_STARTS_WITH ("QSetEnableAsyncProfiling;")) return eServerPacketType_QSetEnableAsyncProfiling;
            if (PACKET_STARTS_WITH ("QSyncThreadState:"))         return eServerPacketType_QSyncThreadState;
            break;

        case 'L':
            if (PACKET_STARTS_WITH ("QLaunchArch:"))              return eServerPacketType_QLaunchArch;
            if (PACKET_MATCHES("QListThreadsInStopReply"))        return eServerPacketType_QListThreadsInStopReply;
            break;

        case 'R':
            if (PACKET_STARTS_WITH ("QRestoreRegisterState:"))    return eServerPacketType_QRestoreRegisterState;
            break;

        case 'T':
            if (PACKET_MATCHES ("QThreadSuffixSupported"))        return eServerPacketType_QThreadSuffixSupported;
            break;
        }
        break;
            
    case 'q':
        switch (packet_cstr[1])
        {
        case 's':
            if (PACKET_MATCHES ("qsProcessInfo"))               return eServerPacketType_qsProcessInfo;
            if (PACKET_MATCHES ("qsThreadInfo"))                return eServerPacketType_qsThreadInfo;
            break;

        case 'f':
            if (PACKET_STARTS_WITH ("qfProcessInfo"))           return eServerPacketType_qfProcessInfo;
            if (PACKET_STARTS_WITH ("qfThreadInfo"))            return eServerPacketType_qfThreadInfo;
            break;

        case 'C':
            if (packet_size == 2)                               return eServerPacketType_qC;
            break;

        case 'G':
            if (PACKET_STARTS_WITH ("qGroupName:"))             return eServerPacketType_qGroupName;
            if (PACKET_MATCHES ("qGetWorkingDir"))              return eServerPacketType_qGetWorkingDir;
            if (PACKET_MATCHES ("qGetPid"))                     return eServerPacketType_qGetPid;
            if (PACKET_STARTS_WITH ("qGetProfileData;"))        return eServerPacketType_qGetProfileData;
            if (PACKET_MATCHES ("qGDBServerVersion"))           return eServerPacketType_qGDBServerVersion;
            break;

        case 'H':
            if (PACKET_MATCHES ("qHostInfo"))                   return eServerPacketType_qHostInfo;
            break;

        case 'K':
            if (PACKET_STARTS_WITH ("qKillSpawnedProcess"))     return eServerPacketType_qKillSpawnedProcess;
            break;
        
        case 'L':
            if (PACKET_STARTS_WITH ("qLaunchGDBServer"))        return eServerPacketType_qLaunchGDBServer;
            if (PACKET_MATCHES ("qLaunchSuccess"))              return eServerPacketType_qLaunchSuccess;
            break;
            
        case 'M':
            if (PACKET_STARTS_WITH ("qMemoryRegionInfo:"))      return eServerPacketType_qMemoryRegionInfo;
            if (PACKET_MATCHES ("qMemoryRegionInfo"))           return eServerPacketType_qMemoryRegionInfoSupported;
            break;

        case 'P':
            if (PACKET_STARTS_WITH ("qProcessInfoPID:"))        return eServerPacketType_qProcessInfoPID;
            if (PACKET_STARTS_WITH ("qPlatform_shell:"))        return eServerPacketType_qPlatform_shell;
            if (PACKET_STARTS_WITH ("qPlatform_mkdir:"))        return eServerPacketType_qPlatform_mkdir;
            if (PACKET_STARTS_WITH ("qPlatform_chmod:"))        return eServerPacketType_qPlatform_chmod;
            if (PACKET_MATCHES ("qProcessInfo"))                return eServerPacketType_qProcessInfo;
            break;
                
        case 'R':
            if (PACKET_STARTS_WITH ("qRcmd,"))                  return eServerPacketType_qRcmd;
            if (PACKET_STARTS_WITH ("qRegisterInfo"))           return eServerPacketType_qRegisterInfo;
            break;

        case 'S':
            if (PACKET_STARTS_WITH ("qSpeedTest:"))             return eServerPacketType_qSpeedTest;
            if (PACKET_MATCHES ("qShlibInfoAddr"))              return eServerPacketType_qShlibInfoAddr;
            if (PACKET_MATCHES ("qStepPacketSupported"))        return eServerPacketType_qStepPacketSupported;
            if (PACKET_MATCHES ("qSyncThreadStateSupported"))   return eServerPacketType_qSyncThreadStateSupported;
            break;

        case 'T':
            if (PACKET_STARTS_WITH ("qThreadExtraInfo,"))       return eServerPacketType_qThreadExtraInfo;
            if (PACKET_STARTS_WITH ("qThreadStopInfo"))         return eServerPacketType_qThreadStopInfo;
            break;

        case 'U':
            if (PACKET_STARTS_WITH ("qUserName:"))              return eServerPacketType_qUserName;
            break;

        case 'V':
            if (PACKET_MATCHES ("qVAttachOrWaitSupported"))     return eServerPacketType_qVAttachOrWaitSupported;
            break;

        case 'W':
            if (PACKET_STARTS_WITH ("qWatchpointSupportInfo:")) return eServerPacketType_qWatchpointSupportInfo;
            if (PACKET_MATCHES ("qWatchpointSupportInfo"))      return eServerPacketType_qWatchpointSupportInfoSupported;
            break;
        }
        break;
    case 'v':
            if (PACKET_STARTS_WITH("vFile:"))
            {
                if (PACKET_STARTS_WITH("vFile:open:"))          return eServerPacketType_vFile_open;
                else if (PACKET_STARTS_WITH("vFile:close:"))    return eServerPacketType_vFile_close;
                else if (PACKET_STARTS_WITH("vFile:pread"))     return eServerPacketType_vFile_pread;
                else if (PACKET_STARTS_WITH("vFile:pwrite"))    return eServerPacketType_vFile_pwrite;
                else if (PACKET_STARTS_WITH("vFile:size"))      return eServerPacketType_vFile_size;
                else if (PACKET_STARTS_WITH("vFile:exists"))    return eServerPacketType_vFile_exists;
                else if (PACKET_STARTS_WITH("vFile:stat"))      return eServerPacketType_vFile_stat;
                else if (PACKET_STARTS_WITH("vFile:mode"))      return eServerPacketType_vFile_mode;
                else if (PACKET_STARTS_WITH("vFile:MD5"))       return eServerPacketType_vFile_md5;
                else if (PACKET_STARTS_WITH("vFile:symlink"))   return eServerPacketType_vFile_symlink;
                else if (PACKET_STARTS_WITH("vFile:unlink"))    return eServerPacketType_vFile_unlink;

            } else {
              if (PACKET_STARTS_WITH ("vAttach;"))              return eServerPacketType_vAttach;
              if (PACKET_STARTS_WITH ("vAttachWait;"))          return eServerPacketType_vAttachWait;
              if (PACKET_STARTS_WITH ("vAttachOrWait;"))        return eServerPacketType_vAttachOrWait;
              if (PACKET_STARTS_WITH ("vAttachName;"))          return eServerPacketType_vAttachName;
              if (PACKET_STARTS_WITH("vCont;"))                 return eServerPacketType_vCont;
              if (PACKET_MATCHES ("vCont?"))                    return eServerPacketType_vCont_actions;
            }
            break;
      case '_':
        switch (packet_cstr[1])
        {
        case 'M':
            return eServerPacketType__M;

        case 'm':
            return eServerPacketType__m;
        }
        break;

      case '?':
        if (packet_size == 1) return eServerPacketType_stop_reason;
        break;

      case 'c':
        return eServerPacketType_c;

      case 'C':
        return eServerPacketType_C;

      case 'D':
        if (packet_size == 1) return eServerPacketType_D;
        break;

      case 'g':
        if (packet_size == 1) return eServerPacketType_g;
        break;

      case 'G':
        return eServerPacketType_G;

      case 'H':
        return eServerPacketType_H;

      case 'k':
        if (packet_size == 1) return eServerPacketType_k;
        break;

      case 'm':
        return eServerPacketType_m;

      case 'M':
        return eServerPacketType_M;

      case 'p':
        return eServerPacketType_p;

      case 'P':
        return eServerPacketType_P;

      case 's':
        if (packet_size == 1) return eServerPacketType_s;
        break;

      case 'S':
        return eServerPacketType_S;

      case 'T':
        return eServerPacketType_T;

      case 'z':
        if (packet_cstr[1] >= '0' && packet_cstr[1] <= '4')
          return eServerPacketType_z;
        break;

      case 'Z':
        if (packet_cstr[1] >= '0' && packet_cstr[1] <= '4')
          return eServerPacketType_Z;
        break;
    }
    return eServerPacketType_unimplemented;
}

bool
StringExtractorGDBRemote::IsOKResponse() const
{
    return GetResponseType () == eOK;
}


bool
StringExtractorGDBRemote::IsUnsupportedResponse() const
{
    return GetResponseType () == eUnsupported;
}

bool
StringExtractorGDBRemote::IsNormalResponse() const
{
    return GetResponseType () == eResponse;
}

bool
StringExtractorGDBRemote::IsErrorResponse() const
{
    return GetResponseType () == eError &&
           m_packet.size() == 3 &&
           isxdigit(m_packet[1]) &&
           isxdigit(m_packet[2]);
}

uint8_t
StringExtractorGDBRemote::GetError ()
{
    if (GetResponseType() == eError)
    {
        SetFilePos(1);
        return GetHexU8(255);
    }
    return 0;
}

size_t
StringExtractorGDBRemote::GetEscapedBinaryData (std::string &str)
{
    str.clear();
    char ch;
    while (GetBytesLeft())
    {
        ch = GetChar();
        if (ch == 0x7d)
            ch = (GetChar() ^ 0x20);
        str.append(1,ch);
    }
    return str.size();
}


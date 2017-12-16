//===-- GDBRemoteCommunicationClient.cpp ------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include "GDBRemoteCommunicationClient.h"

// C Includes
#include <sys/stat.h>

// C++ Includes
#include <sstream>

// Other libraries and framework includes
#include "llvm/ADT/Triple.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Core/ConnectionFileDescriptor.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/State.h"
#include "lldb/Core/StreamGDBRemote.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Host/Endian.h"
#include "lldb/Host/Host.h"
#include "lldb/Host/TimeValue.h"

// Project includes
#include "Utility/StringExtractorGDBRemote.h"
#include "ProcessGDBRemote.h"
#include "ProcessGDBRemoteLog.h"
#include "lldb/Host/Config.h"

using namespace lldb;
using namespace lldb_private;

#ifdef LLDB_DISABLE_POSIX
#define SIGSTOP 17
#endif

//----------------------------------------------------------------------
// GDBRemoteCommunicationClient constructor
//----------------------------------------------------------------------
GDBRemoteCommunicationClient::GDBRemoteCommunicationClient(bool is_platform) :
    GDBRemoteCommunication("gdb-remote.client", "gdb-remote.client.rx_packet", is_platform),
    m_supports_not_sending_acks (eLazyBoolCalculate),
    m_supports_thread_suffix (eLazyBoolCalculate),
    m_supports_threads_in_stop_reply (eLazyBoolCalculate),
    m_supports_vCont_all (eLazyBoolCalculate),
    m_supports_vCont_any (eLazyBoolCalculate),
    m_supports_vCont_c (eLazyBoolCalculate),
    m_supports_vCont_C (eLazyBoolCalculate),
    m_supports_vCont_s (eLazyBoolCalculate),
    m_supports_vCont_S (eLazyBoolCalculate),
    m_qHostInfo_is_valid (eLazyBoolCalculate),
    m_qProcessInfo_is_valid (eLazyBoolCalculate),
    m_supports_alloc_dealloc_memory (eLazyBoolCalculate),
    m_supports_memory_region_info  (eLazyBoolCalculate),
    m_supports_watchpoint_support_info  (eLazyBoolCalculate),
    m_supports_detach_stay_stopped (eLazyBoolCalculate),
    m_watchpoints_trigger_after_instruction(eLazyBoolCalculate),
    m_attach_or_wait_reply(eLazyBoolCalculate),
    m_prepare_for_reg_writing_reply (eLazyBoolCalculate),
    m_supports_p (eLazyBoolCalculate),
    m_supports_QSaveRegisterState (eLazyBoolCalculate),
    m_supports_qXfer_libraries_read (eLazyBoolCalculate),
    m_supports_qXfer_libraries_svr4_read (eLazyBoolCalculate),
    m_supports_augmented_libraries_svr4_read (eLazyBoolCalculate),
    m_supports_qProcessInfoPID (true),
    m_supports_qfProcessInfo (true),
    m_supports_qUserName (true),
    m_supports_qGroupName (true),
    m_supports_qThreadStopInfo (true),
    m_supports_z0 (true),
    m_supports_z1 (true),
    m_supports_z2 (true),
    m_supports_z3 (true),
    m_supports_z4 (true),
    m_supports_QEnvironment (true),
    m_supports_QEnvironmentHexEncoded (true),
    m_curr_tid (LLDB_INVALID_THREAD_ID),
    m_curr_tid_run (LLDB_INVALID_THREAD_ID),
    m_num_supported_hardware_watchpoints (0),
    m_async_mutex (Mutex::eMutexTypeRecursive),
    m_async_packet_predicate (false),
    m_async_packet (),
    m_async_result (PacketResult::Success),
    m_async_response (),
    m_async_signal (-1),
    m_thread_id_to_used_usec_map (),
    m_host_arch(),
    m_process_arch(),
    m_os_version_major (UINT32_MAX),
    m_os_version_minor (UINT32_MAX),
    m_os_version_update (UINT32_MAX),
    m_os_build (),
    m_os_kernel (),
    m_hostname (),
    m_default_packet_timeout (0),
    m_max_packet_size (0)
{
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
GDBRemoteCommunicationClient::~GDBRemoteCommunicationClient()
{
    if (IsConnected())
        Disconnect();
}

bool
GDBRemoteCommunicationClient::HandshakeWithServer (Error *error_ptr)
{
    ResetDiscoverableSettings();

    // Start the read thread after we send the handshake ack since if we
    // fail to send the handshake ack, there is no reason to continue...
    if (SendAck())
    {
        // Wait for any responses that might have been queued up in the remote
        // GDB server and flush them all
        StringExtractorGDBRemote response;
        PacketResult packet_result = PacketResult::Success;
        const uint32_t timeout_usec = 10 * 1000; // Wait for 10 ms for a response
        while (packet_result == PacketResult::Success)
            packet_result = WaitForPacketWithTimeoutMicroSecondsNoLock (response, timeout_usec);

        // The return value from QueryNoAckModeSupported() is true if the packet
        // was sent and _any_ response (including UNIMPLEMENTED) was received),
        // or false if no response was received. This quickly tells us if we have
        // a live connection to a remote GDB server...
        if (QueryNoAckModeSupported())
        {
#if 0
            // Set above line to "#if 1" to test packet speed if remote GDB server
            // supports the qSpeedTest packet...
            TestPacketSpeed(10000);
#endif
            return true;
        }
        else
        {
            if (error_ptr)
                error_ptr->SetErrorString("failed to get reply to handshake packet");
        }
    }
    else
    {
        if (error_ptr)
            error_ptr->SetErrorString("failed to send the handshake ack");
    }
    return false;
}

bool
GDBRemoteCommunicationClient::GetAugmentedLibrariesSVR4ReadSupported ()
{
    if (m_supports_augmented_libraries_svr4_read == eLazyBoolCalculate)
    {
        GetRemoteQSupported();
    }
    return (m_supports_augmented_libraries_svr4_read == eLazyBoolYes);
}

bool
GDBRemoteCommunicationClient::GetQXferLibrariesSVR4ReadSupported ()
{
    if (m_supports_qXfer_libraries_svr4_read == eLazyBoolCalculate)
    {
        GetRemoteQSupported();
    }
    return (m_supports_qXfer_libraries_svr4_read == eLazyBoolYes);
}

bool
GDBRemoteCommunicationClient::GetQXferLibrariesReadSupported ()
{
    if (m_supports_qXfer_libraries_read == eLazyBoolCalculate)
    {
        GetRemoteQSupported();
    }
    return (m_supports_qXfer_libraries_read == eLazyBoolYes);
}

uint64_t
GDBRemoteCommunicationClient::GetRemoteMaxPacketSize()
{
    if (m_max_packet_size == 0)
    {
        GetRemoteQSupported();
    }
    return m_max_packet_size;
}

bool
GDBRemoteCommunicationClient::QueryNoAckModeSupported ()
{
    if (m_supports_not_sending_acks == eLazyBoolCalculate)
    {
        m_send_acks = true;
        m_supports_not_sending_acks = eLazyBoolNo;

        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse("QStartNoAckMode", response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
            {
                m_send_acks = false;
                m_supports_not_sending_acks = eLazyBoolYes;
            }
            return true;
        }
    }
    return false;
}

void
GDBRemoteCommunicationClient::GetListThreadsInStopReplySupported ()
{
    if (m_supports_threads_in_stop_reply == eLazyBoolCalculate)
    {
        m_supports_threads_in_stop_reply = eLazyBoolNo;
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse("QListThreadsInStopReply", response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                m_supports_threads_in_stop_reply = eLazyBoolYes;
        }
    }
}

bool
GDBRemoteCommunicationClient::GetVAttachOrWaitSupported ()
{
    if (m_attach_or_wait_reply == eLazyBoolCalculate)
    {
        m_attach_or_wait_reply = eLazyBoolNo;
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse("qVAttachOrWaitSupported", response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                m_attach_or_wait_reply = eLazyBoolYes;
        }
    }
    if (m_attach_or_wait_reply == eLazyBoolYes)
        return true;
    else
        return false;
}

bool
GDBRemoteCommunicationClient::GetSyncThreadStateSupported ()
{
    if (m_prepare_for_reg_writing_reply == eLazyBoolCalculate)
    {
        m_prepare_for_reg_writing_reply = eLazyBoolNo;
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse("qSyncThreadStateSupported", response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                m_prepare_for_reg_writing_reply = eLazyBoolYes;
        }
    }
    if (m_prepare_for_reg_writing_reply == eLazyBoolYes)
        return true;
    else
        return false;
}


void
GDBRemoteCommunicationClient::ResetDiscoverableSettings()
{
    m_supports_not_sending_acks = eLazyBoolCalculate;
    m_supports_thread_suffix = eLazyBoolCalculate;
    m_supports_threads_in_stop_reply = eLazyBoolCalculate;
    m_supports_vCont_c = eLazyBoolCalculate;
    m_supports_vCont_C = eLazyBoolCalculate;
    m_supports_vCont_s = eLazyBoolCalculate;
    m_supports_vCont_S = eLazyBoolCalculate;
    m_supports_p = eLazyBoolCalculate;
    m_supports_QSaveRegisterState = eLazyBoolCalculate;
    m_qHostInfo_is_valid = eLazyBoolCalculate;
    m_qProcessInfo_is_valid = eLazyBoolCalculate;
    m_supports_alloc_dealloc_memory = eLazyBoolCalculate;
    m_supports_memory_region_info = eLazyBoolCalculate;
    m_prepare_for_reg_writing_reply = eLazyBoolCalculate;
    m_attach_or_wait_reply = eLazyBoolCalculate;
    m_supports_qXfer_libraries_read = eLazyBoolCalculate;
    m_supports_qXfer_libraries_svr4_read = eLazyBoolCalculate;
    m_supports_augmented_libraries_svr4_read = eLazyBoolCalculate;

    m_supports_qProcessInfoPID = true;
    m_supports_qfProcessInfo = true;
    m_supports_qUserName = true;
    m_supports_qGroupName = true;
    m_supports_qThreadStopInfo = true;
    m_supports_z0 = true;
    m_supports_z1 = true;
    m_supports_z2 = true;
    m_supports_z3 = true;
    m_supports_z4 = true;
    m_supports_QEnvironment = true;
    m_supports_QEnvironmentHexEncoded = true;
    m_host_arch.Clear();
    m_process_arch.Clear();

    m_max_packet_size = 0;
}

void
GDBRemoteCommunicationClient::GetRemoteQSupported ()
{
    // Clear out any capabilities we expect to see in the qSupported response
    m_supports_qXfer_libraries_svr4_read = eLazyBoolNo;
    m_supports_qXfer_libraries_read = eLazyBoolNo;
    m_supports_augmented_libraries_svr4_read = eLazyBoolNo;
    m_max_packet_size = UINT64_MAX;  // It's supposed to always be there, but if not, we assume no limit

    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse("qSupported",
                                     response,
                                     /*send_async=*/false) == PacketResult::Success)
    {
        const char *response_cstr = response.GetStringRef().c_str();
        if (::strstr (response_cstr, "qXfer:libraries-svr4:read+"))
            m_supports_qXfer_libraries_svr4_read = eLazyBoolYes;
        if (::strstr (response_cstr, "augmented-libraries-svr4-read"))
        {
            m_supports_qXfer_libraries_svr4_read = eLazyBoolYes;  // implied
            m_supports_augmented_libraries_svr4_read = eLazyBoolYes;
        }
        if (::strstr (response_cstr, "qXfer:libraries:read+"))
            m_supports_qXfer_libraries_read = eLazyBoolYes;

        const char *packet_size_str = ::strstr (response_cstr, "PacketSize=");
        if (packet_size_str)
        {
            StringExtractorGDBRemote packet_response(packet_size_str + strlen("PacketSize="));
            m_max_packet_size = packet_response.GetHexMaxU64(/*little_endian=*/false, UINT64_MAX);
            if (m_max_packet_size == 0)
            {
                m_max_packet_size = UINT64_MAX;  // Must have been a garbled response
                Log *log (ProcessGDBRemoteLog::GetLogIfAllCategoriesSet (GDBR_LOG_PROCESS));
                if (log)
                    log->Printf ("Garbled PacketSize spec in qSupported response");
            }
        }
    }
}

bool
GDBRemoteCommunicationClient::GetThreadSuffixSupported ()
{
    if (m_supports_thread_suffix == eLazyBoolCalculate)
    {
        StringExtractorGDBRemote response;
        m_supports_thread_suffix = eLazyBoolNo;
        if (SendPacketAndWaitForResponse("QThreadSuffixSupported", response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                m_supports_thread_suffix = eLazyBoolYes;
        }
    }
    return m_supports_thread_suffix;
}
bool
GDBRemoteCommunicationClient::GetVContSupported (char flavor)
{
    if (m_supports_vCont_c == eLazyBoolCalculate)
    {
        StringExtractorGDBRemote response;
        m_supports_vCont_any = eLazyBoolNo;
        m_supports_vCont_all = eLazyBoolNo;
        m_supports_vCont_c = eLazyBoolNo;
        m_supports_vCont_C = eLazyBoolNo;
        m_supports_vCont_s = eLazyBoolNo;
        m_supports_vCont_S = eLazyBoolNo;
        if (SendPacketAndWaitForResponse("vCont?", response, false) == PacketResult::Success)
        {
            const char *response_cstr = response.GetStringRef().c_str();
            if (::strstr (response_cstr, ";c"))
                m_supports_vCont_c = eLazyBoolYes;

            if (::strstr (response_cstr, ";C"))
                m_supports_vCont_C = eLazyBoolYes;

            if (::strstr (response_cstr, ";s"))
                m_supports_vCont_s = eLazyBoolYes;

            if (::strstr (response_cstr, ";S"))
                m_supports_vCont_S = eLazyBoolYes;

            if (m_supports_vCont_c == eLazyBoolYes &&
                m_supports_vCont_C == eLazyBoolYes &&
                m_supports_vCont_s == eLazyBoolYes &&
                m_supports_vCont_S == eLazyBoolYes)
            {
                m_supports_vCont_all = eLazyBoolYes;
            }
            
            if (m_supports_vCont_c == eLazyBoolYes ||
                m_supports_vCont_C == eLazyBoolYes ||
                m_supports_vCont_s == eLazyBoolYes ||
                m_supports_vCont_S == eLazyBoolYes)
            {
                m_supports_vCont_any = eLazyBoolYes;
            }
        }
    }
    
    switch (flavor)
    {
    case 'a': return m_supports_vCont_any;
    case 'A': return m_supports_vCont_all;
    case 'c': return m_supports_vCont_c;
    case 'C': return m_supports_vCont_C;
    case 's': return m_supports_vCont_s;
    case 'S': return m_supports_vCont_S;
    default: break;
    }
    return false;
}

// Check if the target supports 'p' packet. It sends out a 'p'
// packet and checks the response. A normal packet will tell us
// that support is available.
//
// Takes a valid thread ID because p needs to apply to a thread.
bool
GDBRemoteCommunicationClient::GetpPacketSupported (lldb::tid_t tid)
{
    if (m_supports_p == eLazyBoolCalculate)
    {
        StringExtractorGDBRemote response;
        m_supports_p = eLazyBoolNo;
        char packet[256];
        if (GetThreadSuffixSupported())
            snprintf(packet, sizeof(packet), "p0;thread:%" PRIx64 ";", tid);
        else
            snprintf(packet, sizeof(packet), "p0");
        
        if (SendPacketAndWaitForResponse(packet, response, false) == PacketResult::Success)
        {
            if (response.IsNormalResponse())
                m_supports_p = eLazyBoolYes;
        }
    }
    return m_supports_p;
}

GDBRemoteCommunicationClient::PacketResult
GDBRemoteCommunicationClient::SendPacketsAndConcatenateResponses
(
    const char *payload_prefix,
    std::string &response_string
)
{
    Mutex::Locker locker;
    if (!GetSequenceMutex(locker,
                          "ProcessGDBRemote::SendPacketsAndConcatenateResponses() failed due to not getting the sequence mutex"))
    {
        Log *log (ProcessGDBRemoteLog::GetLogIfAnyCategoryIsSet (GDBR_LOG_PROCESS | GDBR_LOG_PACKETS));
        if (log)
            log->Printf("error: failed to get packet sequence mutex, not sending packets with prefix '%s'",
                        payload_prefix);
        return PacketResult::ErrorNoSequenceLock;
    }

    response_string = "";
    std::string payload_prefix_str(payload_prefix);
    unsigned int response_size = 0x1000;
    if (response_size > GetRemoteMaxPacketSize()) {  // May send qSupported packet
        response_size = GetRemoteMaxPacketSize();
    }

    for (unsigned int offset = 0; true; offset += response_size)
    {
        StringExtractorGDBRemote this_response;
        // Construct payload
        char sizeDescriptor[128];
        snprintf(sizeDescriptor, sizeof(sizeDescriptor), "%x,%x", offset, response_size);
        PacketResult result = SendPacketAndWaitForResponse((payload_prefix_str + sizeDescriptor).c_str(),
                                                           this_response,
                                                           /*send_async=*/false);
        if (result != PacketResult::Success)
            return result;

        const std::string &this_string = this_response.GetStringRef();

        // Check for m or l as first character; l seems to mean this is the last chunk
        char first_char = *this_string.c_str();
        if (first_char != 'm' && first_char != 'l')
        {
            return PacketResult::ErrorReplyInvalid;
        }
        // Skip past m or l
        const char *s = this_string.c_str() + 1;

        // Concatenate the result so far
        response_string += s;
        if (first_char == 'l')
            // We're done
            return PacketResult::Success;
    }
}

GDBRemoteCommunicationClient::PacketResult
GDBRemoteCommunicationClient::SendPacketAndWaitForResponse
(
    const char *payload,
    StringExtractorGDBRemote &response,
    bool send_async
)
{
    return SendPacketAndWaitForResponse (payload, 
                                         ::strlen (payload),
                                         response,
                                         send_async);
}

GDBRemoteCommunicationClient::PacketResult
GDBRemoteCommunicationClient::SendPacketAndWaitForResponseNoLock (const char *payload,
                                                                  size_t payload_length,
                                                                  StringExtractorGDBRemote &response)
{
    PacketResult packet_result = SendPacketNoLock (payload, payload_length);
    if (packet_result == PacketResult::Success)
        packet_result = WaitForPacketWithTimeoutMicroSecondsNoLock (response, GetPacketTimeoutInMicroSeconds ());
    return packet_result;
}

GDBRemoteCommunicationClient::PacketResult
GDBRemoteCommunicationClient::SendPacketAndWaitForResponse
(
    const char *payload,
    size_t payload_length,
    StringExtractorGDBRemote &response,
    bool send_async
)
{
    PacketResult packet_result = PacketResult::ErrorSendFailed;
    Mutex::Locker locker;
    Log *log (ProcessGDBRemoteLog::GetLogIfAllCategoriesSet (GDBR_LOG_PROCESS));
    size_t response_len = 0;
    if (GetSequenceMutex (locker))
    {
        packet_result = SendPacketAndWaitForResponseNoLock (payload, payload_length, response);
    }
    else
    {
        if (send_async)
        {
            if (IsRunning())
            {
                Mutex::Locker async_locker (m_async_mutex);
                m_async_packet.assign(payload, payload_length);
                m_async_packet_predicate.SetValue (true, eBroadcastNever);
                
                if (log) 
                    log->Printf ("async: async packet = %s", m_async_packet.c_str());

                bool timed_out = false;
                if (SendInterrupt(locker, 2, timed_out))
                {
                    if (m_interrupt_sent)
                    {
                        m_interrupt_sent = false;
                        TimeValue timeout_time;
                        timeout_time = TimeValue::Now();
                        timeout_time.OffsetWithSeconds (m_packet_timeout);

                        if (log) 
                            log->Printf ("async: sent interrupt");

                        if (m_async_packet_predicate.WaitForValueEqualTo (false, &timeout_time, &timed_out))
                        {
                            if (log) 
                                log->Printf ("async: got response");

                            // Swap the response buffer to avoid malloc and string copy
                            response.GetStringRef().swap (m_async_response.GetStringRef());
                            response_len = response.GetStringRef().size();
                            packet_result = m_async_result;
                        }
                        else
                        {
                            if (log) 
                                log->Printf ("async: timed out waiting for response");
                        }
                        
                        // Make sure we wait until the continue packet has been sent again...
                        if (m_private_is_running.WaitForValueEqualTo (true, &timeout_time, &timed_out))
                        {
                            if (log)
                            {
                                if (timed_out) 
                                    log->Printf ("async: timed out waiting for process to resume, but process was resumed");
                                else
                                    log->Printf ("async: async packet sent");
                            }
                        }
                        else
                        {
                            if (log) 
                                log->Printf ("async: timed out waiting for process to resume");
                        }
                    }
                    else
                    {
                        // We had a racy condition where we went to send the interrupt
                        // yet we were able to get the lock, so the process must have
                        // just stopped?
                        if (log) 
                            log->Printf ("async: got lock without sending interrupt");
                        // Send the packet normally since we got the lock
                        packet_result = SendPacketAndWaitForResponseNoLock (payload, payload_length, response);
                    }
                }
                else
                {
                    if (log) 
                        log->Printf ("async: failed to interrupt");
                }
            }
            else
            {
                if (log) 
                    log->Printf ("async: not running, async is ignored");
            }
        }
        else
        {
            if (log) 
                log->Printf("error: failed to get packet sequence mutex, not sending packet '%*s'", (int) payload_length, payload);
        }
    }
    return packet_result;
}

static const char *end_delimiter = "--end--;";
static const int end_delimiter_len = 8;

std::string
GDBRemoteCommunicationClient::HarmonizeThreadIdsForProfileData
(   ProcessGDBRemote *process,
    StringExtractorGDBRemote& profileDataExtractor
)
{
    std::map<uint64_t, uint32_t> new_thread_id_to_used_usec_map;
    std::stringstream final_output;
    std::string name, value;

    // Going to assuming thread_used_usec comes first, else bail out.
    while (profileDataExtractor.GetNameColonValue(name, value))
    {
        if (name.compare("thread_used_id") == 0)
        {
            StringExtractor threadIDHexExtractor(value.c_str());
            uint64_t thread_id = threadIDHexExtractor.GetHexMaxU64(false, 0);
            
            bool has_used_usec = false;
            uint32_t curr_used_usec = 0;
            std::string usec_name, usec_value;
            uint32_t input_file_pos = profileDataExtractor.GetFilePos();
            if (profileDataExtractor.GetNameColonValue(usec_name, usec_value))
            {
                if (usec_name.compare("thread_used_usec") == 0)
                {
                    has_used_usec = true;
                    curr_used_usec = strtoull(usec_value.c_str(), NULL, 0);
                }
                else
                {
                    // We didn't find what we want, it is probably
                    // an older version. Bail out.
                    profileDataExtractor.SetFilePos(input_file_pos);
                }
            }

            if (has_used_usec)
            {
                uint32_t prev_used_usec = 0;
                std::map<uint64_t, uint32_t>::iterator iterator = m_thread_id_to_used_usec_map.find(thread_id);
                if (iterator != m_thread_id_to_used_usec_map.end())
                {
                    prev_used_usec = m_thread_id_to_used_usec_map[thread_id];
                }
                
                uint32_t real_used_usec = curr_used_usec - prev_used_usec;
                // A good first time record is one that runs for at least 0.25 sec
                bool good_first_time = (prev_used_usec == 0) && (real_used_usec > 250000);
                bool good_subsequent_time = (prev_used_usec > 0) &&
                    ((real_used_usec > 0) || (process->HasAssignedIndexIDToThread(thread_id)));
                
                if (good_first_time || good_subsequent_time)
                {
                    // We try to avoid doing too many index id reservation,
                    // resulting in fast increase of index ids.
                    
                    final_output << name << ":";
                    int32_t index_id = process->AssignIndexIDToThread(thread_id);
                    final_output << index_id << ";";
                    
                    final_output << usec_name << ":" << usec_value << ";";
                }
                else
                {
                    // Skip past 'thread_used_name'.
                    std::string local_name, local_value;
                    profileDataExtractor.GetNameColonValue(local_name, local_value);
                }
                
                // Store current time as previous time so that they can be compared later.
                new_thread_id_to_used_usec_map[thread_id] = curr_used_usec;
            }
            else
            {
                // Bail out and use old string.
                final_output << name << ":" << value << ";";
            }
        }
        else
        {
            final_output << name << ":" << value << ";";
        }
    }
    final_output << end_delimiter;
    m_thread_id_to_used_usec_map = new_thread_id_to_used_usec_map;
    
    return final_output.str();
}

StateType
GDBRemoteCommunicationClient::SendContinuePacketAndWaitForResponse
(
    ProcessGDBRemote *process,
    const char *payload,
    size_t packet_length,
    StringExtractorGDBRemote &response
)
{
    m_curr_tid = LLDB_INVALID_THREAD_ID;
    Log *log (ProcessGDBRemoteLog::GetLogIfAllCategoriesSet (GDBR_LOG_PROCESS));
    if (log)
        log->Printf ("GDBRemoteCommunicationClient::%s ()", __FUNCTION__);

    Mutex::Locker locker(m_sequence_mutex);
    StateType state = eStateRunning;

    BroadcastEvent(eBroadcastBitRunPacketSent, NULL);
    m_public_is_running.SetValue (true, eBroadcastNever);
    // Set the starting continue packet into "continue_packet". This packet
    // may change if we are interrupted and we continue after an async packet...
    std::string continue_packet(payload, packet_length);
    
    bool got_async_packet = false;
    
    while (state == eStateRunning)
    {
        if (!got_async_packet)
        {
            if (log)
                log->Printf ("GDBRemoteCommunicationClient::%s () sending continue packet: %s", __FUNCTION__, continue_packet.c_str());
            if (SendPacketNoLock(continue_packet.c_str(), continue_packet.size()) != PacketResult::Success)
                state = eStateInvalid;
        
            m_private_is_running.SetValue (true, eBroadcastAlways);
        }
        
        got_async_packet = false;

        if (log)
            log->Printf ("GDBRemoteCommunicationClient::%s () WaitForPacket(%s)", __FUNCTION__, continue_packet.c_str());

        if (WaitForPacketWithTimeoutMicroSecondsNoLock(response, UINT32_MAX) == PacketResult::Success)
        {
            if (response.Empty())
                state = eStateInvalid;
            else
            {
                const char stop_type = response.GetChar();
                if (log)
                    log->Printf ("GDBRemoteCommunicationClient::%s () got packet: %s", __FUNCTION__, response.GetStringRef().c_str());
                switch (stop_type)
                {
                case 'T':
                case 'S':
                    {
                        if (process->GetStopID() == 0)
                        {
                            if (process->GetID() == LLDB_INVALID_PROCESS_ID)
                            {
                                lldb::pid_t pid = GetCurrentProcessID ();
                                if (pid != LLDB_INVALID_PROCESS_ID)
                                    process->SetID (pid);
                            }
                            process->BuildDynamicRegisterInfo (true);
                        }

                        // Privately notify any internal threads that we have stopped
                        // in case we wanted to interrupt our process, yet we might
                        // send a packet and continue without returning control to the
                        // user.
                        m_private_is_running.SetValue (false, eBroadcastAlways);

                        const uint8_t signo = response.GetHexU8 (UINT8_MAX);

                        bool continue_after_async = m_async_signal != -1 || m_async_packet_predicate.GetValue();
                        if (continue_after_async || m_interrupt_sent)
                        {
                            // We sent an interrupt packet to stop the inferior process
                            // for an async signal or to send an async packet while running
                            // but we might have been single stepping and received the
                            // stop packet for the step instead of for the interrupt packet.
                            // Typically when an interrupt is sent a SIGINT or SIGSTOP
                            // is used, so if we get anything else, we need to try and
                            // get another stop reply packet that may have been sent
                            // due to sending the interrupt when the target is stopped
                            // which will just re-send a copy of the last stop reply
                            // packet. If we don't do this, then the reply for our
                            // async packet will be the repeat stop reply packet and cause
                            // a lot of trouble for us!
                            if (signo != SIGINT && signo != SIGSTOP)
                            {
                                continue_after_async = false;

                                // We didn't get a a SIGINT or SIGSTOP, so try for a
                                // very brief time (1 ms) to get another stop reply
                                // packet to make sure it doesn't get in the way
                                StringExtractorGDBRemote extra_stop_reply_packet;
                                uint32_t timeout_usec = 1000;
                                if (WaitForPacketWithTimeoutMicroSecondsNoLock (extra_stop_reply_packet, timeout_usec) == PacketResult::Success)
                                {
                                    switch (extra_stop_reply_packet.GetChar())
                                    {
                                    case 'T':
                                    case 'S':
                                        // We did get an extra stop reply, which means
                                        // our interrupt didn't stop the target so we
                                        // shouldn't continue after the async signal
                                        // or packet is sent...
                                        continue_after_async = false;
                                        break;
                                    }
                                }
                            }
                        }

                        if (m_async_signal != -1)
                        {
                            if (log)
                                log->Printf ("async: send signo = %s", Host::GetSignalAsCString (m_async_signal));

                            // Save off the async signal we are supposed to send
                            const int async_signal = m_async_signal;
                            // Clear the async signal member so we don't end up
                            // sending the signal multiple times...
                            m_async_signal = -1;
                            // Check which signal we stopped with
                            if (signo == async_signal)
                            {
                                if (log) 
                                    log->Printf ("async: stopped with signal %s, we are done running", Host::GetSignalAsCString (signo));

                                // We already stopped with a signal that we wanted
                                // to stop with, so we are done
                            }
                            else
                            {
                                // We stopped with a different signal that the one
                                // we wanted to stop with, so now we must resume
                                // with the signal we want
                                char signal_packet[32];
                                int signal_packet_len = 0;
                                signal_packet_len = ::snprintf (signal_packet,
                                                                sizeof (signal_packet),
                                                                "C%2.2x",
                                                                async_signal);

                                if (log) 
                                    log->Printf ("async: stopped with signal %s, resume with %s", 
                                                       Host::GetSignalAsCString (signo),
                                                       Host::GetSignalAsCString (async_signal));

                                // Set the continue packet to resume even if the
                                // interrupt didn't cause our stop (ignore continue_after_async)
                                continue_packet.assign(signal_packet, signal_packet_len);
                                continue;
                            }
                        }
                        else if (m_async_packet_predicate.GetValue())
                        {
                            Log * packet_log (ProcessGDBRemoteLog::GetLogIfAllCategoriesSet (GDBR_LOG_PACKETS));

                            // We are supposed to send an asynchronous packet while
                            // we are running.
                            m_async_response.Clear();
                            if (m_async_packet.empty())
                            {
                                m_async_result = PacketResult::ErrorSendFailed;
                                if (packet_log)
                                    packet_log->Printf ("async: error: empty async packet");                            

                            }
                            else
                            {
                                if (packet_log) 
                                    packet_log->Printf ("async: sending packet");
                                
                                m_async_result = SendPacketAndWaitForResponse (&m_async_packet[0],
                                                                               m_async_packet.size(),
                                                                               m_async_response,
                                                                               false);
                            }
                            // Let the other thread that was trying to send the async
                            // packet know that the packet has been sent and response is
                            // ready...
                            m_async_packet_predicate.SetValue(false, eBroadcastAlways);

                            if (packet_log) 
                                packet_log->Printf ("async: sent packet, continue_after_async = %i", continue_after_async);

                            // Set the continue packet to resume if our interrupt
                            // for the async packet did cause the stop
                            if (continue_after_async)
                            {
                                // Reverting this for now as it is causing deadlocks
                                // in programs (<rdar://problem/11529853>). In the future
                                // we should check our thread list and "do the right thing"
                                // for new threads that show up while we stop and run async
                                // packets. Setting the packet to 'c' to continue all threads
                                // is the right thing to do 99.99% of the time because if a
                                // thread was single stepping, and we sent an interrupt, we
                                // will notice above that we didn't stop due to an interrupt
                                // but stopped due to stepping and we would _not_ continue.
                                continue_packet.assign (1, 'c');
                                continue;
                            }
                        }
                        // Stop with signal and thread info
                        state = eStateStopped;
                    }
                    break;

                case 'W':
                case 'X':
                    // process exited
                    state = eStateExited;
                    break;

                case 'O':
                    // STDOUT
                    {
                        got_async_packet = true;
                        std::string inferior_stdout;
                        inferior_stdout.reserve(response.GetBytesLeft () / 2);
                        char ch;
                        while ((ch = response.GetHexU8()) != '\0')
                            inferior_stdout.append(1, ch);
                        process->AppendSTDOUT (inferior_stdout.c_str(), inferior_stdout.size());
                    }
                    break;

                case 'A':
                    // Async miscellaneous reply. Right now, only profile data is coming through this channel.
                    {
                        got_async_packet = true;
                        std::string input = response.GetStringRef().substr(1); // '1' to move beyond 'A'
                        if (m_partial_profile_data.length() > 0)
                        {
                            m_partial_profile_data.append(input);
                            input = m_partial_profile_data;
                            m_partial_profile_data.clear();
                        }
                        
                        size_t found, pos = 0, len = input.length();
                        while ((found = input.find(end_delimiter, pos)) != std::string::npos)
                        {
                            StringExtractorGDBRemote profileDataExtractor(input.substr(pos, found).c_str());
                            std::string profile_data = HarmonizeThreadIdsForProfileData(process, profileDataExtractor);
                            process->BroadcastAsyncProfileData (profile_data);
                            
                            pos = found + end_delimiter_len;
                        }
                        
                        if (pos < len)
                        {
                            // Last incomplete chunk.
                            m_partial_profile_data = input.substr(pos);
                        }
                    }
                    break;

                case 'E':
                    // ERROR
                    state = eStateInvalid;
                    break;

                default:
                    if (log)
                        log->Printf ("GDBRemoteCommunicationClient::%s () unrecognized async packet", __FUNCTION__);
                    state = eStateInvalid;
                    break;
                }
            }
        }
        else
        {
            if (log)
                log->Printf ("GDBRemoteCommunicationClient::%s () WaitForPacket(...) => false", __FUNCTION__);
            state = eStateInvalid;
        }
    }
    if (log)
        log->Printf ("GDBRemoteCommunicationClient::%s () => %s", __FUNCTION__, StateAsCString(state));
    response.SetFilePos(0);
    m_private_is_running.SetValue (false, eBroadcastAlways);
    m_public_is_running.SetValue (false, eBroadcastAlways);
    return state;
}

bool
GDBRemoteCommunicationClient::SendAsyncSignal (int signo)
{
    Mutex::Locker async_locker (m_async_mutex);
    m_async_signal = signo;
    bool timed_out = false;
    Mutex::Locker locker;
    if (SendInterrupt (locker, 1, timed_out))
        return true;
    m_async_signal = -1;
    return false;
}

// This function takes a mutex locker as a parameter in case the GetSequenceMutex
// actually succeeds. If it doesn't succeed in acquiring the sequence mutex 
// (the expected result), then it will send the halt packet. If it does succeed
// then the caller that requested the interrupt will want to keep the sequence
// locked down so that no one else can send packets while the caller has control.
// This function usually gets called when we are running and need to stop the 
// target. It can also be used when we are running and and we need to do something
// else (like read/write memory), so we need to interrupt the running process
// (gdb remote protocol requires this), and do what we need to do, then resume.

bool
GDBRemoteCommunicationClient::SendInterrupt
(
    Mutex::Locker& locker, 
    uint32_t seconds_to_wait_for_stop,             
    bool &timed_out
)
{
    timed_out = false;
    Log *log (ProcessGDBRemoteLog::GetLogIfAnyCategoryIsSet (GDBR_LOG_PROCESS | GDBR_LOG_PACKETS));

    if (IsRunning())
    {
        // Only send an interrupt if our debugserver is running...
        if (GetSequenceMutex (locker))
        {
            if (log)
                log->Printf ("SendInterrupt () - got sequence mutex without having to interrupt");
        }
        else
        {
            // Someone has the mutex locked waiting for a response or for the
            // inferior to stop, so send the interrupt on the down low...
            char ctrl_c = '\x03';
            ConnectionStatus status = eConnectionStatusSuccess;
            size_t bytes_written = Write (&ctrl_c, 1, status, NULL);
            if (log)
                log->PutCString("send packet: \\x03");
            if (bytes_written > 0)
            {
                m_interrupt_sent = true;
                if (seconds_to_wait_for_stop)
                {
                    TimeValue timeout;
                    if (seconds_to_wait_for_stop)
                    {
                        timeout = TimeValue::Now();
                        timeout.OffsetWithSeconds (seconds_to_wait_for_stop);
                    }
                    if (m_private_is_running.WaitForValueEqualTo (false, &timeout, &timed_out))
                    {
                        if (log)
                            log->PutCString ("SendInterrupt () - sent interrupt, private state stopped");
                        return true;
                    }
                    else
                    {
                        if (log)
                            log->Printf ("SendInterrupt () - sent interrupt, timed out wating for async thread resume");
                    }
                }
                else
                {
                    if (log)
                        log->Printf ("SendInterrupt () - sent interrupt, not waiting for stop...");
                    return true;
                }
            }
            else
            {
                if (log)
                    log->Printf ("SendInterrupt () - failed to write interrupt");
            }
            return false;
        }
    }
    else
    {
        if (log)
            log->Printf ("SendInterrupt () - not running");
    }
    return true;
}

lldb::pid_t
GDBRemoteCommunicationClient::GetCurrentProcessID ()
{
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse("qC", strlen("qC"), response, false) == PacketResult::Success)
    {
        if (response.GetChar() == 'Q')
            if (response.GetChar() == 'C')
                return response.GetHexMaxU32 (false, LLDB_INVALID_PROCESS_ID);
    }
    return LLDB_INVALID_PROCESS_ID;
}

bool
GDBRemoteCommunicationClient::GetLaunchSuccess (std::string &error_str)
{
    error_str.clear();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse("qLaunchSuccess", strlen("qLaunchSuccess"), response, false) == PacketResult::Success)
    {
        if (response.IsOKResponse())
            return true;
        if (response.GetChar() == 'E')
        {
            // A string the describes what failed when launching...
            error_str = response.GetStringRef().substr(1);
        }
        else
        {
            error_str.assign ("unknown error occurred launching process");
        }
    }
    else
    {
        error_str.assign ("timed out waiting for app to launch");
    }
    return false;
}

int
GDBRemoteCommunicationClient::SendArgumentsPacket (const ProcessLaunchInfo &launch_info)
{
    // Since we don't get the send argv0 separate from the executable path, we need to
    // make sure to use the actual exectuable path found in the launch_info...
    std::vector<const char *> argv;
    FileSpec exe_file = launch_info.GetExecutableFile();
    std::string exe_path;
    const char *arg = NULL;
    const Args &launch_args = launch_info.GetArguments();
    if (exe_file)
        exe_path = exe_file.GetPath();
    else
    {
        arg = launch_args.GetArgumentAtIndex(0);
        if (arg)
            exe_path = arg;
    }
    if (!exe_path.empty())
    {
        argv.push_back(exe_path.c_str());
        for (uint32_t i=1; (arg = launch_args.GetArgumentAtIndex(i)) != NULL; ++i)
        {
            if (arg)
                argv.push_back(arg);
        }
    }
    if (!argv.empty())
    {
        StreamString packet;
        packet.PutChar('A');
        for (size_t i = 0, n = argv.size(); i < n; ++i)
        {
            arg = argv[i];
            const int arg_len = strlen(arg);
            if (i > 0)
                packet.PutChar(',');
            packet.Printf("%i,%i,", arg_len * 2, (int)i);
            packet.PutBytesAsRawHex8 (arg, arg_len);
        }

        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

int
GDBRemoteCommunicationClient::SendEnvironmentPacket (char const *name_equal_value)
{
    if (name_equal_value && name_equal_value[0])
    {
        StreamString packet;
        bool send_hex_encoding = false;
        for (const char *p = name_equal_value; *p != '\0' && send_hex_encoding == false; ++p)
        {
            if (isprint(*p))
            {
                switch (*p)
                {
                    case '$':
                    case '#':
                        send_hex_encoding = true;
                        break;
                    default:
                        break;
                }
            }
            else
            {
                // We have non printable characters, lets hex encode this...
                send_hex_encoding = true;
            }
        }
        
        StringExtractorGDBRemote response;
        if (send_hex_encoding)
        {
            if (m_supports_QEnvironmentHexEncoded)
            {
                packet.PutCString("QEnvironmentHexEncoded:");
                packet.PutBytesAsRawHex8 (name_equal_value, strlen(name_equal_value));
                if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
                {
                    if (response.IsOKResponse())
                        return 0;
                    uint8_t error = response.GetError();
                    if (error)
                        return error;
                    if (response.IsUnsupportedResponse())
                        m_supports_QEnvironmentHexEncoded = false;
                }
            }
            
        }
        else if (m_supports_QEnvironment)
        {
            packet.Printf("QEnvironment:%s", name_equal_value);
            if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
            {
                if (response.IsOKResponse())
                    return 0;
                uint8_t error = response.GetError();
                if (error)
                    return error;
                if (response.IsUnsupportedResponse())
                    m_supports_QEnvironment = false;
            }
        }
    }
    return -1;
}

int
GDBRemoteCommunicationClient::SendLaunchArchPacket (char const *arch)
{
    if (arch && arch[0])
    {
        StreamString packet;
        packet.Printf("QLaunchArch:%s", arch);
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

bool
GDBRemoteCommunicationClient::GetOSVersion (uint32_t &major, 
                                            uint32_t &minor, 
                                            uint32_t &update)
{
    if (GetHostInfo ())
    {
        if (m_os_version_major != UINT32_MAX)
        {
            major = m_os_version_major;
            minor = m_os_version_minor;
            update = m_os_version_update;
            return true;
        }
    }
    return false;
}

bool
GDBRemoteCommunicationClient::GetOSBuildString (std::string &s)
{
    if (GetHostInfo ())
    {
        if (!m_os_build.empty())
        {
            s = m_os_build;
            return true;
        }
    }
    s.clear();
    return false;
}


bool
GDBRemoteCommunicationClient::GetOSKernelDescription (std::string &s)
{
    if (GetHostInfo ())
    {
        if (!m_os_kernel.empty())
        {
            s = m_os_kernel;
            return true;
        }
    }
    s.clear();
    return false;
}

bool
GDBRemoteCommunicationClient::GetHostname (std::string &s)
{
    if (GetHostInfo ())
    {
        if (!m_hostname.empty())
        {
            s = m_hostname;
            return true;
        }
    }
    s.clear();
    return false;
}

ArchSpec
GDBRemoteCommunicationClient::GetSystemArchitecture ()
{
    if (GetHostInfo ())
        return m_host_arch;
    return ArchSpec();
}

const lldb_private::ArchSpec &
GDBRemoteCommunicationClient::GetProcessArchitecture ()
{
    if (m_qProcessInfo_is_valid == eLazyBoolCalculate)
        GetCurrentProcessInfo ();
    return m_process_arch;
}


bool
GDBRemoteCommunicationClient::GetHostInfo (bool force)
{
    if (force || m_qHostInfo_is_valid == eLazyBoolCalculate)
    {
        m_qHostInfo_is_valid = eLazyBoolNo;
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse ("qHostInfo", response, false) == PacketResult::Success)
        {
            if (response.IsNormalResponse())
            {
                std::string name;
                std::string value;
                uint32_t cpu = LLDB_INVALID_CPUTYPE;
                uint32_t sub = 0;
                std::string arch_name;
                std::string os_name;
                std::string vendor_name;
                std::string triple;
                std::string distribution_id;
                uint32_t pointer_byte_size = 0;
                StringExtractor extractor;
                ByteOrder byte_order = eByteOrderInvalid;
                uint32_t num_keys_decoded = 0;
                while (response.GetNameColonValue(name, value))
                {
                    if (name.compare("cputype") == 0)
                    {
                        // exception type in big endian hex
                        cpu = Args::StringToUInt32 (value.c_str(), LLDB_INVALID_CPUTYPE, 0);
                        if (cpu != LLDB_INVALID_CPUTYPE)
                            ++num_keys_decoded;
                    }
                    else if (name.compare("cpusubtype") == 0)
                    {
                        // exception count in big endian hex
                        sub = Args::StringToUInt32 (value.c_str(), 0, 0);
                        if (sub != 0)
                            ++num_keys_decoded;
                    }
                    else if (name.compare("arch") == 0)
                    {
                        arch_name.swap (value);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("triple") == 0)
                    {
                        // The triple comes as ASCII hex bytes since it contains '-' chars
                        extractor.GetStringRef().swap(value);
                        extractor.SetFilePos(0);
                        extractor.GetHexByteString (triple);
                        ++num_keys_decoded;
                    }
                    else if (name.compare ("distribution_id") == 0)
                    {
                        extractor.GetStringRef ().swap (value);
                        extractor.SetFilePos (0);
                        extractor.GetHexByteString (distribution_id);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("os_build") == 0)
                    {
                        extractor.GetStringRef().swap(value);
                        extractor.SetFilePos(0);
                        extractor.GetHexByteString (m_os_build);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("hostname") == 0)
                    {
                        extractor.GetStringRef().swap(value);
                        extractor.SetFilePos(0);
                        extractor.GetHexByteString (m_hostname);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("os_kernel") == 0)
                    {
                        extractor.GetStringRef().swap(value);
                        extractor.SetFilePos(0);
                        extractor.GetHexByteString (m_os_kernel);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("ostype") == 0)
                    {
                        os_name.swap (value);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("vendor") == 0)
                    {
                        vendor_name.swap(value);
                        ++num_keys_decoded;
                    }
                    else if (name.compare("endian") == 0)
                    {
                        ++num_keys_decoded;
                        if (value.compare("little") == 0)
                            byte_order = eByteOrderLittle;
                        else if (value.compare("big") == 0)
                            byte_order = eByteOrderBig;
                        else if (value.compare("pdp") == 0)
                            byte_order = eByteOrderPDP;
                        else
                            --num_keys_decoded;
                    }
                    else if (name.compare("ptrsize") == 0)
                    {
                        pointer_byte_size = Args::StringToUInt32 (value.c_str(), 0, 0);
                        if (pointer_byte_size != 0)
                            ++num_keys_decoded;
                    }
                    else if (name.compare("os_version") == 0)
                    {
                        Args::StringToVersion (value.c_str(), 
                                               m_os_version_major,
                                               m_os_version_minor,
                                               m_os_version_update);
                        if (m_os_version_major != UINT32_MAX)
                            ++num_keys_decoded;
                    }
                    else if (name.compare("watchpoint_exceptions_received") == 0)
                    {
                        ++num_keys_decoded;
                        if (strcmp(value.c_str(),"before") == 0)
                            m_watchpoints_trigger_after_instruction = eLazyBoolNo;
                        else if (strcmp(value.c_str(),"after") == 0)
                            m_watchpoints_trigger_after_instruction = eLazyBoolYes;
                        else
                            --num_keys_decoded;
                    }
                    else if (name.compare("default_packet_timeout") == 0)
                    {
                        m_default_packet_timeout = Args::StringToUInt32(value.c_str(), 0);
                        if (m_default_packet_timeout > 0)
                        {
                            SetPacketTimeout(m_default_packet_timeout);
                            ++num_keys_decoded;
                        }
                    }

                }
                
                if (num_keys_decoded > 0)
                    m_qHostInfo_is_valid = eLazyBoolYes;

                if (triple.empty())
                {
                    if (arch_name.empty())
                    {
                        if (cpu != LLDB_INVALID_CPUTYPE)
                        {
                            m_host_arch.SetArchitecture (eArchTypeMachO, cpu, sub);
                            if (pointer_byte_size)
                            {
                                assert (pointer_byte_size == m_host_arch.GetAddressByteSize());
                            }
                            if (byte_order != eByteOrderInvalid)
                            {
                                assert (byte_order == m_host_arch.GetByteOrder());
                            }

                            if (!os_name.empty() && vendor_name.compare("apple") == 0 && os_name.find("darwin") == 0)
                            {
                                switch (m_host_arch.GetMachine())
                                {
                                case llvm::Triple::arm:
                                case llvm::Triple::thumb:
                                    os_name = "ios";
                                    break;
                                default:
                                    os_name = "macosx";
                                    break;
                                }
                            }
                            if (!vendor_name.empty())
                                m_host_arch.GetTriple().setVendorName (llvm::StringRef (vendor_name));
                            if (!os_name.empty())
                                m_host_arch.GetTriple().setOSName (llvm::StringRef (os_name));
                                
                        }
                    }
                    else
                    {
                        std::string triple;
                        triple += arch_name;
                        if (!vendor_name.empty() || !os_name.empty())
                        {
                            triple += '-';
                            if (vendor_name.empty())
                                triple += "unknown";
                            else
                                triple += vendor_name;
                            triple += '-';
                            if (os_name.empty())
                                triple += "unknown";
                            else
                                triple += os_name;
                        }
                        m_host_arch.SetTriple (triple.c_str());
                        
                        llvm::Triple &host_triple = m_host_arch.GetTriple();
                        if (host_triple.getVendor() == llvm::Triple::Apple && host_triple.getOS() == llvm::Triple::Darwin)
                        {
                            switch (m_host_arch.GetMachine())
                            {
                                case llvm::Triple::arm:
                                case llvm::Triple::thumb:
                                    host_triple.setOS(llvm::Triple::IOS);
                                    break;
                                default:
                                    host_triple.setOS(llvm::Triple::MacOSX);
                                    break;
                            }
                        }
                        if (pointer_byte_size)
                        {
                            assert (pointer_byte_size == m_host_arch.GetAddressByteSize());
                        }
                        if (byte_order != eByteOrderInvalid)
                        {
                            assert (byte_order == m_host_arch.GetByteOrder());
                        }
                        
                    }
                }
                else
                {
                    m_host_arch.SetTriple (triple.c_str());
                    if (pointer_byte_size)
                    {
                        assert (pointer_byte_size == m_host_arch.GetAddressByteSize());
                    }
                    if (byte_order != eByteOrderInvalid)
                    {
                        assert (byte_order == m_host_arch.GetByteOrder());
                    }
                }
                if (!distribution_id.empty ())
                    m_host_arch.SetDistributionId (distribution_id.c_str ());
            }
        }
    }
    return m_qHostInfo_is_valid == eLazyBoolYes;
}

int
GDBRemoteCommunicationClient::SendAttach 
(
    lldb::pid_t pid, 
    StringExtractorGDBRemote& response
)
{
    if (pid != LLDB_INVALID_PROCESS_ID)
    {
        char packet[64];
        const int packet_len = ::snprintf (packet, sizeof(packet), "vAttach;%" PRIx64, pid);
        assert (packet_len < (int)sizeof(packet));
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            if (response.IsErrorResponse())
                return response.GetError();
            return 0;
        }
    }
    return -1;
}

const lldb_private::ArchSpec &
GDBRemoteCommunicationClient::GetHostArchitecture ()
{
    if (m_qHostInfo_is_valid == eLazyBoolCalculate)
        GetHostInfo ();
    return m_host_arch;
}

uint32_t
GDBRemoteCommunicationClient::GetHostDefaultPacketTimeout ()
{
    if (m_qHostInfo_is_valid == eLazyBoolCalculate)
        GetHostInfo ();
    return m_default_packet_timeout;
}

addr_t
GDBRemoteCommunicationClient::AllocateMemory (size_t size, uint32_t permissions)
{
    if (m_supports_alloc_dealloc_memory != eLazyBoolNo)
    {
        m_supports_alloc_dealloc_memory = eLazyBoolYes;
        char packet[64];
        const int packet_len = ::snprintf (packet, sizeof(packet), "_M%" PRIx64 ",%s%s%s",
                                           (uint64_t)size,
                                           permissions & lldb::ePermissionsReadable ? "r" : "",
                                           permissions & lldb::ePermissionsWritable ? "w" : "",
                                           permissions & lldb::ePermissionsExecutable ? "x" : "");
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            if (!response.IsErrorResponse())
                return response.GetHexMaxU64(false, LLDB_INVALID_ADDRESS);
        }
        else
        {
            m_supports_alloc_dealloc_memory = eLazyBoolNo;
        }
    }
    return LLDB_INVALID_ADDRESS;
}

bool
GDBRemoteCommunicationClient::DeallocateMemory (addr_t addr)
{
    if (m_supports_alloc_dealloc_memory != eLazyBoolNo)
    {
        m_supports_alloc_dealloc_memory = eLazyBoolYes;
        char packet[64];
        const int packet_len = ::snprintf(packet, sizeof(packet), "_m%" PRIx64, (uint64_t)addr);
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return true;
        }
        else
        {
            m_supports_alloc_dealloc_memory = eLazyBoolNo;
        }
    }
    return false;
}

Error
GDBRemoteCommunicationClient::Detach (bool keep_stopped)
{
    Error error;
    
    if (keep_stopped)
    {
        if (m_supports_detach_stay_stopped == eLazyBoolCalculate)
        {
            char packet[64];
            const int packet_len = ::snprintf(packet, sizeof(packet), "qSupportsDetachAndStayStopped:");
            assert (packet_len < (int)sizeof(packet));
            StringExtractorGDBRemote response;
            if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
            {
                m_supports_detach_stay_stopped = eLazyBoolYes;        
            }
            else
            {
                m_supports_detach_stay_stopped = eLazyBoolNo;
            }
        }

        if (m_supports_detach_stay_stopped == eLazyBoolNo)
        {
            error.SetErrorString("Stays stopped not supported by this target.");
            return error;
        }
        else
        {
            PacketResult packet_result = SendPacket ("D1", 2);
            if (packet_result != PacketResult::Success)
                error.SetErrorString ("Sending extended disconnect packet failed.");
        }
    }
    else
    {
        PacketResult packet_result = SendPacket ("D", 1);
        if (packet_result != PacketResult::Success)
            error.SetErrorString ("Sending disconnect packet failed.");
    }
    return error;
}

Error
GDBRemoteCommunicationClient::GetMemoryRegionInfo (lldb::addr_t addr, 
                                                  lldb_private::MemoryRegionInfo &region_info)
{
    Error error;
    region_info.Clear();

    if (m_supports_memory_region_info != eLazyBoolNo)
    {
        m_supports_memory_region_info = eLazyBoolYes;
        char packet[64];
        const int packet_len = ::snprintf(packet, sizeof(packet), "qMemoryRegionInfo:%" PRIx64, (uint64_t)addr);
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            std::string name;
            std::string value;
            addr_t addr_value;
            bool success = true;
            bool saw_permissions = false;
            while (success && response.GetNameColonValue(name, value))
            {
                if (name.compare ("start") == 0)
                {
                    addr_value = Args::StringToUInt64(value.c_str(), LLDB_INVALID_ADDRESS, 16, &success);
                    if (success)
                        region_info.GetRange().SetRangeBase(addr_value);
                }
                else if (name.compare ("size") == 0)
                {
                    addr_value = Args::StringToUInt64(value.c_str(), 0, 16, &success);
                    if (success)
                        region_info.GetRange().SetByteSize (addr_value);
                }
                else if (name.compare ("permissions") == 0 && region_info.GetRange().IsValid())
                {
                    saw_permissions = true;
                    if (region_info.GetRange().Contains (addr))
                    {
                        if (value.find('r') != std::string::npos)
                            region_info.SetReadable (MemoryRegionInfo::eYes);
                        else
                            region_info.SetReadable (MemoryRegionInfo::eNo);

                        if (value.find('w') != std::string::npos)
                            region_info.SetWritable (MemoryRegionInfo::eYes);
                        else
                            region_info.SetWritable (MemoryRegionInfo::eNo);

                        if (value.find('x') != std::string::npos)
                            region_info.SetExecutable (MemoryRegionInfo::eYes);
                        else
                            region_info.SetExecutable (MemoryRegionInfo::eNo);
                    }
                    else
                    {
                        // The reported region does not contain this address -- we're looking at an unmapped page
                        region_info.SetReadable (MemoryRegionInfo::eNo);
                        region_info.SetWritable (MemoryRegionInfo::eNo);
                        region_info.SetExecutable (MemoryRegionInfo::eNo);
                    }
                }
                else if (name.compare ("error") == 0)
                {
                    StringExtractorGDBRemote name_extractor;
                    // Swap "value" over into "name_extractor"
                    name_extractor.GetStringRef().swap(value);
                    // Now convert the HEX bytes into a string value
                    name_extractor.GetHexByteString (value);
                    error.SetErrorString(value.c_str());
                }
            }

            // We got a valid address range back but no permissions -- which means this is an unmapped page
            if (region_info.GetRange().IsValid() && saw_permissions == false)
            {
                region_info.SetReadable (MemoryRegionInfo::eNo);
                region_info.SetWritable (MemoryRegionInfo::eNo);
                region_info.SetExecutable (MemoryRegionInfo::eNo);
            }
        }
        else
        {
            m_supports_memory_region_info = eLazyBoolNo;
        }
    }

    if (m_supports_memory_region_info == eLazyBoolNo)
    {
        error.SetErrorString("qMemoryRegionInfo is not supported");
    }
    if (error.Fail())
        region_info.Clear();
    return error;

}

Error
GDBRemoteCommunicationClient::GetWatchpointSupportInfo (uint32_t &num)
{
    Error error;

    if (m_supports_watchpoint_support_info == eLazyBoolYes)
    {
        num = m_num_supported_hardware_watchpoints;
        return error;
    }

    // Set num to 0 first.
    num = 0;
    if (m_supports_watchpoint_support_info != eLazyBoolNo)
    {
        char packet[64];
        const int packet_len = ::snprintf(packet, sizeof(packet), "qWatchpointSupportInfo:");
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            m_supports_watchpoint_support_info = eLazyBoolYes;        
            std::string name;
            std::string value;
            while (response.GetNameColonValue(name, value))
            {
                if (name.compare ("num") == 0)
                {
                    num = Args::StringToUInt32(value.c_str(), 0, 0);
                    m_num_supported_hardware_watchpoints = num;
                }
            }
        }
        else
        {
            m_supports_watchpoint_support_info = eLazyBoolNo;
        }
    }

    if (m_supports_watchpoint_support_info == eLazyBoolNo)
    {
        error.SetErrorString("qWatchpointSupportInfo is not supported");
    }
    return error;

}

lldb_private::Error
GDBRemoteCommunicationClient::GetWatchpointSupportInfo (uint32_t &num, bool& after)
{
    Error error(GetWatchpointSupportInfo(num));
    if (error.Success())
        error = GetWatchpointsTriggerAfterInstruction(after);
    return error;
}

lldb_private::Error
GDBRemoteCommunicationClient::GetWatchpointsTriggerAfterInstruction (bool &after)
{
    Error error;
    
    // we assume watchpoints will happen after running the relevant opcode
    // and we only want to override this behavior if we have explicitly
    // received a qHostInfo telling us otherwise
    if (m_qHostInfo_is_valid != eLazyBoolYes)
        after = true;
    else
        after = (m_watchpoints_trigger_after_instruction != eLazyBoolNo);
    return error;
}

int
GDBRemoteCommunicationClient::SetSTDIN (char const *path)
{
    if (path && path[0])
    {
        StreamString packet;
        packet.PutCString("QSetSTDIN:");
        packet.PutBytesAsRawHex8(path, strlen(path));

        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

int
GDBRemoteCommunicationClient::SetSTDOUT (char const *path)
{
    if (path && path[0])
    {
        StreamString packet;
        packet.PutCString("QSetSTDOUT:");
        packet.PutBytesAsRawHex8(path, strlen(path));
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

int
GDBRemoteCommunicationClient::SetSTDERR (char const *path)
{
    if (path && path[0])
    {
        StreamString packet;
        packet.PutCString("QSetSTDERR:");
        packet.PutBytesAsRawHex8(path, strlen(path));
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

bool
GDBRemoteCommunicationClient::GetWorkingDir (std::string &cwd)
{
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse ("qGetWorkingDir", response, false) == PacketResult::Success)
    {
        if (response.IsUnsupportedResponse())
            return false;
        if (response.IsErrorResponse())
            return false;
        response.GetHexByteString (cwd);
        return !cwd.empty();
    }
    return false;
}

int
GDBRemoteCommunicationClient::SetWorkingDir (char const *path)
{
    if (path && path[0])
    {
        StreamString packet;
        packet.PutCString("QSetWorkingDir:");
        packet.PutBytesAsRawHex8(path, strlen(path));
        
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            if (response.IsOKResponse())
                return 0;
            uint8_t error = response.GetError();
            if (error)
                return error;
        }
    }
    return -1;
}

int
GDBRemoteCommunicationClient::SetDisableASLR (bool enable)
{
    char packet[32];
    const int packet_len = ::snprintf (packet, sizeof (packet), "QSetDisableASLR:%i", enable ? 1 : 0);
    assert (packet_len < (int)sizeof(packet));
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.IsOKResponse())
            return 0;
        uint8_t error = response.GetError();
        if (error)
            return error;
    }
    return -1;
}

bool
GDBRemoteCommunicationClient::DecodeProcessInfoResponse (StringExtractorGDBRemote &response, ProcessInstanceInfo &process_info)
{
    if (response.IsNormalResponse())
    {
        std::string name;
        std::string value;
        StringExtractor extractor;

        uint32_t cpu = LLDB_INVALID_CPUTYPE;
        uint32_t sub = 0;
        std::string vendor;
        std::string os_type;
        
        while (response.GetNameColonValue(name, value))
        {
            if (name.compare("pid") == 0)
            {
                process_info.SetProcessID (Args::StringToUInt32 (value.c_str(), LLDB_INVALID_PROCESS_ID, 0));
            }
            else if (name.compare("ppid") == 0)
            {
                process_info.SetParentProcessID (Args::StringToUInt32 (value.c_str(), LLDB_INVALID_PROCESS_ID, 0));
            }
            else if (name.compare("uid") == 0)
            {
                process_info.SetUserID (Args::StringToUInt32 (value.c_str(), UINT32_MAX, 0));
            }
            else if (name.compare("euid") == 0)
            {
                process_info.SetEffectiveUserID (Args::StringToUInt32 (value.c_str(), UINT32_MAX, 0));
            }
            else if (name.compare("gid") == 0)
            {
                process_info.SetGroupID (Args::StringToUInt32 (value.c_str(), UINT32_MAX, 0));
            }
            else if (name.compare("egid") == 0)
            {
                process_info.SetEffectiveGroupID (Args::StringToUInt32 (value.c_str(), UINT32_MAX, 0));
            }
            else if (name.compare("triple") == 0)
            {
                // The triple comes as ASCII hex bytes since it contains '-' chars
                extractor.GetStringRef().swap(value);
                extractor.SetFilePos(0);
                extractor.GetHexByteString (value);
                process_info.GetArchitecture ().SetTriple (value.c_str());
            }
            else if (name.compare("name") == 0)
            {
                StringExtractor extractor;
                // The process name from ASCII hex bytes since we can't 
                // control the characters in a process name
                extractor.GetStringRef().swap(value);
                extractor.SetFilePos(0);
                extractor.GetHexByteString (value);
                process_info.GetExecutableFile().SetFile (value.c_str(), false);
            }
            else if (name.compare("cputype") == 0)
            {
                cpu = Args::StringToUInt32 (value.c_str(), LLDB_INVALID_CPUTYPE, 16);
            }
            else if (name.compare("cpusubtype") == 0)
            {
                sub = Args::StringToUInt32 (value.c_str(), 0, 16);
            }
            else if (name.compare("vendor") == 0)
            {
                vendor = value;
            }
            else if (name.compare("ostype") == 0)
            {
                os_type = value;
            }
        }

        if (cpu != LLDB_INVALID_CPUTYPE && !vendor.empty() && !os_type.empty())
        {
            if (vendor == "apple")
            {
                process_info.GetArchitecture().SetArchitecture (eArchTypeMachO, cpu, sub);
                process_info.GetArchitecture().GetTriple().setVendorName (llvm::StringRef (vendor));
                process_info.GetArchitecture().GetTriple().setOSName (llvm::StringRef (os_type));
            }
        }
        
        if (process_info.GetProcessID() != LLDB_INVALID_PROCESS_ID)
            return true;
    }
    return false;
}

bool
GDBRemoteCommunicationClient::GetProcessInfo (lldb::pid_t pid, ProcessInstanceInfo &process_info)
{
    process_info.Clear();
    
    if (m_supports_qProcessInfoPID)
    {
        char packet[32];
        const int packet_len = ::snprintf (packet, sizeof (packet), "qProcessInfoPID:%" PRIu64, pid);
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            return DecodeProcessInfoResponse (response, process_info);
        }
        else
        {
            m_supports_qProcessInfoPID = false;
            return false;
        }
    }
    return false;
}

bool
GDBRemoteCommunicationClient::GetCurrentProcessInfo ()
{
    if (m_qProcessInfo_is_valid == eLazyBoolYes)
        return true;
    if (m_qProcessInfo_is_valid == eLazyBoolNo)
        return false;

    GetHostInfo ();

    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse ("qProcessInfo", response, false) == PacketResult::Success)
    {
        if (response.IsNormalResponse())
        {
            std::string name;
            std::string value;
            uint32_t cpu = LLDB_INVALID_CPUTYPE;
            uint32_t sub = 0;
            std::string arch_name;
            std::string os_name;
            std::string vendor_name;
            std::string triple;
            uint32_t pointer_byte_size = 0;
            StringExtractor extractor;
            ByteOrder byte_order = eByteOrderInvalid;
            uint32_t num_keys_decoded = 0;
            while (response.GetNameColonValue(name, value))
            {
                if (name.compare("cputype") == 0)
                {
                    cpu = Args::StringToUInt32 (value.c_str(), LLDB_INVALID_CPUTYPE, 16);
                    if (cpu != LLDB_INVALID_CPUTYPE)
                        ++num_keys_decoded;
                }
                else if (name.compare("cpusubtype") == 0)
                {
                    sub = Args::StringToUInt32 (value.c_str(), 0, 16);
                    if (sub != 0)
                        ++num_keys_decoded;
                }
                else if (name.compare("ostype") == 0)
                {
                    os_name.swap (value);
                    ++num_keys_decoded;
                }
                else if (name.compare("vendor") == 0)
                {
                    vendor_name.swap(value);
                    ++num_keys_decoded;
                }
                else if (name.compare("endian") == 0)
                {
                    ++num_keys_decoded;
                    if (value.compare("little") == 0)
                        byte_order = eByteOrderLittle;
                    else if (value.compare("big") == 0)
                        byte_order = eByteOrderBig;
                    else if (value.compare("pdp") == 0)
                        byte_order = eByteOrderPDP;
                    else
                        --num_keys_decoded;
                }
                else if (name.compare("ptrsize") == 0)
                {
                    pointer_byte_size = Args::StringToUInt32 (value.c_str(), 0, 16);
                    if (pointer_byte_size != 0)
                        ++num_keys_decoded;
                }
            }
            if (num_keys_decoded > 0)
                m_qProcessInfo_is_valid = eLazyBoolYes;
            if (cpu != LLDB_INVALID_CPUTYPE && !os_name.empty() && !vendor_name.empty())
            {
                m_process_arch.SetArchitecture (eArchTypeMachO, cpu, sub);
                if (pointer_byte_size)
                {
                    assert (pointer_byte_size == m_process_arch.GetAddressByteSize());
                }
                m_host_arch.GetTriple().setVendorName (llvm::StringRef (vendor_name));
                m_host_arch.GetTriple().setOSName (llvm::StringRef (os_name));
                return true;
            }
        }
    }
    else
    {
        m_qProcessInfo_is_valid = eLazyBoolNo;
    }

    return false;
}


uint32_t
GDBRemoteCommunicationClient::FindProcesses (const ProcessInstanceInfoMatch &match_info,
                                             ProcessInstanceInfoList &process_infos)
{
    process_infos.Clear();
    
    if (m_supports_qfProcessInfo)
    {
        StreamString packet;
        packet.PutCString ("qfProcessInfo");
        if (!match_info.MatchAllProcesses())
        {
            packet.PutChar (':');
            const char *name = match_info.GetProcessInfo().GetName();
            bool has_name_match = false;
            if (name && name[0])
            {
                has_name_match = true;
                NameMatchType name_match_type = match_info.GetNameMatchType();
                switch (name_match_type)
                {
                case eNameMatchIgnore:  
                    has_name_match = false;
                    break;

                case eNameMatchEquals:  
                    packet.PutCString ("name_match:equals;"); 
                    break;

                case eNameMatchContains:
                    packet.PutCString ("name_match:contains;"); 
                    break;
                
                case eNameMatchStartsWith:
                    packet.PutCString ("name_match:starts_with;"); 
                    break;
                
                case eNameMatchEndsWith:
                    packet.PutCString ("name_match:ends_with;"); 
                    break;

                case eNameMatchRegularExpression:
                    packet.PutCString ("name_match:regex;"); 
                    break;
                }
                if (has_name_match)
                {
                    packet.PutCString ("name:");
                    packet.PutBytesAsRawHex8(name, ::strlen(name));
                    packet.PutChar (';');
                }
            }
            
            if (match_info.GetProcessInfo().ProcessIDIsValid())
                packet.Printf("pid:%" PRIu64 ";",match_info.GetProcessInfo().GetProcessID());
            if (match_info.GetProcessInfo().ParentProcessIDIsValid())
                packet.Printf("parent_pid:%" PRIu64 ";",match_info.GetProcessInfo().GetParentProcessID());
            if (match_info.GetProcessInfo().UserIDIsValid())
                packet.Printf("uid:%u;",match_info.GetProcessInfo().GetUserID());
            if (match_info.GetProcessInfo().GroupIDIsValid())
                packet.Printf("gid:%u;",match_info.GetProcessInfo().GetGroupID());
            if (match_info.GetProcessInfo().EffectiveUserIDIsValid())
                packet.Printf("euid:%u;",match_info.GetProcessInfo().GetEffectiveUserID());
            if (match_info.GetProcessInfo().EffectiveGroupIDIsValid())
                packet.Printf("egid:%u;",match_info.GetProcessInfo().GetEffectiveGroupID());
            if (match_info.GetProcessInfo().EffectiveGroupIDIsValid())
                packet.Printf("all_users:%u;",match_info.GetMatchAllUsers() ? 1 : 0);
            if (match_info.GetProcessInfo().GetArchitecture().IsValid())
            {
                const ArchSpec &match_arch = match_info.GetProcessInfo().GetArchitecture();
                const llvm::Triple &triple = match_arch.GetTriple();
                packet.PutCString("triple:");
                packet.PutCStringAsRawHex8(triple.getTriple().c_str());
                packet.PutChar (';');
            }
        }
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false) == PacketResult::Success)
        {
            do
            {
                ProcessInstanceInfo process_info;
                if (!DecodeProcessInfoResponse (response, process_info))
                    break;
                process_infos.Append(process_info);
                response.GetStringRef().clear();
                response.SetFilePos(0);
            } while (SendPacketAndWaitForResponse ("qsProcessInfo", strlen ("qsProcessInfo"), response, false) == PacketResult::Success);
        }
        else
        {
            m_supports_qfProcessInfo = false;
            return 0;
        }
    }
    return process_infos.GetSize();
    
}

bool
GDBRemoteCommunicationClient::GetUserName (uint32_t uid, std::string &name)
{
    if (m_supports_qUserName)
    {
        char packet[32];
        const int packet_len = ::snprintf (packet, sizeof (packet), "qUserName:%i", uid);
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            if (response.IsNormalResponse())
            {
                // Make sure we parsed the right number of characters. The response is
                // the hex encoded user name and should make up the entire packet.
                // If there are any non-hex ASCII bytes, the length won't match below..
                if (response.GetHexByteString (name) * 2 == response.GetStringRef().size())
                    return true;
            }
        }
        else
        {
            m_supports_qUserName = false;
            return false;
        }        
    }
    return false;

}

bool
GDBRemoteCommunicationClient::GetGroupName (uint32_t gid, std::string &name)
{
    if (m_supports_qGroupName)
    {
        char packet[32];
        const int packet_len = ::snprintf (packet, sizeof (packet), "qGroupName:%i", gid);
        assert (packet_len < (int)sizeof(packet));
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse (packet, packet_len, response, false) == PacketResult::Success)
        {
            if (response.IsNormalResponse())
            {
                // Make sure we parsed the right number of characters. The response is
                // the hex encoded group name and should make up the entire packet.
                // If there are any non-hex ASCII bytes, the length won't match below..
                if (response.GetHexByteString (name) * 2 == response.GetStringRef().size())
                    return true;
            }
        }
        else
        {
            m_supports_qGroupName = false;
            return false;
        }
    }
    return false;
}

void
GDBRemoteCommunicationClient::TestPacketSpeed (const uint32_t num_packets)
{
    uint32_t i;
    TimeValue start_time, end_time;
    uint64_t total_time_nsec;
    if (SendSpeedTestPacket (0, 0))
    {
        static uint32_t g_send_sizes[] = { 0, 64, 128, 512, 1024 };
        static uint32_t g_recv_sizes[] = { 0, 64, 128, 512, 1024 }; //, 4*1024, 8*1024, 16*1024, 32*1024, 48*1024, 64*1024, 96*1024, 128*1024 };
        const size_t k_num_send_sizes = sizeof(g_send_sizes)/sizeof(uint32_t);
        const size_t k_num_recv_sizes = sizeof(g_recv_sizes)/sizeof(uint32_t);
        const uint64_t k_recv_amount = 4*1024*1024; // Receive 4MB
        for (uint32_t send_idx = 0; send_idx < k_num_send_sizes; ++send_idx)
        {
            const uint32_t send_size = g_send_sizes[send_idx];
            for (uint32_t recv_idx = 0; recv_idx < k_num_recv_sizes; ++recv_idx)
            {
                const uint32_t recv_size = g_recv_sizes[recv_idx];
                StreamString packet;
                packet.Printf ("qSpeedTest:response_size:%i;data:", recv_size);
                uint32_t bytes_left = send_size;
                while (bytes_left > 0)
                {
                    if (bytes_left >= 26)
                    {
                        packet.PutCString("abcdefghijklmnopqrstuvwxyz");
                        bytes_left -= 26;
                    }
                    else
                    {
                        packet.Printf ("%*.*s;", bytes_left, bytes_left, "abcdefghijklmnopqrstuvwxyz");
                        bytes_left = 0;
                    }
                }

                start_time = TimeValue::Now();
                if (recv_size == 0)
                {
                    for (i=0; i<num_packets; ++i)
                    {
                        StringExtractorGDBRemote response;
                        SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false);
                    }
                }
                else
                {
                    uint32_t bytes_read = 0;
                    while (bytes_read < k_recv_amount)
                    {
                        StringExtractorGDBRemote response;
                        SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false);
                        bytes_read += recv_size;
                    }
                }
                end_time = TimeValue::Now();
                total_time_nsec = end_time.GetAsNanoSecondsSinceJan1_1970() - start_time.GetAsNanoSecondsSinceJan1_1970();
                if (recv_size == 0)
                {
                    float packets_per_second = (((float)num_packets)/(float)total_time_nsec) * (float)TimeValue::NanoSecPerSec;
                    printf ("%u qSpeedTest(send=%-7u, recv=%-7u) in %" PRIu64 ".%9.9" PRIu64 " sec for %f packets/sec.\n",
                            num_packets, 
                            send_size,
                            recv_size,
                            total_time_nsec / TimeValue::NanoSecPerSec,
                            total_time_nsec % TimeValue::NanoSecPerSec, 
                            packets_per_second);
                }
                else
                {
                    float mb_second = ((((float)k_recv_amount)/(float)total_time_nsec) * (float)TimeValue::NanoSecPerSec) / (1024.0*1024.0);
                    printf ("%u qSpeedTest(send=%-7u, recv=%-7u) sent 4MB in %" PRIu64 ".%9.9" PRIu64 " sec for %f MB/sec.\n",
                            num_packets,
                            send_size,
                            recv_size,
                            total_time_nsec / TimeValue::NanoSecPerSec,
                            total_time_nsec % TimeValue::NanoSecPerSec,
                            mb_second);
                }
            }
        }
    }
}

bool
GDBRemoteCommunicationClient::SendSpeedTestPacket (uint32_t send_size, uint32_t recv_size)
{
    StreamString packet;
    packet.Printf ("qSpeedTest:response_size:%i;data:", recv_size);
    uint32_t bytes_left = send_size;
    while (bytes_left > 0)
    {
        if (bytes_left >= 26)
        {
            packet.PutCString("abcdefghijklmnopqrstuvwxyz");
            bytes_left -= 26;
        }
        else
        {
            packet.Printf ("%*.*s;", bytes_left, bytes_left, "abcdefghijklmnopqrstuvwxyz");
            bytes_left = 0;
        }
    }

    StringExtractorGDBRemote response;
    return SendPacketAndWaitForResponse (packet.GetData(), packet.GetSize(), response, false)  == PacketResult::Success;
}

uint16_t
GDBRemoteCommunicationClient::LaunchGDBserverAndGetPort (lldb::pid_t &pid, const char *remote_accept_hostname)
{
    pid = LLDB_INVALID_PROCESS_ID;
    StringExtractorGDBRemote response;
    StreamString stream;
    stream.PutCString("qLaunchGDBServer;");
    std::string hostname;
    if (remote_accept_hostname  && remote_accept_hostname[0])
        hostname = remote_accept_hostname;
    else
    {
        if (Host::GetHostname (hostname))
        {
            // Make the GDB server we launch only accept connections from this host
            stream.Printf("host:%s;", hostname.c_str());
        }
        else
        {
            // Make the GDB server we launch accept connections from any host since we can't figure out the hostname
            stream.Printf("host:*;");
        }
    }
    const char *packet = stream.GetData();
    int packet_len = stream.GetSize();

    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        std::string name;
        std::string value;
        uint16_t port = 0;
        while (response.GetNameColonValue(name, value))
        {
            if (name.compare("port") == 0)
                port = Args::StringToUInt32(value.c_str(), 0, 0);
            else if (name.compare("pid") == 0)
                pid = Args::StringToUInt64(value.c_str(), LLDB_INVALID_PROCESS_ID, 0);
        }
        return port;
    }
    return 0;
}

bool
GDBRemoteCommunicationClient::KillSpawnedProcess (lldb::pid_t pid)
{
    StreamString stream;
    stream.Printf ("qKillSpawnedProcess:%" PRId64 , pid);
    const char *packet = stream.GetData();
    int packet_len = stream.GetSize();

    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.IsOKResponse())
            return true;
    }
    return false;
}

bool
GDBRemoteCommunicationClient::SetCurrentThread (uint64_t tid)
{
    if (m_curr_tid == tid)
        return true;

    char packet[32];
    int packet_len;
    if (tid == UINT64_MAX)
        packet_len = ::snprintf (packet, sizeof(packet), "Hg-1");
    else
        packet_len = ::snprintf (packet, sizeof(packet), "Hg%" PRIx64, tid);
    assert (packet_len + 1 < (int)sizeof(packet));
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.IsOKResponse())
        {
            m_curr_tid = tid;
            return true;
        }
    }
    return false;
}

bool
GDBRemoteCommunicationClient::SetCurrentThreadForRun (uint64_t tid)
{
    if (m_curr_tid_run == tid)
        return true;

    char packet[32];
    int packet_len;
    if (tid == UINT64_MAX)
        packet_len = ::snprintf (packet, sizeof(packet), "Hc-1");
    else
        packet_len = ::snprintf (packet, sizeof(packet), "Hc%" PRIx64, tid);

    assert (packet_len + 1 < (int)sizeof(packet));
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.IsOKResponse())
        {
            m_curr_tid_run = tid;
            return true;
        }
    }
    return false;
}

bool
GDBRemoteCommunicationClient::GetStopReply (StringExtractorGDBRemote &response)
{
    if (SendPacketAndWaitForResponse("?", 1, response, false) == PacketResult::Success)
        return response.IsNormalResponse();
    return false;
}

bool
GDBRemoteCommunicationClient::GetThreadStopInfo (lldb::tid_t tid, StringExtractorGDBRemote &response)
{
    if (m_supports_qThreadStopInfo)
    {
        char packet[256];
        int packet_len = ::snprintf(packet, sizeof(packet), "qThreadStopInfo%" PRIx64, tid);
        assert (packet_len < (int)sizeof(packet));
        if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
        {
            if (response.IsUnsupportedResponse())
                m_supports_qThreadStopInfo = false;
            else if (response.IsNormalResponse())
                return true;
            else
                return false;
        }
        else
        {
            m_supports_qThreadStopInfo = false;
        }
    }
    return false;
}


uint8_t
GDBRemoteCommunicationClient::SendGDBStoppointTypePacket (GDBStoppointType type, bool insert,  addr_t addr, uint32_t length)
{
    // Check if the stub is known not to support this breakpoint type
    if (!SupportsGDBStoppointPacket(type))
        return UINT8_MAX;
    // Construct the breakpoint packet
    char packet[64];
    const int packet_len = ::snprintf (packet, 
                                       sizeof(packet), 
                                       "%c%i,%" PRIx64 ",%x",
                                       insert ? 'Z' : 'z', 
                                       type, 
                                       addr, 
                                       length);
    // Check we havent overwritten the end of the packet buffer
    assert (packet_len + 1 < (int)sizeof(packet));
    StringExtractorGDBRemote response;
    // Try to send the breakpoint packet, and check that it was correctly sent
    if (SendPacketAndWaitForResponse(packet, packet_len, response, true) == PacketResult::Success)
    {
        // Receive and OK packet when the breakpoint successfully placed
        if (response.IsOKResponse())
            return 0;

        // Error while setting breakpoint, send back specific error
        if (response.IsErrorResponse())
            return response.GetError();

        // Empty packet informs us that breakpoint is not supported
        if (response.IsUnsupportedResponse())
        {
            // Disable this breakpoint type since it is unsupported
            switch (type)
            {
            case eBreakpointSoftware:   m_supports_z0 = false; break;
            case eBreakpointHardware:   m_supports_z1 = false; break;
            case eWatchpointWrite:      m_supports_z2 = false; break;
            case eWatchpointRead:       m_supports_z3 = false; break;
            case eWatchpointReadWrite:  m_supports_z4 = false; break;
            }
        }
    }
    // Signal generic faliure
    return UINT8_MAX;
}

size_t
GDBRemoteCommunicationClient::GetCurrentThreadIDs (std::vector<lldb::tid_t> &thread_ids, 
                                                   bool &sequence_mutex_unavailable)
{
    Mutex::Locker locker;
    thread_ids.clear();
    
    if (GetSequenceMutex (locker, "ProcessGDBRemote::UpdateThreadList() failed due to not getting the sequence mutex"))
    {
        sequence_mutex_unavailable = false;
        StringExtractorGDBRemote response;
        
        PacketResult packet_result;
        for (packet_result = SendPacketAndWaitForResponseNoLock ("qfThreadInfo", strlen("qfThreadInfo"), response);
             packet_result == PacketResult::Success && response.IsNormalResponse();
             packet_result = SendPacketAndWaitForResponseNoLock ("qsThreadInfo", strlen("qsThreadInfo"), response))
        {
            char ch = response.GetChar();
            if (ch == 'l')
                break;
            if (ch == 'm')
            {
                do
                {
                    tid_t tid = response.GetHexMaxU64(false, LLDB_INVALID_THREAD_ID);
                    
                    if (tid != LLDB_INVALID_THREAD_ID)
                    {
                        thread_ids.push_back (tid);
                    }
                    ch = response.GetChar();    // Skip the command separator
                } while (ch == ',');            // Make sure we got a comma separator
            }
        }
    }
    else
    {
#if defined (LLDB_CONFIGURATION_DEBUG)
        // assert(!"ProcessGDBRemote::UpdateThreadList() failed due to not getting the sequence mutex");
#else
        Log *log (ProcessGDBRemoteLog::GetLogIfAnyCategoryIsSet (GDBR_LOG_PROCESS | GDBR_LOG_PACKETS));
        if (log)
            log->Printf("error: failed to get packet sequence mutex, not sending packet 'qfThreadInfo'");
#endif
        sequence_mutex_unavailable = true;
    }
    return thread_ids.size();
}

lldb::addr_t
GDBRemoteCommunicationClient::GetShlibInfoAddr()
{
    if (!IsRunning())
    {
        StringExtractorGDBRemote response;
        if (SendPacketAndWaitForResponse("qShlibInfoAddr", ::strlen ("qShlibInfoAddr"), response, false) == PacketResult::Success)
        {
            if (response.IsNormalResponse())
                return response.GetHexMaxU64(false, LLDB_INVALID_ADDRESS);
        }
    }
    return LLDB_INVALID_ADDRESS;
}

lldb_private::Error
GDBRemoteCommunicationClient::RunShellCommand (const char *command,           // Shouldn't be NULL
                                               const char *working_dir,       // Pass NULL to use the current working directory
                                               int *status_ptr,               // Pass NULL if you don't want the process exit status
                                               int *signo_ptr,                // Pass NULL if you don't want the signal that caused the process to exit
                                               std::string *command_output,   // Pass NULL if you don't want the command output
                                               uint32_t timeout_sec)          // Timeout in seconds to wait for shell program to finish
{
    lldb_private::StreamString stream;
    stream.PutCString("qPlatform_shell:");
    stream.PutBytesAsRawHex8(command, strlen(command));
    stream.PutChar(',');
    stream.PutHex32(timeout_sec);
    if (working_dir && *working_dir)
    {
        stream.PutChar(',');
        stream.PutBytesAsRawHex8(working_dir, strlen(working_dir));
    }
    const char *packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
            return Error("malformed reply");
        if (response.GetChar() != ',')
            return Error("malformed reply");
        uint32_t exitcode = response.GetHexMaxU32(false, UINT32_MAX);
        if (exitcode == UINT32_MAX)
            return Error("unable to run remote process");
        else if (status_ptr)
            *status_ptr = exitcode;
        if (response.GetChar() != ',')
            return Error("malformed reply");
        uint32_t signo = response.GetHexMaxU32(false, UINT32_MAX);
        if (signo_ptr)
            *signo_ptr = signo;
        if (response.GetChar() != ',')
            return Error("malformed reply");
        std::string output;
        response.GetEscapedBinaryData(output);
        if (command_output)
            command_output->assign(output);
        return Error();
    }
    return Error("unable to send packet");
}

Error
GDBRemoteCommunicationClient::MakeDirectory (const char *path,
                                             uint32_t file_permissions)
{
    lldb_private::StreamString stream;
    stream.PutCString("qPlatform_mkdir:");
    stream.PutHex32(file_permissions);
    stream.PutChar(',');
    stream.PutBytesAsRawHex8(path, strlen(path));
    const char *packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        return Error(response.GetHexMaxU32(false, UINT32_MAX), eErrorTypePOSIX);
    }
    return Error();

}

Error
GDBRemoteCommunicationClient::SetFilePermissions (const char *path,
                                                  uint32_t file_permissions)
{
    lldb_private::StreamString stream;
    stream.PutCString("qPlatform_chmod:");
    stream.PutHex32(file_permissions);
    stream.PutChar(',');
    stream.PutBytesAsRawHex8(path, strlen(path));
    const char *packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        return Error(response.GetHexMaxU32(false, UINT32_MAX), eErrorTypePOSIX);
    }
    return Error();
    
}

static uint64_t
ParseHostIOPacketResponse (StringExtractorGDBRemote &response,
                           uint64_t fail_result,
                           Error &error)
{
    response.SetFilePos(0);
    if (response.GetChar() != 'F')
        return fail_result;
    int32_t result = response.GetS32 (-2);
    if (result == -2)
        return fail_result;
    if (response.GetChar() == ',')
    {
        int result_errno = response.GetS32 (-2);
        if (result_errno != -2)
            error.SetError(result_errno, eErrorTypePOSIX);
        else
            error.SetError(-1, eErrorTypeGeneric);
    }
    else
        error.Clear();
    return  result;
}
lldb::user_id_t
GDBRemoteCommunicationClient::OpenFile (const lldb_private::FileSpec& file_spec,
                                        uint32_t flags,
                                        mode_t mode,
                                        Error &error)
{
    lldb_private::StreamString stream;
    stream.PutCString("vFile:open:");
    std::string path (file_spec.GetPath());
    if (path.empty())
        return UINT64_MAX;
    stream.PutCStringAsRawHex8(path.c_str());
    stream.PutChar(',');
    const uint32_t posix_open_flags = File::ConvertOpenOptionsForPOSIXOpen(flags);
    stream.PutHex32(posix_open_flags);
    stream.PutChar(',');
    stream.PutHex32(mode);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        return ParseHostIOPacketResponse (response, UINT64_MAX, error);
    }
    return UINT64_MAX;
}

bool
GDBRemoteCommunicationClient::CloseFile (lldb::user_id_t fd,
                                         Error &error)
{
    lldb_private::StreamString stream;
    stream.Printf("vFile:close:%i", (int)fd);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        return ParseHostIOPacketResponse (response, -1, error) == 0;
    }
    return false;
}

// Extension of host I/O packets to get the file size.
lldb::user_id_t
GDBRemoteCommunicationClient::GetFileSize (const lldb_private::FileSpec& file_spec)
{
    lldb_private::StreamString stream;
    stream.PutCString("vFile:size:");
    std::string path (file_spec.GetPath());
    stream.PutCStringAsRawHex8(path.c_str());
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
            return UINT64_MAX;
        uint32_t retcode = response.GetHexMaxU64(false, UINT64_MAX);
        return retcode;
    }
    return UINT64_MAX;
}

Error
GDBRemoteCommunicationClient::GetFilePermissions(const char *path, uint32_t &file_permissions)
{
    Error error;
    lldb_private::StreamString stream;
    stream.PutCString("vFile:mode:");
    stream.PutCStringAsRawHex8(path);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
        {
            error.SetErrorStringWithFormat ("invalid response to '%s' packet", packet);
        }
        else
        {
            const uint32_t mode = response.GetS32(-1);
            if (mode == -1)
            {
                if (response.GetChar() == ',')
                {
                    int response_errno = response.GetS32(-1);
                    if (response_errno > 0)
                        error.SetError(response_errno, lldb::eErrorTypePOSIX);
                    else
                        error.SetErrorToGenericError();
                }
                else
                    error.SetErrorToGenericError();
            }
            else
            {
                file_permissions = mode & (S_IRWXU|S_IRWXG|S_IRWXO);
            }
        }
    }
    else
    {
        error.SetErrorStringWithFormat ("failed to send '%s' packet", packet);
    }
    return error;
}

uint64_t
GDBRemoteCommunicationClient::ReadFile (lldb::user_id_t fd,
                                        uint64_t offset,
                                        void *dst,
                                        uint64_t dst_len,
                                        Error &error)
{
    lldb_private::StreamString stream;
    stream.Printf("vFile:pread:%i,%" PRId64 ",%" PRId64, (int)fd, dst_len, offset);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
            return 0;
        uint32_t retcode = response.GetHexMaxU32(false, UINT32_MAX);
        if (retcode == UINT32_MAX)
            return retcode;
        const char next = (response.Peek() ? *response.Peek() : 0);
        if (next == ',')
            return 0;
        if (next == ';')
        {
            response.GetChar(); // skip the semicolon
            std::string buffer;
            if (response.GetEscapedBinaryData(buffer))
            {
                const uint64_t data_to_write = std::min<uint64_t>(dst_len, buffer.size());
                if (data_to_write > 0)
                    memcpy(dst, &buffer[0], data_to_write);
                return data_to_write;
            }
        }
    }
    return 0;
}

uint64_t
GDBRemoteCommunicationClient::WriteFile (lldb::user_id_t fd,
                                         uint64_t offset,
                                         const void* src,
                                         uint64_t src_len,
                                         Error &error)
{
    lldb_private::StreamGDBRemote stream;
    stream.Printf("vFile:pwrite:%i,%" PRId64 ",", (int)fd, offset);
    stream.PutEscapedBytes(src, src_len);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
        {
            error.SetErrorStringWithFormat("write file failed");
            return 0;
        }
        uint64_t bytes_written = response.GetU64(UINT64_MAX);
        if (bytes_written == UINT64_MAX)
        {
            error.SetErrorToGenericError();
            if (response.GetChar() == ',')
            {
                int response_errno = response.GetS32(-1);
                if (response_errno > 0)
                    error.SetError(response_errno, lldb::eErrorTypePOSIX);
            }
            return 0;
        }
        return bytes_written;
    }
    else
    {
        error.SetErrorString ("failed to send vFile:pwrite packet");
    }
    return 0;
}

Error
GDBRemoteCommunicationClient::CreateSymlink (const char *src, const char *dst)
{
    Error error;
    lldb_private::StreamGDBRemote stream;
    stream.PutCString("vFile:symlink:");
    // the unix symlink() command reverses its parameters where the dst if first,
    // so we follow suit here
    stream.PutCStringAsRawHex8(dst);
    stream.PutChar(',');
    stream.PutCStringAsRawHex8(src);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() == 'F')
        {
            uint32_t result = response.GetU32(UINT32_MAX);
            if (result != 0)
            {
                error.SetErrorToGenericError();
                if (response.GetChar() == ',')
                {
                    int response_errno = response.GetS32(-1);
                    if (response_errno > 0)
                        error.SetError(response_errno, lldb::eErrorTypePOSIX);
                }
            }
        }
        else
        {
            // Should have returned with 'F<result>[,<errno>]'
            error.SetErrorStringWithFormat("symlink failed");
        }
    }
    else
    {
        error.SetErrorString ("failed to send vFile:symlink packet");
    }
    return error;
}

Error
GDBRemoteCommunicationClient::Unlink (const char *path)
{
    Error error;
    lldb_private::StreamGDBRemote stream;
    stream.PutCString("vFile:unlink:");
    // the unix symlink() command reverses its parameters where the dst if first,
    // so we follow suit here
    stream.PutCStringAsRawHex8(path);
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() == 'F')
        {
            uint32_t result = response.GetU32(UINT32_MAX);
            if (result != 0)
            {
                error.SetErrorToGenericError();
                if (response.GetChar() == ',')
                {
                    int response_errno = response.GetS32(-1);
                    if (response_errno > 0)
                        error.SetError(response_errno, lldb::eErrorTypePOSIX);
                }
            }
        }
        else
        {
            // Should have returned with 'F<result>[,<errno>]'
            error.SetErrorStringWithFormat("unlink failed");
        }
    }
    else
    {
        error.SetErrorString ("failed to send vFile:unlink packet");
    }
    return error;
}

// Extension of host I/O packets to get whether a file exists.
bool
GDBRemoteCommunicationClient::GetFileExists (const lldb_private::FileSpec& file_spec)
{
    lldb_private::StreamString stream;
    stream.PutCString("vFile:exists:");
    std::string path (file_spec.GetPath());
    stream.PutCStringAsRawHex8(path.c_str());
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
            return false;
        if (response.GetChar() != ',')
            return false;
        bool retcode = (response.GetChar() != '0');
        return retcode;
    }
    return false;
}

bool
GDBRemoteCommunicationClient::CalculateMD5 (const lldb_private::FileSpec& file_spec,
                                            uint64_t &high,
                                            uint64_t &low)
{
    lldb_private::StreamString stream;
    stream.PutCString("vFile:MD5:");
    std::string path (file_spec.GetPath());
    stream.PutCStringAsRawHex8(path.c_str());
    const char* packet = stream.GetData();
    int packet_len = stream.GetSize();
    StringExtractorGDBRemote response;
    if (SendPacketAndWaitForResponse(packet, packet_len, response, false) == PacketResult::Success)
    {
        if (response.GetChar() != 'F')
            return false;
        if (response.GetChar() != ',')
            return false;
        if (response.Peek() && *response.Peek() == 'x')
            return false;
        low = response.GetHexMaxU64(false, UINT64_MAX);
        high = response.GetHexMaxU64(false, UINT64_MAX);
        return true;
    }
    return false;
}

bool
GDBRemoteCommunicationClient::ReadRegister(lldb::tid_t tid, uint32_t reg, StringExtractorGDBRemote &response)
{
    Mutex::Locker locker;
    if (GetSequenceMutex (locker, "Didn't get sequence mutex for p packet."))
    {
        const bool thread_suffix_supported = GetThreadSuffixSupported();
        
        if (thread_suffix_supported || SetCurrentThread(tid))
        {
            char packet[64];
            int packet_len = 0;
            if (thread_suffix_supported)
                packet_len = ::snprintf (packet, sizeof(packet), "p%x;thread:%4.4" PRIx64 ";", reg, tid);
            else
                packet_len = ::snprintf (packet, sizeof(packet), "p%x", reg);
            assert (packet_len < ((int)sizeof(packet) - 1));
            return SendPacketAndWaitForResponse(packet, response, false) == PacketResult::Success;
        }
    }
    return false;

}


bool
GDBRemoteCommunicationClient::ReadAllRegisters (lldb::tid_t tid, StringExtractorGDBRemote &response)
{
    Mutex::Locker locker;
    if (GetSequenceMutex (locker, "Didn't get sequence mutex for g packet."))
    {
        const bool thread_suffix_supported = GetThreadSuffixSupported();

        if (thread_suffix_supported || SetCurrentThread(tid))
        {
            char packet[64];
            int packet_len = 0;
            // Get all registers in one packet
            if (thread_suffix_supported)
                packet_len = ::snprintf (packet, sizeof(packet), "g;thread:%4.4" PRIx64 ";", tid);
            else
                packet_len = ::snprintf (packet, sizeof(packet), "g");
            assert (packet_len < ((int)sizeof(packet) - 1));
            return SendPacketAndWaitForResponse(packet, response, false) == PacketResult::Success;
        }
    }
    return false;
}
bool
GDBRemoteCommunicationClient::SaveRegisterState (lldb::tid_t tid, uint32_t &save_id)
{
    save_id = 0; // Set to invalid save ID
    if (m_supports_QSaveRegisterState == eLazyBoolNo)
        return false;
    
    m_supports_QSaveRegisterState = eLazyBoolYes;
    Mutex::Locker locker;
    if (GetSequenceMutex (locker, "Didn't get sequence mutex for QSaveRegisterState."))
    {
        const bool thread_suffix_supported = GetThreadSuffixSupported();
        if (thread_suffix_supported || SetCurrentThread(tid))
        {
            char packet[256];
            if (thread_suffix_supported)
                ::snprintf (packet, sizeof(packet), "QSaveRegisterState;thread:%4.4" PRIx64 ";", tid);
            else
                ::strncpy (packet, "QSaveRegisterState", sizeof(packet));
            
            StringExtractorGDBRemote response;

            if (SendPacketAndWaitForResponse(packet, response, false) == PacketResult::Success)
            {
                if (response.IsUnsupportedResponse())
                {
                    // This packet isn't supported, don't try calling it again
                    m_supports_QSaveRegisterState = eLazyBoolNo;
                }
                    
                const uint32_t response_save_id = response.GetU32(0);
                if (response_save_id != 0)
                {
                    save_id = response_save_id;
                    return true;
                }
            }
        }
    }
    return false;
}

bool
GDBRemoteCommunicationClient::RestoreRegisterState (lldb::tid_t tid, uint32_t save_id)
{
    // We use the "m_supports_QSaveRegisterState" variable here becuase the
    // QSaveRegisterState and QRestoreRegisterState packets must both be supported in
    // order to be useful
    if (m_supports_QSaveRegisterState == eLazyBoolNo)
        return false;
    
    Mutex::Locker locker;
    if (GetSequenceMutex (locker, "Didn't get sequence mutex for QRestoreRegisterState."))
    {
        const bool thread_suffix_supported = GetThreadSuffixSupported();
        if (thread_suffix_supported || SetCurrentThread(tid))
        {
            char packet[256];
            if (thread_suffix_supported)
                ::snprintf (packet, sizeof(packet), "QRestoreRegisterState:%u;thread:%4.4" PRIx64 ";", save_id, tid);
            else
                ::snprintf (packet, sizeof(packet), "QRestoreRegisterState:%u" PRIx64 ";", save_id);
            
            StringExtractorGDBRemote response;
            
            if (SendPacketAndWaitForResponse(packet, response, false) == PacketResult::Success)
            {
                if (response.IsOKResponse())
                {
                    return true;
                }
                else if (response.IsUnsupportedResponse())
                {
                    // This packet isn't supported, don't try calling this packet or
                    // QSaveRegisterState again...
                    m_supports_QSaveRegisterState = eLazyBoolNo;
                }
            }
        }
    }
    return false;
}

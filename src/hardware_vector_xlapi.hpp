#pragma once

#ifdef _WIN32

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace jcan
{

    namespace vector
    {
        namespace xlapi
        {

            static constexpr long XL_SUCCESS = 0;
            static constexpr long XL_ERR_QUEUE_IS_EMPTY = 10;

            static constexpr unsigned XL_BUS_TYPE_CAN = 0x00000001;
            static constexpr unsigned XL_ACTIVATE_RESET_CLOCK = 8;
            static constexpr unsigned XL_INTERFACE_VERSION_V3 = 3;
            static constexpr unsigned XL_OUTPUT_MODE_NORMAL = 1;
            static constexpr unsigned XL_OUTPUT_MODE_SILENT = 0;
            static constexpr unsigned XL_CONFIG_MAX_CHANNELS = 64;
            static constexpr unsigned XL_MAX_LENGTH = 31;

            static constexpr unsigned XL_RECEIVE_MSG = 0x01;
            static constexpr unsigned XL_CHIP_STATE = 0x04;
            static constexpr unsigned XL_TRANSMIT_MSG = 0x0A;

            static constexpr unsigned XL_CAN_MSG_FLAG_ERROR_FRAME = 0x01;
            static constexpr unsigned XL_CAN_MSG_FLAG_REMOTE_FRAME = 0x10;
            static constexpr unsigned XL_CAN_MSG_FLAG_TX_COMPLETED = 0x40;
            static constexpr unsigned XL_CAN_EXT_MSG_ID = 0x80000000;

            using XLstatus = long;
            using XLaccess = unsigned long long;
            using XLportHandle = long;

#pragma pack(push, 1)

            struct XLbusParams
            {
                unsigned int busType;
                unsigned char data[28];
            }; // 32 bytes

            struct XLchannelConfig
            {
                char name[XL_MAX_LENGTH + 1];                // 32
                unsigned char hwType;                        // 1
                unsigned char hwIndex;                       // 1
                unsigned char hwChannel;                     // 1
                unsigned short transceiverType;              // 2
                unsigned short transceiverState;             // 2
                unsigned short configError;                  // 2
                unsigned char channelIndex;                  // 1
                unsigned long long channelMask;              // 8
                unsigned int channelCapabilities;            // 4
                unsigned int channelBusCapabilities;         // 4
                unsigned char isOnBus;                       // 1
                unsigned int connectedBusType;               // 4
                XLbusParams busParams;                       // 32
                unsigned int _doNotUse;                      // 4
                unsigned int driverVersion;                  // 4
                unsigned int interfaceVersion;               // 4
                unsigned int raw_data[10];                   // 40
                unsigned int serialNumber;                   // 4
                unsigned int articleNumber;                  // 4
                char transceiverName[XL_MAX_LENGTH + 1];     // 32
                unsigned int specialCabFlags;                // 4
                unsigned int dominantTimeout;                // 4
                unsigned char dominantRecessiveDelay;        // 1
                unsigned char recessiveDominantDelay;        // 1
                unsigned char connectionInfo;                // 1
                unsigned char currentlyAvailableTimestamps;  // 1
                unsigned short minimalSupplyVoltage;         // 2
                unsigned short maximalSupplyVoltage;         // 2
                unsigned int maximalBaudrate;                // 4
                unsigned char fpgaCoreCapabilities;          // 1
                unsigned char specialDeviceStatus;           // 1
                unsigned short channelBusActiveCapabilities; // 2
                unsigned short breakOffset;                  // 2
                unsigned short delimiterOffset;              // 2
                unsigned int reserved[3];                    // 12
            }; // 227 bytes

            struct XLdriverConfig
            {
                unsigned int dllVersion;
                unsigned int channelCount;
                unsigned int reserved[10];
                XLchannelConfig channel[XL_CONFIG_MAX_CHANNELS];
            };

            struct XLcanMsg
            {
                unsigned int id;         // 4  — MSB = extended flag
                unsigned short flags;    // 2
                unsigned short dlc;      // 2
                unsigned long long res1; // 8
                unsigned char data[8];   // 8
                unsigned long long res2; // 8
            }; // 32 bytes

            struct XLchipState
            {
                unsigned char busStatus;
                unsigned char txErrorCounter;
                unsigned char rxErrorCounter;
            };

            union XLtagData
            {
                XLcanMsg msg;
                XLchipState chipState;
                unsigned char raw[32];
            };

            struct XLevent
            {
                unsigned char tag;            // 1
                unsigned char chanIndex;      // 1
                unsigned short transId;       // 2
                unsigned short portHandle;    // 2
                unsigned char flags;          // 1
                unsigned char reserved;       // 1
                unsigned long long timeStamp; // 8
                XLtagData tagData;            // 32
            }; // 48 bytes

#pragma pack(pop)

            static_assert(sizeof(XLbusParams) == 32, "XLbusParams size mismatch");
            static_assert(sizeof(XLchannelConfig) == 227, "XLchannelConfig size mismatch");
            static_assert(sizeof(XLcanMsg) == 32, "XLcanMsg size mismatch");
            static_assert(sizeof(XLevent) == 48, "XLevent size mismatch");

            using fn_xlOpenDriver = XLstatus(__cdecl*)(void);
            using fn_xlCloseDriver = XLstatus(__cdecl*)(void);
            using fn_xlGetDriverConfig = XLstatus(__cdecl*)(XLdriverConfig*);
            using fn_xlGetErrorString = char*(__cdecl*) (XLstatus);
            using fn_xlOpenPort = XLstatus(__cdecl*)(XLportHandle*, char*, XLaccess, XLaccess*, unsigned int, unsigned int, unsigned int);
            using fn_xlClosePort = XLstatus(__cdecl*)(XLportHandle);
            using fn_xlActivateChannel = XLstatus(__cdecl*)(XLportHandle, XLaccess, unsigned int, unsigned int);
            using fn_xlDeactivateChannel = XLstatus(__cdecl*)(XLportHandle, XLaccess);
            using fn_xlCanSetChannelBitrate = XLstatus(__cdecl*)(XLportHandle, XLaccess, unsigned long);
            using fn_xlCanSetChannelOutput = XLstatus(__cdecl*)(XLportHandle, XLaccess, unsigned int);
            using fn_xlCanTransmit = XLstatus(__cdecl*)(XLportHandle, XLaccess, unsigned int*, void*);
            using fn_xlReceive = XLstatus(__cdecl*)(XLportHandle, unsigned int*, XLevent*);
            using fn_xlSetNotification = XLstatus(__cdecl*)(XLportHandle, HANDLE*, int);
            using fn_xlFlushReceiveQueue = XLstatus(__cdecl*)(XLportHandle);

            struct api
            {
                HMODULE dll{nullptr};

                fn_xlOpenDriver pOpenDriver{};
                fn_xlCloseDriver pCloseDriver{};
                fn_xlGetDriverConfig pGetDriverConfig{};
                fn_xlGetErrorString pGetErrorString{};
                fn_xlOpenPort pOpenPort{};
                fn_xlClosePort pClosePort{};
                fn_xlActivateChannel pActivateChannel{};
                fn_xlDeactivateChannel pDeactivateChannel{};
                fn_xlCanSetChannelBitrate pCanSetChannelBitrate{};
                fn_xlCanSetChannelOutput pCanSetChannelOutput{};
                fn_xlCanTransmit pCanTransmit{};
                fn_xlReceive pReceive{};
                fn_xlSetNotification pSetNotification{};
                fn_xlFlushReceiveQueue pFlushReceiveQueue{};

                bool driver_open{false};

                bool loaded() const
                {
                    return dll != nullptr;
                }

                bool load()
                {
                    if (dll)
                        return true;

                    dll = LoadLibraryA("vxlapi64.dll");
                    if (!dll)
                        dll = LoadLibraryA("vxlapi.dll");

                    if (!dll)
                    {
                        static const char* reg_keys[] = {
                            "SOFTWARE\\Vector\\XL Driver Library",
                            "SOFTWARE\\WOW6432Node\\Vector\\XL Driver Library",
                            "SOFTWARE\\Vector Informatik\\XL Driver Library",
                        };
                        static const char* reg_values[] = {
                            "Install Dir",
                            "InstallDir",
                            "Path",
                        };
                        for (const char* key : reg_keys)
                        {
                            if (dll)
                                break;
                            HKEY hk = nullptr;
                            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hk) != ERROR_SUCCESS)
                                continue;
                            for (const char* val_name : reg_values)
                            {
                                char path_buf[512]{};
                                DWORD buf_size = sizeof(path_buf) - 1;
                                DWORD type = 0;
                                if (RegQueryValueExA(hk, val_name, nullptr, &type, reinterpret_cast<LPBYTE>(path_buf), &buf_size) == ERROR_SUCCESS
                                    && (type == REG_SZ || type == REG_EXPAND_SZ) && buf_size > 0)
                                {
                                    std::string dir(path_buf);
                                    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
                                        dir += '\\';
                                    std::fprintf(stderr, "[vector] vxlapi: trying registry path: %s\n", dir.c_str());

                                    dll = LoadLibraryA((dir + "vxlapi64.dll").c_str());
                                    if (!dll)
                                        dll = LoadLibraryA((dir + "vxlapi.dll").c_str());
                                    if (dll)
                                        break;
                                }
                            }
                            RegCloseKey(hk);
                        }
                    }

                    if (!dll)
                    {
                        DWORD err = GetLastError();
                        std::fprintf(stderr,
                                     "[vector] vxlapi: cannot load vxlapi64.dll or vxlapi.dll "
                                     "(GetLastError=%lu)\n",
                                     err);
                        std::fprintf(stderr,
                                     "[vector] hint: install the Vector XL Driver Library "
                                     "(included with Vector Driver Setup)\n");
                        return false;
                    }

                    std::fprintf(stderr, "[vector] vxlapi: DLL loaded successfully\n");

                    auto get = [&](const char* name) { return GetProcAddress(dll, name); };

                    pOpenDriver = (fn_xlOpenDriver) get("xlOpenDriver");
                    pCloseDriver = (fn_xlCloseDriver) get("xlCloseDriver");
                    pGetDriverConfig = (fn_xlGetDriverConfig) get("xlGetDriverConfig");
                    pGetErrorString = (fn_xlGetErrorString) get("xlGetErrorString");
                    pOpenPort = (fn_xlOpenPort) get("xlOpenPort");
                    pClosePort = (fn_xlClosePort) get("xlClosePort");
                    pActivateChannel = (fn_xlActivateChannel) get("xlActivateChannel");
                    pDeactivateChannel = (fn_xlDeactivateChannel) get("xlDeactivateChannel");
                    pCanSetChannelBitrate = (fn_xlCanSetChannelBitrate) get("xlCanSetChannelBitrate");
                    pCanSetChannelOutput = (fn_xlCanSetChannelOutput) get("xlCanSetChannelOutput");
                    pCanTransmit = (fn_xlCanTransmit) get("xlCanTransmit");
                    pReceive = (fn_xlReceive) get("xlReceive");
                    pSetNotification = (fn_xlSetNotification) get("xlSetNotification");
                    pFlushReceiveQueue = (fn_xlFlushReceiveQueue) get("xlFlushReceiveQueue");

                    if (!pOpenDriver || !pCloseDriver || !pGetDriverConfig || !pOpenPort || !pClosePort || !pActivateChannel || !pDeactivateChannel || !pCanTransmit || !pReceive)
                    {
                        std::fprintf(stderr,
                                     "[vector] vxlapi: DLL loaded but required functions "
                                     "not found (wrong DLL version?)\n");
                        FreeLibrary(dll);
                        dll = nullptr;
                        return false;
                    }

                    return true;
                }

                bool ensure_driver_open()
                {
                    if (!loaded() && !load())
                        return false;
                    if (!driver_open)
                    {
                        XLstatus s = pOpenDriver();
                        if (s != XL_SUCCESS)
                        {
                            std::fprintf(stderr, "[vector] vxlapi: xlOpenDriver failed: %s (%ld)\n", error_string(s), s);
                            return false;
                        }
                        driver_open = true;
                        std::fprintf(stderr, "[vector] vxlapi: driver opened\n");
                    }
                    return true;
                }

                const char* error_string(XLstatus s)
                {
                    if (pGetErrorString)
                        return pGetErrorString(s);
                    return "unknown error";
                }
            };

            inline api& get_api()
            {
                static api instance;
                return instance;
            }

        } // namespace xlapi
    } // namespace vector

    struct vector_xl
    {
        vector::xlapi::XLportHandle port_{-1};
        vector::xlapi::XLaccess channel_mask_{0};
        vector::xlapi::XLaccess permission_mask_{0};
        HANDLE rx_event_{nullptr};
        bool open_{false};
        uint8_t channel_index_{0};

        static bool debug()
        {
            return std::getenv("JCAN_DEBUG") != nullptr;
        }

        [[nodiscard]] result<> open(const std::string& port, slcan_bitrate bitrate = slcan_bitrate::s6, [[maybe_unused]] unsigned baud = 0)
        {
            if (open_)
                return std::unexpected(error_code::already_open);

            auto& xl = vector::xlapi::get_api();
            if (!xl.ensure_driver_open())
            {
                std::fprintf(stderr,
                             "[vector] vxlapi: cannot load vxlapi64.dll / vxlapi.dll\n"
                             "[vector] hint: install the Vector XL Driver Library "
                             "(included with Vector Driver Setup)\n");
                return std::unexpected(error_code::port_open_failed);
            }

            unsigned ch_idx = 0;
            if (port.starts_with("xl:"))
            {
                ch_idx = static_cast<unsigned>(std::atoi(port.c_str() + 3));
            }
            else if (auto pos = port.find(':'); pos != std::string::npos)
            {
                ch_idx = static_cast<unsigned>(std::atoi(port.substr(pos + 1).c_str()));
            }
            else
            {
                ch_idx = static_cast<unsigned>(std::atoi(port.c_str()));
            }
            channel_index_ = static_cast<uint8_t>(ch_idx);

            vector::xlapi::XLdriverConfig config{};
            auto s = xl.pGetDriverConfig(&config);
            if (s != vector::xlapi::XL_SUCCESS)
            {
                if (debug())
                    std::fprintf(stderr, "[vector] vxlapi: xlGetDriverConfig failed: %s (%ld)\n", xl.error_string(s), s);
                return std::unexpected(error_code::port_not_found);
            }

            bool found = false;
            for (unsigned i = 0; i < config.channelCount; ++i)
            {
                auto& ch = config.channel[i];
                if (ch.channelIndex == ch_idx)
                {
                    channel_mask_ = ch.channelMask;
                    found = true;
                    if (debug())
                        std::fprintf(stderr, "[vector] vxlapi: found channel %u (%s) mask=0x%llX\n", ch_idx, ch.name, ch.channelMask);
                    break;
                }
            }
            if (!found)
            {
                if (debug())
                    std::fprintf(stderr, "[vector] vxlapi: channel index %u not found in driver config\n", ch_idx);
                return std::unexpected(error_code::port_not_found);
            }

            permission_mask_ = channel_mask_;
            char app_name[] = "jcan";
            s = xl.pOpenPort(&port_, app_name, channel_mask_, &permission_mask_, 256, vector::xlapi::XL_INTERFACE_VERSION_V3, vector::xlapi::XL_BUS_TYPE_CAN);
            if (s != vector::xlapi::XL_SUCCESS)
            {
                std::fprintf(stderr, "[vector] vxlapi: xlOpenPort failed: %s (%ld)\n", xl.error_string(s), s);
                return std::unexpected(error_code::port_open_failed);
            }

            static constexpr unsigned long bitrate_bps_map[] = {
                10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
            };
            unsigned long br_bps = bitrate_bps_map[static_cast<unsigned>(bitrate) % (sizeof(bitrate_bps_map) / sizeof(bitrate_bps_map[0]))];

            if (permission_mask_ & channel_mask_)
            {
                if (xl.pCanSetChannelOutput)
                {
                    s = xl.pCanSetChannelOutput(port_, channel_mask_, vector::xlapi::XL_OUTPUT_MODE_NORMAL);
                    if (s != vector::xlapi::XL_SUCCESS && debug())
                        std::fprintf(stderr, "[vector] vxlapi: xlCanSetChannelOutput failed: %s\n", xl.error_string(s));
                }
                if (xl.pCanSetChannelBitrate)
                {
                    s = xl.pCanSetChannelBitrate(port_, channel_mask_, br_bps);
                    if (s != vector::xlapi::XL_SUCCESS && debug())
                        std::fprintf(stderr, "[vector] vxlapi: xlCanSetChannelBitrate(%lu) failed: %s\n", br_bps, xl.error_string(s));
                }
            }
            else
            {
                if (debug())
                    std::fprintf(stderr,
                                 "[vector] vxlapi: no init access on channel %u, using "
                                 "existing bus config\n",
                                 ch_idx);
            }

            // Set up RX notification event
            if (xl.pSetNotification)
            {
                s = xl.pSetNotification(port_, &rx_event_, 1);
                if (s != vector::xlapi::XL_SUCCESS && debug())
                    std::fprintf(stderr, "[vector] vxlapi: xlSetNotification failed: %s\n", xl.error_string(s));
            }

            // Flush stale data
            if (xl.pFlushReceiveQueue)
            {
                xl.pFlushReceiveQueue(port_);
            }

            s = xl.pActivateChannel(port_, channel_mask_, vector::xlapi::XL_BUS_TYPE_CAN, vector::xlapi::XL_ACTIVATE_RESET_CLOCK);
            if (s != vector::xlapi::XL_SUCCESS)
            {
                std::fprintf(stderr, "[vector] vxlapi: xlActivateChannel failed: %s (%ld)\n", xl.error_string(s), s);
                xl.pClosePort(port_);
                port_ = -1;
                return std::unexpected(error_code::port_open_failed);
            }

            open_ = true;
            if (debug())
                std::fprintf(stderr, "[vector] vxlapi: opened channel %u, bitrate %lu bps\n", ch_idx, br_bps);
            return {};
        }

        [[nodiscard]] result<> close()
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& xl = vector::xlapi::get_api();
            xl.pDeactivateChannel(port_, channel_mask_);
            xl.pClosePort(port_);
            port_ = -1;
            channel_mask_ = 0;
            permission_mask_ = 0;
            if (rx_event_)
            {
                CloseHandle(rx_event_);
                rx_event_ = nullptr;
            }
            open_ = false;
            if (debug())
                std::fprintf(stderr, "[vector] vxlapi: closed\n");
            return {};
        }

        [[nodiscard]] result<> send(const can_frame& frame)
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& xl = vector::xlapi::get_api();

            vector::xlapi::XLevent evt{};
            evt.tag = static_cast<unsigned char>(vector::xlapi::XL_TRANSMIT_MSG);
            auto& msg = evt.tagData.msg;

            msg.id = frame.id;
            if (frame.extended)
                msg.id |= vector::xlapi::XL_CAN_EXT_MSG_ID;
            msg.dlc = frame.dlc;
            if (frame.rtr)
                msg.flags |= static_cast<unsigned short>(vector::xlapi::XL_CAN_MSG_FLAG_REMOTE_FRAME);

            uint8_t len = std::min(frame.dlc, static_cast<uint8_t>(8));
            std::memcpy(msg.data, frame.data.data(), len);

            unsigned int count = 1;
            auto s = xl.pCanTransmit(port_, channel_mask_, &count, &evt);
            if (s != vector::xlapi::XL_SUCCESS)
            {
                if (debug())
                    std::fprintf(stderr, "[vector] vxlapi: xlCanTransmit failed: %s\n", xl.error_string(s));
                return std::unexpected(error_code::write_error);
            }

            if (debug())
            {
                std::fprintf(stderr, "[vector] vxlapi: TX id=0x%X dlc=%u ext=%d", frame.id, frame.dlc, frame.extended);
                for (uint8_t i = 0; i < std::min(len, uint8_t{8}); ++i) std::fprintf(stderr, " %02X", frame.data[i]);
                std::fprintf(stderr, "\n");
            }

            return {};
        }

        [[nodiscard]] result<std::optional<can_frame>> recv(unsigned timeout_ms = 100)
        {
            auto batch = recv_many(timeout_ms);
            if (!batch)
                return std::unexpected(batch.error());
            if (batch->empty())
                return std::optional<can_frame>{std::nullopt};
            return std::optional<can_frame>{batch->front()};
        }

        [[nodiscard]] result<std::vector<can_frame>> recv_many(unsigned timeout_ms = 100)
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& xl = vector::xlapi::get_api();
            std::vector<can_frame> frames;

            if (rx_event_)
            {
                DWORD wait_result = WaitForSingleObject(rx_event_, timeout_ms);
                if (wait_result == WAIT_TIMEOUT)
                    return frames;
            }

            while (true)
            {
                vector::xlapi::XLevent evt{};
                unsigned int count = 1;
                auto s = xl.pReceive(port_, &count, &evt);

                if (s == vector::xlapi::XL_ERR_QUEUE_IS_EMPTY)
                    break;
                if (s != vector::xlapi::XL_SUCCESS)
                {
                    if (debug())
                        std::fprintf(stderr, "[vector] vxlapi: xlReceive error: %s\n", xl.error_string(s));
                    break;
                }

                if (evt.tag == vector::xlapi::XL_RECEIVE_MSG)
                {
                    auto& msg = evt.tagData.msg;

                    if (msg.flags & vector::xlapi::XL_CAN_MSG_FLAG_TX_COMPLETED)
                        continue;
                    if (msg.flags & vector::xlapi::XL_CAN_MSG_FLAG_ERROR_FRAME)
                        continue;

                    can_frame f{};
                    f.timestamp = can_frame::clock::now();
                    f.extended = (msg.id & vector::xlapi::XL_CAN_EXT_MSG_ID) != 0;
                    f.id = msg.id & 0x1FFFFFFF;
                    if (!f.extended)
                        f.id &= 0x7FF;
                    f.dlc = static_cast<uint8_t>(msg.dlc & 0x0F);
                    f.rtr = (msg.flags & vector::xlapi::XL_CAN_MSG_FLAG_REMOTE_FRAME) != 0;

                    uint8_t len = std::min(f.dlc, static_cast<uint8_t>(8));
                    std::memcpy(f.data.data(), msg.data, len);

                    if (debug())
                    {
                        std::fprintf(stderr, "[vector] vxlapi: RX id=0x%X dlc=%u ext=%d", f.id, f.dlc, f.extended);
                        for (uint8_t i = 0; i < std::min(len, uint8_t{8}); ++i) std::fprintf(stderr, " %02X", f.data[i]);
                        std::fprintf(stderr, "\n");
                    }

                    frames.push_back(f);
                }
            }

            return frames;
        }
    };

} // namespace jcan

#endif // _WIN32

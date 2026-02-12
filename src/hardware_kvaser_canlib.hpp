#pragma once

#ifdef _WIN32

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace jcan
{

    namespace canlib
    {

        using canStatus = int;
        inline constexpr canStatus canOK = 0;
        inline constexpr canStatus canERR_NOMSG = -2;
        inline constexpr canStatus canERR_NOTFOUND = -3;
        inline constexpr canStatus canERR_PARAM = -1;
        inline constexpr canStatus canERR_INVHANDLE = -10;

        inline constexpr int canOPEN_ACCEPT_VIRTUAL = 0x0020;
        inline constexpr int canOPEN_REQUIRE_INIT_ACCESS = 0x0100;

        inline constexpr unsigned int canMSG_RTR = 0x0001;
        inline constexpr unsigned int canMSG_STD = 0x0002;
        inline constexpr unsigned int canMSG_EXT = 0x0004;
        inline constexpr unsigned int canMSG_ERROR_FRAME = 0x0020;
        inline constexpr unsigned int canMSGERR_OVERRUN = 0x0600;

        inline constexpr unsigned int canFDMSG_FDF = 0x010000;
        inline constexpr unsigned int canFDMSG_BRS = 0x020000;
        inline constexpr unsigned int canFDMSG_ESI = 0x040000;

        inline constexpr int canDRIVER_NORMAL = 4;

        inline constexpr int canBITRATE_10K = -9;
        inline constexpr int canBITRATE_50K = -7;
        inline constexpr int canBITRATE_62K = -6;
        inline constexpr int canBITRATE_83K = -5;
        inline constexpr int canBITRATE_100K = -4;
        inline constexpr int canBITRATE_125K = -3;
        inline constexpr int canBITRATE_250K = -2;
        inline constexpr int canBITRATE_500K = -1;
        inline constexpr int canBITRATE_1M = -8;

        using canHandle = int;
        inline constexpr canHandle canINVALID_HANDLE = -1;

        using fn_canInitializeLibrary = void(__stdcall*)();
        using fn_canUnloadLibrary = canStatus(__stdcall*)();
        using fn_canGetNumberOfChannels = canStatus(__stdcall*)(int* channelCount);
        using fn_canGetChannelData = canStatus(__stdcall*)(int channel, int item, void* buffer, size_t bufsize);
        using fn_canOpenChannel = canHandle(__stdcall*)(int channel, int flags);
        using fn_canClose = canStatus(__stdcall*)(canHandle hnd);
        using fn_canSetBusParams = canStatus(__stdcall*)(canHandle hnd, long freq, unsigned int tseg1, unsigned int tseg2, unsigned int sjw, unsigned int noSamp,
                                                         unsigned int syncmode);
        using fn_canBusOn = canStatus(__stdcall*)(canHandle hnd);
        using fn_canBusOff = canStatus(__stdcall*)(canHandle hnd);
        using fn_canWrite = canStatus(__stdcall*)(canHandle hnd, long id, void* msg, unsigned int dlc, unsigned int flag);
        using fn_canRead = canStatus(__stdcall*)(canHandle hnd, long* id, void* msg, unsigned int* dlc, unsigned int* flag, unsigned long* time);
        using fn_canReadWait = canStatus(__stdcall*)(canHandle hnd, long* id, void* msg, unsigned int* dlc, unsigned int* flag, unsigned long* time, unsigned long timeout);
        using fn_canSetBusOutputControl = canStatus(__stdcall*)(canHandle hnd, unsigned int drivertype);
        using fn_canGetErrorText = canStatus(__stdcall*)(canStatus err, char* buf, unsigned int bufsiz);

        inline constexpr int canCHANNELDATA_CHANNEL_NAME = 13;
        inline constexpr int canCHANNELDATA_DEVDESCR_ASCII = 26;
        inline constexpr int canCHANNELDATA_CHAN_NO_ON_CARD = 7;
        inline constexpr int canCHANNELDATA_CARD_NUMBER = 14;
        inline constexpr int canCHANNELDATA_CARD_UPC_NO = 3;
        inline constexpr int canCHANNELDATA_CARD_SERIAL_NO = 8;
        inline constexpr int canCHANNELDATA_CHANNEL_FLAGS = 4;
        inline constexpr unsigned int canCHANNEL_IS_OPEN = 0x01;
        inline constexpr unsigned int canCHANNEL_IS_CANFD = 0x02;
        inline constexpr unsigned int canCHANNEL_IS_LIN = 0x10;
        inline constexpr unsigned int canCHANNEL_IS_VIRTUAL = 0x20;

        struct canlib_api
        {
            HMODULE dll{nullptr};

            fn_canInitializeLibrary canInitializeLibrary{nullptr};
            fn_canUnloadLibrary canUnloadLibrary{nullptr};
            fn_canGetNumberOfChannels canGetNumberOfChannels{nullptr};
            fn_canGetChannelData canGetChannelData{nullptr};
            fn_canOpenChannel canOpenChannel{nullptr};
            fn_canClose canClose{nullptr};
            fn_canSetBusParams canSetBusParams{nullptr};
            fn_canBusOn canBusOn{nullptr};
            fn_canBusOff canBusOff{nullptr};
            fn_canWrite canWrite{nullptr};
            fn_canRead canRead{nullptr};
            fn_canReadWait canReadWait{nullptr};
            fn_canSetBusOutputControl canSetBusOutputControl{nullptr};
            fn_canGetErrorText canGetErrorText{nullptr};

            bool load()
            {
                dll = LoadLibraryA("canlib32.dll");
                if (!dll)
                    return false;

                auto get = [&](const char* name) { return GetProcAddress(dll, name); };

                canInitializeLibrary = reinterpret_cast<fn_canInitializeLibrary>(get("canInitializeLibrary"));
                canUnloadLibrary = reinterpret_cast<fn_canUnloadLibrary>(get("canUnloadLibrary"));
                canGetNumberOfChannels = reinterpret_cast<fn_canGetNumberOfChannels>(get("canGetNumberOfChannels"));
                canGetChannelData = reinterpret_cast<fn_canGetChannelData>(get("canGetChannelData"));
                canOpenChannel = reinterpret_cast<fn_canOpenChannel>(get("canOpenChannel"));
                canClose = reinterpret_cast<fn_canClose>(get("canClose"));
                canSetBusParams = reinterpret_cast<fn_canSetBusParams>(get("canSetBusParams"));
                canBusOn = reinterpret_cast<fn_canBusOn>(get("canBusOn"));
                canBusOff = reinterpret_cast<fn_canBusOff>(get("canBusOff"));
                canWrite = reinterpret_cast<fn_canWrite>(get("canWrite"));
                canRead = reinterpret_cast<fn_canRead>(get("canRead"));
                canReadWait = reinterpret_cast<fn_canReadWait>(get("canReadWait"));
                canSetBusOutputControl = reinterpret_cast<fn_canSetBusOutputControl>(get("canSetBusOutputControl"));
                canGetErrorText = reinterpret_cast<fn_canGetErrorText>(get("canGetErrorText"));

                if (!canInitializeLibrary || !canOpenChannel || !canClose || !canSetBusParams || !canBusOn || !canBusOff || !canWrite || !canReadWait || !canSetBusOutputControl)
                {
                    FreeLibrary(dll);
                    dll = nullptr;
                    return false;
                }

                canInitializeLibrary();
                return true;
            }

            void unload()
            {
                if (dll)
                {
                    if (canUnloadLibrary)
                        canUnloadLibrary();
                    FreeLibrary(dll);
                    dll = nullptr;
                }
            }

            bool loaded() const
            {
                return dll != nullptr;
            }
        };

        inline canlib_api& api()
        {
            static canlib_api instance;
            return instance;
        }

        inline bool ensure_loaded()
        {
            auto& a = api();
            if (a.loaded())
                return true;
            return a.load();
        }

        inline int slcan_bitrate_to_canlib(slcan_bitrate br)
        {
            switch (br)
            {
            case slcan_bitrate::s0:
                return canBITRATE_10K;
            case slcan_bitrate::s1:
                return canBITRATE_10K; // 20K not available, use 10K
            case slcan_bitrate::s2:
                return canBITRATE_50K;
            case slcan_bitrate::s3:
                return canBITRATE_100K;
            case slcan_bitrate::s4:
                return canBITRATE_125K;
            case slcan_bitrate::s5:
                return canBITRATE_250K;
            case slcan_bitrate::s6:
                return canBITRATE_500K;
            case slcan_bitrate::s7:
                return canBITRATE_500K; // 800K not available
            case slcan_bitrate::s8:
                return canBITRATE_1M;
            }
            return canBITRATE_500K;
        }

        struct channel_info
        {
            int canlib_channel;
            std::string name;
            std::string device_name;
            int channel_on_card;
        };

        inline std::vector<channel_info> enumerate_channels()
        {
            std::vector<channel_info> out;
            bool dbg = std::getenv("JCAN_DEBUG") != nullptr;

            if (!ensure_loaded())
            {
                if (dbg)
                    std::fprintf(stderr, "[canlib] canlib32.dll not loaded\n");
                return out;
            }

            auto& a = api();
            int count = 0;
            if (!a.canGetNumberOfChannels || a.canGetNumberOfChannels(&count) != canOK)
            {
                if (dbg)
                    std::fprintf(stderr, "[canlib] canGetNumberOfChannels failed\n");
                return out;
            }

            if (dbg)
                std::fprintf(stderr, "[canlib] canGetNumberOfChannels = %d\n", count);

            for (int i = 0; i < count; ++i)
            {
                auto test_hnd = a.canOpenChannel(i, 0);
                if (test_hnd < 0)
                {
                    if (dbg)
                        std::fprintf(stderr, "[canlib] ch %d: canOpenChannel probe failed (%d), skipping\n", i, test_hnd);
                    continue;
                }
                a.canClose(test_hnd);
                if (dbg)
                    std::fprintf(stderr, "[canlib] ch %d: probe OK (hardware present)\n", i);

                channel_info ci;
                ci.canlib_channel = i;

                char name_buf[256]{};
                if (a.canGetChannelData && a.canGetChannelData(i, canCHANNELDATA_DEVDESCR_ASCII, name_buf, sizeof(name_buf)) == canOK)
                {
                    ci.device_name = name_buf;
                }

                char chan_name[256]{};
                if (a.canGetChannelData && a.canGetChannelData(i, canCHANNELDATA_CHANNEL_NAME, chan_name, sizeof(chan_name)) == canOK)
                {
                    ci.name = chan_name;
                }

                int ch_on_card = 0;
                if (a.canGetChannelData && a.canGetChannelData(i, canCHANNELDATA_CHAN_NO_ON_CARD, &ch_on_card, sizeof(ch_on_card)) == canOK)
                {
                    ci.channel_on_card = ch_on_card;
                }

                if (ci.device_name.empty())
                    ci.device_name = "Kvaser";
                out.push_back(std::move(ci));
            }

            return out;
        }

    } // namespace canlib

    struct kvaser_canlib
    {
        canlib::canHandle hnd_{canlib::canINVALID_HANDLE};
        bool open_{false};
        int canlib_channel_{-1};

        static bool debug()
        {
            return std::getenv("JCAN_DEBUG") != nullptr;
        }

        [[nodiscard]] result<> open(const std::string& port, slcan_bitrate bitrate = slcan_bitrate::s6, [[maybe_unused]] unsigned baud = 0)
        {
            if (open_)
                return std::unexpected(error_code::already_open);

            if (!canlib::ensure_loaded())
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canlib32.dll not found\n");
                return std::unexpected(error_code::port_open_failed);
            }

            canlib_channel_ = 0;
            if (auto pos = port.find(':'); pos != std::string::npos)
            {
                canlib_channel_ = std::atoi(port.substr(pos + 1).c_str());
            }
            else
            {
                try
                {
                    canlib_channel_ = std::stoi(port);
                }
                catch (...)
                {
                    canlib_channel_ = 0;
                }
            }

            auto& a = canlib::api();

            hnd_ = a.canOpenChannel(canlib_channel_, canlib::canOPEN_ACCEPT_VIRTUAL | canlib::canOPEN_REQUIRE_INIT_ACCESS);
            if (hnd_ < 0)
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canOpenChannel(%d) failed: %d\n", canlib_channel_, hnd_);
                hnd_ = canlib::canINVALID_HANDLE;
                return std::unexpected(error_code::port_open_failed);
            }

            int canlib_bitrate = canlib::slcan_bitrate_to_canlib(bitrate);
            auto stat = a.canSetBusParams(hnd_, canlib_bitrate, 0, 0, 0, 0, 0);
            if (stat != canlib::canOK)
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canSetBusParams failed: %d\n", stat);
                a.canClose(hnd_);
                hnd_ = canlib::canINVALID_HANDLE;
                return std::unexpected(error_code::port_config_failed);
            }

            if (a.canSetBusOutputControl)
            {
                a.canSetBusOutputControl(hnd_, canlib::canDRIVER_NORMAL);
            }

            stat = a.canBusOn(hnd_);
            if (stat != canlib::canOK)
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canBusOn failed: %d\n", stat);
                a.canClose(hnd_);
                hnd_ = canlib::canINVALID_HANDLE;
                return std::unexpected(error_code::port_open_failed);
            }

            open_ = true;
            if (debug())
                std::fprintf(stderr, "[kvaser-canlib] opened channel %d\n", canlib_channel_);
            return {};
        }

        [[nodiscard]] result<> close()
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& a = canlib::api();
            a.canBusOff(hnd_);
            a.canClose(hnd_);
            hnd_ = canlib::canINVALID_HANDLE;
            open_ = false;

            if (debug())
                std::fprintf(stderr, "[kvaser-canlib] closed\n");
            return {};
        }

        [[nodiscard]] result<> send(const can_frame& frame)
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& a = canlib::api();
            uint8_t payload_len = frame_payload_len(frame);

            unsigned int flags = 0;
            if (frame.extended)
                flags |= canlib::canMSG_EXT;
            else
                flags |= canlib::canMSG_STD;
            if (frame.rtr)
                flags |= canlib::canMSG_RTR;

            uint8_t buf[64]{};
            std::memcpy(buf, frame.data.data(), payload_len);

            auto stat = a.canWrite(hnd_, static_cast<long>(frame.id), buf, payload_len, flags);
            if (stat != canlib::canOK)
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canWrite failed: %d\n", stat);
                return std::unexpected(error_code::write_error);
            }
            return {};
        }

        [[nodiscard]] result<std::optional<can_frame>> recv(unsigned timeout_ms = 100)
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            auto& a = canlib::api();
            long id = 0;
            uint8_t buf[64]{};
            unsigned int dlc = 0;
            unsigned int flags = 0;
            unsigned long timestamp = 0;

            auto stat = a.canReadWait(hnd_, &id, buf, &dlc, &flags, &timestamp, timeout_ms);

            if (stat == canlib::canERR_NOMSG)
            {
                return std::optional<can_frame>{std::nullopt};
            }
            if (stat != canlib::canOK)
            {
                if (debug())
                    std::fprintf(stderr, "[kvaser-canlib] canReadWait failed: %d\n", stat);
                return std::unexpected(error_code::read_error);
            }

            can_frame f{};
            f.timestamp = can_frame::clock::now();
            f.id = static_cast<uint32_t>(id);
            f.extended = (flags & canlib::canMSG_EXT) != 0;
            f.rtr = (flags & canlib::canMSG_RTR) != 0;
            f.error = (flags & canlib::canMSG_ERROR_FRAME) != 0;
            f.dlc = static_cast<uint8_t>(dlc);
            f.fd = (flags & canlib::canFDMSG_FDF) != 0;
            f.brs = (flags & canlib::canFDMSG_BRS) != 0;

            uint8_t payload_len = frame_payload_len(f);
            std::memcpy(f.data.data(), buf, payload_len);

            return std::optional<can_frame>{f};
        }

        [[nodiscard]] result<std::vector<can_frame>> recv_many(unsigned timeout_ms = 100)
        {
            if (!open_)
                return std::unexpected(error_code::not_open);

            std::vector<can_frame> frames;
            auto& a = canlib::api();

            {
                auto r = recv(timeout_ms);
                if (!r)
                    return std::unexpected(r.error());
                if (r->has_value())
                    frames.push_back(r->value());
            }

            for (int i = 0; i < 1000; ++i)
            {
                long id = 0;
                uint8_t buf[64]{};
                unsigned int dlc = 0;
                unsigned int flags = 0;
                unsigned long timestamp = 0;

                auto stat = a.canRead ? a.canRead(hnd_, &id, buf, &dlc, &flags, &timestamp) : a.canReadWait(hnd_, &id, buf, &dlc, &flags, &timestamp, 0);

                if (stat == canlib::canERR_NOMSG)
                    break;
                if (stat != canlib::canOK)
                    break;

                can_frame f{};
                f.timestamp = can_frame::clock::now();
                f.id = static_cast<uint32_t>(id);
                f.extended = (flags & canlib::canMSG_EXT) != 0;
                f.rtr = (flags & canlib::canMSG_RTR) != 0;
                f.error = (flags & canlib::canMSG_ERROR_FRAME) != 0;
                f.dlc = static_cast<uint8_t>(dlc);
                f.fd = (flags & canlib::canFDMSG_FDF) != 0;
                f.brs = (flags & canlib::canFDMSG_BRS) != 0;

                uint8_t payload_len = frame_payload_len(f);
                std::memcpy(f.data.data(), buf, payload_len);

                frames.push_back(f);
            }

            return frames;
        }
    };

} // namespace jcan

#endif // _WIN32

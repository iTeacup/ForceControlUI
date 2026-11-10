#include <winsock2.h>
#include "Nokov/NokovSDKClient.h"
#include "CPSHandler.h"
#include "ConsoleControl.h"
#include <memory>
#include "FrameQueue.h"
#include "NokovCfgParser.h"
#include <thread>
#include <iostream>
int main(int argc, char** argv)
{
    NokovCfgParser cfg;
    const auto path =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::absolute(argv[0]).parent_path() / "nokovcfg.json";
    if (!cfg.LoadCfg(path))
        return 1;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
        return 2;
    ConsoleControl console;
    Handler handler;
    handler.api = CCPSAPI::CreateAPI();
    if (handler.api->Init(E_CPS_TYPE_DEVICE, nokov::Device, 0, cfg.busIp.c_str(), cfg.busPort, "", 0, &handler))
    {
        handler.api->Release();
        WSACleanup();
        return 3;
    }
    FrameQueue queue;
    queue.capacity = cfg.queueCapacity;
    auto client = std::make_unique<NokovSDKClient>();
    std::cout << "NOKOV server started. Enter q to exit." << std::endl;
    bool connected = false;
    using Clock = std::chrono::steady_clock;
    Clock::time_point nextConnect{}, nextStatus{};
    while (!console.StopRequested())
    {
        auto now = Clock::now();
        if (connected && !handler.desired)
        {
            client->Uninitialize();
            connected = false;
            std::lock_guard<std::mutex> lock(queue.mutex);
            queue.frames.clear();
            queue.status.connected = 0;
        }
        if (!connected && handler.desired && now >= nextConnect)
        {
            queue.Reset();
            int result = client->Initialize(cfg.serverIp.data());
            if (!result)
            {
                sDataDescriptions* descriptions = nullptr;
                int descriptionResult = client->GetDataDescriptions(&descriptions);
                {
                    std::lock_guard<std::mutex> lock(queue.mutex);
                    queue.names.Load(descriptionResult == 0 ? descriptions : nullptr);
                }
                if (descriptions)
                    client->FreeDataDescriptions(descriptions);
            }
            if (!result)
                result = client->SetDataCallback(FrameQueue::Callback, &queue);
            connected = result == 0;
            {
                std::lock_guard<std::mutex> lock(queue.mutex);
                queue.status.connected = connected;
                queue.status.error = result;
            }
            if (!connected)
                client->Uninitialize();
            nextConnect = Clock::now() + std::chrono::seconds(2);
        }
        nokov::Frame frame;
        bool ready = false;
        if (connected && handler.online)
        {
            std::lock_guard<std::mutex> lock(queue.mutex);
            if (!queue.frames.empty())
            {
                frame = std::move(queue.frames.front());
                queue.frames.pop_front();
                ready = true;
            }
        }
        if (ready)
        {
            auto bytes = nokov::Encode(frame);
            if (handler.api->SendDeviceMsg(-1, nokov::FrameMessage, bytes.data(), static_cast<uint32_t>(bytes.size())))
            {
                std::lock_guard<std::mutex> lock(queue.mutex);
                ++queue.status.failed;
            }
        }
        if (now >= nextStatus)
        {
            nokov::Status status;
            {
                std::lock_guard<std::mutex> lock(queue.mutex);
                status = queue.status;
            }
            if (handler.online)
                handler.api->SendDeviceMsg(-1, nokov::StatusMessage, reinterpret_cast<const char*>(&status),
                                           sizeof(status));
            nextStatus = now + std::chrono::seconds(1);
        }
        if (!ready)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    client->SetDataCallback(nullptr, nullptr);
    if (connected)
        client->Uninitialize();
    client.reset(); // Destroy SDK while Winsock and callback storage are still alive.
    handler.desired = false;
    handler.api->Release();
    handler.api = nullptr;
    WSACleanup();
    return 0;
}

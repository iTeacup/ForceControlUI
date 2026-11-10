#pragma once
#include <windows.h>
#include <string>

// Console close handlers run on a Windows thread. Keep the process alive until
// the main thread has stopped SDK callbacks and released the bus API.
class ConsoleControl
{
public:
    ConsoleControl()
    {
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        cleanupEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        SetConsoleCtrlHandler(Handle, TRUE);
    }
    ~ConsoleControl()
    {
        SetEvent(cleanupEvent);
        SetConsoleCtrlHandler(Handle, FALSE);
        // Process-lifetime event handles: a close callback may still be waiting.
        // Windows releases both handles when the process exits.
    }
    bool StopRequested()
    {
        if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0)
            return true;
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(input, &mode))
        {
            DWORD available = 0;
            while (GetNumberOfConsoleInputEvents(input, &available) && available)
            {
                INPUT_RECORD record;
                DWORD read = 0;
                if (!ReadConsoleInputW(input, &record, 1, &read) || !read)
                    break;
                if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                    continue;
                const auto& key = record.Event.KeyEvent;
                if (key.wVirtualKeyCode == VK_RETURN)
                {
                    if (command == "q" || command == "Q")
                        return true;
                    command.clear();
                }
                else if (key.wVirtualKeyCode == VK_BACK)
                {
                    if (!command.empty())
                        command.pop_back();
                }
                else if (key.uChar.UnicodeChar > 0 && key.uChar.UnicodeChar < 128)
                {
                    command.push_back(static_cast<char>(key.uChar.UnicodeChar));
                }
            }
        }
        else
        {
            DWORD available = 0;
            const DWORD type = GetFileType(input);
            if (type == FILE_TYPE_PIPE)
            {
                if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr))
                    return true;
            }
            else if (type == FILE_TYPE_DISK)
                available = 1;
            if (available)
            {
                char c;
                DWORD read = 0;
                if (!ReadFile(input, &c, 1, &read, nullptr) || !read)
                    return true;
                if (c == '\n')
                {
                    if (command == "q" || command == "Q")
                        return true;
                    command.clear();
                }
                else if (c != '\r')
                    command.push_back(c);
            }
        }
        return false;
    }

private:
    inline static HANDLE stopEvent = nullptr, cleanupEvent = nullptr;
    std::string command;
    static BOOL WINAPI Handle(DWORD type)
    {
        switch (type)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            SetEvent(stopEvent);
            return TRUE;
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SetEvent(stopEvent);
            WaitForSingleObject(cleanupEvent, 4000);
            return TRUE;
        default:
            return FALSE;
        }
    }
};

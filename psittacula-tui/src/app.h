#ifndef APP_INCLUDED_H
#define APP_INCLUDED_H

#include "screen.h"
#include "keyboard.h"
#include "ai_client.h"
#include "exit_dialog.h"
#include "console_writer.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class ChatCommandDispatcher;

namespace tui {
    /// Application: contains main loop, manages keyboard input and screen output
    class App: public console::TextReceiver
    {
        public:
            App();
            ~App();

            App(const App&) = delete;
            App& operator=(const App&) = delete;

            void Run();

            /// Callback to send a chunk of LLM text response to output at a screen
            void SendText(const std::string txt, TextOrigin origin = TextOrigin::normal);

            void SetClient(std::unique_ptr<AiClient> &client);

            void SetCommandDispatcher(std::unique_ptr<ChatCommandDispatcher> &cmd_dispatcher);

            // Text receiver interface
            void WriteLine(const std::string &message, TextOrigin origin = TextOrigin::normal) override;
            void Write(const std::string &message, TextOrigin origin = TextOrigin::normal) override;
            void Clear() override;
            void Flush() override;

        private:
            void MainLoop();
            void ProcessCommand(const std::string &command);
            void ProcessQuery(const std::string &query);

            /// Worker body: runs the blocking query, reports result to UI state
            void ProcessQueryWorker(std::string query);

            /// Joins a finished worker without blocking the UI (non-blocking)
            void ReapFinishedQuery();

            /// Shows "working" status; call from UI thread each loop iteration
            void ShowQueryStatus();

            Screen m_screen;
            Keyboard m_keyboard;

            ExitDialog m_exit_dialog;

            std::unique_ptr<AiClient> m_client;
            std::unique_ptr<ChatCommandDispatcher> m_cmd_dispatcher;

            // Streaming query state: only the worker thread touches m_client
            std::thread m_query_thread;
            std::atomic<bool> m_query_running{false};
            std::mutex m_query_mutex;
            std::string m_query_result; // "ok", "cancelled", or "error: ..."
            bool m_query_status_shown = false;
    };
}

#endif
#ifndef PANEL_STATUS_INCLUDED_H
#define PANEL_STATUS_INCLUDED_H

#include "panel.h"
#include <string>

namespace tui {
    /// Status panel - displays status information at the top of the screen
    class PanelStatus : public Panel
    {
        public:
            explicit PanelStatus(Panel *parent = nullptr);
            ~PanelStatus() override = default;

            /// Sets the status message to display
            void SetStatus(const std::string &status);

            /// Clears the status (displays empty or default message)
            void Clear();

            /// Draws the status panel
            void Draw();

            void Refresh();

            void MoveSpinner();

            /// Updates the size of the status panel
            void UpdateSize(int width, int height);

        private:
            std::string m_status = "Ready";
            std::string m_prefix = "";
    };
}

#endif // PANEL_STATUS_INCLUDED_H
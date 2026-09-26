#ifndef PANEL_INCLUDED_H
#define PANEL_INCLUDED_H

namespace tui {
    /// Base panel
    class Panel
    {
        public:
            Panel(Panel *parent = nullptr)
                : m_parent(parent)
            { }

            virtual ~Panel() = default;

            void MoveCursorTo(int x, int y);

            void GetCursorPosition(int &x, int &y);

            void HideCursor(bool hide);

            void SetDimensions(int anchor_x, int anchor_y, int width, int heigth);

            virtual void OnUpdatedChild(Panel *updated_panel = nullptr)
            { }

            virtual void NotifyParentAboutChanges()
            {
                if(m_parent)
                    m_parent->OnUpdatedChild(this);
            }

        protected:
            int m_width = 10;
            int m_height = 10;
            int m_anchor_x = 0;
            int m_anchor_y = 0;

            Panel *m_parent = nullptr;
    };
}

#endif // PANEL_INCLUDED_H
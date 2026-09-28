#ifndef JSON_DIALOGUE_INCLUDED_H
#define JSON_DIALOGUE_INCLUDED_H

#include <string>

/// Autosaves the current dialogue JSON body to a log file after each
/// iteration of exchange with the AI (a user message round-trip, or a
/// single step of the tool-calls loop).
///
/// A single timestamped file is created lazily on the first call to Write()
/// and is then overwritten on every subsequent call, so the file on disk
/// always reflects the latest state of the dialogue for the current session.
///
///\see AiClientImpl, where an instance of this class is kept as a field
class JsonDialogue
{
    public:
        JsonDialogue() = default;

        ~JsonDialogue() = default;

        /// Writes (overwrites) the current dialogue JSON body to the session log file.
        /// Intended to be called after each iteration of the exchange with the AI.
        void Write(const std::string &json_body);

        /// Returns the path of the current session's log file
        /// (empty until the first call to Write)
        std::string GetFilePath() const;

        /// Forces the file path to be regenerated on the next Write() call
        /// (e.g. when the context is cleared and a fresh log makes sense)
        void Reset();

    private:
        // Lazily builds a timestamped file path inside the working dir logs directory
        void EnsureFilePath();

        std::string m_file_path;
};

#endif // JSON_DIALOGUE_INCLUDED_H

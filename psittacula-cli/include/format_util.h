#ifndef FORMAT_UTIL_H_INCLUDED
#define FORMAT_UTIL_H_INCLUDED

#include <sstream>
#include <cstring>
#include <array>
#include <istream>
#include <memory>

///\brief Class for filling strings with formatted arguments.
/// The format specifier is '%?'.
/// Arguments are output into string in the same way as into 'std::cout'.
/// It is possible to manipulate with format flags and settings for output via following methods:
/// Flags, Precision, Imbue, SetF, UnSetF (analogues of flags, precision, imbue, setf, and unsetf for std::ios_base)
/// The class is header-only.
/// Example:
///    Formatter formatter;
///    std::string result = formatter.Format("Num value: %?, string value: %?", 10.5, "xyz");
///
class Formatter
{
    public:
        Formatter()
           : m_ptr_locale(new std::locale()),
             m_flags(std::ios_base::skipws | std::ios_base::dec),
             m_precision(6)
        { }

        Formatter(const std::locale& loc,
                  std::ios_base::fmtflags flags = std::ios_base::skipws | std::ios_base::dec,
                  std::streamsize precision = 6)
           : m_ptr_locale(new std::locale(loc)),
             m_flags(flags),
             m_precision(precision)
        { }

        ~Formatter()
        { }

        ///\brief Generates string from char sequence and fills it with parameters.
        ///\param seq - pointer to sequence (for example, char*)
        ///\param args - list of arguments
        ///\return built string
        template<typename T, typename... Args>
        std::basic_string<T> Format(const T* seq, Args... args)
        {
            std::basic_string<T> str(seq);
            return Format(str, args...);
        }

        ///\brief Generates string from another string filled with parameters.
        ///\param str - initial string
        ///\param args - list of arguments
        ///\return built string
        template<typename T, typename... Args>
        std::basic_string<T> Format(const std::basic_string<T> &str, Args... args)
        {
            std::basic_stringstream<T> stream;
            AssignStreamSettings(stream);
            // Collect parameters (arguments string-values)
            const size_t args_sz = sizeof...(args);
            std::array<std::basic_string<T>, args_sz> parameter_values;
            GetOutputParameters(stream, parameter_values.begin(), args...);
            // Parse, insert parameters, and build final string
            size_t pos = 0;
            size_t last_pos = 0;
            size_t arg_counter = 0;
            size_t mask_len = std::strlen(SUBSTITUTE_MASK);
            while((pos = str.find(SUBSTITUTE_MASK, last_pos))!=str.npos)
            {
                if(pos > 0 && str.at(pos - 1)=='%') // Ignore screened '%%?'-value
                {
                    stream << str.substr(last_pos, pos - last_pos - 1) << SUBSTITUTE_MASK;
                    last_pos = (pos += mask_len);
                    continue;
                }
                stream << str.substr(last_pos, pos - last_pos);
                stream << (arg_counter < args_sz ? parameter_values[arg_counter++] : "?");
                last_pos = (pos += mask_len);
            }
            stream << str.substr(last_pos, str.size() - last_pos);
            return stream.str();
        }

        ///\brief No parameters string processing: returns the initial string
        ///\param str -- initial string
        template<typename T>
        std::string Format(const std::basic_string<T> &str)
        {
            return str;
        }

        /// Returns current formatting settings
        ///\return Formatting flags
        ///\see the method is analogue of std::ios_base::fags
        std::ios_base::fmtflags Flags() const
        {
            return m_flags;
        }

        /// Replaces current formatting settings with given ones
        ///\param flags - new formatting setting
        ///\return The formatting flags before the call to the function
        ///\see the method is analogue of std::ios_base::flags
        std::ios_base::fmtflags Flags(std::ios_base::fmtflags flags)
        {
            const std::ios_base::fmtflags old_flags = m_flags;
            m_flags = flags;
            return old_flags;
        }

        /// Sets the formatting flags identified by flags
        ///\param flags - new formatting setting
        ///\return The formatting flags before the call to the function
        ///\see the method is analogue of std::ios_base::setf
        std::ios_base::fmtflags SetF(std::ios_base::fmtflags flags)
        {
            const std::ios_base::fmtflags old_flags = m_flags;
            m_flags |= flags;
            return old_flags;
        }

        /// Clears the formatting flags under mask, and sets the cleared flags to those specified by flags
        ///\param flags - new formatting setting
        ///\param mask - defines which flags can be altered
        ///\return The formatting flags before the call to the function
        ///\see the method is analogue of std::ios_base::setf
        std::ios_base::fmtflags SetF(std::ios_base::fmtflags flags, std::ios_base::fmtflags mask)
        {
            const std::ios_base::fmtflags old_flags = m_flags;
            m_flags &= ~mask;
            m_flags |= (flags & mask);
            return old_flags;
        }

        /// Unsets the formatting flags identified by flags
        ///\param flags - formatting flags to unset
        ///\see the method is analogue of std::ios_base::unsetf
        void UnSetF(std::ios_base::fmtflags flags)
        {
            m_flags &= ~flags;
        }

        /// Associates locale to the formatter inner stream as the new locale object
        /// to be used with locale-sensitive operations
        ///\param loc - new locale for the formatter's inner stream
        ///\return The locale object associated with the stream before the call
        ///\see the method is analogue of std::ios_base::imbue
        std::locale Imbue(const std::locale& loc)
        {
            const std::locale old_locale = *m_ptr_locale;
            *m_ptr_locale = loc;
            return old_locale;
        }

        /// Returns the locale object currently associated with the formatter inner stream.
        ///\return The locale object
        ///\see the method is analogue of std::ios_base::getloc
        std::locale Getloc() const
        {
            return *m_ptr_locale;
        }

        /// Returns the current precision of formatter
        ///\return The precision value
        ///\see the method is analogue of std::ios_base::precision
        std::streamsize Precision() const
        {
            return m_precision;
        }

        /// Manages the precision (i.e. how many digits are generated) of floating point output
        ///\param - new precision setting
        ///\return The precision before the call to the function
        ///\see the method is analogue of std::ios_base::precision
        std::streamsize Precision(std::streamsize prec)
        {
            const std::streamsize old_prec = m_precision;
            m_precision = prec;
            return old_prec;
        }

        // Proxy class for the output via 'operator<<'
        // The class is used in 'Output'-method (see below)
        template<typename T>
        struct FWrapper
        {
            T t;
        };

        /// The helper-method for suppressing ambiguous overloading for output.
        /// Returns a proxy-object which wraps a real object.
        /// The proxy-object has its own operator<< which calls operator<< for the wrapped object directly, without SFINAE.
        /// For example,
        ///     SomeObject some_object;
        ///     Formatter formatter;
        ///     std::string result = formatter.format("String object value: %?", formatter.Output(some_object));
        /// In this case the 'some_object' value will be output to the string directly via operator<<.
        /// It is a simple way to avoid an ambiguous overloading, if occurs
        ///\param t - object for direct invoking operator<<
        ///\return The proxy object which can be output
        template<typename T>
        FWrapper<T> Output(T t)
        {
            FWrapper<T> w;
            w.t = t;
            return w;
        }

    private:
        // The format specifier
        const char *SUBSTITUTE_MASK = "%?";

        // Current locale for formatting
        std::unique_ptr<std::locale> m_ptr_locale;
        // Current set of flags for formatting
        std::ios_base::fmtflags m_flags;
        // Current precision for formatting of numeric values
        std::streamsize m_precision;

        // Assigns locale, precision and flags for the given stream
        template <typename Stream>
        void AssignStreamSettings(Stream &stream)
        {
            stream.precision(m_precision);
            stream.imbue(*m_ptr_locale);
            stream.flags(m_flags);
        }

        // Puts output value into an array at the iterator position
        // stream - stream to get a string value
        // it - current iterator of a dynamic array
        // t - current argument
        // args - other arguments
        template <typename Stream, typename T, typename It, typename... Args>
        void GetOutputParameters(Stream &stream, It it, T t, Args... args)
        {
            OutputValue(stream, t);
            *it = stream.str();
            stream.str("");
            GetOutputParameters(stream, ++it, args...);
        }

        // Puts the last output value into an array at the iterator position
        // stream - stream to get a string value
        // it - current iterator of a dynamic array
        // t - last argument
        template <typename Stream, typename T, typename It>
        void GetOutputParameters(Stream &stream, It it, T t)
        {
            OutputValue(stream, t);
            *it = stream.str();
            stream.str("");
        }

        // Outputs type which has an 'operator<<', to a string stream
        // stream - stream to get a string value
        // t - type value
        template<typename Stream, typename T,
                typename Output = decltype(std::cout << std::declval<T>())>
        void OutputValue(Stream &stream, const T &t)
        {
            stream << t;
        }

        // Outputs type which can be iterated, to a string stream
        // stream - stream to get a string value
        // t - type value
        template<typename Stream, typename T,
                typename It = typename T::const_iterator,
                typename Type = typename T::value_type,
                typename Begin = decltype(std::declval<T>().begin()),
                typename End = decltype(std::declval<T>().end())>
        void OutputValue(Stream &stream, const T &t)
        {
            stream << "[";
            if(t.empty())
            {
                stream << "]";
                return;
            }
            It last = --t.end();
            for(It it=t.begin(); it!=t.end(); ++it)
            {
                OutputValue(stream, *it);
                if(it!=last)
                    stream << ", ";
            }
            stream << "]";
        }

        // Outputs value for basic_string derivatives to a string stream
        // stream - stream to get a string value
        // str - string value
        template<typename Stream, typename T,
                typename Output = decltype(std::cout << std::declval<std::basic_string<T>>())>
        void OutputValue(Stream &stream, const std::basic_string<T> &str)
        {
            stream << str;
        }

        // Outputs pair-values in braces to a string stream
        // stream - stream to get a string value
        // value - pair value
        template<typename Stream, typename T, typename V>
        void OutputValue(Stream &stream, const std::pair<T,V> &value)
        {
            stream << '{';
            OutputValue(stream, value.first);
            stream << " : ";
            OutputValue(stream, value.second);
            stream << '}';
        }

        // Outputs bool-values to a string stream: as 'true' or 'false'
        template<typename Stream>
        void OutputValue(Stream &stream, bool b)
        {
            stream << (b ? "true" : "false");
        }

        // Outputs unknown type to a string stream as a '?'-character
        template<typename Stream>
        void OutputValue(Stream &stream, ...)
        {
            stream << "?";
        }
};

// Helper function for output proxy-object (FWrapper) via operator<<.
// See method 'Output' of Formatter-class
template<typename T>
std::ostream& operator<<(std::ostream& os, Formatter::FWrapper<T> fw)
{
    os << fw.t;
    return os;
}

#endif // FORMAT_UTIL_H_INCLUDED

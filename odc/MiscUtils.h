/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__MiscUtils__
#define __ODC__MiscUtils__

#include <odc/LoggerSeverity.h>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <ctime>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace odc::core
{

/// @brief concatenates a variable number of args with the << operator via a stringstream
/// @param t objects to be concatenated
/// @return concatenated string
template<typename... T>
auto toString(T&&... t) -> std::string
{
    std::stringstream ss;
    (void)std::initializer_list<int>{ (ss << t, 0)... };
    return ss.str();
}

// generates UUID string
inline std::string uuid()
{
    boost::uuids::random_generator gen;
    boost::uuids::uuid u = gen();
    return boost::uuids::to_string(u);
}

// generates UUID and returns its hash
inline std::size_t uuidHash()
{
    boost::uuids::random_generator gen;
    boost::hash<boost::uuids::uuid> uuid_hasher;
    boost::uuids::uuid u = gen();
    return uuid_hasher(u);
}

inline bool strStartsWith(std::string const& str, std::string const& start)
{
    if (str.length() >= start.length()) {
        return (0 == str.compare(0, start.length(), start));
    } else {
        return false;
    }
}

//
// smart_path implementation from DDS
//

/**
 *
 *  @brief finds elements in a string match a specified string and replaces it.
 *  @param[in,out] _pString - The string to be processed.
 *  @param[in] _what - String to be replaced.
 *  @param[in] _with - Replacing string.
 *  @return A reference to the string object from which the elements have been replaced.
 *
 */
template<typename _T>
_T& replace(_T* _pString, const _T& _what, const _T& _with)
{
    typename _T::size_type pos = 0;
    typename _T::size_type withLen = _with.length();
    typename _T::size_type whatLen = _what.length();
    while ((pos = _pString->find(_what, pos)) != _T::npos) {
        _pString->replace(pos, _what.length(), _with);
        if (withLen > whatLen)
            pos += withLen - whatLen + 1;
    }
    return (*_pString);
}

/**
 *
 *  @brief appends character _ItemToAdd to the string _pString if there is no such suffix on the end of _pString.
 *  @param[in] _pString - The string to be processed.
 *  @param[in] _ItemToAdd - The target characters to be checked and added.
 *  @return A pointer to the processed string object.
 *
 */
template<typename _T>
_T* smart_append(_T* _pString, const typename _T::value_type _ItemToAdd)
{
    if (!_pString)
        return _pString;

    if (_pString->empty() || (*_pString)[_pString->size() - 1] != _ItemToAdd)
        _pString->push_back(_ItemToAdd);

    return _pString;
}

/**
 *
 *  @brief trims leading characters from the string.
 *  @param[in] _pString - The string to be trimmed.
 *  @param[in] _chWhat - The target character to be trimmed.
 *  @return A reference to the string object from which the elements have been trimmed.
 *
 */
template<typename _T>
_T& trim_left(_T* _pString, const typename _T::value_type& _chWhat)
{
    return _pString->erase(0, _pString->find_first_not_of(_chWhat));
}

/**
 * @brief The function returns current user name.
 * @param[out] _RetVal - A pinter to string buffer where user name will be stored. Must not be NULL.
 **/
inline void get_cuser_name(std::string* _RetVal)
{
    if (!_RetVal)
        return;

    passwd* pwd(getpwuid(geteuid()));
    *_RetVal = pwd ? std::string(pwd->pw_name) : "";
}
/**
 * @brief The function returns home directory path of the given user.
 * @param[in] _uid - user id the home directory of which should be returned.
 * @param[out] _RetVal - A pointer to string buffer where path will be stored. Must not be NULL.
 * @return In case of error, function returns an empty buffer.
 **/
inline void get_homedir(uid_t _uid, std::string* _RetVal)
{
    if (!_RetVal)
        return;

    passwd* pwd = getpwuid(_uid);
    *_RetVal = pwd ? std::string(pwd->pw_dir) : "";
}
/**
 * @brief The function returns home directory path of the given user.
 * @param[in] _UName - a name of the user the home directory of which should be returned.
 * @param[out] _RetVal - A pointer to string buffer where path will be stored. Must not be NULL.
 * @return In case of error, function returns an empty buffer.
 **/
inline void get_homedir(const char* _UName, std::string* _RetVal)
{
    if (!_RetVal)
        return;

    passwd* pwd = getpwnam(_UName);
    *_RetVal = pwd ? std::string(pwd->pw_dir) : "";
}
/**
 * @brief The function returns home directory path of the current user.
 * @param[out] _RetVal - A pointer to string buffer where path will be stored. Must not be NULL.
 * @return In case of error, function returns an empty buffer.
 **/
inline void get_cuser_homedir(std::string* _RetVal) { get_homedir(getuid(), _RetVal); }
/**
 * @brief The function extends any environment variable found in the give path to its value.
 * @brief This function also extends "~/" or "~user_name/" to a real user's home directory path.
 * @brief When, for example, there is a variable $GLITE_LOCATE = /opt/glite and the given path
 * @brief is "$GLITE_LOCATION/etc/test.xml", the return value will be a path "/opt/glite/etc/test.xml"
 * @param[in,out] _Path - A pointer to a string buffer which represents a path to extend. Must not be NULL.
 **/
template<class _T>
inline void smart_path(_T* _Path)
{
    if (nullptr == _Path || _Path->empty())
        return;

    // Checking for "~/"
    std::string path(*_Path);
    trim_left(&path, ' ');
    if ('~' == path[0]) {
        std::string path2(*_Path);
        // ~/.../.../
        if ('/' == path2[1]) {
            std::string sHome;
            get_cuser_homedir(&sHome);
            smart_append(&sHome, '/');

            path.erase(path2.begin(), path2.begin() + 2);
            sHome += path2;
            path2.swap(sHome);
            _Path->swap(path2);
        } else // ~user/.../.../
        {
            typename _T::size_type p = path2.find(_T( "/" ));
            if (_T::npos != p) {
                const std::string uname = path2.substr(1, p - 1);
                std::string home_dir;
                get_homedir(uname.c_str(), &home_dir);
                path2.erase(path2.begin(), path2.begin() + p);
                path2 = home_dir + path2;
                _Path->swap(path2);
            }
        }
    }

    typename _T::size_type p_begin = _Path->find(_T( "$" ));
    if (_T::npos == p_begin) {
        // make the path to be the canonicalized absolute pathname
        char resolved_path[PATH_MAX];
        char* res = realpath(_Path->c_str(), resolved_path);
        if (NULL != res) {
            // add trailing slash if needed, since realpath removes it
            std::string::iterator it = _Path->end() - 1;
            bool trailing_slash = (*it == '/');
            *_Path = resolved_path;
            if (trailing_slash)
                smart_append(_Path, '/');
        };

        return;
    }

    ++p_begin; // Excluding '$' from the name

    typename _T::size_type p_end = _Path->find(_T( "/" ), p_begin);
    if (_T::npos == p_end)
        p_end = _Path->size();

    const _T env_var(_Path->substr(p_begin, p_end - p_begin));
    // TODO: needs to be fixed to wide char: getenv
    const char* szvar(getenv(env_var.c_str()));
    if (!szvar)
        return;
    const _T var_val(szvar);
    if (var_val.empty())
        return;

    replace(_Path, _T( "$" ) + env_var, var_val);

    smart_path(_Path);
}

template<class _T>
inline _T smart_path(const _T& _Path)
{
    _T tmp(_Path);
    smart_path(&tmp);
    return tmp;
}

inline std::string getDateTime()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}

} // namespace odc::core

#endif /*__ODC__MiscUtils__*/

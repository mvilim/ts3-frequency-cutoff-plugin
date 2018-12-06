/*
 * Copyright (c) 2018 Michael Vilim
 *
 * This file is part of the teamspeak frequency cutoff filter plugin. It is
 * currently hosted at https://github.com/mvilim/ts3-frequency-cutoff-plugin
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

#include <ts3_functions.h>

using std::string;
using std::unique_ptr;

template <typename... Args>
string string_format(const string& format, Args... args) {
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return string(buf.get(), buf.get() + size - 1);
}

static const char* freq_cutoff_name() { return "Frequency Cutoff Plugin"; }

template <typename... Args>
void log(const struct TS3Functions& ts3Functions, enum LogLevel log_level,
         const string& format, Args... args) {
    ts3Functions.logMessage(string_format(format, args...).c_str(), log_level,
                            freq_cutoff_name(), 0);
}

template <typename... Args>
void log_info(const struct TS3Functions& ts3Functions,
              const string& format, Args... args) {
    log(ts3Functions, LogLevel::LogLevel_INFO, format, args...);
}

template <typename... Args>
void log_error(const struct TS3Functions& ts3Functions,
               const string& format, Args... args) {
    log(ts3Functions, LogLevel::LogLevel_INFO, format, args...);
}


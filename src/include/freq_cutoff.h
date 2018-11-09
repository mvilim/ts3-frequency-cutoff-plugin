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

#include <math.h>
#include <string.h>
#include <fstream>
#include <map>
#include <memory>

#include <teamspeak/public_definitions.h>
#include <ts3_log.h>

extern "C" {
#include <iir.h>
}

using std::atomic;
using std::endl;
using std::map;
using std::shared_ptr;
using std::string;

// although it does not appear that a 48k hz sample rate is guaranteed by
// teamspeak, the main codecs used all have that sample rate
// -- should read from the CHANNEL_CODEC property
constexpr double sample_rate = 48000.0;
// order of the butterworth filter
constexpr const int buffer_size = 8;

class ButterworthCoefficients {
   public:
    int cutoff_freq;
    double b[buffer_size + 1];
    double a[buffer_size + 1];

    ButterworthCoefficients(int cutoff_freq) : cutoff_freq(cutoff_freq) {
        double ff = cutoff_freq / (sample_rate / 2.0);
        double scale = sf_bwlp(buffer_size, ff);
        double* new_a = dcof_bwlp(buffer_size, ff);
        int* new_b = ccof_bwlp(buffer_size);
        for (int i = 0; i <= buffer_size; i++) {
            a[i] = new_a[i];
            b[i] = new_b[i] * scale;
        }
        delete[] new_a;
        delete[] new_b;
    };
};

class FilterConf {
   public:
    bool enabled;
    ButterworthCoefficients coefficients;

    FilterConf(bool enabled, int cutoffFreq)
        : enabled(enabled), coefficients(cutoffFreq){};

    bool operator==(const FilterConf other) const {
        return enabled == other.enabled &&
               coefficients.cutoff_freq == other.coefficients.cutoff_freq;
    }
};

class ButterworthChannelFilter {
   public:
    double x[buffer_size] = {0};
    double y[buffer_size] = {0};
    int index = 0;

    void reset() {
        std::fill(x, x + buffer_size, 0);
        std::fill(y, y + buffer_size, 0);
        index = 0;
    }
};

class ButterworthFilter {
   public:
    int cutoff_freq;

    ButterworthFilter(int cutoff_freq) : cutoff_freq(cutoff_freq){};

    map<int, ButterworthChannelFilter> channel_map;

    void reset() {
        for (auto& channel : channel_map) {
            channel.second.reset();
        }
    };
};

class ServerFilterGroup {
   public:
    map<anyID, const string> resolvedIds;
    map<anyID, ButterworthFilter> client_id_to_filter;
};

class ApplicationFilterGroup {
   private:
    map<const string, FilterConf> file_confs;
    shared_ptr<map<const string, FilterConf>> confs;
    const string config_filename;
    const TS3Functions& ts3_functions;

   public:
    ApplicationFilterGroup(const TS3Functions& ts3_functions,
                           const string config_filename)
        : ts3_functions(ts3_functions), config_filename(config_filename) {
        string line;
        string delimiter = " ";

        std::ifstream config_file(config_filename);
        if (config_file.good() && config_file.is_open()) {
            while (std::getline(config_file, line)) {
                size_t first_delim = line.find(delimiter, 0);
                size_t second_delim = line.find(delimiter, 1);
                std::string name = line.substr(0, first_delim);
                std::string str_freq =
                    line.substr(first_delim + 1, second_delim);
                std::string st_enabled = line.substr(second_delim + 1);
                int freq = std::stoi(str_freq);
                bool enabled = std::stoi(str_freq);
                file_confs.emplace(name, FilterConf(enabled, freq));
            }
            config_file.close();
        }

        store_atomic(file_confs);
    };

    typedef map<const string, FilterConf> ConfMap;

    map<uint64, ServerFilterGroup> server_filter_groups;

    shared_ptr<ConfMap> load_atomic() {
        return std::atomic_load<ConfMap>(&confs);
    }

    void store_atomic(ConfMap new_confs) {
        std::atomic_store<ConfMap>(&confs,
                                   std::make_shared<ConfMap>(new_confs));
    }

    void log_persist_error(const char* details = "") {
        log_error(ts3_functions,
                  "Error while trying to save config file. Settings "
                  "will not be persisted. %s",
                  details);
    }

    void persist() {
        if (file_confs != *confs) {
            try {
                std::ofstream config_file(config_filename);
                if (config_file.good() && config_file.is_open()) {
                    for (auto const& line : *load_atomic()) {
                        printf("writing name %s\n", line.first.c_str());
                        config_file << line.first << " "
                                    << line.second.coefficients.cutoff_freq
                                    << " " << line.second.enabled;
                    }
                    config_file.close();
                    file_confs = *confs;
                } else {
                    log_persist_error();
                }
            } catch (const std::runtime_error& re) {
                log_persist_error(re.what());
            } catch (const std::exception& ex) {
                log_persist_error(ex.what());
            } catch (...) {
                log_persist_error();
            }
        }
    }
};

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

#include <cutoff_dialog.h>
#include <freq_cutoff.h>

#include <teamspeak/clientlib_publicdefinitions.h>
#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/public_errors_rare.h>
#include <teamspeak/public_rare_definitions.h>
#include <ts3_functions.h>

#define DISPLAY_NAME_BUFSIZE 512

const char* freq_cutoff_version() { return "1.5"; }

const char* freq_cutoff_author() { return "Michael Vilim"; }

const char* freq_cutoff_description() {
    return "This plugin cuts off all sounds above a given frequency "
           "(configured per user).";
}

unique_ptr<ApplicationFilterGroup> filter_group;

static const char* config_filename = "frequency_cutoff_plugin.conf";

int freq_cutoff_init(const char* config_path,
                     const struct TS3Functions& ts3_functions) {
    try {
        log_info(ts3_functions, "Config path: %s", config_path);
        std::string name = std::string(config_path) + "/" + config_filename;

        filter_group =
            std::make_unique<ApplicationFilterGroup>(ts3_functions, name);

        return 0;
    } catch (...) {
        log_error(
            ts3_functions,
            "Error loading persisted settings. Most likely it has become "
            "corrupted for some reason. To reset to an empty state, please "
            "delete %s from %s.",
            config_filename, config_path);
        return 1;
    }
}

const char* freq_cutoff_infoTitle() { return freq_cutoff_name(); }

void resolve_id(const struct TS3Functions& ts3_functions, uint64 server_id,
                anyID client_id) {
    ServerFilterGroup& server_filters =
        filter_group->server_filter_groups[server_id];
    if (!server_filters.resolvedIds.count(client_id)) {
        char* uname;
        if (ts3_functions.getClientVariableAsString(
                server_id, client_id, ClientProperties::CLIENT_UNIQUE_IDENTIFIER,
                &uname) != ERROR_ok) {
            log_error(ts3_functions,
                      "Error resolving client identity for client id %i",
                      client_id);
        }

        log_info(ts3_functions, "Resolving uid for client id %i -- found %s",
                 client_id, uname);

        server_filters.resolvedIds.emplace(client_id, uname);

        ts3_functions.freeMemory(uname);
    }
}

string display_name(const struct TS3Functions& ts3_functions, uint64 server_id,
                    anyID client_id) {
    char dname[DISPLAY_NAME_BUFSIZE];
    if (ts3_functions.getClientDisplayName(server_id, client_id, dname,
                                           DISPLAY_NAME_BUFSIZE) != ERROR_ok) {
        log_error(ts3_functions,
                  "Error resolving client display name for client id %i.",
                  client_id);
        return "UNKNOWN";
    }

    return string(dname);
}

ButterworthFilter& get_filter(const struct TS3Functions& ts3_functions,
                              ServerFilterGroup& server_filters,
                              FilterConf& filter_conf, anyID client_id) {
    if (server_filters.client_id_to_filter.count(client_id)) {
        ButterworthFilter& filter =
            server_filters.client_id_to_filter.at(client_id);
        if (filter.cutoff_freq != filter_conf.coefficients.cutoff_freq) {
            log_info(ts3_functions, "Updating filter cutoff for client id %i.",
                     client_id);
            filter = ButterworthFilter(filter_conf.coefficients.cutoff_freq);
        }
        return filter;
    } else {
        log_info(ts3_functions, "Creating filter for client id %i.", client_id);
        server_filters.client_id_to_filter.emplace(
            client_id, ButterworthFilter(filter_conf.coefficients.cutoff_freq));
        return server_filters.client_id_to_filter.at(client_id);
    }
}

// For simplicitly, we never actually clean up the server and client maps --
// this will cause them to grow without bound as servers are joined and new
// users speak. Fixing this is difficult for two reasons: 1) there are many ways
// a user can leave our "scope" (moving channels, leaving server, getting
// kicked, etc) and a similar number of ways a server can disappear. 2) the
// events that indicate such changes are not synchronized with the audio played
// during this event (e.g. audio from a user can play after we have received
// their "left server" event. Because of these difficulties in exactly tracking
// what filters are possibily accessible, we simply never clean up the filters.
// The amount of memory used is very small. One map entry per server, one map
// entry per client with an active filter, and 4x the filter order size (e.g.
// 4x8 doubles) for filter coefficients and buffer.
//
// Other solutions could include: removing servers/clients based on timeout or
// removing them based on map size (i.e. maximum number of active filters)
void freq_cutoff_onEditPlaybackVoiceDataEvent(
    const struct TS3Functions& ts3_functions, uint64 server_id,
    anyID client_id, short* samples, int sample_count, int channels) {
    resolve_id(ts3_functions, server_id, client_id);
    ServerFilterGroup& server_filters =
        filter_group->server_filter_groups[server_id];
    shared_ptr<map<const string, FilterConf>> confs =
        filter_group->load_atomic();
    const string& uname = server_filters.resolvedIds[client_id];

    if (confs->count(uname)) {
        FilterConf& filter_conf = confs->at(uname);
        if (filter_conf.enabled) {
            ButterworthFilter& filter =
                get_filter(ts3_functions, server_filters, filter_conf, client_id);

            ButterworthCoefficients& coefficients = filter_conf.coefficients;

            for (int c = 0; c < channels; c++) {
                ButterworthChannelFilter& channel_filter = filter.channel_map[c];
                for (int s = 0; s < sample_count; s++) {
                    double new_x = (double)samples[s * channels + c];
                    double new_y = (coefficients.b[0]) * new_x;
                    for (int i = 1; i <= buffer_size; i++) {
                        new_y +=
                            (coefficients.b[i] *
                             channel_filter
                                 .x[(channel_filter.index - i + buffer_size) %
                                    buffer_size]);
                        new_y -=
                            (coefficients.a[i] *
                             channel_filter
                                 .y[(channel_filter.index - i + buffer_size) %
                                    buffer_size]);
                    }

                    channel_filter.x[channel_filter.index % buffer_size] = new_x;
                    channel_filter.y[channel_filter.index % buffer_size] = new_y;
                    for (int c = 0; c < channels; c++) {
                        samples[s * channels + c] = (short)new_y;
                    }
                    channel_filter.index =
                        (channel_filter.index + 1) % buffer_size;
                }
            }
            return;
        }
    } else {
        server_filters.client_id_to_filter.erase(client_id);
    }
}

void open_dialog(QWidget* parent_widget, const struct TS3Functions& ts3_functions,
                 uint64 server_id, anyID client_id) {
    const string dname =
        display_name(ts3_functions, server_id, client_id);
    ServerFilterGroup& server_filters =
        filter_group->server_filter_groups[server_id];
    resolve_id(ts3_functions, server_id, client_id);
    const string& uname = server_filters.resolvedIds[client_id];
    ConfigureCutoffDialog* dialog =
        new ConfigureCutoffDialog(dname, uname, *filter_group, parent_widget);
    dialog->show();
}

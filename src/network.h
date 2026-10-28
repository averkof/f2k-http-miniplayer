#pragma once
#include <string>
#include <regex>
#include <atomic>
#include <thread>
#include "httplib.h"
#include <iostream>

using namespace std;

const regex SONG_SEARCH_PATTERN{R"delim("columns":\["([^"]+)","([^"]+)","([^"]+)"\],"duration":([\d.]+))delim"};
const regex TITLE_SEARCH_PATTERN{R"delim("columns":\["([^\]]+)"\])delim"};
const regex POSIT_SEARCH_PATTERN{R"delim("position":([\d.]+))delim"};
const int API_QUERY_SUCCESS_CODE{200};

struct SongDetails {
    wstring artist, album, title, time;
    double duration{0}, position{0};
    string artwork;
    unsigned long long sample_tick;
};

class NetworkHandler {
    private:
        thread worker_thread;
        httplib::Client local_client{""};
        atomic<bool> worker_running{true};
        WINBOOL (*update_function)(HWND, UINT, WPARAM, LPARAM);
        HWND hwnd;
        UINT msg_update_details;
        UINT msg_update_position;

        wstring widen_string(const string &str) {
            if (str.empty()) return L"Nothing Playing";
            int n = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
            wstring wstr(n, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], n);
            return wstr;
        }

        string fetch_artwork() {
            string artwork;
            if (auto res = local_client.Get("/api/artwork/current"))
                if (res->status == API_QUERY_SUCCESS_CODE) {
                    artwork = move(res->body);
                }
            return artwork;
        }

        void add_leading_zeros(string &str, size_t width) {
            while (str.size() < width) {
                str.insert(str.begin(), '0');
            }
        }

        void watch_api_stream() {
            string buffer;
            string previous_song;
            string current_song;
            httplib::Client worker_client("http://localhost:8880");
            worker_client.set_read_timeout(3600);
            worker_client.set_connection_timeout(0, 500000);
            // GET http://localhost:8880/api/query/updates?player=true&trcolumns=%25title%25
            worker_client.Get("/api/query/updates", httplib::Params{{"player", "true"}, {"trcolumns", "%title%"}}, httplib::Headers(), [&](const char *data, size_t len) {
                buffer.assign(data, len);

                if (buffer.size() > 11) {
                    smatch title_match;
                    regex_search(buffer, title_match, TITLE_SEARCH_PATTERN);
                    previous_song = current_song;
                    current_song = title_match[1].str();
                    if (current_song != previous_song) {
                        update_function(hwnd, msg_update_details, 0, 0);
                    }
                } else {
                    update_function(hwnd, msg_update_position, 0, 0);
                }

                return worker_running.load();
            });
            worker_client.stop();
        }

    public:
        SongDetails song_details;
        const wstring *details[4] = { &song_details.title, &song_details.album, &song_details.artist, &song_details.time };
        NetworkHandler(WINBOOL (*funcptr)(HWND, UINT, WPARAM, LPARAM), HWND hwnd, UINT detail_msg, UINT pos_msg) {
            this->hwnd = hwnd;
            this->msg_update_details = detail_msg;
            this->msg_update_position = pos_msg;
            update_function = funcptr;
        }

        void start() {
            local_client = httplib::Client("http://localhost:8880");
            if (local_client.Get("/api/player")) cout << "yes hi" << endl;
            worker_thread = thread(&NetworkHandler::watch_api_stream, this);
        }

        void stop() {
            worker_running.store(false);
            worker_thread.join();
            local_client.stop();
        }

        void update_pos() {
            double double_time = (GetTickCount64() - song_details.sample_tick) / 1000.0;
            song_details.position += double_time;
        }

        void format_time_display() {
            string pos_min{to_string(static_cast<int>(song_details.position) / 60)};
            string pos_sec{to_string(static_cast<int>(song_details.position) % 60)};
            string dur_min{to_string(static_cast<int>(song_details.duration) / 60)};
            string dur_sec{to_string(static_cast<int>(song_details.duration) % 60)};
            add_leading_zeros(pos_sec, 2);
            add_leading_zeros(dur_sec, 2);
            string time_pos = pos_min + ":" + pos_sec;
            string time_dur = dur_min + ":" + dur_sec;
            string time = time_pos + " / " + time_dur;
            song_details.time = widen_string(time);
        }

        void update_details() {
            song_details.sample_tick = GetTickCount64();
            smatch detail_matches;
            auto res = local_client.Get("/api/query", httplib::Params{{"player", "true"}, {"trcolumns", "%artist%,%album%,%title%"}});
            if (res->status != API_QUERY_SUCCESS_CODE) return;
            regex_search(res->body, detail_matches, SONG_SEARCH_PATTERN);
            song_details.artist     = widen_string(detail_matches[1].str());
            song_details.album      = widen_string(detail_matches[2].str());
            song_details.title      = widen_string(detail_matches[3].str());
            song_details.duration   = stod(detail_matches[4].str());
            song_details.artwork    = fetch_artwork();
        }

        void update_position() {
            smatch detail_matches;
            auto res = local_client.Get("/api/query", httplib::Params{{"player", "true"}});
            if (res->status != API_QUERY_SUCCESS_CODE) return;
            regex_search(res->body, detail_matches, POSIT_SEARCH_PATTERN);
            song_details.position = stod(detail_matches[1].str());
            format_time_display();
        }
};
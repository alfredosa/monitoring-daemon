#include <windows.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <csignal>
#include <atomic>
#include <curl/curl.h>

#pragma comment(lib, "libcurl")

// OS definitions remain the same...
#define WINDOWS "windows"
#define LINUX "linux"
#define MAC "mac"

#ifdef _WIN32
#define OS WINDOWS
#elif __linux__
#define OS LINUX
#elif __APPLE__
#define OS MAC
#else
#define OS "Unknown"
#endif

// Global variables
std::unordered_map<std::string, int> counter;

char *auth_token;
char *api_url;
std::atomic<bool> running{true}; // Control flag for main loop

// Signal handler function
void signal_handler(int signal_num)
{
    std::cout << "\nReceived signal " << signal_num << std::endl;
    running = false; // Set the flag to stop the main loop
}

// Function to set up all signal handlers
void setup_signal_handlers()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGBREAK, signal_handler);
    signal(SIGABRT, signal_handler);
}

// Existing utility function declarations...
static inline std::string &ltrim(std::string &s);
static inline std::string &rtrim(std::string &s);
static inline std::string &trim(std::string &s);
void add_to_counter(std::string title);
std::vector<std::string> splitBy(const std::string &input, const char delimiter);
std::string clean_and_return_last_string(const std::string &input, const char delimiter);
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output);
std::string post_request(std::string url, std::string data);
std::string map_to_json(const std::unordered_map<std::string, int> &map_data);

// Enhanced initialization function, I add authentication...
bool init()
{
    try
    {
        auth_token = getenv("AUTH_TOKEN");
        if (auth_token == NULL)
        {
            std::cerr << "ERROR: AUTH_TOKEN is not set\n";
            return false;
        }

        api_url = getenv("MY_API_URL");
        if (api_url == NULL)
        {
            std::cerr << "ERROR: MY_API_URL is not set\n";
            return false;
        }

        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        {
            std::cerr << "ERROR: Failed to initialize CURL\n";
            return false;
        }

        setup_signal_handlers();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR during initialization: " << e.what() << std::endl;
        return false;
    }
}

// Enhanced cleanup function
void clean()
{
    try
    {
        counter.clear();
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR during cleanup: " << e.what() << std::endl;
    }
}

void post_exit_clean()
{
    try
    {
        clean();
        curl_global_cleanup();
        std::cout << "Completed full cleanup\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR during exit cleanup: " << e.what() << std::endl;
    }
}

int main()
{
    if (OS == "Unknown")
    {
        std::cerr << "ERROR: Unsupported operating system\n";
        return 1;
    }

    std::cout << "Setting up Monitoring for " << OS << std::endl;

    if (!init())
    {
        std::cerr << "ERROR: Initialization failed\n";
        return 1;
    }

    // Main monitoring loop
    while (running)
    {
        try
        {
            char window_title[256];
            HWND fw;

            for (int i = 0; i < 60 && running; i++)
            {
                fw = GetForegroundWindow();
                if (GetWindowTextA(fw, window_title, sizeof(window_title)) > 0)
                {
                    std::string title = window_title;
                    title = clean_and_return_last_string(title, '-');
                    add_to_counter(trim(title));
                }
                Sleep(1000);
            }

            if (!running)
                break; // Check if we should exit

            std::string data = map_to_json(counter);
            std::cout << "Sending data: " << data << std::endl;

            std::string response = post_request(api_url, data);

            if (response.find("Stored") != std::string::npos ||
                response.find("Data") != std::string::npos ||
                response.find("Success") != std::string::npos)
            {
                std::cout << "Data sent successfully\n";
            }
            else if (!response.empty())
            {
                std::cerr << "Warning: Unexpected response content: " << response << std::endl;
            }

            clean();
        }
        catch (const std::exception &e)
        {
            std::cerr << "ERROR in main loop: " << e.what() << std::endl;
            Sleep(5000); // Wait before retrying
        }
    }

    std::cout << "Shutting down gracefully...\n";
    post_exit_clean();
    return 0;
}

// Left trim
static inline std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
    return s;
}

// Right trim
static inline std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
    return s;
}

// Trim both ends
static inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

void add_to_counter(std::string title)
{
    if (counter.find(title) == counter.end())
    {
        counter[title] = 1;
    }
    else
    {
        counter[title]++;
    }
}

// Callback function to handle received data
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output)
{
    size_t total_size = size * nmemb;
    output->append((char *)contents, total_size);
    return total_size;
}

// If you don't have the nlohmann/json library, here's a simple JSON conversion function
std::string map_to_json(const std::unordered_map<std::string, int> &map_data)
{
    std::string json = "{";
    bool first = true;

    for (const auto &pair : map_data)
    {
        if (!first)
        {
            json += ",";
        }
        json += "\"" + pair.first + "\":" + std::to_string(pair.second);
        first = false;
    }

    json += "}";
    return json;
}

std::string post_request(std::string url, std::string data)
{
    CURL *curl;
    CURLcode res;
    std::string response;
    struct curl_slist *headers = NULL;
    long http_code = 0;

    curl = curl_easy_init();
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (curl)
    {
        char *auth_token = getenv("AUTH_TOKEN");
        std::string auth_header = "Authorization: Bearer " + std::string(auth_token);
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        curl_easy_cleanup(curl);
    }

    if (http_code >= 400)
    {
        std::cout << "Error: " << response << std::endl;
    }

    return response;
}

std::vector<std::string> splitBy(const std::string &input, const char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string segment;

    while (std::getline(ss, segment, delimiter))
    {
        result.push_back(segment);
    }

    return result;
}

std::string clean_and_return_last_string(const std::string &input, const char delimiter)
{
    std::vector<std::string> parts = splitBy(input, delimiter);

    if (parts.size() > 1)
    {
        return parts[parts.size() - 1];
    }

    return parts[0];
}

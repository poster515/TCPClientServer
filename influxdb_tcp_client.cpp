#include "influxdb.hpp"
#include <iostream>
#include <chrono>
using namespace std;

#define INFLUXDB_TOKEN "UZ0JnxIbwh8fLSa_0aoCAxDuW_HxVsuwXOUxmqaUkjNeydhBkO8x0Eiblz0oxa7xFBXFrdO4EzjTuicoQe5kPg=="

// Visit https://github.com/orca-zhang/influxdb-c for more test cases

int main(int argc, char const *argv[])
{
    #ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }
    #endif

    std::string host ("192.168.0.4");
    std::string org ("Raytheon"), bkt ("crawler-data");
    influxdb_cpp::server_info si(host, 8086, org, INFLUXDB_TOKEN, bkt);
    // post_http demo with resp[optional]
    using namespace std::chrono;
    long long ns = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    string resp;
    int ret = influxdb_cpp::builder()
        .meas("test")
        .tag("k", "v")
        .tag("x", "y")
        .field("x", 20)
        .field("y", 40.3, 2)
        .field("b", !!10)
        .timestamp(std::chrono::system_clock::now().time_since_epoch().count())
        .post_http(si, &resp);

    cout << ret << endl << resp << endl;

    // // query from table
    // influxdb_cpp::server_info si_new("127.0.0.1", 8086, "", "test", "test");
    // influxdb_cpp::flux_query(resp, "show databases", si);
    // cout << resp << endl;
}
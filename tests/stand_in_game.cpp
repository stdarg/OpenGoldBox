// Stands in for a running game in the export guard's test: it sleeps until it
// is killed, so its command line can be found while it runs.
#include <chrono>
#include <thread>

int main()
{
    std::this_thread::sleep_for(std::chrono::minutes(2));
}

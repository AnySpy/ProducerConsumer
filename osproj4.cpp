// **********************************************************
// *
// * Sam Young
// * Operating Systems
// * Programming Project 3: Process Synchronization
// * March 14, 2025
// * Dr. Siming Liu
// *
// **********************************************************

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <cstring>
#include <unordered_map>
#include "buffer.h"

void    *producer   (void *param);
void    *consumer   (void *param);
void    endLog      (int, int, int, int);

std::unordered_map<pid_t, int> produced_counts; // ? A map for storing each thread's produced items count
std::unordered_map<pid_t, int> consumed_counts; // ? A map for storing each thread's consumed items count
bool execute = true;                            // ? A bool value for whether or not a thread should continue with execution.
bool buff_snap = false;                         // ? Handles buffer snapshot

int main(int argc, char* argv[])
{
    // ? The program requires 5 arguments:
    // ?    The time main() should sleep
    // ?    The maxumum length that a thread should sleep
    // ?    The number of producer threads
    // ?    The number of consumer threads
    // ?    Whether buffer snapshots should be displayed in the output
    // ? This just makes sure that the function recieves all the required arguments
    if(argc != 6)
    {
        printf("\u001b[37;1m%s: \u001b[31;1mUsage:\u001b[0m %s <int> <int> <int> <int> <\"yes\"/\"no\">\n", argv[0], argv[0]);

        return 1;
    }

    srand(time(NULL));      // ? Generates a seed for rand() to use
    
    // ? Initializes all the required variables
    int main_sleep      = atoi(argv[1]);
    int thread_maxsleep = atoi(argv[2]);
    int prod_threads    = atoi(argv[3]);
    int cons_threads    = atoi(argv[4]);

    if (main_sleep < 0)
    {
        printf("\u001b[37;1m%s: \u001b[31;1mError:\u001b[0m First argument must be at least 0\n", argv[0]);
        return 0;
    }
    if (thread_maxsleep < 0)
    {
        printf("\u001b[37;1m%s: \u001b[31;1mError:\u001b[0m Second argument must be at least 0\n", argv[0]);
        return 0;
    }
    if (prod_threads < 0)
    {
        printf("\u001b[37;1m%s: \u001b[31;1mError:\u001b[0m Third argument must be at least 0\n", argv[0]);
        return 0;
    }
    if (cons_threads < 0)
    {
        printf("\u001b[37;1m%s: \u001b[31;1mError:\u001b[0m Fourth argument must be at least 0\n", argv[0]);
        return 0;
    }
    if (strcasecmp(argv[5], "YES") == 0)
    {
        buff_snap = true;
    }
    else if (strcasecmp(argv[5], "NO") == 0)
    {
        buff_snap = false;
    }
    else
    {
        printf("\u001b[37;1m%s: \u001b[31;1mError:\u001b[0m Last argument must be \"yes\" or \"no\"\n", argv[0]);
        return 0;
    }
    
    // ? Initializes our pthreads
    pthread_attr_t attr;
    pthread_t tid[prod_threads + cons_threads];

    printf("Starting threads...\n");
    
    // ? Initializes our buffer (in buffer.h)
    buffer_initialize();

    // ? Initializes thread attributes
    pthread_attr_init(&attr);

    // ? Creates our producer threads
    for (int i = 0; i < prod_threads; ++i)
    {
        pthread_create(&tid[i], &attr, producer, (void*)&thread_maxsleep);  // ? Passes thread_maxsleep as the variable to the runner
    }
    
    // ? Creates our consumer threads
    for (int i = 0; i < cons_threads; ++i)
    {
        pthread_create(&tid[prod_threads + i], &attr, consumer, (void*)&thread_maxsleep);   // ? Passes thread_maxsleep as the variable to the runner
    }

    // ? Sleeps the main thread for the specified length of time (passed by the user).
    // ? This returns 0 on successful execution, so I just set execute's value to this.
    execute = sleep(main_sleep);

    // ? Sends a signal to cancel thread execution
    for (int i = 0; i < prod_threads + cons_threads; ++i)
    {
        pthread_cancel(tid[i]);
    }

    // ? Joins every thread to the main process if it wasn't canceled (was in a sensitive area)
    for (int i = 0; i < prod_threads + cons_threads; ++i)
    {
        pthread_join(tid[i], NULL);
    }

    // ? Produces the ending log to spec
    endLog(main_sleep, thread_maxsleep, prod_threads, cons_threads);

    // ? Returns successful
    return 0;
}

/// @name producer
/// @brief Produces a random integer and then places it in the buffer if space allows.
/// @param param The length of the maximum time this thread can sleep, passed as a void*
/// @return NULL
/// @note This function makes use of the global variable:
/// @note - execute (from project3.cpp): A bool of whether the thread should continue execution
void *producer(void *param)
{
    int* sleep_len = (int*)param;       // ? We convert the sleep length from void* to int*
    buffer_item item;                   // ? And create a new buffer item for item generation

    while(execute)                      // ? While the main thread wants execution to be continuing
    {                                   // ? Sleep for a random amount of time and generate a random number
        if(*sleep_len != 0) sleep(std::rand() % (*sleep_len) + 1);
        item = std::rand() % 100;
        // ? If the insert item function fails, something went wrong, so we log it.
        if ( !buffer_insert_item(item) )
        {
            std::cerr << "\u001b[36mProducer\u001b[0m: Could not insert item " << item << '\n';
        }
    }

    return NULL;                        // ? Return NULL to end the thread
}

/// @name consumer
/// @brief Consumes an integer in the buffer if available. Also detects if the consumed integer is prime.
/// @param param The length of the maximum time this thread can sleep, passed as a void*
/// @return NULL
/// @note This function makes use of the global variable:
/// @note - execute (from project3.cpp): A bool of whether the thread should continue execution
void *consumer(void *param)
{
    int* sleep_len = (int*)param;           // ? We convert the sleep length from void* to int*

    while(execute)                          // ? While the main thread wants execution to be continuing
    {
        if (*sleep_len != 0) sleep(rand() % (*sleep_len) + 1);   // ? Sleep for a random amount of time
        
        if( !buffer_remove_item() )         // ? If the item couldn't be removed, log the error
        {
            std::cerr << "\u001b[35mConsumer\u001b[0m: Could not read item...\n";
        }
    }

    return NULL;                            // ? Return NULL to end the thread
}

/// @name endLog
/// @brief Outputs a log of the simulation to stdout.
/// @param main_sleep The length of the main thread's sleep time
/// @param thread_sleep The maximum length that a thread may sleep
/// @param prod_count The number of producer threads
/// @param cons_count The number of consumer threads
/// @note This function makes use of the following global variables:
/// @note - `consumed_counts`   (from project3.cpp): A map of how many integers each thread read.
/// @note - `produced_counts`   (from project3.cpp): A map of how many integers each thread produced.
/// @note - `consumed_count`    (from buffer.h): A count of how many integers were read.
/// @note - `produced_count`    (from buffer.h): A count of how many integers were created.
/// @note - `BUFFER_SIZE`       (from buffer.h): The size of the buffer.
/// @note - `empty_count`       (from buffer.h): A count of how many times the buffer was empty.
/// @note - `full_count`        (from buffer.h): A count of how many times the buffer was full.
/// @note - `count`             (from buffer.h): A count of how many items are currently stored in the buffer.
void endLog(int main_sleep, int thread_sleep, int prod_count, int cons_count)
{
    printf("PRODUCER / CONSUMER SIMULATION COMPLETE\n");
    printf("=======================================\n");
    printf("Simulation Time:                     %i\n", main_sleep);
    printf("Maximum Thread Sleep Time:           %i\n", thread_sleep);
    printf("Number of Producer Threads:          %i\n", prod_count);
    printf("Number of Consumer Threads:          %i\n", cons_count);
    printf("Size of buffer:                      %i\n", BUFFER_SIZE);
    printf("\n");
    printf("Total Number of Items Produced:      %i\n", produced_count);
    int i = 1;
    for (auto& entry : produced_counts)
    {
        printf("\tThread %i:                    %i\n", i++, entry.second);
    }
    printf("\n");
    printf("Total Number of Items Consumed:      %i\n", consumed_count);
    for (auto& entry : consumed_counts)
    {
        printf("\tThread %i:                    %i\n", i++, entry.second);
    }
    printf("\n");
    printf("Number Of Items Remaining in Buffer: %i\n", count);
    printf("Number Of Times Buffer Was Full:     %i\n", full_count);
    printf("Number Of Times Buffer Was Empty:    %i\n", empty_count);
}
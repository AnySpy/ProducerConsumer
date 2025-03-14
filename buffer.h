// **********************************************************
// *
// * Sam Young
// * Operating Systems
// * Programming Project 3: Process Synchronization
// * March 14, 2025
// * Dr. Siming Liu
// *
// **********************************************************

#ifndef _BUFFER_H_DEFINED_
#define _BUFFER_H_DEFINED_

#define BUFFER_SIZE 5

#include <semaphore.h>
#include <unordered_map>

typedef int buffer_item;

buffer_item         buffer[BUFFER_SIZE];    // ? The buffer
pthread_mutex_t     mutex;                  // ? The mutex
pthread_mutex_t     print_mutex;            // ? A mutex so printing doesn't overlap (Unlikely, but could happen)
sem_t               empty, full;            // ? Two semaphores, empty representing empty slots and full representing full slots
int                 count;                  // ? The count of the number of items currently in the buffer
int                 head;                   // ? An integer representing where the current index for reading is
int                 tail;                   // ? An integer representing where the current index for writing is
int                 produced_count;         // ? An integer representing how many items have been produced
int                 consumed_count;         // ? An integer representing how many items have been consumed
int                 full_count;             // ? An integer representing how many times the buffer has been full
int                 empty_count;            // ? An integer representing how many times the buffer has been empty

extern std::unordered_map<pid_t, int> produced_counts;  // ? A map for storing each thread's produced items count (from project3.cpp)
extern std::unordered_map<pid_t, int> consumed_counts;  // ? A map for storing each thread's consumed items count (from project3.cpp)
extern bool buff_snap;                                  // ? Handles buffer snapshot (from project3.cpp)

/// @name prime
/// @brief This function detects whether a provided number is prime or not
/// @param n The number to detect
/// @return true if the number is prime, false otherwise
bool prime(int n)
{
    if (n <= 1)     // ? If the number is 0 or 1, it's not prime. Return false
    {
        return false;
    }
    else            // ? Otherwise, we iterate through numbers until n. If we find a number that evenly divides n, it's not prime, and we return false
    {
        for (int i = 2; i < n; ++i)
        {
            if (n % i == 0)
            {
                return 0;
            }
        }
        return 1;   // ? If it doesn't find a number, return true
    }
}

/// @brief Prints out the contents of the buffer
/// @param num Defaults to -1. If a number other is sent, checks if it's prime and outputs whether it is.
/// @note This function uses the following global variables:
/// @note - BUFFER_SIZE (from buffer.h): The buffer's size
/// @note - print_mutex (from buffer.h): A mutex so printing doesn't overlap
/// @note - buffer[i] (from buffer.h): The buffer
/// @note - head (from buffer.h): An integer representing where the current index for reading is
/// @note - tail (from buffer.h): An integer representing where the current index for writing is
void buffer_print(int num = -1)
{
    pthread_mutex_lock(&print_mutex);
    printf("(buffers occupied %i)", count);
    
    if (num != -1)
    {
        if (prime(num))
        {
            printf("\t* * * PRIME * * *");
        }
    }
    
    printf("\n");
    printf("buffers: ");
    
    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        printf("%*i", 5, buffer[i]);
    }
    
    printf("\n\t  ");
    
    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        printf("---- ");
    }
    
    printf("\n\t  ");
    
    for(int i = 0; i < BUFFER_SIZE; ++i)
    {
        if (i == head && i == tail)
        {
            printf(" RW ");
        }
        else if (i == head)
        {
            printf(" R  ");
        }
        else if (i == tail)
        {
            printf("  W ");
        }
        else
        {
            printf("    ");
        }
        printf(" ");
    }
    
    printf("\n");
    pthread_mutex_unlock(&print_mutex);
}

/// @name buffer_initialize
/// @brief Initializes the buffer
/// @note This function uses the following global variables:
/// @note - produced_count (from buffer.h): A count of how many items have been produced
/// @note - consumed_count (from buffer.h): A count of how many items have been read
/// @note - print_mutex (from buffer.h): A mutex so printing doesn't overlap
/// @note - empty_count (from buffer.h): A count of how many times the buffer has been empty
/// @note - full_count (from buffer.h): A count of how many times the buffer has been full
/// @note - buff_snap (from project3.cpp): Whether the user wants buffer snapshots displayes
/// @note - buffer (from buffer.h): The buffer
/// @note - mutex (from buffer.h): The mutex
/// @note - empty (from buffer.h): A semaphore representing the number of empty slots
/// @note - count (from buffer.h): The count of how many items are in the buffer
/// @note - full (from buffer.h): A semaphore representing the number of full slots
/// @note - head (from buffer.h): The current index for writing
/// @note - tail (from buffer.h): The current index for reading
void buffer_initialize()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);
    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full, 0, 0);
    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        buffer[i] = -1;
    }
    count = 0;
    head = 0;
    tail = 0;
    produced_count = 0;
    consumed_count = 0;
    empty_count = 0;
    full_count = 0;
    if (buff_snap) buffer_print();
}

/// @brief Inserts an item into the buffer
/// @param item The item to insert
/// @return 1 on success, 0 on failure
/// @note This function uses the following global variables:
/// @note - produced_counts (from project3.cpp): A map of how many items each thread has produced
/// @note - produced_count (from buffer.h): A count of how many items have been produced
/// @note - print_mutex (from buffer.h): A mutex so printing doesn't overlap
/// @note - full_count (from buffer.h): A count of how many times the buffer has been full
/// @note - buff_snap (from project3.cpp): Whether the user wants buffer snapshots displayed
/// @note - buffer (from buffer.h): The circular buffer where items are stored
/// @note - mutex (from buffer.h): The mutex used to synchronize access to the buffer
/// @note - empty (from buffer.h): A semaphore representing the number of empty slots in the buffer
/// @note - full (from buffer.h): A semaphore representing the number of full slots in the buffer
/// @note - count (from buffer.h): The number of items currently in the buffer
/// @note - tail (from buffer.h): The current index for writing new items into the buffer
bool buffer_insert_item( buffer_item item )
{

    if (sem_trywait(&empty) == -1)                      // ? Waits for a sign that the buffer has an open slot. If trywait fails, a message is output and the waiting continues!
    {
        pthread_mutex_lock(&print_mutex);
        printf("All buffers full. Producer %i waits.\n", gettid());
        pthread_mutex_unlock(&print_mutex);

        sem_wait(&empty);
    }
    pthread_mutex_lock(&mutex);                             // ? Locks the mutex to change the buffer
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);   // ? Makes sure the pthread can't be canceled in this state
    
    buffer[tail] = item;                                    // ? Sets the current writing index to that item and logs success
    pthread_mutex_lock(&print_mutex);
    printf("\u001b[36mProducer %i\u001b[0m: writes %i\n", gettid(), item);
    pthread_mutex_unlock(&print_mutex);
    tail = (tail + 1) % BUFFER_SIZE;                        // ? Uses a curcular incrementation to increment the buffer's write index
    ++count;                                                // ? Increments the count of the buffer
    if (buff_snap) buffer_print();                          // ? Prints its status
    ++produced_counts[gettid()];                            // ? Increments the value for this thread in the map
    ++produced_count;                                       // ? Increments the count of total produced items
    if (count == 5) ++full_count;                           // ? If the buffer is full, increments the count of times the buffer has been full

    pthread_mutex_unlock(&mutex);                           // ? Unlocks the mutex
    sem_post(&full);                                        // ? Posts that there is a new item in the buffer
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);    // ? Enables pthread cancelation again

    return true;                                            // ? Returns success
}

/// @brief Removes an item from the buffer
/// @param item The item to remove as a pointer
/// @return 1 on success, 0 on failure
/// @note This function uses the following global variables:
/// @note - consumed_count (from buffer.h): A count of how many items have been consumed
/// @note - consumed_counts (from project3.cpp): A map of how many items each thread has consumed
/// @note - print_mutex (from buffer.h): A mutex so printing doesn't overlap
/// @note - empty_count (from buffer.h): A count of how many times the buffer has been empty
/// @note - buff_snap (from project3.cpp): Whether the user wants buffer snapshots displayed
/// @note - buffer (from buffer.h): The circular buffer where items are stored
/// @note - mutex (from buffer.h): The mutex used to synchronize access to the buffer
/// @note - empty (from buffer.h): A semaphore representing the number of empty slots in the buffer
/// @note - full (from buffer.h): A semaphore representing the number of full slots in the buffer
/// @note - count (from buffer.h): The number of items currently in the buffer
/// @note - head (from buffer.h): The current index for reading items from the buffer
bool buffer_remove_item()
{
    buffer_item item;

    if (sem_trywait(&full))                                 // ? Waits for a sign that the buffer has a full slot. If trywait fails, a message is output and the waiting continues!
    {
        pthread_mutex_lock(&print_mutex);
        printf("All buffers empty. Consumer %i waits.\n", gettid());
        pthread_mutex_unlock(&print_mutex);

        sem_wait(&full);
    }
    pthread_mutex_lock(&mutex);                             // ? Locks the mutex to change the buffer
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);   // ? Makes sure the pthread can't be canceled in this state

    item = buffer[head];                                    // ? Sets the item to the item in the current reading index
    pthread_mutex_lock(&print_mutex);
    printf("\u001b[35mConsumer %i\u001b[0m: reads %i\n", gettid(), item);
    pthread_mutex_unlock(&print_mutex);
    head = (head + 1) % BUFFER_SIZE;                        // ? Uses a curcular incrementation to increment the buffer's read index
    --count;                                                // ? Decrements the count of the buffer
    if (buff_snap) buffer_print();                          // ? Prints its status
    ++consumed_counts[gettid()];                            // ? Increments the thread's count of how many times it has eaten an item
    ++consumed_count;                                       // ? Increments the count of how many items have been consumed
    if (count == 0) ++empty_count;                          // ? If the count of items in the buffer is 0, increments the total number of times the buffer has been empty
    
    pthread_mutex_unlock(&mutex);                           // ? Unlocks the mutex
    sem_post(&empty);                                       // ? Posts that there is a new empty slot in the buffer
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);    // ? Enables the pthread's ability to be canceled
    
    return true;                                            // ? Returns success
}

#endif // _BUFFER_H_DEFINED_

#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <pthread.h>
#include <unistd.h>


//------------------------------------------

// Number of threads
static const int THREAD_NUM  = 200;
// Max size of queue when it refuses to add more msgs
static const int BUFFER_SIZE = 200;
// Amount of msgs to push into the queue
static const int MSG_NUM     = 5;

//------------------------------------------

/*
class Locker {

public:
    explicit Locker( pthread_mutex_t& mutexToLock ) : _mutex( mutexToLock ) {
        pthread_mutex_lock( &_mutex );
    }
    ~Locker() {
        pthread_mutex_unlock( &_mutex );
    }

private:
    pthread_mutex_t& _mutex;

};
*/

//------------------------------------------

// Multithread safe queue
class MultithreadQueue {

public:
    MultithreadQueue() : _isFinished( false ) {
        pthread_mutex_init( &_mutex, nullptr );
        pthread_cond_init( &_cond, nullptr );

        pthread_mutex_init( &_mutex_overflow, nullptr );
        pthread_cond_init( &_cond_overflow, nullptr );
    }
    ~MultithreadQueue() {
        _isFinished = true;

        pthread_cond_broadcast( &_cond );
        pthread_cond_broadcast( &_cond_overflow );

        pthread_cond_destroy( &_cond );
        pthread_mutex_destroy( &_mutex );

        pthread_cond_destroy( &_cond_overflow );
        pthread_mutex_destroy( &_mutex_overflow );
    }

    size_t size() { return _msg_queue.size(); }
    // Tells threads that queue is stopped working and they need to be shut down
    void finish() {
        _isFinished = true;

        pthread_cond_broadcast( &_cond );
        pthread_cond_broadcast( &_cond_overflow );
    }
    // Pushed new msg into the queue
    int addMsg( std::string inputMsg ) {
        pthread_mutex_lock( &_mutex );

        while ( _msg_queue.size() > BUFFER_SIZE ) {
            pthread_mutex_unlock( &_mutex );
            pthread_mutex_lock( &_mutex_overflow );

            pthread_cond_wait( &_cond_overflow, &_mutex_overflow );

            pthread_mutex_lock( &_mutex );
            pthread_mutex_unlock( &_mutex_overflow );
        }

        _msg_queue.push( inputMsg );

        pthread_cond_signal( &_cond );
        pthread_mutex_unlock( &_mutex );
        return 0;
    }
    // Pops msg from the queue
    std::string getMsg() {
        pthread_mutex_lock( &_mutex );

        std::string nextMsg;
        if ( _msg_queue.size() == 0 ) {
            while ( true ) {
                if ( _isFinished ) {
                    pthread_mutex_unlock( &_mutex );
                    return "";
                }
                pthread_cond_wait( &_cond, &_mutex );
                // While it was waking up, someone stole its msg
                if ( _msg_queue.size() == 0 ) {
                    continue;
                }

                // Everything's all right
                pthread_mutex_lock( &_mutex_overflow );

                nextMsg = _msg_queue.front();
                _msg_queue.pop();

                pthread_cond_signal( &_cond_overflow );
                pthread_mutex_unlock( &_mutex_overflow );

                pthread_mutex_unlock( &_mutex );
                return nextMsg;
            }
        }
        else {
            //pthread_mutex_lock( &_mutex_overflow );

            nextMsg = _msg_queue.front();
            _msg_queue.pop();

            pthread_cond_signal( &_cond_overflow );
            pthread_mutex_unlock( &_mutex_overflow );

            pthread_mutex_unlock( &_mutex );
            return nextMsg;
        }
    }

private:


private:
    bool            _isFinished;

    // Normal mutex to control mthreads
    pthread_mutex_t _mutex;
    pthread_cond_t  _cond;

    // Mutex to contorl queue "overflow" ( when ( size of queue > BUFFER_SIZE ) )
    pthread_mutex_t _mutex_overflow;
    pthread_cond_t  _cond_overflow;

    std::queue< std::string > _msg_queue;

};
static MultithreadQueue mtQueue;

//------------------------------------------

void* printMsgs( void* rawData ) {
    std::string threadNum = reinterpret_cast< const char* >( rawData );
    std::string msg;
    printf( "Thread %s started!\n", threadNum.c_str() );
    //std::cout << "Thread " << threadNum << " started!\n";
    while ( ( msg = mtQueue.getMsg() ) != "" ) {
        //std::cout << threadNum << ": " << msg;
        printf( "%s: %s;\n", threadNum.c_str(), msg.c_str() );
    }

    printf( "Thread %s stopped!\n", threadNum.c_str() );
    //std::cout << "Thread " << threadNum << " stopped!\n";
    pthread_exit( nullptr );
}

//------------------------------------------

int main()
{
    pthread_t printingThreads[THREAD_NUM];

    int i = 0;
    for ( i = 0; i < THREAD_NUM; ++i ) {
        pthread_create( &printingThreads[i], nullptr, printMsgs, reinterpret_cast< void* >( const_cast< char* >( std::to_string( i ).c_str() ) ) );
        usleep( 5000 ); // Need this to see correct numbers of threads. Line is safe to be commented

        //pthread_detach( printingThreads[i] );
        //std::cout << "Thread " << i << " started!\n";
    }

    //sleep( 1 );

    i = 0;
    std::string msg = "Test: ";
    while ( i < MSG_NUM ) {
        mtQueue.addMsg( msg + std::to_string( i ) );
        ++i;
        //sleep( 1 );
    }

    mtQueue.finish();

    void* tmp = nullptr;
    for ( i = 0; i < THREAD_NUM; ++i ) {
        pthread_join( printingThreads[i], &tmp );
        printf( "Thread %d stopped in main!\n", i );
        //std::cout << "Thread " << i << " stopped in main!\n";
    }

    std::cout << '\n' << '\n' << mtQueue.size() << '\n';

    return 0;
}

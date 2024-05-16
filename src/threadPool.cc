#define MAX_THREADS 3

struct Task{
    void (*func)(void*);
    void *argMem;
};

#if(WIN)
DWORD WINAPI threadFunc(LPVOID arg);
#endif

struct ThreadPool{
    DynamicArray<Task> tasks;
    bool shouldBeRunning;
#if(WIN)
    HANDLE threadHandles[MAX_THREADS];
    HANDLE mut;
#elif(LIN)
    //TODO:
#endif

    void init(){
        shouldBeRunning = true;
        tasks.init();
#if(WIN)
        mut = CreateMutex(nullptr, FALSE, nullptr);
        for(u32 x=0; x<MAX_THREADS; x++){
            threadHandles[x] = CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            threadFunc,             // thread function name
            this,                   // argument to thread function 
            0,                      // use default creation flags 
            NULL);                  // returns the thread identifier
        };
#endif
    };
    void uninit(){
        while(tasks.count != 0){
#if(WIN)
            WaitForSingleObject(mut, INFINITE);
            Task t = tasks.pop();
            ReleaseMutex(mut);
            t.func(t.argMem);
#endif
        };
        shouldBeRunning = false;
#if(WIN)
        WaitForMultipleObjects(MAX_THREADS, threadHandles, TRUE, INFINITE);
        CloseHandle(mut);
        for(u32 x=0; x<MAX_THREADS; x++){CloseHandle(threadHandles[x]);};
#endif
        tasks.uninit();
    }
    void pushTask(Task tsk){
#if(WIN)
        WaitForSingleObject(mut, INFINITE);
        tasks.push(tsk);
        ReleaseMutex(mut);
#endif
    };
};

#if(WIN)
DWORD WINAPI threadFunc(LPVOID arg){
    ThreadPool *pool = (ThreadPool*)arg;
    while(pool->shouldBeRunning){
        WaitForSingleObject(pool->mut, INFINITE);
        if(pool->tasks.count == 0){
            ReleaseMutex(pool->mut);
            Sleep(500);
            continue;
        };
        Task t = pool->tasks.pop();
        ReleaseMutex(pool->mut);
        t.func(t.argMem);
    };
    return 0;
};
#endif
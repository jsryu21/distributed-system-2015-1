#include <thread>
// http://stackoverflow.com/questions/17185734/whats-the-difference-between-atomic-and-cstdatomic
#include <cstdatomic>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>

const static int hurdle = 1000000;
const static int testCnt = 10;
const static int numThread = 8;

class ILock {
    public:
        virtual void Lock() = 0;
        virtual void Unlock() = 0;
};

class TASlock : public ILock {
    public:
        TASlock()
            : state(ATOMIC_FLAG_INIT)
        {
        }
        void Lock()
        {
            while (state.test_and_set(std::memory_order_acquire)) {}
        }
        void Unlock()
        {
            state.clear();
        }
    private:
        // http://en.cppreference.com/w/cpp/atomic/atomic_flag
        // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2427.html
        std::atomic_flag state;
};

class TASlock2 : public ILock {
    public:
        TASlock2()
            : state(false)
        {
        }
        void Lock()
        {
            while (state.exchange(true)) {}
        }
        void Unlock()
        {
            state.store(false);
        }
    private:
        std::atomic< bool > state;
};

class TASlock3 : public ILock {
    public:
        TASlock3()
            : state(false)
        {
        }
        void Lock()
        {
            // https://gcc.gnu.org/onlinedocs/gcc-4.2.2/gcc/Atomic-Builtins.html
            while (__sync_lock_test_and_set(&state, true)) {}
        }
        void Unlock()
        {
            __sync_lock_release(&state);
        }
    private:
        bool state;
};

class TTASlock : public ILock {
    public:
        TTASlock()
            : state(false)
        {
        }
        void Lock()
        {
            while (true)
            {
                while (state.load()) {}
                if (state.exchange(true) == false)
                {
                    return;
                }
            }
        }
        void Unlock()
        {
            state.store(false);
        }
    private:
        // http://en.cppreference.com/w/cpp/atomic/atomic
        std::atomic< bool > state;
};

class TTASlock2 : public ILock {
    public:
        TTASlock2()
            : state(false)
        {
        }
        void Lock()
        {
            while (true)
            {
                while (state) {}
                if (__sync_lock_test_and_set(&state, true) == false)
                {
                    return;
                }
            }
        }
        void Unlock()
        {
            __sync_lock_release(&state);
        }
    private:
        bool state;
};

class CASlock : public ILock {
    // https://github.com/mmhl/threads/blob/41c1b344b870798e13473e497ff9527b32f13c65/mutex.h
    public:
        CASlock()
            : state(false)
        {
        }
        void Lock()
        {
            // https://gcc.gnu.org/onlinedocs/gcc-4.2.2/gcc/Atomic-Builtins.html
            while (__sync_bool_compare_and_swap(&state, false, true) == false) {}
        }
        void Unlock()
        {
            __sync_bool_compare_and_swap(&state, true, false);
        }
    private:
        bool state;
};

class FAAlock : public ILock {
    // http://en.wikipedia.org/wiki/Fetch-and-add
    // https://github.com/Maddoc42/BS1415/blob/ec4994079540fec2c78cc50ebd29f8a3db82dff3/machine/ticketlock.cc
    public:
        FAAlock()
            : ticketNumber(0), turn(0)
        {
        }
        void Lock()
        {
            int myturn = __sync_fetch_and_add(&ticketNumber, 1);
            while (turn != myturn) {}
        }
        void Unlock()
        {
            __sync_fetch_and_add(&turn, 1);
        }
    private:
        int ticketNumber;
        int turn;
};

void Incr(ILock& lock, int& num)
{
    while (true)
    {
        int prior;
        lock.Lock();
        prior = ++num;
        lock.Unlock();
        if (prior > hurdle)
        {
            return;
        }
    }
}

class LockTest {
    public:
        LockTest(ILock& lock)
            : lock(lock)
        {
        }
        double GetAverageTestTime(int numThread, int testCnt)
        {
            // http://ko.cppreference.com/w/cpp/chrono
            std::chrono::time_point< std::chrono::system_clock > start, end;
            double timeAcc = 0;
            for (int i = 0; i < testCnt; ++i)
            {
                int num = 0;
                // http://en.cppreference.com/w/cpp/thread/thread/thread
                std::vector< std::thread > threads;
                start = std::chrono::system_clock::now();
                for (int j = 0; j < numThread; ++j)
                {
                    threads.emplace_back(Incr, std::ref(lock), std::ref(num));
                }
                for (int j = 0; j < numThread; ++j)
                {
                    threads[j].join();
                }
                end = std::chrono::system_clock::now();
                std::chrono::duration< double > elapsedSeconds = end - start;
                timeAcc += elapsedSeconds.count();
                //std::cout << threads.size() << std::endl;
                //std::cout << num << std::endl;
            }
            return timeAcc / testCnt;
        }
    private:
        ILock& lock;

};

int main(int argc, char* argv[])
{
    // http://en.cppreference.com/w/cpp/string/basic_string/stol
    //int numThread = std::stoi(std::string(argv[1]));
    {
        std::cout << "TASlock" << std::endl;
        TASlock lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "TASlock2" << std::endl;
        TASlock2 lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "TASlock3" << std::endl;
        TASlock3 lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "TTASlock" << std::endl;
        TTASlock lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "TTASlock2" << std::endl;
        TTASlock2 lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "CASlock" << std::endl;
        CASlock lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
    {
        std::cout << "FAAlock" << std::endl;
        FAAlock lock;
        LockTest lockTest(lock);
        for (int i = 1; i <= numThread; ++i)
        {
            std::cout << lockTest.GetAverageTestTime(i, testCnt) << std::endl;
        }
    }
}

#pragma once

template <typename T>
class Singleton{
private:
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>&) = delete;
protected:
    Singleton(){}

private:
    static T* _inst;
public:
    virtual ~Singleton(){}

    static T& getSingleton(){
        if(_inst == nullptr)
            _inst = new T();
        return *_inst;
    }

    static void deinstantiate(){
        if(_inst != nullptr){
            delete _inst;
            _inst = nullptr;
        }
    }
    
};
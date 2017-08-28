#pragma once

namespace Syx {
  class Config {
  public:
    enum class LogLevel : unsigned char {
      Warning,
      Error
    };
    virtual ~Config() {}
    virtual void * alignedAllocate(size_t size, size_t alignment) = 0;
    virtual void alignedFree(void * data) = 0;

    virtual void log(LogLevel level, char const* format, ...) = 0;
    virtual void assert(char const* exp, char const* function, char const * file, int line, char const* format, ...) = 0;

    virtual unsigned int submitTaskToTheadPool(void(*taskfn)(void * data), void * data) = 0;
    virtual bool isTaskCompeted(unsigned int) = 0;
    virtual void waitTillTaskIsCompeted(unsigned int) = 0;
  };


  template <typename ObjectT>
  inline void deleteObject(Config* _config, ObjectT* _object, size_t _align = 0) {
    if(nullptr != _object) {
      _object->~ObjectT();
      _config->alignedFree(_object);
    }
  }

}//Syx


#define SYX_ALIGNED_NEW(_config, _type, _align)           ::new(_config->alignedAllocate( sizeof(_type), _align) ) _type
#define SYX_ALIGNED_DELETE(_config, _ptr, _align)         Syx::deleteObject(_config, _ptr, _align)

#define SYX_STRINGIZE(_x) #_x

#ifdef _WIN32
#define SYX_FUNCTION_NAME __FUNCSIG__
#define SYX_ISSUE_BREAK() __debugbreak()
#elif
#define SYX_FUNCTION_NAME __PRETTY_FUNCTION__
#define SYX_ISSUE_BREAK() 
#endif



#ifdef _DEBUG
#define SYX_ASSERT(_config, _exp, ...) do{ if( !(_exp)) { _config->Assert( SYX_STRINGIZE(_exp),  SYX_FUNCTION_NAME, __FILE__, __LINE__, __VA_ARGS__); } }while(0) 
#elif
#define SYX_ASSERT(_config, _exp, ...) 
#endif
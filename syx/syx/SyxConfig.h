#pragma once

namespace Syx
{
  class Config
  {
  public:
    enum class LogLevel : unsigned char
    {
      Warning,
      Error
    };
    virtual ~Config() {}
    virtual void * AlignedAllocate(size_t size, size_t alignment) = 0;
    virtual void AlignedFree(void * data) = 0;

    virtual void Log(LogLevel level, char const* format, ...) = 0;
    virtual void Assert(char const* exp, char const* function, char const * file, int line, char const* format, ...) = 0;

    virtual unsigned int SubmitTaskToTheadPool(void(*taskfn)(void * data), void * data) = 0;
    virtual bool IsTaskCompeted(unsigned int) = 0;
    virtual void WaitTillTaskIsCompeted(unsigned int) = 0;


  };


  template <typename ObjectT>
  inline void DeleteObject(Config* _config, ObjectT* _object, size_t _align = 0)
  {
    if (nullptr != _object)
    {
      _object->~ObjectT();
      _config->AlignedFree( _object);
    }
  }

}//Syx


#define SYX_ALIGNED_NEW(_config, _type, _align)           ::new(_config->AlignedAllocate( sizeof(_type), _align) ) _type
#define SYX_ALIGNED_DELETE(_config, _ptr, _align)         Syx::DeleteObject(_config, _ptr, _align)

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
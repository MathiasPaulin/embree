// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "default.h"
#include "device.h"

namespace embree
{
  /*! Implements an API data buffer object. This class may or may not own the data. */
  class Buffer : public RefCount
  {
  public:
    /*! Buffer construction */
    Buffer() 
      : device(nullptr), ptr(nullptr), stride(0), num(0), shared(false), modified(true), userData(0) {}

    /*! Buffer construction */
    Buffer(Device* device, size_t num_in, size_t stride_in, void* ptr_in = nullptr)
      : device(device), stride(stride_in), num(num_in), modified(true), userData(0) 
    {
      if (ptr_in)
      {
        shared = true;
        ptr = (char*)ptr_in;
      }
      else
      {
        shared = false;
        alloc();
      }
    }
    
    /*! Buffer destruction */
    ~Buffer() {
      free();
    }
    
    /*! this class is not copyable */
  private:
    Buffer(const Buffer& other) DELETED; // do not implement
    Buffer& operator =(const Buffer& other) DELETED; // do not implement
    
  public:
    /* inits and allocates the buffer */
    void create(Device* device_in, size_t num_in, size_t stride_in) 
    {
      init(device_in, num_in, stride_in);
      alloc();
    }
    
    /* inits the buffer */
    void init(Device* device_in, size_t num_in, size_t stride_in) 
    {
      free();
      device = device_in;
      ptr = nullptr;
      num = num_in;
      stride = stride_in;
      shared = false;
      modified = true;
    }

    /*! sets shared buffer */
    void set(Device* device_in, void* ptr_in, size_t num_in, size_t stride_in)
    {
      free();
      device = device_in;
      ptr = (char*)ptr_in;
      if (num_in != (size_t)-1)
        num = num_in;
      stride = stride_in;
      shared = true;
    }
    
    /*! allocated buffer */
    void alloc()
    {
      if (device)
        device->memoryMonitor(this->bytes(), false);
      size_t b = (this->bytes()+15) & ssize_t(-16);
      ptr = (char*)alignedMalloc(b);
    }
    
    /*! frees the buffer */
    void free()
    {
      if (shared) return;
      alignedFree(ptr); 
      if (device)
        device->memoryMonitor(-ssize_t(this->bytes()), true);
      ptr = nullptr;
    }
    
    /*! gets buffer pointer */
    void* data()
    {
      /* report error if buffer is not existing */
      if (!device)
        throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "invalid buffer specified");
      
      /* return buffer */
      return ptr;
    }

    /*! returns pointer to first element */
    __forceinline const char* getPtr() const {
      return ptr;
    }

    /*! returns pointer to ith element */
    __forceinline const char* getPtr(size_t i) const
    {
      assert(i<num);
      return ptr + i*stride;
    }

    /*! returns the number of elements of the buffer */
    __forceinline size_t size() const { 
      return num; 
    }

    /*! returns the number of bytes of the buffer */
    __forceinline size_t bytes() const { 
      return num*stride; 
    }

    /*! returns buffer stride */
    __forceinline unsigned getStride() const
    {
      assert(stride <= unsigned(inf));
      return unsigned(stride);
    }
    
    /*! mark buffer as modified or unmodified */
    __forceinline void setModified(bool b) {
      modified = b;
    }
    
    /*! mark buffer as modified or unmodified */
    __forceinline bool isModified() const {
      return modified;
    }
    
    /*! returns true of the buffer is not empty */
    __forceinline operator bool() const { 
      return ptr; 
    }
    
    /*! checks padding to 16 byte check, fails hard */
    __forceinline void checkPadding16() const 
    {
      if (ptr && size()) 
        volatile int MAYBE_UNUSED w = *((int*)getPtr(size()-1)+3); // FIXME: is failing hard avoidable?
    }

  public:
    Device* device;  //!< device to report memory usage to 
    char* ptr;       //!< pointer to buffer data
    size_t stride;   //!< stride of the buffer in bytes
    size_t num;      //!< number of elements in the buffer
    bool shared;     //!< set if memory is shared with application
    bool modified;   //!< true if the buffer got modified
    int userData;    //!< special data
  };

  /*! An untyped contiguous range of a buffer. This class does not own the buffer content. */
  class RawBufferView
  {
  public:
    /*! Buffer construction */
    RawBufferView(size_t num = 0, size_t stride = 0, RTCFormat format = RTC_FORMAT_UNDEFINED)
      : ptr_ofs(nullptr), stride(stride), num(num), format(format) {}

  public:
    /*! sets the buffer view */
    void set(const Ref<Buffer>& buffer_in, size_t offset_in, size_t num_in, RTCFormat format_in)
    {
      if ((offset_in + buffer_in->stride * num_in) > (buffer_in->stride * buffer_in->num))
        throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "buffer range out of bounds");

      ptr_ofs = buffer_in->ptr + offset_in;
      stride = buffer_in->stride;
      num = num_in;
      format = format_in;
      buffer = buffer_in;
    }

    void set(const Ref<Buffer>& buffer_in, RTCFormat format_in)
    {
      set(buffer_in, 0, buffer_in->size(), format_in);
    }

    /*! returns pointer to the first element */
    __forceinline const char* getPtr() const {
      return ptr_ofs;
    }

    /*! returns pointer to the i'th element */
    __forceinline const char* getPtr(size_t i) const
    {
      assert(i<num);
      return ptr_ofs + i*stride;
    }

    /*! returns the number of elements of the buffer */
    __forceinline size_t size() const { 
      return num; 
    }

    /*! returns the number of bytes of the buffer */
    __forceinline size_t bytes() const { 
      return num*stride; 
    }
    
    /*! returns the buffer stride */
    __forceinline unsigned getStride() const
    {
      assert(stride <= unsigned(inf));
      return unsigned(stride);
    }

    /*! return the buffer format */
    __forceinline RTCFormat getFormat() const {
      return format;
    }

    /*! returns true of the buffer is not empty */
    __forceinline operator bool() const { 
      return ptr_ofs; 
    }

  protected:
    char* ptr_ofs;      //!< base pointer plus offset
    size_t stride;      //!< stride of the buffer in bytes
    size_t num;         //!< number of elements in the buffer
    RTCFormat format;   //!< format of the buffer
    Ref<Buffer> buffer; //!< reference to the parent buffer (optional)
  };

  /*! A typed contiguous range of a buffer. This class does not own the buffer content. */
  template<typename T>
  class BufferView : public RawBufferView
  {
  public:
    typedef T value_type;

    BufferView(size_t num = 0, size_t stride = 0) 
      : RawBufferView(num, stride) {}

    /*! access to the ith element of the buffer */
    __forceinline       T& operator [](size_t i)       { assert(i<num); return *(T*)(ptr_ofs + i*stride); }
    __forceinline const T& operator [](size_t i) const { assert(i<num); return *(T*)(ptr_ofs + i*stride); }
  };

  template<>
  class BufferView<Vec3fa> : public RawBufferView
  {
  public:
    typedef Vec3fa value_type;

    BufferView(size_t num = 0, size_t stride = 0) 
      : RawBufferView(num, stride) {}

    /*! access to the ith element of the buffer */
    __forceinline const Vec3fa operator [](size_t i) const
    {
      assert(i<num);
      return Vec3fa(vfloat4::loadu((float*)(ptr_ofs + i*stride)));
    }
    
    /*! writes the i'th element */
    __forceinline void store(size_t i, const Vec3fa& v)
    {
      assert(i<num);
      vfloat4::storeu((float*)(ptr_ofs + i*stride), (vfloat4)v);
    }
  };
}

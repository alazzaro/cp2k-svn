/******************************************************************************
** Copyright (c) 2014-2015, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#if defined(LIBXSTREAM_EXPORTED) || defined(__LIBXSTREAM)
#include "libxstream_event.hpp"
#include "libxstream_workitem.hpp"

#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD)
# include <offload.h>
#endif


libxstream_event::libxstream_event()
{
  m_slots = new slot_type*[LIBXSTREAM_MAX_NTHREADS];
  std::fill_n(m_slots, LIBXSTREAM_MAX_NTHREADS, static_cast<slot_type*>(0));
  m_expected = new size_t[LIBXSTREAM_MAX_NTHREADS];
  std::fill_n(m_expected, LIBXSTREAM_MAX_NTHREADS, 0);
}


libxstream_event::~libxstream_event()
{
  //LIBXSTREAM_CHECK_CALL_ASSERT(wait());
  for (int i = 0; i < LIBXSTREAM_MAX_NTHREADS; ++i) {
    delete[] m_slots[i];
  }
  delete[] m_slots;
  delete[] m_expected;
}


void libxstream_event::swap(libxstream_event& other)
{
  std::swap(m_slots, other.m_slots);
  std::swap(m_expected, other.m_expected);
}


int libxstream_event::enqueue(libxstream_stream& stream, bool reset, bool sync)
{ 
  const int thread = this_thread_id();
  size_t& n = m_expected[thread];
  if (reset) {
    n = 0;
  }
  LIBXSTREAM_CHECK_CONDITION((LIBXSTREAM_MAX_NDEVICES)*(LIBXSTREAM_MAX_NSTREAMS) >= n);

  LIBXSTREAM_ASYNC_BEGIN
  {
    int& status = LIBXSTREAM_ASYNC_QENTRY.status();
#if defined(LIBXSTREAM_OFFLOAD)
    if (0 <= LIBXSTREAM_ASYNC_DEVICE) {
      if (LIBXSTREAM_ASYNC_READY) {
#       pragma offload LIBXSTREAM_ASYNC_TARGET_SIGNAL //out(status)
        {
          status = LIBXSTREAM_ERROR_NONE;
        }
      }
      else {
#       pragma offload LIBXSTREAM_ASYNC_TARGET_WAIT //out(status)
        {
          status = LIBXSTREAM_ERROR_NONE;
        }
      }
    }
    else
#endif
    {
      status = LIBXSTREAM_ERROR_NONE;
    }
  }
  LIBXSTREAM_ASYNC_END(stream, LIBXSTREAM_CALL_DEFAULT | LIBXSTREAM_CALL_ERROR | (sync ? LIBXSTREAM_CALL_SYNC : 0), work);

  slot_type *const slots = slot(thread);
  LIBXSTREAM_ASSERT(0 != slots);

  slots[n] = &LIBXSTREAM_ASYNC_INTERNAL(work);
  const libxstream_workitem *const item = work.item();
  n += 0 != item ? 1 : 0;

  const int result = 0 != item ? LIBXSTREAM_ERROR_NONE : work.status();
  LIBXSTREAM_ASSERT(LIBXSTREAM_ERROR_NONE == result);
  return result;
}


int libxstream_event::query(bool& occurred, const libxstream_stream* exclude) const
{
  occurred = true; // everythig "occurred" if nothing is expected
  const int thread = this_thread_id();
  const size_t n = m_expected[thread];

  if (0 < n) {
    LIBXSTREAM_CHECK_CONDITION((LIBXSTREAM_MAX_NDEVICES)*(LIBXSTREAM_MAX_NSTREAMS) >= n);
    const slot_type *const slots = m_slots[thread];

    if (slots) {
      for (size_t i = 0; i < n; ++i) {
        const slot_type slot = slots[i];
        const libxstream_workitem *const item = slot ? slot->item() : 0;
        if (0 != item && exclude != item->stream()) {
          occurred = false;
          i = n; // break
        }
      }
    }
  }

  return LIBXSTREAM_ERROR_NONE;
}


int libxstream_event::wait(const libxstream_stream* exclude)
{
  int result = LIBXSTREAM_ERROR_NONE;
  const int thread = this_thread_id();
  size_t& n = m_expected[thread];

  if (0 < n) {
    LIBXSTREAM_CHECK_CONDITION((LIBXSTREAM_MAX_NDEVICES)*(LIBXSTREAM_MAX_NSTREAMS) >= n);

    slot_type *const slots = slot(thread);

    for (size_t i = 0; i < n; ++i) {
      const slot_type slot = slots[i];
      const libxstream_workitem *const item = slot ? slot->item() : 0;
      if (0 != item && exclude != item->stream()) {
        result = slot->wait();
        LIBXSTREAM_CHECK_ERROR(result);
        slots[i] = 0;
      }
    }

    n = 0;
  }

  LIBXSTREAM_ASSERT(LIBXSTREAM_ERROR_NONE == result);
  return result;
}


libxstream_event::slot_type* libxstream_event::slot(size_t i)
{
  LIBXSTREAM_ASSERT(i < (LIBXSTREAM_MAX_NTHREADS));
  libxstream_event::slot_type*& slot = m_slots[i];
  if (0 == slot) {
    slot = new slot_type[(LIBXSTREAM_MAX_NDEVICES)*(LIBXSTREAM_MAX_NSTREAMS)];
    std::fill_n(slot, (LIBXSTREAM_MAX_NDEVICES)*(LIBXSTREAM_MAX_NSTREAMS), slot_type(0));
  }
  LIBXSTREAM_ASSERT(0 != slot);
  return slot;
}

#endif // defined(LIBXSTREAM_EXPORTED) || defined(__LIBXSTREAM)

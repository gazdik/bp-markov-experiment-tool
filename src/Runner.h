/*
 * Copyright (C) 2016 Peter Gazdik
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef RUNNER_H_
#define RUNNER_H_

#define __CL_ENABE_EXCEPTIONS

#include <CL/cl.hpp>

#include <vector>
#include <string>

#include "CLMarkovPassGen.h"

struct RunnerOptions : public CLMarkovPassGenOptions
{
  unsigned gws = 1024;
  unsigned platform = 0;
};

class Runner
{
public:
  Runner(RunnerOptions & options);
  ~Runner();

  void Run();

private:
  CLMarkovPassGen * _passgen;

  unsigned _gws;

  cl::Context _context;
  std::vector<cl::CommandQueue> _command_queues;
  std::vector<cl::Kernel> _passgen_kernels;
  std::vector<cl::Event> _events;

  std::vector<cl::Buffer> _passwords_buffers;
  cl_uchar * _passwords;
  size_t _passwords_size;

  std::vector<cl::Buffer> _flags_buffers;
  cl_uchar * _flags;
  size_t _flags_size;

  cl_uchar _flag = 0;
  cl::Buffer _flag_buffer;

  void createContext(unsigned platform_number);
};

#endif /* RUNNER_H_ */

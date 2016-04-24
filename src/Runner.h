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

#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>

#include <vector>
#include <string>

#include "CLMarkovPassGen.h"
#include "Cracker.h"

#define PASS_EXTRA_BYTES 1
#define PASS_PAYLOAD_OFFSET 1
#define PASS_LENGTH_OFFSET 0

#define FLAG_RUN 0
#define FLAG_END 1

class Runner
{
public:
  struct Options : public CLMarkovPassGen::Options, Cracker::Options
  {
    unsigned gws = 1024;
    std::string devices = "0";
    bool verbose = false;
  };

  Runner(Options & options);
  ~Runner();

  void Run();

  void Details();

private:
  CLMarkovPassGen * _passgen;
  Cracker * _cracker;

  unsigned _gws;
  bool _verbose;
  unsigned _selected_platform;
  std::vector<unsigned> _selected_device;

  cl::Context _context;
  std::vector<cl::CommandQueue> _command_queue;
  std::vector<cl::Kernel> _passgen_kernel;
  std::vector<cl::Kernel> _cracker_kernel;
  std::vector<cl::Device> _device;

  std::vector<cl_uchar *> _passwords;
  cl_uint _passwords_entry_size;
  size_t _passwords_size;
  unsigned _passwords_num_items;
  std::vector<cl::Buffer> _passwords_buffer;

  std::vector<cl_uchar *> _flag;
  size_t _flag_size;
  std::vector<cl::Buffer> _flag_buffer;

  void createContext();
  void initGenerator();
  void initCracker();

  void runThread(unsigned thread_number);

  void parseOptions(Options & options);
};

#endif /* RUNNER_H_ */

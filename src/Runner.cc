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

#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>

#include <thread>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "Runner.h"

using namespace std;

Runner::Runner(Options & options) :
    _gws { options.gws }, _verbose { options.verbose }
{
  _passgen = new CLMarkovPassGen { options };
  _cracker = new Cracker { options };

  createContext(options.platform);
  initGenerator();
  initCracker();
}

void Runner::Run()
{

  vector<thread> threads;
  unsigned num_threads = _device.size();

  for (unsigned i = 0; i < num_threads; i++)
  {
    threads.push_back(thread {&Runner::runThread, this, i});
  }

  // Wait for all threads to complete
  for (auto &i: threads)
  {
    i.join();
  }

  unsigned num_devices = _device.size();
  unsigned found = 0;

  for (int d = 0; d < num_devices; d++)
  {
    for (int i = 0; i < _cnt_found_num_items; i++)
    {
      found += _cnt_found[d][i];
    }
  }
  cout << "Found passwords: " << found << endl;
}

void Runner::createContext(unsigned platform_number)
{
  std::vector<cl::Platform> platform_list;

  // Get list of all available platforms
  cl::Platform::get(&platform_list);

  // Get list of all devices available on platform
  platform_list[platform_number].getDevices( CL_DEVICE_TYPE_GPU, &_device);

  // Create contexts for every device in selected platform
  cl_context_properties context_properties[] = {
  CL_CONTEXT_PLATFORM,
      (cl_context_properties) (platform_list[platform_number])(), 0 };
  _context = cl::Context { CL_DEVICE_TYPE_GPU, context_properties };

  // Create command queues
  for (int i = 0; i < _device.size(); i++)
  {
    cl::CommandQueue queue { _context, _device[i] };
    _command_queue.push_back(queue);
  }
}

Runner::~Runner()
{
  for (auto i : _passwords)
    delete[] i;

  for (auto i : _flag)
    delete i;

  for (auto i : _cnt_found)
    delete[] i;

  delete _passgen;
  delete _cracker;
}

void Runner::initGenerator()
{
  // Calculate step for generator
  cl_uint step = _gws * _device.size();

  unsigned num_devices = _device.size();

  // Get kernel source for generator
  ifstream source_file { _passgen->GetKernelSource(), ifstream::in };
  if (not source_file.is_open())
    throw invalid_argument { "Kernel code missing" };
  string source { (istreambuf_iterator<char>(source_file)), istreambuf_iterator<
      char>() };

  // Create and build program
  cl::Program program { _context, source, true };
  program.build("-Werror");

  // Create kernel's memory objects
  _passwords_entry_size = _passgen->MaxPasswordLength() + 1;
  _passwords_num_items = _passwords_entry_size * _gws;
  _passwords_size = _passwords_num_items * sizeof(cl_uchar);

  _flag_size = sizeof(cl_uchar);

  for (int i = 0; i < num_devices; i++)
  {
    cl_uchar *passwords = new cl_uchar[_passwords_num_items];
    memset(passwords, 0, _passwords_size);
    _passwords.push_back(passwords);

    cl::Buffer passwords_buffer { _context, CL_MEM_READ_WRITE, _passwords_size };
    _command_queue[i].enqueueWriteBuffer(passwords_buffer, CL_TRUE, 0,
                                         _passwords_size, passwords);
    _passwords_buffer.push_back(passwords_buffer);

    cl_uchar *flag = new cl_uchar;
    memset(flag, 0, _flag_size);
    _flag.push_back(flag);

    cl::Buffer flag_buffer = cl::Buffer { _context,
    CL_MEM_READ_WRITE, _flag_size };
    _command_queue[i].enqueueWriteBuffer(flag_buffer, CL_TRUE, 0, _flag_size,
                                         flag);
    _flag_buffer.push_back(flag_buffer);
  }

  // Create kernels
  for (int i = 0; i < _device.size(); i++)
  {
    cl::Kernel kernel { program, _passgen->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flag_buffer[i]);

    _passgen_kernel.push_back(kernel);
  }

  // Initialize generator's kernel
  for (int i = 0; i < _passgen_kernel.size(); i++)
  {
    _passgen->InitKernel(_passgen_kernel[i], _command_queue[i], _context, _gws,
                         step);
  }
}

void Runner::initCracker()
{
  unsigned num_devices = _device.size();

  // Get kernel source for generator
  ifstream source_file { _cracker->GetKernelSource(), ifstream::in };
  if (not source_file.is_open())
    throw invalid_argument { "Cracker's kernel code missing" };
  string source { (istreambuf_iterator<char>(source_file)), istreambuf_iterator<
      char>() };

  // Create and build program
  cl::Program program { _context, source, true };
  program.build("-Werror");

  // Create kernel's memory objects
  _cnt_found_num_items = _gws;
  _cnt_found_size = _cnt_found_num_items * sizeof(cl_uint);

  for (int i = 0; i < num_devices; i++)
  {
    cl_uint *cnt_found = new cl_uint[_cnt_found_num_items];
    memset(cnt_found, 0, _cnt_found_size);
    _cnt_found.push_back(cnt_found);

    cl::Buffer cnt_found_buffer = cl::Buffer { _context, CL_MEM_READ_WRITE,
        _cnt_found_size };
    _command_queue[i].enqueueWriteBuffer(cnt_found_buffer, CL_TRUE, 0,
                                         _cnt_found_size, cnt_found);
    _cnt_found_buffer.push_back(cnt_found_buffer);
  }

  // Create kernels
  for (int i = 0; i < num_devices; i++)
  {
    cl::Kernel kernel { program, _cracker->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flag_buffer[i]);
    kernel.setArg(3, _cnt_found_buffer[i]);

    _cracker_kernel.push_back(kernel);
  }

  // Initialize generator's kernel
  for (int i = 0; i < num_devices; i++)
  {
    _cracker->InitKernel(_cracker_kernel[i], _command_queue[i], _context);
  }
}

void Runner::runThread(unsigned i)
{
  vector<cl::Event> passgen_events;
  vector<cl::Event> cracker_events;
  bool cracked_pass = false;

  while (*_flag[i] == FLAG_NONE)
  {
    passgen_events.clear();
    cracker_events.clear();

    cl::Event event;
    _command_queue[i].enqueueNDRangeKernel(_passgen_kernel[i], cl::NullRange,
                                           cl::NDRange(_gws), cl::NullRange,
                                           nullptr, &event);

    passgen_events.push_back(event);

    _command_queue[i].enqueueNDRangeKernel(_cracker_kernel[i], cl::NullRange,
                                           cl::NDRange(_gws), cl::NullRange,
                                           &passgen_events, &event);

    cracker_events.push_back(event);

    if (_verbose && cracked_pass)
    {
      cracked_pass = false;
      printCrackedPasswords(i);
    }

    _command_queue[i].enqueueReadBuffer(_flag_buffer[i], CL_TRUE, 0, _flag_size,
                                        _flag[i], &cracker_events);

    if (*_flag[i] == FLAG_CRACKED || *_flag[i] == FLAG_CRACKED_END)
    {
      cracked_pass = true;
      _command_queue[i].enqueueReadBuffer(_passwords_buffer[i], CL_TRUE, 0,
                                          _passwords_size, _passwords[i],
                                          &cracker_events);
    }

    if (*_flag[i] == FLAG_CRACKED)
    {
      *_flag[i] = FLAG_NONE;
      _command_queue[0].enqueueWriteBuffer(_flag_buffer[i], CL_FALSE, 0,
                                           _flag_size, _flag[i]);
    }
  }

  _command_queue[i].enqueueReadBuffer(_cnt_found_buffer[i], CL_TRUE, 0,
                                      _cnt_found_size, _cnt_found[i],
                                      &cracker_events);

  if (_verbose && cracked_pass)
    printCrackedPasswords(i);
}

void Runner::printCrackedPasswords(unsigned thread_number)
{
  _mutex_output.lock();

  for (int i = 0; i < _gws; i++)
  {
    unsigned index = i * _passwords_entry_size;
    unsigned length = _passwords[thread_number][index];

    if (length == 0)
      continue;

    for (int j = 1; j <= length; j++)
    {
      cout << (char) _passwords[thread_number][index + j];
    }
    cout << "\n";
  }

  _mutex_output.unlock();
}

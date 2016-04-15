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

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <pthread.h>

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

  unsigned num_threads = _device.size();
  pthread_t threads[num_threads];
  thread_args args[num_threads];

  for (unsigned i = 0; i < num_threads; i++)
  {
    args[i].runner = this;
    args[i].thread_number = i;
    if (pthread_create(&threads[i], nullptr, &Runner::start_thread, &args[i]))
      throw runtime_error { "pthread_create" };
  }

  // Wait for all threads to complete
  for (unsigned i = 0; i < num_threads; i++)
  {
    pthread_join(threads[i], nullptr);
  }

  _command_queue[0].enqueueReadBuffer(_found_buffer[0], CL_TRUE, 0,
                                      _found_buffer_size * _device.size(), _found);

  unsigned found = 0;
  for (int i = 0; i < _found_items; i++)
  {
    found += _found[i];
  }
  cout << "Found: " << found << endl;
}

void Runner::createContext(unsigned platform_number)
{
  std::vector<cl::Platform> platform_list;

  // Get list of all available platforms
  cl::Platform::get(&platform_list);

  // Get list of all devices available on platform
  platform_list[platform_number].getDevices( CL_DEVICE_TYPE_GPU, &_device);

  // Create context
  cl_context_properties context_properties[] = {
  CL_CONTEXT_PLATFORM,
      (cl_context_properties) (platform_list[platform_number])(), 0 };
  _context = cl::Context { CL_DEVICE_TYPE_ALL, context_properties };

  // Create command queues
  for (auto & device : _device)
  {
    cl::CommandQueue queue { _context, device };
    _command_queue.push_back(queue);
  }
}

Runner::~Runner()
{
  delete[] _passwords;
  delete _passgen;
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
  _passwords_size = _passwords_entry_size * _gws * _device.size();
  _passwords = new cl_uchar[_passwords_size];
  memset(_passwords, 0, sizeof(cl_uchar) * _passwords_size);

  _passwords_buffer_size = (_passgen->MaxPasswordLength() + 1)
      * sizeof(cl_uchar) * _gws;
  cl::Buffer passwords_buffer { _context,
  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, _passwords_buffer_size
      * _device.size(), _passwords };
  _passwords_buffer.push_back(passwords_buffer);

  _flags_buffer_size = sizeof(u_char);
  _flags = new u_char[_flags_buffer_size * num_devices];
  memset(_flags, 0, _flags_buffer_size * num_devices);
  cl::Buffer flags_buffer = cl::Buffer { _context,
  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, _flags_buffer_size * num_devices,
      _flags };
  _flags_buffer.push_back(flags_buffer);

  // If there is more devices than just one, create also subbuffers
  for (int i = 1; i < _device.size(); i++)
  {
    cl_buffer_region passwords_region = { _passwords_buffer_size * i,
        _passwords_buffer_size };
    cl::Buffer passwords_subbuffer = passwords_buffer.createSubBuffer(
        CL_MEM_WRITE_ONLY, CL_BUFFER_CREATE_TYPE_REGION, &passwords_region);

    cl_buffer_region flags_region =
        { _flags_buffer_size * i, _flags_buffer_size };

    cl::Buffer flags_subbufer = flags_buffer.createSubBuffer(CL_MEM_WRITE_ONLY,
    CL_BUFFER_CREATE_TYPE_REGION,
                                                             &flags_region);
    _passwords_buffer.push_back(passwords_subbuffer);
    _flags_buffer.push_back(flags_subbufer);
  }

  // Create kernels
  for (int i = 0; i < _device.size(); i++)
  {
    cl::Kernel kernel { program, _passgen->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flags_buffer[i]);

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
  //  passgen_program.build("-Werror -g -s \"/home/gazdik/Documents/FIT/IBP/workspace/clMarkovPassGen/bin/kernels/CLMarkovPassGen.cl\"");
  program.build("-Werror");

  // Create kernel's memory objects
  _found_items = _gws * num_devices;
  _found_buffer_size = _gws * sizeof(cl_uint);
  _found = new cl_uint[_found_buffer_size * num_devices];
  memset(_found, 0, _found_buffer_size * num_devices);

  cl::Buffer found_buffer = cl::Buffer { _context, CL_MEM_READ_WRITE
      | CL_MEM_COPY_HOST_PTR, _found_buffer_size * num_devices, _found };

  _found_buffer.push_back(found_buffer);

  // If there is more devices than just one, create also subbuffers
  for (int i = 1; i < _device.size(); i++)
  {
    cl_buffer_region found_region =
        { _found_buffer_size * i, _found_buffer_size };

    cl::Buffer found_subbufer = found_buffer.createSubBuffer(CL_MEM_READ_WRITE,
    CL_BUFFER_CREATE_TYPE_REGION,&found_region);
    _found_buffer.push_back(found_subbufer);

  }

  // Create kernels
  for (int i = 0; i < _device.size(); i++)
  {
    cl::Kernel kernel { program, _cracker->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flags_buffer[i]);
    kernel.setArg(3, _found_buffer[i]);

    _cracker_kernel.push_back(kernel);
  }

  // Initialize generator's kernel
  for (int i = 0; i < _cracker_kernel.size(); i++)
  {
    _cracker->InitKernel(_cracker_kernel[i], _command_queue[i], _context);
  }
}

void Runner::runThread(unsigned i)
{
  vector<cl::Event> passgen_events;
  vector<cl::Event> cracker_events;

  while (_flags[i] == FLAG_NONE)
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

    _command_queue[i].enqueueReadBuffer(_flags_buffer[i], CL_TRUE, 0,
                                        _flags_buffer_size, &_flags[i],
                                        &cracker_events);

    if (_verbose)
    {
      if (_flags[i] == FLAG_CRACKED || _flags[i] == FLAG_CRACKED_END)
      {
        cl_uchar *passwords = &_passwords[i * _gws];
        _command_queue[i].enqueueReadBuffer(_passwords_buffer[i], CL_TRUE, 0,
                                            _passwords_buffer_size,
                                            passwords,
                                            &cracker_events);

        for (int i = 0; i < _gws; i++)
        {
          unsigned index = i * _passwords_entry_size;
          unsigned length = passwords[index];

          if (length == 0)
            continue;

          for (int j = 1; j <= length; j++)
          {
            cout << (char) passwords[index + j];
          }
          cout << "\n";
        }

      }
    }

    if (_flags[i] == FLAG_CRACKED)
    {
      _flags[i] = FLAG_NONE;
      _command_queue[0].enqueueWriteBuffer(_flags_buffer[i], CL_FALSE, 0,
                                           sizeof(cl_uchar), &_flags[i]);
    }
  }
}

void* Runner::start_thread(void* arg)
{
  thread_args *args = static_cast<thread_args *>(arg);

  args->runner->runThread(args->thread_number);

  pthread_exit(NULL);

  return (nullptr);
}

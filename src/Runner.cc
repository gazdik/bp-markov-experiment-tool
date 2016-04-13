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
  vector<cl::Event> passgen_events;
  vector<cl::Event> cracker_events;
  while (_flag == FLAG_NONE)
  {
    passgen_events.clear();
    cracker_events.clear();

    for (int i = 0; i < _command_queues.size(); i++)
    {
      cl::Event event;
      _command_queues[i].enqueueNDRangeKernel(_passgen_kernels[i],
                                              cl::NullRange, cl::NDRange(_gws),
                                              cl::NullRange, nullptr, &event);

      passgen_events.push_back(event);
    }

    for (int i = 0; i < _command_queues.size(); i++)
    {
      cl::Event event;
      _command_queues[i].enqueueNDRangeKernel(_cracker_kernels[i],
                                              cl::NullRange, cl::NDRange(_gws),
                                              cl::NullRange, &passgen_events,
                                              &event);

      cracker_events.push_back(event);
    }

    _command_queues[0].enqueueReadBuffer(_flag_buffer, CL_TRUE, 0,
                                         sizeof(_flag), &_flag,
                                         &cracker_events);

    if (_verbose)
    {
      if (_flag == FLAG_CRACKED || _flag == FLAG_CRACKED_END)
      {
        _command_queues[0].enqueueReadBuffer(_passwords_buffers[0], CL_TRUE, 0,
                                             _passwords_size * sizeof(cl_uchar),
                                             _passwords, &cracker_events);

        for (int i = 0; i < _gws * _command_queues.size(); i++)
        {
          unsigned index = i * _passwords_entry_size;
          unsigned length = _passwords[index];

          if (length == 0)
            continue;

          for (int j = 1; j <= length; j++)
          {
            cout << (char) _passwords[index + j];
          }
          cout << "\n";
        }

      }
    }

    if (_flag == FLAG_CRACKED)
    {
      _flag = FLAG_NONE;
      _command_queues[0].enqueueWriteBuffer(_flag_buffer, CL_FALSE, 0,
                                            sizeof(_flag), &_flag);
    }
  }

  _command_queues[0].enqueueReadBuffer(_found_buffer, CL_TRUE, 0,
                                       _found_buffer_size, _found, &cracker_events);


  unsigned found = 0;
  for (int i = 0; i < _found_buffer_items; i++)
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
  platform_list[platform_number].getDevices( CL_DEVICE_TYPE_GPU, &_devices);

// Create context
  cl_context_properties context_properties[] = {
  CL_CONTEXT_PLATFORM,
      (cl_context_properties) (platform_list[platform_number])(), 0 };
  _context = cl::Context { CL_DEVICE_TYPE_ALL, context_properties };

// TODO Remove
  cl::Device device = _devices[0];
  _devices.clear();
  _devices.push_back(device);

// Create command queues
  for (auto & device : _devices)
  {
    cl::CommandQueue queue { _context, device };
    _command_queues.push_back(queue);
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
  cl_uint step = _gws * _devices.size();

// Get kernel source for generator
  ifstream source_file { _passgen->GetKernelSource(), ifstream::in };
  if (not source_file.is_open())
    throw invalid_argument { "Kernel code missing" };
  string source { (istreambuf_iterator<char>(source_file)), istreambuf_iterator<
      char>() };

// Create and build program
  cl::Program program { _context, source, true };
//  passgen_program.build("-Werror -g -s \"/home/gazdik/Documents/FIT/IBP/workspace/clMarkovPassGen/bin/kernels/CLMarkovPassGen.cl\"");
  program.build("-Werror");

// Create kernel's memory objects
  _passwords_entry_size = _passgen->MaxPasswordLength() + 1;
  _passwords_size = _passwords_entry_size * _gws * _devices.size();
  _passwords = new cl_uchar[_passwords_size];
  memset(_passwords, 0, sizeof(cl_uchar) * _passwords_size);

  size_t passwords_buffer_size = (_passgen->MaxPasswordLength() + 1)
      * sizeof(cl_uchar) * _gws;
  cl::Buffer passwords_buffer { _context,
  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, passwords_buffer_size
      * _devices.size(), _passwords };
  _passwords_buffers.push_back(passwords_buffer);

  _flag_buffer = cl::Buffer { _context,
  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(_flag), &_flag };

// If there is more devices than just one, create also subbuffers
  for (int i = 1; i < _devices.size(); i++)
  {
    cl_buffer_region passwords_region = { passwords_buffer_size * i,
        passwords_buffer_size };
    cl::Buffer passwords_subbuffer = passwords_buffer.createSubBuffer(
        CL_MEM_WRITE_ONLY, CL_BUFFER_CREATE_TYPE_REGION, &passwords_region);

    _passwords_buffers.push_back(passwords_subbuffer);
  }

// Create kernels
  for (int i = 0; i < _devices.size(); i++)
  {
    cl::Kernel kernel { program, _passgen->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffers[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flag_buffer);

    _passgen_kernels.push_back(kernel);
  }

// Initialize generator's kernel
  for (int i = 0; i < _passgen_kernels.size(); i++)
  {
    _passgen->InitKernel(_passgen_kernels[i], _command_queues[i], _context,
                         _gws, step);
  }
}

void Runner::initCracker()
{
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
  _found_buffer_items = _gws * _devices.size();
  _found_buffer_size = _found_buffer_items * sizeof(cl_uint);
  _found = new cl_uint[_found_buffer_size];
  memset(_found, 0, sizeof(cl_uint) * _found_buffer_size);

  _found_buffer = cl::Buffer { _context, CL_MEM_READ_WRITE
      | CL_MEM_COPY_HOST_PTR, _found_buffer_size, _found };

  // TODO
  // If there is more devices than just one, create also subbuffers
  for (int i = 1; i < _devices.size(); i++)
  {
    throw runtime_error { "There is more devices!" };

  }

// Create kernels
  for (int i = 0; i < _devices.size(); i++)
  {
    cl::Kernel kernel { program, _cracker->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffers[i]);
    kernel.setArg(1, _passwords_entry_size);
    kernel.setArg(2, _flag_buffer);
    kernel.setArg(3, _found_buffer);

    _cracker_kernels.push_back(kernel);
  }

// Initialize generator's kernel
  for (int i = 0; i < _cracker_kernels.size(); i++)
  {
    _cracker->InitKernel(_cracker_kernels[i], _command_queues[i], _context);
  }
}

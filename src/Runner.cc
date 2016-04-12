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

Runner::Runner(RunnerOptions & options) :
    _gws { options.gws }
{
  _passgen = new CLMarkovPassGen { options };

  createContext(options.platform);
}

void Runner::Run()
{
  _events.clear();
  while (_flag == 0)
  {
    for (int i = 0; i < _command_queues.size(); i++)
    {
      cl::Event event;
      _command_queues[i].enqueueNDRangeKernel(_passgen_kernels[i],
                                              cl::NullRange, cl::NDRange(_gws),
                                              cl::NullRange, nullptr, &event);

      _events.push_back(event);
    }

//    _command_queues[0].enqueueReadBuffer(_passwords_buffers[0], CL_TRUE, 0,
//                                         _passwords_size * sizeof(cl_uchar),
//                                         _passwords, &_events);

    _command_queues[0].enqueueReadBuffer(_flag_buffer, CL_TRUE, 0,
                                         sizeof(cl_uchar), &_flag, &_events);

//    unsigned max_pass_length = _passgen->MaxPasswordLength();
//    for (int i = 0; i < _gws * _command_queues.size(); i++)
//    {
//      unsigned index = i * (max_pass_length + 1);
//      unsigned length = _passwords[index];
//
//      for (int j = 1; j <= length; j++)
//      {
//        cout << (char) _passwords[index + j];
//      }
//      cout << endl;
//    }
  }

}

void Runner::createContext(unsigned platform_number)
{
  std::vector<cl::Platform> platform_list;

  // Get list of all available platforms
  cl::Platform::get(&platform_list);

  // Get list of all devices available on platform
  vector<cl::Device> devices;
  platform_list[platform_number].getDevices( CL_DEVICE_TYPE_ALL, &devices);

  // Calculate step for generator
  cl_uint step = _gws * devices.size();

  // Create context
  cl_context_properties context_properties[] = {
  CL_CONTEXT_PLATFORM,
      (cl_context_properties) (platform_list[platform_number])(), 0 };
  _context = cl::Context { CL_DEVICE_TYPE_ALL, context_properties };

  // Get kernel source for generator
  KernelCode passgen_code = _passgen->GetKernelCode();
  ifstream passgen_file { passgen_code.file_name, ifstream::in };
  if (not passgen_file.is_open())
    throw invalid_argument { "Kernel code missing" };
  string passgen_source { (istreambuf_iterator<char>(passgen_file)),
      istreambuf_iterator<char>() };

  // Create and create program
  cl::Program passgen_program { _context, passgen_source, true };
  passgen_program.build("-Werror");
//  passgen_program.build("-Werror -g -s \"/home/gazdik/Documents/FIT/IBP/workspace/clMarkovPassGen/bin/kernels/CLMarkovPassGen.cl\"");

// Create buffers for passwords and flags
  _passwords_size = (_passgen->MaxPasswordLength() + 1) * _gws * devices.size();
  _passwords = new cl_uchar[_passwords_size];
  memset(_passwords, 0, sizeof(cl_uchar) * _passwords_size);

  size_t passwords_buffer_size = (_passgen->MaxPasswordLength() + 1)
      * sizeof(cl_uchar) * _gws;
  cl::Buffer passwords_buffer { _context, CL_MEM_READ_WRITE
      | CL_MEM_COPY_HOST_PTR, passwords_buffer_size * devices.size(), _passwords };
  _passwords_buffers.push_back(passwords_buffer);

  _flag_buffer = cl::Buffer { _context,
  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(_flag), &_flag };

  // If there is more devices than just one, create also subbuffers
  for (int i = 1; i < devices.size(); i++)
  {
    cl_buffer_region passwords_region = { passwords_buffer_size * i,
        passwords_buffer_size };
    cl::Buffer passwords_subbuffer = passwords_buffer.createSubBuffer(
        CL_MEM_WRITE_ONLY, CL_BUFFER_CREATE_TYPE_REGION, &passwords_region);

    _passwords_buffers.push_back(passwords_subbuffer);
  }

  // Create kernels and command queues for devices
  for (int i = 0; i < devices.size(); i++)
  {
    cl::CommandQueue queue { _context, devices[i] };
    _command_queues.push_back(queue);

    cl::Kernel kernel { passgen_program, passgen_code.name.c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffers[i]);
    kernel.setArg(1, _flag_buffer);

    _passgen_kernels.push_back(kernel);
  }

  // Initialize generator's kernel
  for (int i = 0; i < _passgen_kernels.size(); i++)
  {
    _passgen->InitKernel(_passgen_kernels[i], _command_queues[i], _context,
                         _gws, step);
  }
}

Runner::~Runner()
{
  delete[] _passwords;
  delete _passgen;
}

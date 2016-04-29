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
#include <sstream>

#include "Runner.h"

using namespace std;

Runner::Runner(Options & options) :
    _gws { options.gws }, _verbose { options.verbose }
{
  parseOptions(options);

  _passgen = new CLMarkovPassGen { options };
  _cracker = new Cracker { options };

  createContext();
  initGenerator();
  initCracker();
}

void Runner::Run()
{

  vector<thread> threads;
  unsigned num_threads = _device.size();

  for (unsigned i = 0; i < num_threads; i++)
  {
    threads.push_back(thread { &Runner::runThread, this, i });
  }

  // Wait for all threads to complete
  for (auto &i : threads)
  {
    i.join();
  }

  _cracker->PrintResults();
}

void Runner::createContext()
{
  std::vector<cl::Platform> platform_list;

  // Get list of all available platforms
  cl::Platform::get(&platform_list);

  vector<cl::Device> available_devices;

  // Get list of all devices available on platform
  platform_list[_selected_platform].getDevices( CL_DEVICE_TYPE_ALL, &available_devices);

  if (_selected_device.empty())
  {
    platform_list[_selected_platform].getDevices(CL_DEVICE_TYPE_GPU, &_device);
  }
  else
  {
    for (auto i: _selected_device)
      _device.push_back(available_devices[i]);
  }

  // Create contexts for every device in selected platform
  cl_context_properties context_properties[] = {
  CL_CONTEXT_PLATFORM,
      (cl_context_properties) (platform_list[_selected_platform])(), 0 };
  _context = cl::Context { _device, context_properties };

  // Create command queues
  for (unsigned i = 0; i < _device.size(); i++)
  {
    cl::CommandQueue queue { _context, _device[i] };
    _command_queue.push_back(queue);
  }
}

Runner::~Runner()
{
  delete _passgen;
  delete _cracker;
}

void Runner::initGenerator()
{
  _passgen->SetGWS(_gws);

  unsigned num_devices = _device.size();

  // Get kernel source for generator
  ifstream source_file { _passgen->GetKernelSource(), ifstream::in };
  if (!source_file.is_open())
    throw invalid_argument { "Kernel code missing" };
  string source { (istreambuf_iterator<char>(source_file)), istreambuf_iterator<
      char>() };

  // Create and build program
  cl::Program program{ _context, source, true };
  try {
	  program.build("-Werror");
  }
  catch (cl::Error &err)
  {
	  cl::STRING_CLASS log;
	  program.getBuildInfo(_device[0], CL_PROGRAM_BUILD_LOG, &log);
	  cout << log << endl;
	  exit(EXIT_FAILURE);
  }

  // Create kernel's memory objects
  _passwords_entry_size = _passgen->MaxPasswordLength() + PASS_EXTRA_BYTES;
  size_t passwords_num_items = _passwords_entry_size * _gws;
  size_t passwords_size = passwords_num_items * sizeof(cl_uchar);

  for (unsigned i = 0; i < num_devices; i++)
  {
    cl::Buffer passwords_buffer { _context, CL_MEM_READ_WRITE, passwords_size };
    _passwords_buffer.push_back(passwords_buffer);
  }

  // Create kernels
  for (unsigned i = 0; i < _device.size(); i++)
  {
    cl::Kernel kernel { program, _passgen->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);

    _passgen_kernel.push_back(kernel);
  }

  // Initialize generator's kernel
  _passgen->InitKernel(_passgen_kernel, _command_queue, _context);
}

void Runner::initCracker()
{
  unsigned num_devices = _device.size();

  // Get kernel source for generator
  ifstream source_file { _cracker->GetKernelSource(), ifstream::in };
  if (!source_file.is_open())
    throw invalid_argument { "Cracker's kernel code missing" };
  string source { (istreambuf_iterator<char>(source_file)), istreambuf_iterator<
      char>() };

  // Create and build program
  cl::Program program { _context, source, true };
  program.build("-Werror");

  // Create kernels
  for (unsigned i = 0; i < num_devices; i++)
  {
    cl::Kernel kernel { program, _cracker->GetKernelName().c_str() };

    // Set password buffer as first argument
    kernel.setArg(0, _passwords_buffer[i]);
    kernel.setArg(1, _passwords_entry_size);

    _cracker_kernel.push_back(kernel);
  }

  // Initialize cracker's kernel
  _cracker->InitKernel(_cracker_kernel, _command_queue, _context);
}

void Runner::runThread(unsigned device_num)
{
  vector<cl::Event> passgen_events;
  vector<cl::Event> cracker_events;
  cl::Event event;

  bool flag = true;

  while (flag)
  {
    passgen_events.clear();
    _command_queue[device_num].enqueueNDRangeKernel(_passgen_kernel[device_num],
                                                 cl::NullRange,
                                                 cl::NDRange(_gws),
                                                 cl::NullRange, &cracker_events,
                                                 &event);
    passgen_events.push_back(event);

    cracker_events.clear();
    _command_queue[device_num].enqueueNDRangeKernel(_cracker_kernel[device_num],
                                                 cl::NullRange,
                                                 cl::NDRange(_gws),
                                                 cl::NullRange, &passgen_events,
                                                 &event);
    cracker_events.push_back(event);

    flag = _passgen->NextKernelStep(device_num);
  }
}

void Runner::Details()
{
}

void Runner::parseOptions(Options& options)
{
  stringstream ss;
  string substr;

  ss << options.devices;

  std::getline(ss, substr, ':');
  _selected_platform = stoi(substr);

  while (std::getline(ss, substr, ','))
  {
    _selected_device.push_back(stoi(substr));
  }
}

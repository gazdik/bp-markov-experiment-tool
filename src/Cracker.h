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

#ifndef CRACKER_H_
#define CRACKER_H_

#include "HashTable.h"

#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>

#include <string>

class Cracker
{
public:
  struct Options
  {
    std::string dictionary;
  };

  Cracker(Options options);
  ~Cracker();

  std::string GetKernelSource();
  std::string GetKernelName();

  void InitKernel(cl::Kernel & kernel, cl::CommandQueue & queue,
                  cl::Context & context);

  cl::Buffer _hash_table_buffer;

  void Debug();

private:
  const std::string _kernel_name = "cracker";
  const std::string _kernel_source = "kernels/Cracker.cl";

  HashTable *_hash_table;

};

#endif /* CRACKER_H_ */

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

#include <Cracker.h>

#include <CL/cl.hpp>

// TODO Debug
#include <iostream>

#include <fstream>
#include <algorithm>

using namespace std;

Cracker::Cracker(Options options)
{
  ifstream dictionary { options.dictionary, ifstream::in };

  unsigned num_lines = count(istreambuf_iterator<char>(dictionary),
                             istreambuf_iterator<char>(), '\n') + 1;

  dictionary.close();
  dictionary.open(options.dictionary, ifstream::in);

  _hash_table = new HashTable { num_lines, options.max_load_factor };

  string word;
  while (dictionary.good())
  {
    getline(dictionary, word);
    _hash_table->Insert(word);
  }

}

Cracker::~Cracker()
{
  delete _hash_table;
}

std::string Cracker::GetKernelSource()
{
  return (_kernel_source);
}

std::string Cracker::GetKernelName()
{
  return (_kernel_name);
}

void Cracker::InitKernel(cl::Kernel& kernel, cl::CommandQueue& queue,
                         cl::Context& context)
{
  cl_uchar *hash_table;
  cl_uint num_rows, num_entries, entry_size, row_size;
  unsigned hash_table_size = _hash_table->Serialize(&hash_table, num_rows,
                                                    num_entries, entry_size,
                                                    row_size);

  // Create buffer for hash table
  _hash_table_buffer = cl::Buffer { context, CL_MEM_READ_ONLY
      | CL_MEM_COPY_HOST_PTR, hash_table_size, hash_table };

  kernel.setArg(4, _hash_table_buffer);
  kernel.setArg(5, num_rows);
  kernel.setArg(6, num_entries);
  kernel.setArg(7, entry_size);
  kernel.setArg(8, row_size);
}

void Cracker::Debug()
{
  _hash_table->Debug();
}

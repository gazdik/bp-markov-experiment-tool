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

#include <iostream>

#include <fstream>
#include <algorithm>

using namespace std;

std::string makeString(cl_uchar *ht_element)
{
  char buffer[256];
  unsigned length = ht_element[HT_LENGTH_OFFSET];

  for (unsigned i = 0; i < length; i++)
  {
    buffer[i] = ht_element[HT_PAYLOAD_OFFSET + i];
  }

  return (std::string { buffer, length });
}

Cracker::Cracker(Options options) :
    _print_passwords { options.print_passwords }
{
  ifstream dictionary { options.dictionary, ifstream::in };

  unsigned num_lines = count(istreambuf_iterator<char>(dictionary),
                             istreambuf_iterator<char>(), '\n') + 1;

  dictionary.close();
  dictionary.open(options.dictionary, ifstream::in);

  HashTable *hash_table;
  hash_table = new HashTable { num_lines, options.max_load_factor };

  string word;
  while (dictionary.good())
  {
    getline(dictionary, word);
    hash_table->Insert(word);
  }

  _hash_table_size = hash_table->Serialize(&_flat_hash_table, _num_rows,
                                           _num_entries, _entry_size,
                                           _row_size);

  hash_table->Details();

  delete hash_table;

}

Cracker::~Cracker()
{
  delete[] _flat_hash_table;
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
  _cmd_queue.push_back(queue);

  cl::Buffer hash_table_buffer { context, CL_MEM_READ_WRITE, _hash_table_size };
  queue.enqueueWriteBuffer(hash_table_buffer, CL_TRUE, 0, _hash_table_size,
                           _flat_hash_table);
  _hash_table_buffer.push_back(hash_table_buffer);

  kernel.setArg(2, hash_table_buffer);
  kernel.setArg(3, _num_rows);
  kernel.setArg(4, _num_entries);
  kernel.setArg(5, _entry_size);
  kernel.setArg(6, _row_size);
}

void Cracker::Details()
{
}

void Cracker::PrintResults()
{
  cl_uchar *tmp_hash_table = new cl_uchar[_hash_table_size];
  unsigned total_num_elements = _num_entries * _num_rows;
  unsigned index;


  // Update flags in hash table
  for (int i = 0; i < _cmd_queue.size(); i++)
  {
    _cmd_queue[i].enqueueReadBuffer(_hash_table_buffer[i], CL_TRUE, 0,
                                    _hash_table_size, tmp_hash_table);

    for (unsigned i = 0; i < total_num_elements; i++)
    {
      index = _entry_size * i + HT_FLAG_OFFSET;

      if (tmp_hash_table[index] == HT_FOUND)
      {
        _flat_hash_table[index] = HT_FOUND;
      }
    }
  }

  delete[] tmp_hash_table;

  // Count cracked passwords
  unsigned num_cracked_passwords = 0;
  vector<string> cracked_passwords;


  for (int i = 0; i < total_num_elements; i++)
  {
    index = _entry_size * i;

    if (_flat_hash_table[index + HT_FLAG_OFFSET] == HT_FOUND)
    {
      num_cracked_passwords++;
      if (_print_passwords)
        cracked_passwords.push_back(makeString(&_flat_hash_table[index]));
    }
  }

  // Print results
  cout << "Cracked passwords: " << num_cracked_passwords << "\n";
  for (auto pass: cracked_passwords)
    cout << pass << "\n";
}

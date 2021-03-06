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

#define NOMINMAX

#include <CLMarkovPassGen.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>     // ntohl, ntohs
#endif
#include <cstdlib>         // atoi, qsort

#include <sstream>
#include <string>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <vector>
#include <algorithm>       // max_element

using namespace std;


void CLMarkovPassGen::InitKernel(std::vector<cl::Kernel>& kernels,
                                 std::vector<cl::CommandQueue>& queues,
                                 cl::Context& context)
{
  _kernels = kernels;

  for (int dev_num = 0; dev_num < kernels.size(); dev_num++)
  {
    cl::Kernel & kernel = kernels[dev_num];
    cl::CommandQueue & queue = queues[dev_num];

    // Invalid values to prevent kernel execution without reserved passwords
    _local_start_indexes.push_back(1);
    _local_stop_indexes.push_back(0);

    cl::Buffer markov_table_buffer { context, CL_MEM_READ_ONLY,
        _markov_table_size * sizeof(cl_uchar) };
    queue.enqueueWriteBuffer(markov_table_buffer, CL_TRUE, 0,
                             _markov_table_size * sizeof(cl_uchar),
                             _markov_table);
    _markov_table_buffer.push_back(markov_table_buffer);

    cl::Buffer thresholds_buffer { context, CL_MEM_READ_ONLY, _max_length
        * sizeof(cl_uint) };
    queue.enqueueWriteBuffer(thresholds_buffer, CL_TRUE, 0,
                             _max_length * sizeof(cl_uint), _thresholds);
    _thresholds_buffer.push_back(thresholds_buffer);

    cl::Buffer permutations_buffer { context, CL_MEM_READ_ONLY,
        (_max_length + 2) * sizeof(cl_ulong) };
    queue.enqueueWriteBuffer(permutations_buffer, CL_TRUE, 0,
                             (_max_length + 2) * sizeof(cl_ulong),
                             _permutations);
    _permutations_buffer.push_back(permutations_buffer);

    kernel.setArg(2, markov_table_buffer);
    kernel.setArg(3, thresholds_buffer);
    kernel.setArg(4, permutations_buffer);
    kernel.setArg(5, _max_threshold);
    kernel.setArg(6, _local_start_indexes[dev_num]);
    kernel.setArg(7, _local_stop_indexes[dev_num]);
  }

  freeUnusedMemory();
}

CLMarkovPassGen::CLMarkovPassGen(Options & options) :
    _mask { options.mask }, _stat_file { options.stat_file }
{
  _thresholds = new cl_uint[MAX_PASS_LENGTH];
  _permutations = new cl_ulong[MAX_PASS_LENGTH + 1];

  parseOptions(options);

  // Determine maximal threshold
  _max_threshold = *max_element(_thresholds, _thresholds + MAX_PASS_LENGTH);

  // Initialize memory
  initMemory();

  _global_start_index = _permutations[_min_length - 1];
  _global_stop_index = _permutations[_max_length];

  Details();
}

CLMarkovPassGen::~CLMarkovPassGen()
{
}

unsigned CLMarkovPassGen::MaxPasswordLength()
{
  return _max_length;
}

int CLMarkovPassGen::compareSortElements(const void* p1, const void* p2)
{
  const SortElement *e1 = static_cast<const SortElement *>(p1);
  const SortElement *e2 = static_cast<const SortElement *>(p2);

  if (!isValidChar(e1->next_state) && isValidChar(e2->next_state))
    return (1);

  if (isValidChar(e1->next_state) && !isValidChar(e2->next_state))
	  return (-1);

  if (!isValidChar(e1->next_state) && !isValidChar(e2->next_state))
    return (e2->next_state - e1->next_state);

  int result = e2->probability - e1->probability;

  if (result == 0)
    return (e2->next_state - e1->next_state);

  return (result);
}

void CLMarkovPassGen::parseOptions(Options & options)
{
  stringstream ss;
  string substr;

  // Parse length values
  ss << options.length;

  std::getline(ss, substr, ':');
  _min_length = stoi(substr);

  std::getline(ss, substr);
  _max_length = stoi(substr);

  // Parse threshold values
  ss.clear();
  ss << options.thresholds;

  // Set global threshold
  std::getline(ss, substr, ':');
  for (int i = 0; i < MAX_PASS_LENGTH; i++)
  {
    _thresholds[i] = stoi(substr);
  }

  // Adjust threshold values according to mask
  for (int i = 0; i < MAX_PASS_LENGTH; i++)
  {
    unsigned mask_chars_count = _mask[i].Count();

    if (mask_chars_count < _thresholds[i])
      _thresholds[i] = mask_chars_count;
  }

  // Set positional thresholds
  int i = 0;
  while (std::getline(ss, substr, ','))
  {
    _thresholds[i] = stoi(substr);
    i++;
  }

  // Parse model
  if (options.model == "classic")
    _model = Model::CLASSIC;
  else if (options.model == "layered")
    _model = Model::LAYERED;
  else
    throw invalid_argument("Invalid value for argument 'model'");

}

void CLMarkovPassGen::initMemory()
{
  // Calculate permutations for all lengths
  _permutations[0] = 0;
  for (int i = 1; i < MAX_PASS_LENGTH + 1; i++)
  {
    _permutations[i] = _permutations[i - 1] + numPermutations(i);
  }

  // Open file with statistics and find appropriate statistics
  ifstream input { _stat_file, ifstream::in | ifstream::binary };
  unsigned stat_length = findStatistics(input);

  // Create Markov matrix from statistics
  const unsigned markov_matrix_size = CHARSET_SIZE * CHARSET_SIZE
      * MAX_PASS_LENGTH;
  uint16_t *markov_matrix_buffer = new uint16_t[markov_matrix_size];
  uint16_t *markov_matrix[MAX_PASS_LENGTH][CHARSET_SIZE];
  auto markov_matrix_ptr = markov_matrix_buffer;

  for (int p = 0; p < MAX_PASS_LENGTH; p++)
  {
    for (int i = 0; i < CHARSET_SIZE; i++)
    {
      markov_matrix[p][i] = markov_matrix_ptr;
      markov_matrix_ptr += CHARSET_SIZE;
    }
  }

  input.read(reinterpret_cast<char *>(markov_matrix_buffer), stat_length);

  // In case of classic Markov model, copy statistics to all positions
  if (_model == Model::CLASSIC)
  {
    markov_matrix_ptr = markov_matrix_buffer;
    for (int p = 1; p < MAX_PASS_LENGTH; p++)
    {
      markov_matrix_ptr += CHARSET_SIZE * CHARSET_SIZE;

      memcpy(markov_matrix_ptr, markov_matrix_buffer,
             CHARSET_SIZE * CHARSET_SIZE * sizeof(uint16_t));
    }
  }

  // Convert these values to host byte order
  for (int i = 0; i < markov_matrix_size; i++)
  {
    markov_matrix_buffer[i] = ntohs(markov_matrix_buffer[i]);
  }

  // Create temporary Markov table to order elements
  SortElement *markov_sort_table_buffer = new SortElement[markov_matrix_size];
  SortElement *markov_sort_table[MAX_PASS_LENGTH][CHARSET_SIZE];
  auto markov_sort_table_ptr = markov_sort_table_buffer;

  for (int p = 0; p < MAX_PASS_LENGTH; p++)
  {
    for (int i = 0; i < CHARSET_SIZE; i++)
    {
      markov_sort_table[p][i] = markov_sort_table_ptr;
      markov_sort_table_ptr += CHARSET_SIZE;
    }
  }

  for (int p = 0; p < MAX_PASS_LENGTH; p++)
  {
    for (int i = 0; i < CHARSET_SIZE; i++)
    {
      for (int j = 0; j < CHARSET_SIZE; j++)
      {
        markov_sort_table[p][i][j].next_state = static_cast<uint8_t>(j);
        markov_sort_table[p][i][j].probability = markov_matrix[p][i][j];
      }
    }
  }

  // Apply mask
  applyMask(markov_sort_table);

  // Order elements by probability
  for (int p = 0; p < MAX_PASS_LENGTH; p++)
  {
    for (int i = 0; i < CHARSET_SIZE; i++)
    {
      qsort(markov_sort_table[p][i], CHARSET_SIZE, sizeof(SortElement),
            compareSortElements);
    }
  }

  // Create final Markov table
  _markov_table_size = _max_length * CHARSET_SIZE * _max_threshold;
  _markov_table = new cl_uchar[_markov_table_size];

  unsigned index, index1, index2;
  for (unsigned p = 0; p < _max_length; p++)
  {
    index1 = p * CHARSET_SIZE * _max_threshold;
    for (unsigned i = 0; i < CHARSET_SIZE; i++)
    {
      index2 = i * _max_threshold;
      for (unsigned j = 0; j < _max_threshold; j++)
      {
        index = index1 + index2 + j;
        _markov_table[index] = markov_sort_table[p][i][j].next_state;
      }
    }
  }

  delete[] markov_matrix_buffer;
  delete[] markov_sort_table_buffer;
}

bool CLMarkovPassGen::isValidChar(uint8_t value)
{
//  return ((value >= 32 && value <= 126) ? true : false);
  return ((value >= 32) ? true : false);
}

uint64_t CLMarkovPassGen::numPermutations(const unsigned length)
{
  uint64_t result = 1;

  for (unsigned i = 0; i < length; i++)
  {
    result *= _thresholds[i];
  }

  return (result);
}

unsigned CLMarkovPassGen::findStatistics(std::ifstream& stat_file)
{
  // Skip header
  stat_file.ignore(numeric_limits<streamsize>::max(), ETX);

  uint8_t type;
  uint32_t length;

  while (stat_file)
  {
    stat_file.read(reinterpret_cast<char *>(&type), sizeof(type));
    stat_file.read(reinterpret_cast<char *>(&length), sizeof(length));
    length = ntohl(length);

    if (type == _model)
    {
      return (length);
    }

    stat_file.ignore(length);
  }

  throw runtime_error {
      "File doesn't contain statistics for specified Markov model" };
}

void CLMarkovPassGen::applyMask(SortElement* table[MAX_PASS_LENGTH][CHARSET_SIZE])
{
  for (unsigned p = 0; p < _max_length; p++)
  {
    for (unsigned i = 0; i < CHARSET_SIZE; i++)
    {
      for (unsigned j = 0; j < CHARSET_SIZE; j++)
      {
        if (_mask[p].Satisfy(table[p][i][j].next_state))
          table[p][i][j].probability += UINT16_MAX + 1;
      }
    }
  }
}

void CLMarkovPassGen::Details()
{
#ifndef NDEBUG
  cout << "Minimal length: " << _min_length << "\n";
  cout << "Maximal length: " << _max_length << "\n";
#endif

  cout << "Thresholds: ";
  for (int i = 0; i < _max_length; i++)
  {
    cout << _thresholds[i] << " ";
  }
  cout << "\n";

#ifndef NDEBUG
  cout << "Maximal threshold: " << _max_threshold << "\n";

  cout << "Model: ";
  if (_model == Model::CLASSIC)
    cout << "classic";
  else if (_model == Model::LAYERED)
    cout << "layered";
  cout << "\n";
#endif
}

std::string CLMarkovPassGen::GetKernelSource()
{
  return (_kernel_source);
}

std::string CLMarkovPassGen::GetKernelName()
{
  return (_kernel_name);
}

void CLMarkovPassGen::printMarkovTable()
{
  for (unsigned p = 0; p < _max_length; p++)
  {
    cout << "===============  P=" << p <<  " ================" << endl;
    for (unsigned i = 32; i < 127; i++)
    {
      for (unsigned j = 0; j < _thresholds[p]; j++)
      {
        char c;
        c = _markov_table[p * CHARSET_SIZE * _max_threshold + i * _max_threshold
            + j];

        cout << c;
      }
      cout << endl;
    }
  }
}

void CLMarkovPassGen::SetGWS(std::size_t gws)
{
  _gws = gws;
  _resevation_size = 10000 * _gws;
}

bool CLMarkovPassGen::NextKernelStep(unsigned device_number)
{
  if (_local_start_indexes[device_number] < _local_stop_indexes[device_number])
  {
    _local_start_indexes[device_number] += _gws;
    _kernels[device_number].setArg(6, _local_start_indexes[device_number]);
    return true;
  }

  if (reservePasswords(device_number))
  {
    _kernels[device_number].setArg(6, _local_start_indexes[device_number]);
    _kernels[device_number].setArg(7, _local_stop_indexes[device_number]);
    return true;
  }

  return false;
}

bool CLMarkovPassGen::reservePasswords(unsigned thread_number)
{
  _global_index_mutex.lock();
  _local_start_indexes[thread_number] = _global_start_index;
  _global_start_index += _resevation_size;
  _global_index_mutex.unlock();

  _local_stop_indexes[thread_number] = _local_start_indexes[thread_number]
                                         + _resevation_size;

  if (_local_start_indexes[thread_number] > _global_stop_index)
    return false;

  if (_local_stop_indexes[thread_number] > _global_stop_index)
    _local_stop_indexes[thread_number] = _global_stop_index;

  return true;

}

void CLMarkovPassGen::freeUnusedMemory()
{
  delete[] _thresholds;
  _thresholds = nullptr;
  delete[] _permutations;
  _permutations = nullptr;
  delete[] _markov_table;
  _markov_table = nullptr;
}

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

#include <CLMarkovPassGen.h>

#ifdef WIN32
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

void CLMarkovPassGen::InitKernel(cl::Kernel& kernel, cl::CommandQueue& queue,
                                 cl::Context& context, unsigned gws,
                                 cl_uint step)
{
  cl::Buffer markov_table_buffer { context, CL_MEM_READ_ONLY, _markov_table_size
      * sizeof(cl_uchar) };
  queue.enqueueWriteBuffer(markov_table_buffer, CL_TRUE, 0, _markov_table_size * sizeof(cl_uchar), _markov_table);
  _markov_table_buffer.push_back(markov_table_buffer);

  cl::Buffer thresholds_buffer { context, CL_MEM_READ_ONLY, _max_length
      * sizeof(cl_uint) };
  queue.enqueueWriteBuffer(thresholds_buffer, CL_TRUE, 0, _max_length * sizeof(cl_uint), _thresholds);
  _thresholds_buffer.push_back(thresholds_buffer);

  cl::Buffer permutations_buffer { context, CL_MEM_READ_ONLY, (_max_length + 1)
      * sizeof(cl_ulong) };
  queue.enqueueWriteBuffer(permutations_buffer, CL_TRUE, 0, (_max_length + 1) * sizeof(cl_ulong), _permutations);
  _permutations_buffer.push_back(permutations_buffer);

  cl::Buffer indexes_buffer { context, CL_MEM_READ_WRITE, gws * sizeof(cl_ulong) };
  _indexes_buffer.push_back(indexes_buffer);

  // Create indexes and copy them into buffer
  cl_ulong *indexes = new cl_ulong[gws];
  for (int i = 0; i < gws; i++)
  {
    indexes[i] = _global_index;
    _global_index++;
  }

  queue.enqueueWriteBuffer(indexes_buffer, CL_TRUE, 0, sizeof(cl_ulong) * gws,
                           indexes);

  delete[] indexes;

  kernel.setArg(3, indexes_buffer);
  kernel.setArg(4, markov_table_buffer);
  kernel.setArg(5, thresholds_buffer);
  kernel.setArg(6, permutations_buffer);
  kernel.setArg(7, _max_threshold);
  kernel.setArg(8, step);
}

CLMarkovPassGen::CLMarkovPassGen(Options & options) :
    _mask { options.mask }, _stat_file { options.stat_file }
{
  parseOptions(options);

  // Determine maximal threshold
  _max_threshold = *max_element(_thresholds, _thresholds + MAX_PASS_LENGTH);

  // Initialize memory
  initMemory();

  _global_index = _permutations[_min_length - 1];

  cout << "Minimal length: " << _min_length << endl;
  cout << "Maximal length: " << _max_length << endl;

  cout << "Thresholds: ";
  for (int i = 0; i < MAX_PASS_LENGTH; i++)
  {
    cout << _thresholds[i] << " ";
  }
  cout << endl;

  cout << "Model: ";
  if (_model == Model::CLASSIC)
    cout << "classic";
  else if (_model == Model::LAYERED)
    cout << "layered";
  else
    cout << "invalid";
  cout << endl;

  cout << "Mask: " << _mask << endl;

  cout << "Maximal threshold: " << _max_threshold << endl;
}

CLMarkovPassGen::~CLMarkovPassGen()
{
  delete[] _markov_table;
}

unsigned CLMarkovPassGen::MaxPasswordLength()
{
  return _max_length;
}

int CLMarkovPassGen::compareSortElements(const void* p1, const void* p2)
{
  const SortElement *e1 = static_cast<const SortElement *>(p1);
  const SortElement *e2 = static_cast<const SortElement *>(p2);

  if (not isValidChar(e1->next_state))
    return (1);

  return (e2->probability - e1->probability);
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

  std::getline(ss, substr, ':');
  for (int i = 0; i < MAX_PASS_LENGTH; i++)
  {
    _thresholds[i] = stoi(substr);
  }

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
  for (int i = 1; i < MAX_PASS_LENGTH; i++)
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
  for (int p = 0; p < _max_length; p++)
  {
    index1 = p * CHARSET_SIZE * _max_threshold;
    for (int i = 0; i < CHARSET_SIZE; i++)
    {
      index2 = i * _max_threshold;
      for (int j = 0; j < _max_threshold; j++)
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
  return ((value >= 32 && value <= 126) ? true : false);
}

uint64_t CLMarkovPassGen::numPermutations(const unsigned length)
{
  uint64_t result = 1;

  for (int i = 0; i < length; i++)
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

void CLMarkovPassGen::applyMask(
    SortElement* table[MAX_PASS_LENGTH][CHARSET_SIZE])
{
  int position = 0;
  for (int i = 0; i < _mask.length(); i++)
  {
    switch (_mask[i])
    {
      case '?':
        i++;

        if (_mask[i] == '?')
          applyChar(table[position], _mask[i]);
        else
          applyMetachar(table[position], _mask[i]);

        position++;
        break;
      default:
        applyChar(table[position], _mask[i]);

        position++;
        break;
    }
  }
}

void CLMarkovPassGen::applyChar(SortElement** table, char metachar)
{
  for (int i = 0; i < CHARSET_SIZE; i++)
  {
    for (int j = 0; j < CHARSET_SIZE; j++)
    {
      if (satisfyMask(table[i][j].next_state, metachar))
        table[i][j].probability += UINT16_MAX + 1;
    }
  }
}

void CLMarkovPassGen::applyMetachar(SortElement** table, char character)
{
  for (int i = 0; i < CHARSET_SIZE; i++)
  {
    for (int j = 0; j < CHARSET_SIZE; j++)
    {
      if (table[i][j].next_state == character)
        table[i][j].probability += UINT16_MAX + 1;
    }
  }
}

std::string CLMarkovPassGen::getPassword()
{
  string result;

  // Determine current length according to index
  unsigned length = 1;
  while (_global_index >= _permutations[length])
  {
    length++;

  }

  if (length > _max_length)
    return (string { "END" });

  // Convert global index to local index
  uint64_t index = _global_index - _permutations[length - 1];

  uint64_t partial_index;
  char last_char = 0;

  // Create password
  for (int p = 0; p < length; p++)
  {
    partial_index = index % _thresholds[p];
    index = index / _thresholds[p];

    last_char = _markov_table[p * CHARSET_SIZE * _max_threshold
        + last_char * _max_threshold + partial_index];

    result += last_char;
  }

  _global_index++;

  return (result);
}

bool CLMarkovPassGen::satisfyMask(uint8_t character, char mask)
{
  switch (mask)
  {
    case 'u':
      return ((character >= 65 && character <= 90) ? true : false);
      break;
    case 'l':
      return ((character >= 97 && character <= 122) ? true : false);
      break;
    case 'c':
      return (
          (character >= 65 && character <= 90)
              || (character >= 97 && character <= 122) ? true : false);
      break;
    case 'd':
      return ((character >= 48 && character <= 57) ? true : false);
      break;
    case 's':
      return (
          (character >= 32 && character <= 47)
              || (character >= 58 && character <= 64)
              || (character >= 91 && character <= 96)
              || (character >= 123 && character <= 126) ? true : false);
      break;
    case 'a':
      return (
          (character >= 65 && character <= 90)
              || (character >= 97 && character <= 122)
              || (character >= 48 && character <= 57) ? true : false);
      break;
    case 'x':
      return ((character >= 32 && character <= 126) ? true : false);
      break;
    default:
      return (false);
      break;
  }
}

std::string CLMarkovPassGen::GetKernelSource()
{
  return (_kernel_source);
}

std::string CLMarkovPassGen::GetKernelName()
{
  return (_kernel_name);
}

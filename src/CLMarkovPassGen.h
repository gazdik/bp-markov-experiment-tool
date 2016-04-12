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

#ifndef CLMARKOVPASSGEN_H_
#define CLMARKOVPASSGEN_H_

#define __CL_ENABE_EXCEPTIONS

#include <cstdint>
#include <CL/cl.hpp>

#include <string>
#include <fstream>

const unsigned MIN_PASS_LENGTH = 1;
const unsigned MAX_PASS_LENGTH = 64;
const unsigned CHARSET_SIZE = 256;
const unsigned ETX = 3;

struct KernelCode
{
  std::string file_name;
  std::string name;
};

/**
 * Command line options with default values
 */
struct CLMarkovPassGenOptions
{
  std::string stat_file;
  std::string model = "classic";
  std::string thresholds = "5";
  std::string length = "1:64";
  std::string mask;
};

class CLMarkovPassGen
{
public:
  CLMarkovPassGen(CLMarkovPassGenOptions & options);
  ~CLMarkovPassGen();

  /**
   * Get kernel source code
   * @return
   */
  KernelCode GetKernelCode();

  /**
   * Create buffers and set arguments
   * @param kernel
   * @param command_queue
   * @param context
   * @param step
   */
  void InitKernel(cl::Kernel & kernel, cl::CommandQueue & command_queue,
                  cl::Context & context, unsigned gws, cl_uint step);

  /**
   * Return maximum length of password
   */
  unsigned MaxPasswordLength();

  /**
   * TODO Remove
   */
  std::string getPassword();

private:

  struct SortElement
  {
    uint8_t next_state;
    uint32_t probability;
  };

  enum Model
  {
    CLASSIC = 1, LAYERED = 2
  };

  std::string _stat_file;
  std::string _mask;

  Model _model;

  /**
   * 3D Markov table
   */
  cl_uchar *_markov_table;
  /**
   * Number of elements in Markov table
   */
  unsigned _markov_table_size;
  /**
   * Precomputed number of permutations for every length
   */
  cl_ulong _permutations[MAX_PASS_LENGTH + 1];
  /**
   * Minimal password length
   */
  cl_uint _min_length = MIN_PASS_LENGTH;
  /**
   * Maximal password length
   */
  cl_uint _max_length = MAX_PASS_LENGTH;
  /**
   * Number of characters per position
   */
  cl_uint _thresholds[MAX_PASS_LENGTH];
  /**
   * Maximum threshold
   */
  cl_uint _max_threshold;

  std::vector<cl::Buffer> _indexes_buffers;
  cl::Buffer _markov_table_buffer;
  cl::Buffer _thresholds_buffer;
  cl::Buffer _permutations_buffer;

  void initMemory();
  void parseOptions(CLMarkovPassGenOptions & options);

  static int compareSortElements(const void *p1, const void *p2);
  static bool isValidChar(uint8_t value);
  uint64_t numPermutations(unsigned length);
  unsigned findStatistics(std::ifstream & stat_file);
  void applyMask(SortElement *table[MAX_PASS_LENGTH][CHARSET_SIZE]);
  void applyChar(SortElement **table, char metachar);
  void applyMetachar(SortElement **table, char character);
  bool satisfyMask(uint8_t character, char mask);

  // TODO
  uint64_t _global_index;
};

#endif /* CLMARKOVPASSGEN_H_ */

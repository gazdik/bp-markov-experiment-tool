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

#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#define __CL_ENABLE_EXCEPTIONS

#include <cstdint>

#include <string>
#include <unordered_set>

#include <CL/cl.hpp>

#define HT_EXTRA_BYTES 2
#define HT_LENGTH_OFFSET 0
#define HT_FLAG_OFFSET 1
#define HT_PAYLOAD_OFFSET 2
#define HT_FOUND 1
#define HT_NOTFOUND 0

class HashTable
{
public:
  /**
   * Construct hash table
   * @param num_words Number of words in dictionary (only prevents rehashing)
   * @param max_load_factor Average number of entries per bucket (row)
   */
  HashTable(unsigned num_words, float max_load_factor = 1.0);
  ~HashTable();

  /**
   * Insert new string into hash table
   * @param value
   */
  void Insert(std::string & value);

  /**
   * Get length of single entry in bytes (maxLength + 1)
   */
  unsigned GetEntryLength();

  /**
   * Get number of buckets (rows in hash table)
   */
  unsigned GetBucketCount();

  /**
   * Serialize C++ hash table into flat array for GPU
   * @param hash_table
   * @param num_rows
   * @param num_entries
   * @param entry_size
   * @param row_size
   */
  unsigned Serialize(cl_uchar **hash_table, cl_uint & num_rows,
                     cl_uint & num_entries, cl_uint & entry_size,
                     cl_uint & row_size);

  void Details();


private:
  class hash_func
  {
  public:
    size_t operator()(const std::string &value) const
    {
      uint32_t hash = 5381;

      for (auto c : value)
      {
        hash = ((hash << 5) + hash) + c;
      }
      return (hash);
    }
  };

  unsigned longestBucketSize();

  std::unordered_set<std::string, hash_func> _hash_table;

  unsigned _max_length = 0;

};

#endif /* HASHTABLE_H_ */

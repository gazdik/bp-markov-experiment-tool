/*
 * HashTable.h
 *
 *  Created on: Apr 12, 2016
 *      Author: gazdik
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
  HashTable(unsigned num_words, float max_load_factor = 1.0);
  ~HashTable();

  void Insert(std::string & value);
  unsigned GetEntryLength();
  unsigned GetBucketCount();
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
  void printTable();

  std::unordered_set<std::string, hash_func> _hash_table;

  unsigned _max_length = 0;

};

#endif /* HASHTABLE_H_ */

/*
 * HashTable.h
 *
 *  Created on: Apr 12, 2016
 *      Author: gazdik
 */

#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#define __CL_ENABLE_EXCEPTIONS

#include <string>
#include <unordered_set>

#include <CL/cl.hpp>

class HashTable
{
public:
  HashTable(unsigned num_words, unsigned average_bucket_size = 1);
  ~HashTable();

  void Insert(std::string & value);
  unsigned GetEntryLength();
  unsigned GetBucketCount();
  unsigned GetNumEntries();
  // TODO
  unsigned Serialize(cl_uchar **hash_table, cl_uint & num_rows,
                     cl_uint & num_entries, cl_uint & entry_size,
                     cl_uint & row_size);

  // TODO Debug
  void Debug();

private:
  class hash_func
  {
  public:
    size_t operator()(const std::string &value) const
    {
      // TODO change to explicit 64bit integer
      size_t hash = 5381;

      for (auto c : value)
      {
        hash = ((hash << 5) + hash) + c;
      }
      return (hash);
    }
  };

  unsigned longestBucketSize();

  std::unordered_set<std::string, hash_func> _hash_table;
//  std::unordered_set<std::string> _hash_table;
  cl_uchar *_flat_hash_table = nullptr;

  unsigned _max_length = 0;

};

#endif /* HASHTABLE_H_ */

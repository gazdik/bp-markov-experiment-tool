/*
 * HashTable.cpp
 *
 *  Created on: Apr 12, 2016
 *      Author: gazdik
 */

#include "HashTable.h"

#include <iostream>

using namespace std;

HashTable::HashTable(unsigned num_words, unsigned average_bucket_size)
{
  size_t buckets = num_words / average_bucket_size;

  _hash_table.rehash(buckets);
}

HashTable::~HashTable()
{
}

void HashTable::Insert(std::string & value)
{
  if (value.length() > _max_length)
    _max_length = value.length();

  _hash_table.insert(value);
}

unsigned HashTable::GetEntryLength()
{
  return (_max_length + 1);
}

unsigned HashTable::GetBucketCount()
{
  return (_hash_table.bucket_count());
}


unsigned HashTable::Serialize(cl_uchar** hash_table, cl_uint& num_rows,
                              cl_uint& num_entries, cl_uint& entry_size,
                              cl_uint& row_size)
{
  num_rows = _hash_table.bucket_count();
  entry_size = GetEntryLength();
  num_entries = longestBucketSize();

  row_size = entry_size * num_entries;

  unsigned hash_table_size = num_rows * row_size * sizeof(cl_uchar);
  _flat_hash_table = new cl_uchar[hash_table_size];
  *hash_table = _flat_hash_table;

  unsigned bucket_size;
  cl_uchar *row;
  cl_uchar *entry;
  for (int i = 0; i < num_rows; i++)
  {
    row = &_flat_hash_table[i * row_size];

    bucket_size = _hash_table.bucket_size(i);

    if (bucket_size == 0)
    {
      row[0] = 0;
      continue;
    }

    int j = 0;
    char length;
    for (auto it = _hash_table.begin(i); it != _hash_table.end(i); it++)
    {
      string word = *it;
      entry = &row[j * entry_size];

      length = word.length();
      if (length == 0)
        continue;

      entry[0] = length;

      for (int i = 0; i < length; i++)
      {
        entry[i + 1] = word[i];
      }
      j++;
    }

    if (j < num_entries)
    {
      entry = &row[j * entry_size];
      entry[0] = 0;
    }
  }

  return hash_table_size;
}

void HashTable::Debug()
{
  cout << "Load factor: " << _hash_table.load_factor() << endl;
  cout << "Longest bucket size: " << longestBucketSize() << endl;

  unordered_set<std::string> orig_set;
  unordered_set<std::string>::hasher orig_hash = orig_set.hash_function();

  hash_func my_hash;

  cout << "Build-in hash function ('hovno'): " << orig_hash("hovno") << endl;
  cout << "My hash function ('hovno'): " << my_hash("hovno") << endl;

  unsigned n = _hash_table.bucket_count();

  cout << "Hash table has " << n << " buckets." << endl;

  unsigned not_empty;
  for (int i = 0; i < n; i++)
  {
    if (_hash_table.bucket_size(i) > 0)
    {
      not_empty = i;
      break;
    }
  }

  string entry = *_hash_table.begin(not_empty);
  cout << "Bucket #" << not_empty << " contains:" << entry << endl;
  cout << "Word " << entry << " should be in row " << my_hash(entry) % n
      << endl;
  cout << "and not in row " << orig_hash(entry) % n << endl;

}

unsigned HashTable::GetNumEntries()
{
}

unsigned HashTable::longestBucketSize()
{
  unsigned num_buckets = _hash_table.bucket_count();

  int longest_bucket = -1;
  unsigned longest_bucket_size = 0;

  unsigned bucket_size;
  for (int i = 0; i < num_buckets; i++)
  {
    bucket_size = _hash_table.bucket_size(i);
    if (bucket_size > longest_bucket_size)
    {
      longest_bucket = i;
      longest_bucket_size = bucket_size;
    }
  }

  if (longest_bucket_size == -1)
    throw runtime_error { "Hash table is probably empty" };

  return (longest_bucket_size);
}

/*
 * HashTable.cpp
 *
 *  Created on: Apr 12, 2016
 *      Author: gazdik
 */

#include "HashTable.h"

#include <iostream>

using namespace std;

HashTable::HashTable(unsigned num_words, float max_load_factor)
{
  size_t buckets = num_words;

  _hash_table.max_load_factor(max_load_factor);
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
  return (_max_length + HT_EXTRA_BYTES);
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

  unsigned hash_table_size = num_rows * row_size;

  cl_uchar *hash_table_ptr = new cl_uchar[hash_table_size];
  *hash_table = hash_table_ptr;
  memset(hash_table_ptr, 0, hash_table_size * sizeof(cl_uchar));

  unsigned bucket_size;
  cl_uchar *row;
  cl_uchar *entry;
  for (unsigned i = 0; i < num_rows; i++)
  {
    row = &hash_table_ptr[i * row_size];
    bucket_size = _hash_table.bucket_size(i);

    if (bucket_size == 0)
    {
      row[HT_LENGTH_OFFSET] = 0;
      continue;
    }

    unsigned j = 0;
    char length;
    for (auto it = _hash_table.begin(i); it != _hash_table.end(i); it++)
    {
      string word = *it;
      entry = &row[j * entry_size];

      length = word.length();
      if (length == 0)
        continue;

      entry[HT_LENGTH_OFFSET] = length;

      for (int i = 0; i < length; i++)
      {
        entry[i + HT_PAYLOAD_OFFSET] = word[i];
      }

      j++;
    }

    // TODO What is it?
    if (j < num_entries)
    {
      entry = &row[j * entry_size];
      entry[HT_LENGTH_OFFSET] = 0;
    }
  }

  return hash_table_size;
}

void HashTable::Details()
{
  cout << "Load factor: " << _hash_table.load_factor() << "\n";
  cout << "Longest bucket size: " << longestBucketSize() << "\n";
  cout << "Total buckets: " << _hash_table.bucket_count() << "\n";
  cout << "Maximal length of word in dictionary: " << _max_length << "\n";
}

unsigned HashTable::longestBucketSize()
{
  unsigned num_buckets = _hash_table.bucket_count();

  int longest_bucket = -1;
  unsigned longest_bucket_size = 0;

  unsigned bucket_size;
  for (unsigned i = 0; i < num_buckets; i++)
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

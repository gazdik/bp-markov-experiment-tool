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
  for (unsigned i = 0; i < num_rows; i++)
  {
    row = &_flat_hash_table[i * row_size];

    bucket_size = _hash_table.bucket_size(i);

    if (bucket_size == 0)
    {
      row[0] = 0;
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
  cout << "Total buckets: " << _hash_table.bucket_count() << endl;

  printTable();
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

void HashTable::printTable()
{
//unsigned HashTable::Serialize(cl_uchar** hash_table, cl_uint& num_rows,
//                              cl_uint& num_entries, cl_uint& entry_size,
//                              cl_uint& row_size)
  cl_uchar *hash_table;
  cl_uint num_rows, num_entries, entry_size, row_size;

  Serialize(&hash_table, num_rows, num_entries, entry_size, row_size);
  hash_func hash;

  for (unsigned row = 0; row < num_rows; row++)
  {
    cl_uchar *table_row = &hash_table[row * row_size];


    cout << "Row: " << row << "\n";
    for (unsigned i = 0; i < num_entries; i++)
    {
      cl_uchar *entry = &table_row[i * entry_size + 1];
      cl_uchar entry_length = entry[-1];

      if (entry_length == 0)
        break;

      std::string word;
      for (int i = 0; i < entry_length; i++)
      {
        word.insert(word.end(), entry[i]);
      }
      unsigned hash_index = hash(word) % num_rows;
      cout << "[" << hash_index << "] " << word << "\n";
    }
  }


}

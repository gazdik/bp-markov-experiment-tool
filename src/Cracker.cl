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

#define CHARSET_SIZE 256
#define FLAG_NONE 0
#define FLAG_END 1
#define FLAG_CRACKED 2
#define FLAG_CRACKED_END 3

#define HT_EXTRA_BYTES 2
#define HT_LENGTH_OFFSET 0
#define HT_FLAG_OFFSET 1
#define HT_PAYLOAD_OFFSET 2
#define HT_FOUND 1
#define HT_NOTFOUND 0

#define PASS_EXTRA_BYTES 1
#define PASS_PAYLOAD_OFFSET 1
#define PASS_LENGTH_OFFSET 0

/**
 * Calc index to hash table for given string
 * @param  str c string
 * @param  str_length length of string
 * @param  num_rows number of rows in hash table
 * @return row index
 */
uint table_row_index (__global uchar *str, uchar str_length, uint num_rows)
{
  uint hash = 5381;

  for (int i = 0; i < str_length; i++)
  {
    hash = ((hash << 5) + hash) + str[i];
  }

  return hash % num_rows;
}

/**
 * Compare two strings
 * @return TRUE if the contents of both strings are equal, FALSE otherwise
 */
bool strcmp (__global const uchar *str1, uchar str1_length,
             __global const uchar *str2, uchar str2_length)
{
  if (str1_length != str2_length)
  {
    return false;
  }

  for (int i = 0; i < str1_length; i++)
  {
    if (str1[i] != str2[i])
    {
      return false;
    }
  }

  return true;
}

__kernel void cracker (__global uchar *passwords, uint password_entry_size,
                       __global uchar *hash_table, uint num_rows,
                       uint num_entries, uint entry_size, uint row_size)
{
  size_t id = get_global_id(0);
  __global uchar *password = &passwords[id * password_entry_size];
  uchar password_length = password[PASS_LENGTH_OFFSET];

  if (password_length == 0)
  {
    return;
  }

  uint row_index = table_row_index(&password[PASS_PAYLOAD_OFFSET],
                                   password_length, num_rows);
  __global uchar *table_row = &hash_table[row_index * row_size];

  for (int i = 0; i < num_entries; i++)
  {
    __global uchar *entry = &table_row[i * entry_size];
    uchar entry_length = entry[HT_LENGTH_OFFSET];

    if (entry_length == 0)
    {
      break;
    }

    if (strcmp(&password[PASS_PAYLOAD_OFFSET], password_length,
               &entry[HT_PAYLOAD_OFFSET], entry_length))
    {
      entry[HT_FLAG_OFFSET] = HT_FOUND;
      return;
    }
  }
}

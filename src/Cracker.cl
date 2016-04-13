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

uint table_row_index (__global uchar *password, uchar password_length, uint num_rows)
{
  ulong hash = 5381;

  for (int i = 0; i < password_length; i++)
  {
    hash = ((hash << 5) + hash) + password[i];
  }

  return hash % num_rows;
}

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
                       __global uchar *flag, __global const uchar *hash_table,
                       uint num_rows, uint num_entries, uint entry_size,
                       uint row_size)
{
  size_t id = get_global_id(0);

  // printf("num_rows: %u\n", num_rows);
  // printf("num_entries: %u\n", num_entries);
  // printf("entry_size: %u\n", entry_size);
  // printf("row_size: %u\n", row_size);

  __global uchar *password = &passwords[id * password_entry_size + 1];
  uchar password_length = password[-1];

  if (password_length == 0)
  {
    return;
  }

  uint row_index = table_row_index(password, password_length, num_rows);
  __global const uchar *table_row = &hash_table[row_index * row_size];

  for (int i = 0; i < num_entries; i++)
  {
    __global const uchar *entry = &table_row[i * entry_size + 1];
    uchar entry_length = entry[-1];

    if (entry_length == 0)
    {
      break;
    }

    if (strcmp(password, password_length, entry, entry_length))
    {
      if (flag[0] == FLAG_END)
        flag[0] = FLAG_CRACKED_END;
      else
        flag[0] = FLAG_CRACKED;

      return;
    }
  }

  password[-1] = 0;
}

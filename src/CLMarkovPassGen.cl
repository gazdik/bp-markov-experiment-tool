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
#define FLAG_END 1

__kernel void markovGenerator (__global uchar *passwords, __global uchar *flag,
                    __global ulong *indexes,
                    __global uchar *markov_table, __constant uint *thresholds,
                    __constant ulong *permutations, uint max_threshold,
                    uint max_length, uint step)
{
  // DEBUG
  // for (int i = 0; i < 2; i++)
  // {
  //   printf("indexes[%d] = %lu\n", i, indexes[i]);
  //   printf("markov_table[%d] = %c\n", i, markov_table[i]);
  //   printf("thresholds[%d] = %u\n", i, thresholds[i]);
  //   printf("permutations[%d] = %lu\n", i, permutations[i]);
  // }
  // printf("max_threshold = %u\n", max_threshold);
  // printf("max_length = %u\n", max_length);
  // printf("step = %u\n", step);


  uint password_length = max_length + 1;
  // prefetch(passwords, password_length * get_global_size(0));

  size_t id = get_global_id(0);
  ulong global_index = indexes[id];
  __global uchar *password = passwords + id * password_length;

  // Determine current length according to index
  uint length = 1;
  while (global_index >= permutations[length])
  {
    length++;
  }

  if (length > max_length)
  {
    flag[0] = FLAG_END;
    password[0] = 0;
    return;
  }

  // Convert global index to local index
  ulong index = global_index - permutations[length - 1];
  ulong partial_index;
  uchar last_char = 0;

  // Create password
  password[0] = length;
  for (int p = 0; p < length; p++)
  {
    partial_index = index % thresholds[p];
    index = index / thresholds[p];

    last_char = markov_table[p * CHARSET_SIZE * max_threshold
                             + last_char * max_threshold + partial_index];

    password[p + 1] = last_char;
  }

  indexes[id] = global_index + step;
}

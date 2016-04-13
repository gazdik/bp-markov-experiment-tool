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

#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>
#include <getopt.h>
#include <cstdlib>

#include <iostream>
#include <vector>
#include <string>

#include "Runner.h"
#include "Cracker.h"

using namespace std;

struct Options : Runner::Options
{
  bool help = false;
  bool verbose = false;
  bool list_platforms = false;
};

const char * help_msg = "clMarkovGen [OPTIONS]\n\n"
		"Informations:\n"
		"   -h, --help              prints this help\n"
		"   -v, --verbose           verbose mode\n"
    "   --list-platforms        print all available OpenCL platforms\n"
    "Common:\n"
    "   -p, --platform          platform number (default 0)\n"
    "   -g, --gws               global work size (default 1024)\n"
    "Cracker:\n"
		"   -d, --dictionary        dictionary with passwords to crack\n"
		"Generator:\n"
		"   -s, --statistics        file with statistics\n"
		"   -t, --thresholds=glob[:pos]\n"
		"                           number of characters per position\n"
		"         - glob - global value for every position in password\n"
    "         - pos - positional comma-separated values(overwrites global value)\n"
		"   -l, --length=min:max    length of password (default 1:64)\n"
		"   -m, --mask              mask\n"
    "   --model                 type of Markov model:\n"
    "         - classic - First-order Markov model (default)\n"
    "         - layered - Layered Markov model\n";

const struct option long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"list-platforms", no_argument, 0, 2},
	{"dictionary", required_argument, 0, 'd'},
	{"platform", required_argument, 0, 'p'},
	{"gws", required_argument, 0, 'g'},
	{"statistics", required_argument, 0, 's'},
	{"thresholds", required_argument, 0, 't'},
	{"length", required_argument, 0, 'l'},
	{"mask", required_argument, 0, 'm'},
	{"model", required_argument, 0, 1},
	{0,0,0,0}
};



int main(int argc, char *argv[])
{
  Options options;
  int opt, option_index;

  while ((opt = getopt_long(argc, argv, "hvp:g:d:s:t:l:m:", long_options,
                            &option_index)) != -1)
  {
    switch (opt)
    {
      case 1:
        options.model = optarg;
        break;
      case 2:
        options.list_platforms = true;
        break;
      case 'h':
        options.help = true;
        break;
      case 'v':
        options.verbose = true;
        break;
      case 'p':
        options.platform = atoi(optarg);
        break;
      case 'g':
        options.gws = atoi(optarg);
        break;
      case 'd':
        options.dictionary = optarg;
        break;
      case 's':
        options.stat_file = optarg;
        break;
      case 't':
        options.thresholds = optarg;
        break;
      case 'l':
        options.length = optarg;
        break;
      case 'm':
        options.mask = optarg;
        break;
      default:
        return (2);
        break;
    }
  }

  if (options.help)
  {
    cout << help_msg;
    return(1);
  }


  if (options.list_platforms)
  {
    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    for (int i = 0; i < platforms.size(); i++)
    {
      vector<cl::Device> devices;
      cout << "[" << i << "] " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

      platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);
      for (auto d : devices)
        cout << "  " <<  d.getInfo<CL_DEVICE_NAME>() << endl;
    }
    return(1);
  }

  if (options.stat_file.empty() || options.dictionary.empty())
  {
    cout << help_msg;
    return (2);
  }


  try
  {
    Runner runner { options };
    runner.Run();
  }
  catch (cl::Error &e)
  {
    cerr << "ERROR: " << e.what() << " (" << e.err() << ")" << endl;
    return(2);
  }

}

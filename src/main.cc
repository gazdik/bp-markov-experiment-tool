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
  bool list_platforms = false;
};

const char * help_msg = "clMarkovGen [OPTIONS]\n\n"
		"Informations:\n"
		"   -h, --help              display this help and exit\n"
		"   -v, --verbose           enable verbose mode\n"
    "   --list-platforms        display all available OpenCL platforms\n"
    "Common:\n"
    "   -D, --devices=platform[:device[,device]]\n"
    "         - platform - platform number (default 0),\n"
    "         - device - device number (default all available devices)\n"
    "   -g, --gws               global work size for all devices (default 1024000)\n"
    "Experiments:\n"
		"   -d, --dictionary        dictionary with passwords for evaluation\n"
    "   --load-factor           maximal load factor for the hash table (default 1) \n"
    "   -p, --print             print cracked passwords\n"
		"Generator:\n"
		"   -s, --statistics        file with statistics for a Markov model\n"
		"   -t, --thresholds=glob[:pos]\n"
		"                           number of characters per position\n"
		"         - glob - global value for every position in password\n"
    "         - pos - positional comma-separated values(overwrites global value)\n"
		"   -l, --length=min:max    length of password (default 1:50)\n"
		"   -m, --mask              mask\n"
    "   -M, --model             type of Markov model:\n"
    "         - classic - First-order Markov model (default)\n"
    "         - layered - Layered Markov model\n";

const struct option long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"dictionary", required_argument, 0, 'd'},
	{"devices", required_argument, 0, 'D'},
	{"gws", required_argument, 0, 'g'},
	{"statistics", required_argument, 0, 's'},
	{"thresholds", required_argument, 0, 't'},
	{"length", required_argument, 0, 'l'},
	{"mask", required_argument, 0, 'm'},
	{"print", no_argument, 0, 'p'},
	{"model", required_argument, 0, 'M'},
	{"list-platforms", no_argument, 0, 2},
	{"load-factor", required_argument, 0, 3},
	{0,0,0,0}
};



int main(int argc, char *argv[])
{
  Options options;
  int opt, option_index;

  while ((opt = getopt_long(argc, argv, "hvg:d:s:t:l:m:pD:M:", long_options,
                            &option_index)) != -1)
  {
    switch (opt)
    {
      case 'M':
        options.model = optarg;
        break;
      case 2:
        options.list_platforms = true;
        break;
      case 3:
        options.max_load_factor = atof(optarg);
        break;
      case 'h':
        options.help = true;
        break;
      case 'v':
        options.verbose = true;
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
      case 'p':
        options.print_passwords = true;
        break;
      case 'D':
        options.devices = optarg;
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

    for (unsigned i = 0; i < platforms.size(); i++)
    {
      vector<cl::Device> devices;
      cout << "[" << i << "] " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

      platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);
      for (unsigned i = 0; i < devices.size(); i++)
        cout << "  [" << i << "] " <<  devices[i].getInfo<CL_DEVICE_NAME>() << endl;
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

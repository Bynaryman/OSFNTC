/*
 * Copyright 2015,2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sstream>
#include <stdlib.h>

#include "AFU.h"

using std::string;
using std::stringstream;

int
main (int argc, char *argv[])
{
    if (argc < 3) {
        fprintf (stderr,
                 "Not enough arguments. Usage: ./afu port_number descriptor_file [parity] [jerror]\n");
        exit (1);
    }

    int
    port = 0;

    string descriptor_file (argv[2]);
    bool parity = false;
    bool jerror = false;

    stringstream ss;

    ss << argv[1];
    ss >> port;

    if (argc == 4 && string (argv[3]) == "parity") {
        printf ("MAIN: AFU parity enabled\n");
        parity = true;
    }

    if (argc == 4 && string (argv[3]) == "jerror") {
        printf ("MAIN: AFU will send jerror not running\n");
        jerror = true;
    }

    if (argc == 5 && string (argv[4]) == "jerror") {
        printf ("MAIN: AFU will send jerror not running\n");
        jerror = true;
    }

    AFU afu (port, descriptor_file, parity, jerror);

    afu.start ();
    debug_msg ("main: AFU quitting");
}

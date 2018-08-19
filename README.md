# Readme for source files from thesis

Øystein Dale, 2016-05-03

### Kernel source

The source code for the Linux kernel used in the thesis is located in the
folder named `omap/`. 

The tracing framework is located in `omap/kernel/trace/sepextracttracer`. Much
of this is based on code written by Stein Kristiansen, and modified as outlined
in the thesis. The file `static.c` contains the trace probes. `sepext_trace.c`
contains the working logic of the tracing framework, including the
`trace_entry()` function. `usnl.c` contains the code for the user-space
application that receives trace entries from the kernel and writes them to a
file.

The instrumented code for the bcmdhd driver is found in
`omap/drivers/net/wireless/bcmdhd`. The files `dhd_linux.c` and `dhd_sdio.c`
are the most important one in this thesis.

The locations of tracepoints throughout the kernel can be found in the file
`omap/sepextusage`.

A Makefile, other sources, and tools required to build this kernel is present
in the `kernelbuild` directory. Issuing the command `make` will build the
kernel using the cross-compilation toolchain, and output a `boot.img` which can
be booted on the Galaxy Nexus device. The command `make public_sources` will
download the actual sources should they be required.

### Analyzer source

The analyzer source code is present in the file named `analyser.go`. In
addition, the program `resolve.go` contains code to translate the addresses
contained in a trace file to readable addresses based on data generated during
compilation of the Linux kernel. A working installation of the Go language
(https://golang.org/) is required to execute these programs.

`trace.1218` is an example trace file. It contains the untranslated addresses
that were recorded by the tracing framework. These addresses are translated
using the `System.map` file generated during compilation of the Linux kernel.
`trace.1218.res` contains a translation that is made by executing:
    
    go run resolve.go trace.1218 > trace.1218.res 
    
`trace.1029.res` is an example input for the analysis program. The analysis is
executed with the command:

    go run analyse.go trace.1029.res
    
This will output signatures into the directory `signatures.mine`, which must be
present prior to executing the program.

### ns-3 source

The ns-3 source is found in the `ns-3.19` folder. The code for the processing
model is found in `ns-3.19/src/processing/model/`. The majority of this code is
Kristiansen's work. `rrscheduler.cc` and `sync.cc` are written entirely by me.
The other files have been modified by me where necessary to support simulation
of multicore execution.

Compiling ns-3 is done with the Waf tool. Issuing `waf configure build` will
configure and build the code. This will only build a minimal set of the files.
Change `.ns3rc` if more modules are required for future experiments. 

The final model for the communication software on the Galaxy Nexus smartphone
is found in the file `ns-3.19/gnex-min.device`.

A sample experiment can be executed by running the script `run_experiment.sh`
inside the `ns-3.19` directory.

The simulation script for the first experiment can be found in the file
`ns-3.19/scratch/gnex.cc`. `ns-3.19/scratch/scale.cc` contains the script for
the experiment used to test scalability of the processing model. The input for
the scalability experiments can be found in `ns-3.19/scaletest`. These
experiments are executed by running the various `scale_*.sh` scripts.

### Experiment script for the ad-hoc network rig

`experiment.sh` contains the script used to execute experiments on the rig.
This script is not designed nor intended for use on machines outside the
experiment rig. This script performs various tasks related to executing the
experiments, such as booting the phone with the correct boot image, setting up
and verifying the network, setting up traffic generation on the source machine
and phone, and executing each of the specified experiments. The various
parameters are specified as lists, and each combination of parameters is
executed at least once.

### Sample experiment data and processing scripts

The folder `2016-02-05T14:26:22` contains the output that is used to generate
the results for Experiment 1 and 3 in the thesis. This includes the pcap files
captured by tshark during the experiment, as well as the traces captured on the
device. The `scripts` folder contains various programs used to generate the
statistics from each experiment. These programs are highly dependent on the
structure of my own file system and are unlikely to work correctly on any other
machine. Nevertheless, they are provided for reference.

